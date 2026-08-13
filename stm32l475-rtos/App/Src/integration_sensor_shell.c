/**
 * @file integration_sensor_shell.c
 * @brief UART receive callbacks and interactive sensor command parser.
 * @author Jerome
 * @author Martin
 *
 *
 * The HAL receive interrupt collects one line without blocking. The output
 * task polls @ref shell_update, tokenizes complete lines, and dispatches the
 * command table. Sensor I/O commands are queued for the sensor task.
 */

#include "integration_sensor_shell.h"

#include "project.h"

#if PROJECT == PROJECT_SENSOR

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "integration_sensor_app.h"
#include "integration_sensor_internal.h"
#include "main.h"

#define SHELL_RX_BUFFER_SIZE 128u /**< Input line capacity including terminator. */
#define SHELL_MAX_ARGS       8u   /**< Maximum whitespace-separated arguments. */

/** @brief Byte currently armed for interrupt-driven reception. */
static uint8_t rx_byte;

/** @brief Input line assembled by the UART receive callback. */
static char rx_buffer[SHELL_RX_BUFFER_SIZE];

/** @brief Next writable position in @ref rx_buffer. */
static volatile uint16_t rx_index;

/** @brief Nonzero when @ref rx_buffer contains a complete line. */
static volatile uint8_t line_ready;

/** @brief Nonzero when input exceeded @ref SHELL_RX_BUFFER_SIZE. */
static volatile uint8_t rx_overflow;

/** @brief Nonzero while periodic @c DATA records are enabled. */
static volatile uint8_t stream_enabled = 1u;

/** @brief UART shared by receive callbacks and the output task. */
extern UART_HandleTypeDef huart1;

/** @brief Print all supported commands. */
static int cmd_help(int argc, char **argv);

/** @brief Set the board LED state. */
static int cmd_led(int argc, char **argv);

/** @brief Queue a sensor sampling-mode change. */
static int cmd_mode(int argc, char **argv);

/** @brief Queue a sensor reset. */
static int cmd_reset(int argc, char **argv);

/** @brief Enable or disable periodic sensor records. */
static int cmd_stream(int argc, char **argv);

/** @brief Queue a sensor status request. */
static int cmd_status(int argc, char **argv);

/** @brief Command name, handler, and help-text entry. */
typedef struct {
    const char *name;                       /**< Command token. */
    int (*function)(int argc, char **argv); /**< Command handler. */
    const char *help;                       /**< One-line help text. */
} command_t;

/** @brief Commands recognized by the shell. */
static const command_t command_table[] = {
    { "help", cmd_help, "Zeigt alle verfuegbaren Befehle" },
    { "led", cmd_led, "Schaltet LED: led <on|off>" },
    { "mode", cmd_mode, "Setzt Sensor-Mode: mode <low|normal|high>" },
    { "reset", cmd_reset, "Triggert Sensor-Reset" },
    { "stream", cmd_stream, "Sensorwerte: stream <on|off>" },
    { "status", cmd_status, "Liest die Sensorregister aus" }
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(command_table[0])) /**< Table length. */

/**
 * @brief Write shell-owned text directly to the UART.
 * @param text Null-terminated text to transmit.
 */
static void shell_print(const char *text) {
    if ((text != NULL) &&
        (HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY) !=
         HAL_OK)) {
        sensor_app_record_error();
    }
}

/**
 * @brief Queue a device command and report queue saturation.
 * @param command Command to submit.
 * @return Zero on success, otherwise @c -1.
 */
static int queue_sensor_command(app_sensor_command_t command) {
    if (app_sensor_command_submit(command) != OS_OK) {
        shell_print("ERROR,SHELL,SENSOR_COMMAND_QUEUE_FULL\r\n");
        return -1;
    }

    return 0;
}

void shell_init(void) {
    rx_index = 0u;
    line_ready = 0u;
    rx_overflow = 0u;

    shell_print("\r\n--- STM32 Shell bereit ---\r\n");

    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1u) != HAL_OK) {
        shell_print("ERROR,UART,RX_START_FAILED\r\n");
    }
}

/**
 * @brief Collect one received UART byte into the shell input line.
 * @param huart UART whose receive operation completed.
 */
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

/**
 * @brief Recover interrupt reception after a UART error.
 * @param huart UART reporting the error.
 */
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
        shell_print("\r\nERROR,SHELL,LINE_TOO_LONG\r\n");
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

    if (argc > 0) {
        uint8_t found = 0u;

        for (size_t i = 0u; i < NUM_COMMANDS; i++) {
            if (strcmp(argv[0], command_table[i].name) == 0) {
                (void)command_table[i].function(argc, argv);
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
}

/**
 * @brief Implement the @c help command.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero.
 */
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

/**
 * @brief Implement @c led on and @c led off.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero on success, otherwise @c -1.
 */
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

/**
 * @brief Implement @c mode low, @c mode normal, and @c mode high.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero on success, otherwise @c -1.
 */
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

/**
 * @brief Implement the asynchronous @c reset command.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero when queued, otherwise @c -1.
 */
static int cmd_reset(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return queue_sensor_command(APP_SENSOR_CMD_RESET);
}

/**
 * @brief Implement @c stream on and @c stream off.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero on success, otherwise @c -1.
 */
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

/**
 * @brief Implement the asynchronous @c status command.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero when queued, otherwise @c -1.
 */
static int cmd_status(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return queue_sensor_command(APP_SENSOR_CMD_STATUS);
}

uint8_t sensor_shell_stream_enabled(void) {
    return stream_enabled;
}

#endif /* PROJECT == PROJECT_SENSOR */
