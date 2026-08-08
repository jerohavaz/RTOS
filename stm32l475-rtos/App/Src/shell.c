#include "shell.h"
#include "hardware.h" 
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32l4xx_hal_i2c.h"
#include "stm32l4xx_hal_uart.h"

#define SHELL_RX_BUFFER_SIZE 128
#define SHELL_MAX_ARGS        8

/* --- Sensor I2C Register-Konfiguration LSM6DSL --- */
#define SENSOR_I2C_ADDR            (0x6A << 1) // 0xD4 (LSM6DSL I2C-Adresse)
#define SENSOR_CTRL_REG1           0x10        // CTRL1_XL (Accel Control)
#define SENSOR_CTRL_REG2           0x11        // CTRL2_G  (Gyro Control)
#define SENSOR_CTRL_REG3           0x12        // CTRL3_C  (SW_RESET liegt auf Bit 0)
#define SENSOR_CTRL_RESET_BIT      (1 << 0)    // SW_RESET Bit

extern I2C_HandleTypeDef hi2c2;
static UART_HandleTypeDef *shell_huart;
static uint8_t rx_byte; // Directly encapsulated in module
static char rx_buffer[SHELL_RX_BUFFER_SIZE];
static uint8_t rx_index = 0;
static volatile uint8_t line_ready = 0;
static uint8_t stream_enabled = 0;

/* --- Befehls-Prototypen & Strukturen --- */
static int cmd_help(int argc, char **argv);
static int cmd_led(int argc, char **argv);
static int cmd_mode(int argc, char **argv);
static int cmd_reset(int argc, char **argv);
static int cmd_stream(int argc, char **argv);
static int cmd_status(int argc, char **argv);

typedef struct {
    const char *name;
    int (*func)(int argc, char **argv);
    const char *help;
} Command_t;

static const Command_t command_table[] = {
    {"help",  cmd_help,  "Zeigt alle verfuegbaren Befehle"},
    {"led",   cmd_led,   "Schaltet LED: led <on|off>"},
    {"mode",  cmd_mode,  "Setzt Sensor-Mode: mode <low|normal|high>"},
    {"reset", cmd_reset, "Triggert Sensor-Reset"},
    {"stream", cmd_stream, "Schalter für Sensorwerte <on|off>"},
    {"status", cmd_status, "Auslesen der Sensorregister"},
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(Command_t))

static void shell_print(const char *str) {
    HAL_UART_Transmit(shell_huart, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

/* --- Initialisierung --- */
void shell_init(UART_HandleTypeDef *huart) {
    shell_huart = huart;
    shell_print("\r\n--- STM32 Shell Bereit ---\r\nCLI> ");
    
    // Interrupt-Empfang direkt beim Start aktivieren
    HAL_UART_Receive_IT(shell_huart, &rx_byte, 1);
}

/* --- HAL Weak-Callback Überschreibung --- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (shell_huart != NULL && huart->Instance == shell_huart->Instance) {
        if (!line_ready) {
            if (rx_byte == '\r' || rx_byte == '\n') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    line_ready = 1;
                    shell_print("\r\n");
                }
            } else if (rx_byte == '\b' || rx_byte == 0x7F) {
                if (rx_index > 0) {
                    rx_index--;
                    shell_print("\b \b");
                }
            } else if (rx_index < SHELL_RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
                HAL_UART_Transmit(shell_huart, &rx_byte, 1, HAL_MAX_DELAY); // Echo
            }
        }
        // Interrupt sofort für das nächste Zeichen neu scharfschalten
        HAL_UART_Receive_IT(shell_huart, &rx_byte, 1);
    }
}

/* --- Verarbeitungslogik im Main-Loop / RTOS-Task --- */
void shell_update(void) {
    if (!line_ready) return;

    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    char *token = strtok(rx_buffer, " ");
    while (token != NULL && argc < SHELL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc > 0) {
        uint8_t found = 0;
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (strcmp(argv[0], command_table[i].name) == 0) {
                command_table[i].func(argc, argv);
                found = 1;
                break;
            }
        }
        if (!found) {
            shell_print("Unbekannter Befehl. 'help' eingeben.\r\n");
        }
    }

    rx_index = 0;
    line_ready = 0;
    shell_print("CLI> ");
}

/* --- Befehls-Implementierungen --- */

static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("Verfuegbare Befehle:\r\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  %-10s - %s\r\n", command_table[i].name, command_table[i].help);
        shell_print(buf);
    }
    return 0;
}

