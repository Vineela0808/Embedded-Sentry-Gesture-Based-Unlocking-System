//Team members
//Vineela Reddy Pippera Badguna(vp2504)
//Riya Shah(rs9177)
//Aviraj Dongare(aud211)
// Header files
#include <mbed.h>
#include "math.h"

// Custom header files interfacing with gyroscope, LCD on the STM32F429I Discovery board
#include "gyroscope.h"
#include "configuration.h"
#include "./drivers/LCD_DISCO_F429ZI.h"

// LCD object
LCD_DISCO_F429ZI lcd;
static BufferedSerial serial_port(USBTX, USBRX);

// Define onboard LEDs
DigitalOut led1(LED1);
DigitalOut led2(LED2);

// Define User Button
DigitalIn userButton(PA_0);

// Timer for button press duration
Timer buttonTimer;
bool buttonTimerStarted = false;

// Variables for button state tracking
int buttonPressed = 0;
int lastButtonPressed = 0;

// Constants for gesture recording
#define RECORD_DURATION 10.0f // 10 seconds
#define SAMPLE_INTERVAL 0.1f  // 100 ms interval
#define NUM_SAMPLES (int)(RECORD_DURATION / SAMPLE_INTERVAL) // Should be 100 samples

// Arrays to store gyroscope data
float recordedData[NUM_SAMPLES][3]; // Recorded key sequence
float unlockData[NUM_SAMPLES][3];   // Unlock attempt sequence

// Flag to check if key has been recorded
bool keyRecorded = false;

// Function prototypes
void setupGyro();
void initializeLCD();
void recordGesture(float data[][3], const char* message);
bool compareGestures(float data1[][3], float data2[][3]);

// Main function
int main() {
    // Initialize the Gyroscope
    setupGyro();

    // Initialize the LCD
    initializeLCD();

    int mode = 0; // 0 - Idle, 1 - Record Key, 2 - Unlock

    while (1) {
        buttonPressed = userButton.read();

        if (buttonPressed && !lastButtonPressed) {
            // Button was just pressed
            buttonTimer.reset();
            buttonTimer.start();
            buttonTimerStarted = true;
        } else if (!buttonPressed && lastButtonPressed) {
            // Button was just released
            buttonTimer.stop();
            float pressDuration = buttonTimer.read();
            buttonTimerStarted = false;

            if (pressDuration >= 2.0f) {
                // Long press - enter Record Key mode
                if (mode == 0) {
                    mode = 1; // Record Key mode

                    // Indicate to the user that recording is starting
                    lcd.Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetFont(&Font16);
                    lcd.DisplayStringAt(0, LINE(3), (uint8_t *)"Recording Key Gesture", CENTER_MODE);

                    // Record the key gesture with counter
                    recordGesture(recordedData, "Recording Key...");

                    // After recording, display "Key Recorded"
                    lcd.Clear(LCD_COLOR_WHITE);
                    lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"Key Recorded", CENTER_MODE);
                    ThisThread::sleep_for(2s);

                    keyRecorded = true;
                    mode = 0; // Return to Idle mode
                    initializeLCD(); // Go back to main menu
                }
            } else {
                // Short press - enter Unlock mode
                if (mode == 0) {
                    if (keyRecorded) {
                        mode = 2; // Unlock mode
                        lcd.Clear(LCD_COLOR_WHITE);
                        BSP_LCD_SetFont(&Font16);
                        lcd.DisplayStringAt(0, LINE(3), (uint8_t *)"Perform Unlock", CENTER_MODE);
                        lcd.DisplayStringAt(0, LINE(4), (uint8_t *)"Gesture Please...", CENTER_MODE);

                        // Record the unlock gesture with counter
                        recordGesture(unlockData, "Recording Unlock...");

                        // Compare unlockData to recordedData
                        bool success = compareGestures(recordedData, unlockData);
                        if (success) {
                            // Indicate success
                            led1 = 1;
                            led2 = 0;
                            lcd.Clear(LCD_COLOR_GREEN);
                            lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"Unlocked!", CENTER_MODE);
                        } else {
                            // Indicate failure
                            led1 = 0;
                            led2 = 1;
                            lcd.Clear(LCD_COLOR_RED);
                            lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"Try Again", CENTER_MODE);
                        }
                        ThisThread::sleep_for(2s);
                        mode = 0; // Return to Idle mode
                        initializeLCD(); // Go back to main menu
                    } else {
                        // No key recorded yet
                        lcd.Clear(LCD_COLOR_WHITE);
                        BSP_LCD_SetFont(&Font16);
                        lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"No Key Recorded", CENTER_MODE);
                        ThisThread::sleep_for(2s);
                        mode = 0;
                        initializeLCD(); // Go back to main menu
                    }
                }
            }
        } else {
            if (buttonTimerStarted && buttonTimer.read() >= 5.0f) {
                // If button is held for too long, reset
                buttonTimer.stop();
                buttonTimerStarted = false;
            }
        }

        lastButtonPressed = buttonPressed;

        ThisThread::sleep_for(10ms); // Small delay to prevent tight looping
    }

    return 0;
}

// Function to initialize the Gyroscope
void setupGyro() {
    // SPI configuration
    spi.format(8, 3);
    spi.frequency(1'000'000);

    // Initializing the gyroscope
    int Gyro_ID = Gyro_Init();
    printf("Gyro_ID: %d\n", Gyro_ID);
}

// Function to initialize the LCD
void initializeLCD() {
    lcd.Clear(LCD_COLOR_WHITE);
    BSP_LCD_SetFont(&Font16);
    lcd.DisplayStringAt(0, LINE(1), (uint8_t *)"Embedded Sentry", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(2), (uint8_t *)"Gesture-Based Lock", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"Hold Button 2s to", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(6), (uint8_t *)"Record Key", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"Tap Button to Unlock", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(13), (uint8_t *)"TEAM MEMBERS", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(14), (uint8_t *)"-----", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(15), (uint8_t *)"vp2504, rs9177", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(16), (uint8_t *)"aud211", CENTER_MODE);
    led1 = 0;
    led2 = 0;
}

// Function to record a gesture with counter display
void recordGesture(float data[][3], const char* message) {
    Timer recordTimer;
    recordTimer.start();

    int lastSecond = -1;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        float gyroData[3];
        Gyro_Get_XYZ(gyroData);
        data[i][0] = gyroData[0];
        data[i][1] = gyroData[1];
        data[i][2] = gyroData[2];

        int elapsedSeconds = (int)recordTimer.read();
        if (elapsedSeconds != lastSecond) {
            lastSecond = elapsedSeconds;
            // Update LCD with counter
            char counterMsg[30];
            sprintf(counterMsg, "%s %d s", message, elapsedSeconds);
            lcd.ClearStringLine(LINE(6));
            lcd.DisplayStringAt(0, LINE(6), (uint8_t *)counterMsg, CENTER_MODE);
        }

        ThisThread::sleep_for(chrono::milliseconds(int(SAMPLE_INTERVAL * 1000)));
    }

    recordTimer.stop();
}

// Function to compare two gestures
bool compareGestures(float data1[][3], float data2[][3]) {
    float totalError = 0.0f;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float diffX = data1[i][0] - data2[i][0];
        float diffY = data1[i][1] - data2[i][1];
        float diffZ = data1[i][2] - data2[i][2];
        float error = sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);
        totalError += error;
    }
    float averageError = totalError / NUM_SAMPLES;
    const float ERROR_THRESHOLD = 15.0f; // Adjust
    if (averageError <= ERROR_THRESHOLD) {
        return true;
    } else {
        return false;
    }
}
