/*
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== uart2echo.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    char input;                    // Serial buffer (1 byte)
    unsigned char state = 0;        // State variable (1 byte)
    const char echoPrompt[] = "Type ON to turn on the LED and OFF to turn off the LED:\r\n";
    UART2_Handle uart;
    UART2_Params uartParams;
    size_t bytesRead;
    size_t bytesWritten = 0;
    uint32_t status     = UART2_STATUS_SUCCESS;

    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Create a UART where the default read and write mode is BLOCKING */
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;

    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    if (uart == NULL)
    {
        /* UART2_open() failed */
        while (1) {}
    }

    /* Turn on user LED to indicate successful initialization */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    UART2_write(uart, echoPrompt, sizeof(echoPrompt), &bytesWritten);

    /* Loop forever to process input */
    while (1)
    {
        bytesRead = 0;
        /* Read a single character */
        status = UART2_read(uart, &input, 1, &bytesRead);

        if (status != UART2_STATUS_SUCCESS || bytesRead == 0)
        {
            /* UART2_read() failed */
            while (1) {}
        }

        /* Echo the character back */
        bytesWritten = 0;
        status = UART2_write(uart, &input, 1, &bytesWritten);
        if (status != UART2_STATUS_SUCCESS || bytesWritten == 0)
        {
            /* UART2_write() failed */
            while (1) {}
        }

        /* State machine to check for "ON" or "OFF" */
        switch (state)
        {
            case 0: // Waiting for 'O'
                if (input == 'O')
                    state = 1;
                break;

            case 1: // Waiting for 'N' (for ON) or 'F' (for OFF)
                if (input == 'N')
                {
                    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);  // Turn LED on
                    state = 0; // Reset state after successful "ON"
                }
                else if (input == 'F')
                    state = 2;
                else
                    state = 0;  // Reset if not matching expected input
                break;

            case 2: // Waiting for 'F' (second 'F' in OFF)
                if (input == 'F')
                    state = 3;
                else
                    state = 0;  // Reset if not matching expected input
                break;

            case 3: // Waiting for 'F' (final 'F' in OFF)
                if (input == 'F')
                {
                    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);  // Turn LED off
                    state = 0;  // Reset state after successful "OFF"
                }
                else
                    state = 0;  // Reset if not matching expected input
                break;

            default:
                state = 0;  // Default to reset state
                break;
        }
    }
}
