/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
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
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Timer.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define DOT_DURATION 1   // One timer period (500ms) for a dot
#define DASH_DURATION 3  // Three timer periods for a dash
#define INTER_ELEMENT_GAP 1 // One timer period for gaps between dots and dashes

volatile int messageType = 0;  // 0 for "SOS", 1 for "OK"
volatile int messageComplete = 1;  // 1 if the message is complete and ready to change
volatile int blinkState = 0;  // State machine for blinking
volatile int currentSymbolIndex = 0;

// Morse code for SOS (... --- ...)
const int sosPattern[] = { DOT_DURATION, INTER_ELEMENT_GAP, DOT_DURATION, INTER_ELEMENT_GAP, DOT_DURATION, INTER_ELEMENT_GAP,
                           DASH_DURATION, INTER_ELEMENT_GAP, DASH_DURATION, INTER_ELEMENT_GAP, DASH_DURATION, INTER_ELEMENT_GAP,
                           DOT_DURATION, INTER_ELEMENT_GAP, DOT_DURATION, INTER_ELEMENT_GAP, DOT_DURATION };

// Morse code for OK (--- -.-)
const int okPattern[] = { DASH_DURATION, INTER_ELEMENT_GAP, DASH_DURATION, INTER_ELEMENT_GAP, DASH_DURATION,
                          INTER_ELEMENT_GAP, DOT_DURATION, INTER_ELEMENT_GAP, DASH_DURATION, INTER_ELEMENT_GAP, DOT_DURATION };

volatile const int* currentPattern = sosPattern;
volatile int currentPatternLength = sizeof(sosPattern) / sizeof(sosPattern[0]);

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 */
void gpioButtonFxn0(uint_least8_t index) {
    if (messageComplete) {
        messageType = !messageType;  // Toggle between 0 (SOS) and 1 (OK)
        messageComplete = 0;         // Mark the message as not complete
        currentSymbolIndex = 0;      // Reset the state machine

        // Update the pattern and length based on the messageType
        if (messageType == 0) {
            currentPattern = sosPattern;
            currentPatternLength = sizeof(sosPattern) / sizeof(sosPattern[0]);
        } else {
            currentPattern = okPattern;
            currentPatternLength = sizeof(okPattern) / sizeof(okPattern[0]);
        }
    }
}

/*
 *  ======== timerCallback ========
 *  Timer callback function called every 500 ms.
 */
void timerCallback(Timer_Handle myHandle, int_fast16_t status) {
    if (currentSymbolIndex < currentPatternLength) {
        if (blinkState == 0) {
            // Turn on LED (green for "SOS", red for "OK")
            if (messageType == 0) {
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            } else {
                GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON);
            }
            blinkState = 1;
        } else {
            // Turn off LED
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
            blinkState = 0;
            currentSymbolIndex++;
        }
    } else {
        // Message complete, reset for next cycle
        messageComplete = 1;
        currentSymbolIndex = 0;
    }
}

/*
 *  ======== initTimer ========
 */
void initTimer(void) {
    Timer_Handle timer0;
    Timer_Params params;
    Timer_init();
    Timer_Params_init(&params);
    params.period = 500000;  // 500000 us (500 ms)
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) {
        /* Failed to initialize timer */
        while (1) {}
    }
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        /* Failed to start timer */
        while (1) {}
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0) {
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn off LEDs initially */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);

    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    /* Initialize the timer */
    initTimer();

    /*
     *  If more than one input pin is available for your device, interrupts
     *  will be enabled on CONFIG_GPIO_BUTTON_1.
     */
    if (CONFIG_GPIO_BUTTON_0 != CONFIG_GPIO_BUTTON_1) {
        /* Configure BUTTON1 pin */
        GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

        /* Install Button callback */
        GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn0);
        GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
    }

    return (NULL);
}
