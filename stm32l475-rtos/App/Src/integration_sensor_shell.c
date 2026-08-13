#include "integration_sensor_shell.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "integration_sensor_app.h"
#include "main.h"

#define SHELL_RX_BUFFER_SIZE 128u
#define SHELL_MAX_ARGS       8u
#define SHELL_CMD_ASYNC      4u

static uint8_t rx_byte;
static char rx_buffer[SHELL_RX_BUFFER_SIZE];
static volatile uint16_t rx_index;
static volatile uint8_t line_ready;
static volatile uint8_t rx_overflow;
static volatile uint8_t stream_enabled = 1u;

extern UART_HandleTypeDef huart1;

static int cmd_help(int argc, char **argv);
static int cmd_led(int argc, char **argv);
static int cmd_mode(int argc, char **argv);
static int cmd_reset(int argc, char **argv);
static int cmd_stream(int argc, char **argv);
static int cmd_status(int argc, char **argv);

typedef struct {
    const char *name;
    int (*function)(int argc, char **argv);
    const char *help;
} command_t;

static const command_t command_table[] = {
    { "help", cmd_help, "Zeigt alle verfuegbaren Befehle" },
    { "led", cmd_led, "Schaltet LED: led <on|off>" },
    { "mode", cmd_mode, "Setzt Sensor-Mode: mode <low|normal|high>" },
    { "reset", cmd_reset, "Triggert Sensor-Reset" },
    { "stream", cmd_stream, "Sensorwerte: stream <on|off>" },
    { "status", cmd_status, "Liest die Sensorregister aus" }
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(command_table[0]))

static void shell_print(const char *text) {
    if (text != NULL) {
        HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
    }
}

static int queue_sensor_command(app_sensor_command_t command) {
    if (app_sensor_command_submit(command) != OS_OK) {
        shell_print("ERROR,SHELL,SENSOR_COMMAND_QUEUE_FULL\r\n");
        return -1;
    }

    return SHELL_CMD_ASYNC;
}

void shell_init(void) {
    rx_index = 0u;
    line_ready = 0u;
    rx_overflow = 0u;

    shell_print("\r\n--- STM32 Shell bereit ---\r\nCLI> ");

    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1u) != HAL_OK) {
        shell_print("ERROR,UART,RX_START_FAILED\r\n");
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart != &huart1) {
        return;
    }

    if (!line_ready) {
        if ((rx_byte == '\r') || (rx_byte == '\n')) {
            if (rx_index > 0u) {
                rx_buffer[rx_index] = '\0';
                line_ready = 1u;
            }
        } else if ((rx_byte == '\b') || (rx_byte == 0x7fu)) {
            if (rx_index > 0u) {
                rx_index--;
            }
        } else if (rx_index < (SHELL_RX_BUFFER_SIZE - 1u)) {
            rx_buffer[rx_index++] = (char)rx_byte;
        } else {
            rx_overflow = 1u;
        }
    }

    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1u);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1u);
    }
}

void shell_update(void) {
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    if (rx_overflow) {
        rx_index = 0u;
        line_ready = 0u;
        rx_overflow = 0u;
        shell_print("\r\nERROR,SHELL,LINE_TOO_LONG\r\nCLI> ");
        return;
    }

    if (!line_ready) {
        return;
    }

    shell_print("\r\n");

    char *token = strtok(rx_buffer, " ");
    while ((token != NULL) && (argc < (int)SHELL_MAX_ARGS)) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    int cmd_result = 0;

    if (argc > 0) {
        uint8_t found = 0u;

        for (size_t i = 0u; i < NUM_COMMANDS; i++) {
            if (strcmp(argv[0], command_table[i].name) == 0) {
                cmd_result = command_table[i].function(argc, argv);
                found = 1u;
                break;
            }
        }

        if (!found) {
            shell_print("ERROR,SHELL,UNKNOWN_COMMAND\r\n");
        }
    }

    rx_index = 0u;
    line_ready = 0u;
    if (cmd_result != SHELL_CMD_ASYNC) {
        shell_print("CLI> ");
    }
}

static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    shell_print("Verfuegbare Befehle:\r\n");
    for (size_t i = 0u; i < NUM_COMMANDS; i++) {
        char text[128];
        snprintf(
            text, sizeof(text), "  %-10s - %s\r\n", command_table[i].name, command_table[i].help);
        shell_print(text);
    }

    return 0;
}

static int cmd_led(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: led <on|off>\r\n");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0) {
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
        shell_print("RESP,LED,ON,OK\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
        shell_print("RESP,LED,OFF,OK\r\n");
    } else {
        shell_print("ERROR,SHELL,INVALID_LED_MODE\r\n");
        return -1;
    }

    return 0;
}

static int cmd_mode(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: mode <low|normal|high>\r\n");
        return -1;
    }

    if (strcmp(argv[1], "low") == 0) {
        return queue_sensor_command(APP_SENSOR_CMD_MODE_LOW);
    }
    if (strcmp(argv[1], "normal") == 0) {
        return queue_sensor_command(APP_SENSOR_CMD_MODE_NORMAL);
    }
    if (strcmp(argv[1], "high") == 0) {
        return queue_sensor_command(APP_SENSOR_CMD_MODE_HIGH);
    }

    shell_print("ERROR,SHELL,INVALID_SENSOR_MODE\r\n");
    return -1;
}

static int cmd_reset(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return queue_sensor_command(APP_SENSOR_CMD_RESET);
}

static int cmd_stream(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Nutzung: stream <on|off>\r\n");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0) {
        stream_enabled = 1u;
        shell_print("RESP,STREAM,ON,OK\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        stream_enabled = 0u;
        shell_print("RESP,STREAM,OFF,OK\r\n");
    } else {
        shell_print("ERROR,SHELL,INVALID_STREAM_MODE\r\n");
        return -1;
    }

    return 0;
}

static int cmd_status(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return queue_sensor_command(APP_SENSOR_CMD_STATUS);
}

uint8_t is_stream_enabled(void) {
    return stream_enabled;
}