static int cmd_led(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: led <on|off>\r\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        shell_print("LED ist AN\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        shell_print("LED ist AUS\r\n");
    } else {
        shell_print("Ungueltiger Parameter!\r\n");
    }
    return 0;
}

/* --- Befehl: Sensor Mode umschalten --- */
static int cmd_mode(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: mode <low|normal|high>\r\n");
        return -1;
    }

    uint8_t ctrl_reg1 = 0;
    uint8_t ctrl_reg2 = 0;

    if (strcmp(argv[1], "low") == 0) {
        ctrl_reg1 = 0x10;
        ctrl_reg2 = 0x10;
        shell_print("Setze Sensor auf Low-Power Mode (12,5 Hz)...\r\n");
    } else if (strcmp(argv[1], "normal") == 0) {
        ctrl_reg1 = 0x40;
        ctrl_reg2 = 0x40;
        shell_print("Setze Sensor auf Normal Mode (104 Hz)...\r\n");
    } else if (strcmp(argv[1], "high") == 0) {
        ctrl_reg1 = 0x90;
        ctrl_reg2 = 0x90;
        shell_print("Setze Sensor auf High-Performance Mode (1,66 kHz)...\r\n");
    } else {
        shell_print("Ungueltiger Modus!\r\n");
        return -1;
    }

    // Register 1 & 2 über I2C2 schreiben
    if (HAL_I2C_Mem_Write(&hi2c2, SENSOR_I2C_ADDR, SENSOR_CTRL_REG1, I2C_MEMADD_SIZE_8BIT, &ctrl_reg1, 1, 100) == HAL_OK &&
        HAL_I2C_Mem_Write(&hi2c2, SENSOR_I2C_ADDR, SENSOR_CTRL_REG2, I2C_MEMADD_SIZE_8BIT, &ctrl_reg2, 1, 100) == HAL_OK) {
        shell_print("Sensor-Modus erfolgreich aktualisiert.\r\n");
    } else {
        shell_print("Fehler beim Schreiben ueber I2C!\r\n");
    }

    return 0;
}

/* --- Befehl: Sensor Reset triggern --- */
static int cmd_reset(int argc, char **argv) {
    (void)argc; (void)argv;
    
    shell_print("Sende Soft-Reset an LSM6DSL...\r\n");

    uint8_t reg3_val = SENSOR_CTRL_RESET_BIT;
    
    if (HAL_I2C_Mem_Write(&hi2c2, SENSOR_I2C_ADDR, SENSOR_CTRL_REG3, I2C_MEMADD_SIZE_8BIT, &reg3_val, 1, 200) == HAL_OK) {
        shell_print("Sensor Soft-Reset erfolgreich ausgeloest.\r\n");
    } else {
        shell_print("Fehler beim Senden des Reset-Befehls!\r\n");
    }

    return 0;
}


void USART1_IRQHandler(void){
    if(shell_huart != 0){
    HAL_UART_IRQHandler(shell_huart);
    }
}




static int cmd_stream(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: stream <on|off>\r\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        stream_enabled = 1;
        shell_print("Sensordaten-Stream AKTIVIERT.\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        stream_enabled = 0;
        shell_print("Sensordaten-Stream DEAKTIVIERT.\r\n");
    }
    return 0;
}

static int cmd_status(int argc, char **argv) {
    (void)argc; (void)argv;
    
    uint8_t reg1_val = 0;
    uint8_t reg2_val = 0;

    // Timeout leicht erhöhen (z. B. 200ms), falls sensor_task gerade sendet
    if (HAL_I2C_Mem_Read(&hi2c2, SENSOR_I2C_ADDR, SENSOR_CTRL_REG1, I2C_MEMADD_SIZE_8BIT, &reg1_val, 1, 200) == HAL_OK &&
        HAL_I2C_Mem_Read(&hi2c2, SENSOR_I2C_ADDR, SENSOR_CTRL_REG2, I2C_MEMADD_SIZE_8BIT, &reg2_val, 1, 200) == HAL_OK) {
        
        char buf[128];
        snprintf(buf, sizeof(buf), "LSM6DSL Register-Status:\r\n  CTRL1_XL: 0x%02X\r\n  CTRL2_G:  0x%02X\r\n", reg1_val, reg2_val);
        shell_print(buf);
    } else {
        shell_print("Fehler beim Lesen der I2C-Register! (Adresse/Bus blockiert)\r\n");
    }

    return 0;
}

uint8_t is_stream_enabled(void) {
    return stream_enabled;
}