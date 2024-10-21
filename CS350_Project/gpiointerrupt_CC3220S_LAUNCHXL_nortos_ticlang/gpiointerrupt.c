#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/I2C.h>

/* Driver configuration */
#include "ti_drivers_config.h"

// Define intervals in milliseconds
#define BUTTON_CHECK_INTERVAL 200
#define TEMP_CHECK_INTERVAL 500
#define UART_UPDATE_INTERVAL 1000

// Global variables for set-point and temperature
volatile int temperature = 20;  // Initial room temperature
volatile int setpoint = 22;     // Default set-point temperature
volatile int heat = 0;          // 0 = off, 1 = on
volatile uint32_t seconds = 0;  // Timer for seconds since reset

char output[64];                // UART output buffer

// Timer flags
volatile bool buttonFlag = false;
volatile bool tempFlag = false;
volatile bool uartFlag = false;

// UART handle
UART2_Handle uart;
UART2_Params uartParams;

// I2C handle
I2C_Handle i2c;
I2C_Params i2cParams;

// TMP102 address (assumed sensor address)
#define TMP102_ADDR 0x48

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for Button 0 press (increase setpoint).
 */
void gpioButtonFxn0(uint_least8_t index) {
    setpoint++;  // Increase set-point by 1 degree
}

/*
 *  ======== gpioButtonFxn1 ========
 *  Callback function for Button 1 press (decrease setpoint).
 */
void gpioButtonFxn1(uint_least8_t index) {
    setpoint--;  // Decrease set-point by 1 degree
}

/*
 *  ======== readTemperature ========
 *  Function to read temperature from the I2C temperature sensor.
 */
int readTemperature() {
    uint8_t txBuffer[1] = {0x00};   // TMP102 temperature register
    uint8_t rxBuffer[2];            // Buffer to store temperature data

    I2C_Transaction i2cTransaction;
    i2cTransaction.targetAddress = TMP102_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 2;

    if (I2C_transfer(i2c, &i2cTransaction)) {
        int temp = ((rxBuffer[0] << 4) | (rxBuffer[1] >> 4));  // Combine MSB and LSB
        if (temp > 0x7FF) {  // Negative temperature
            temp |= 0xF000;
        }
        return temp * 0.0625;  // TMP102 gives temp in 0.0625°C increments
    } else {
        return temperature;  // If I2C fails, return the last known temperature
    }
}

/*
 *  ======== timerCallback ========
 *  Timer interrupt callback to manage task flags.
 */
void timerCallback(Timer_Handle myHandle, int_fast16_t status) {
    static uint32_t msCounter = 0;

    msCounter += 200;  // Increment every 200ms

    if (msCounter % BUTTON_CHECK_INTERVAL == 0) buttonFlag = true;
    if (msCounter % TEMP_CHECK_INTERVAL == 0) tempFlag = true;
    if (msCounter % UART_UPDATE_INTERVAL == 0) {
        uartFlag = true;
        seconds++;  // Increment the seconds counter every second
    }
}

/*
 *  ======== initUART ========
 *  Initialize UART for output.
 */
void initUART() {
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    if (uart == NULL) {
        while (1);  // UART open failed
    }
}

/*
 *  ======== initTimer ========
 *  Initialize the timer for periodic interrupts.
 */
void initTimer() {
    Timer_Handle timer;
    Timer_Params params;

    Timer_Params_init(&params);
    params.period = 200000;  // 200ms in microseconds
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    timer = Timer_open(CONFIG_TIMER_0, &params);
    if (timer == NULL) {
        while (1);  // Timer open failed
    }

    if (Timer_start(timer) == Timer_STATUS_ERROR) {
        while (1);  // Timer start failed
    }
}

/*
 *  ======== initI2C ========
 *  Initialize the I2C peripheral for reading the temperature sensor.
 */
void initI2C() {
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;  // Set bit rate to 400 kHz
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);

    if (i2c == NULL) {
        while (1);  // I2C open failed
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0) {
    // Initialize drivers
    GPIO_init();
    initUART();
    initTimer();
    initI2C();

    // Configure the LED and button pins
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    // Install button callbacks
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);
    GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);

    // Enable interrupts for buttons
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_1);

    while (1) {
        if (buttonFlag) {
            // Check button flags and handle input every 200ms
            buttonFlag = false;
        }

        if (tempFlag) {
            // Read temperature from I2C sensor every 500ms
            tempFlag = false;
            temperature = readTemperature();

            if (temperature < setpoint) {
                heat = 1;  // Turn on heat
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            } else {
                heat = 0;  // Turn off heat
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            }
        }

        if (uartFlag) {
            // Output to UART every second
            uartFlag = false;

            snprintf(output, sizeof(output), "<%02d,%02d,%d,%04d>\n\r",
                     temperature, setpoint, heat, seconds);
            UART2_write(uart, output, sizeof(output), NULL);
        }
    }

    return NULL;
}
