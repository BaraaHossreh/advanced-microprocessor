// ============================================================================
//                                 LIBRARIES
// ============================================================================
// Standard C libraries for variable types (uint32_t) and booleans (true/false)
#include <stdint.h>
#include <stdbool.h>
// Standard I/O library: Used for 'sprintf' to format text strings
#include <stdio.h>

// Hardware Memory Map: Defines the base addresses of all peripherals (GPIO, Timer, etc.)
#include "inc/hw_memmap.h"
// Hardware Types: Defines common data types and macros for register access
#include "inc/hw_types.h"
// Hardware Interrupts: Defines the names of interrupts (like INT_TIMER0A)
#include "inc/hw_ints.h"

// Driver Libraries: These contain the functions to control the hardware
#include "driverlib/sysctl.h"   // System Control (Clock settings)
#include "driverlib/gpio.h"     // General Purpose Input/Output (Pins)
#include "driverlib/interrupt.h"// Interrupt Controller (NVIC)
#include "driverlib/timer.h"    // Hardware Timer
#include "driverlib/adc.h"      // Analog-to-Digital Converter
#include "driverlib/pin_map.h"  // Pin Mapping (Alternative functions)

// ============================================================================
//                             PIN DEFINITIONS
// ============================================================================

// --- LCD Control Pins (Port E) ---
#define LCD_CTRL_PORT       GPIO_PORTE_BASE  // The memory address for Port E
// We use Pins 1, 2, and 3 for Control (RS, RW, EN)
#define LCD_CTRL_PINS       (GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

// --- LCD Data Pins (Port B) ---
#define LCD_DATA_PORT       GPIO_PORTB_BASE  // The memory address for Port B
// We use Pins 4, 5, 6, 7 for Data (4-bit mode)
#define LCD_DATA_PINS       (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7)

// ============================================================================
//                             GLOBAL VARIABLES
// ============================================================================
// 'volatile' tells the computer: "Do not optimize these! They change automatically inside interrupts."
volatile uint32_t g_ui32Hours = 12;   // Stores the current Hour
volatile uint32_t g_ui32Minutes = 0;  // Stores the current Minute
volatile uint32_t g_ui32Seconds = 0;  // Stores the current Second

// This flag acts as a signal.
// False = No new time. True = Time has changed, update the screen!
volatile bool g_bUpdateScreen = true;

// ============================================================================
//                           HELPER FUNCTIONS
// ============================================================================

// Function to create a small delay. 'us' = microseconds.
void delay_us(uint32_t us) {
    // SysCtlClockGet() returns the speed (80,000,000 Hz).
    // Dividing by 3,000,000 gives us roughly 1 microsecond worth of loops.
    SysCtlDelay((SysCtlClockGet() / 3000000) * us);
}

// ============================================================================
//                           LCD DRIVER LOGIC
// ============================================================================

// Function to "Pulse" the Enable (EN) pin.
// This acts like pressing the "Enter" key on a keyboard. It tells the LCD to read data.
void LCD_Pulse_EN(void) {
    // Set Pin 3 (EN) High (1)
    GPIOPinWrite(LCD_CTRL_PORT, GPIO_PIN_3, GPIO_PIN_3);
    delay_us(10); // Wait a tiny bit for signal to stabilize
    // Set Pin 3 (EN) Low (0)
    GPIOPinWrite(LCD_CTRL_PORT, GPIO_PIN_3, 0);
    delay_us(10); // Wait before sending next command
}

// Function to send 4 bits (half a byte) to the LCD data pins
void LCD_Send_Nibble(uint8_t nibble) {
    // We take the 4 bits of data and shift them Left by 4 (<< 4).
    // This moves them from positions 0-3 to positions 4-7, matching our physical pins (PB4-PB7).
    GPIOPinWrite(LCD_DATA_PORT, LCD_DATA_PINS, (nibble << 4));
    // Pulse Enable so the LCD reads these 4 bits
    LCD_Pulse_EN();
}

// Function to send a full Byte (8 bits) to the LCD
// 'is_data' = true means we are printing text. 'is_data' = false means we are sending a command.
void LCD_Send_Byte(uint8_t byte, bool is_data) {
    // Step 1: Set the Register Select (RS) pin (Pin 1)
    // If is_data is true, set Pin 1 High. If false, set Pin 1 Low.
    GPIOPinWrite(LCD_CTRL_PORT, GPIO_PIN_1, is_data ? GPIO_PIN_1 : 0);

    // Step 2: Send the top 4 bits (High Nibble)
    LCD_Send_Nibble(byte >> 4);

    // Step 3: Send the bottom 4 bits (Low Nibble)
    LCD_Send_Nibble(byte & 0x0F);

    // Step 4: Small delay to let LCD process the character
    delay_us(50);
}

// Function to Initialize the LCD hardware
void LCD_Init(void) {
    // Turn on the clock for Port B and Port E
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    // Wait until Port B is ready to be used
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));

    // Configure Pins as Outputs (We send signals OUT to the LCD)
    GPIOPinTypeGPIOOutput(LCD_CTRL_PORT, LCD_CTRL_PINS);
    GPIOPinTypeGPIOOutput(LCD_DATA_PORT, LCD_DATA_PINS);

    // Ensure Read/Write (RW) pin is Low (Write mode)
    GPIOPinWrite(LCD_CTRL_PORT, GPIO_PIN_2, 0);

    // --- Start of "Magic" Setup Sequence ---
    // The LCD needs specific wake-up calls to enter 4-bit mode.
    delay_us(50000);              // Wait for power up
    LCD_Send_Nibble(0x03); delay_us(5000); // Wake up 1
    LCD_Send_Nibble(0x03); delay_us(200);  // Wake up 2
    LCD_Send_Nibble(0x03); delay_us(200);  // Wake up 3
    LCD_Send_Nibble(0x02); delay_us(200);  // Switch to 4-bit mode!

    // --- Configure Settings ---
    LCD_Send_Byte(0x28, false); // Command: 4-bit mode, 2 lines
    LCD_Send_Byte(0x0C, false); // Command: Turn Display ON, Hide Cursor
    LCD_Send_Byte(0x01, false); // Command: Clear the screen completely
    delay_us(2000);             // Clear screen needs extra time
    LCD_Send_Byte(0x06, false); // Command: Entry Mode (Auto-move cursor right)
}

// Function to move the cursor to a specific spot
void LCD_SetCursor(uint8_t col, uint8_t row) {
    // 0x80 is the base command for cursor position.
    // If row is 1, we add 0x40 (the address of the second line).
    // Then we add 'col' to move sideways.
    LCD_Send_Byte(0x80 + (row * 0x40) + col, false);
}

// Function to print a string of text
void LCD_Print(char *str) {
    // Loop through the string until we hit the end (null terminator)
    while(*str) {
        // Send the character, then move to the next one
        LCD_Send_Byte(*str++, true);
    }
}

// ============================================================================
//                         TIMER INTERRUPT (THE CLOCK)
// ============================================================================
// This function is NEVER called by main. The Hardware calls it every 1 second.
void Timer0_ISR(void) {
    // Step 1: Clear the interrupt flag.
    // If we don't do this, the CPU will think the timer is still ringing.
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Step 2: Update the Time Variables
    g_ui32Seconds++; // Add a second
    if (g_ui32Seconds > 59) {
        g_ui32Seconds = 0;   // Reset seconds
        g_ui32Minutes++;     // Add a minute
    }
    if (g_ui32Minutes > 59) {
        g_ui32Minutes = 0;   // Reset minutes
        g_ui32Hours++;       // Add an hour
    }
    if (g_ui32Hours > 23) {
        g_ui32Hours = 0;     // Reset to midnight
    }

    // Step 3: Set the flag to True.
    // This tells the main loop: "Hey! The time changed. Please update the screen."
    g_bUpdateScreen = true;
}

// ============================================================================
//                           ADC FUNCTION (SENSORS)
// ============================================================================
// Function to read the analog value from the sensor
uint32_t Read_ADC(void) {
    uint32_t adcValue[1]; // Array to hold the result

    // Trigger the ADC to start sampling on Sequencer 3
    ADCProcessorTrigger(ADC0_BASE, 3);

    // Wait in a loop until the conversion is finished
    while(!ADCIntStatus(ADC0_BASE, 3, false));

    // Clear the "Finished" interrupt flag
    ADCIntClear(ADC0_BASE, 3);

    // Read the data from the hardware into our array
    ADCSequenceDataGet(ADC0_BASE, 3, adcValue);

    // Return the value (0 to 4095)
    return adcValue[0];
}

// ============================================================================
//                                MAIN PROGRAM
// ============================================================================
int main(void) {
    char buffer[17]; // Temporary storage for the text line (e.g., "12:00:00 A:4095")

    // 1. CLOCK SETUP
    // Set the CPU speed to 80 MHz using the Crystal and PLL
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    // 2. LCD SETUP
    // Initialize the screen so it's ready to show text
    LCD_Init();

    // 3. ADC SETUP (Analog Input)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // Enable ADC Hardware
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE); // Enable Port E

    // Set Pin PE4 as an Analog Input (AIN9)
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);

    // Configure the ADC Sequence:
    // Sequencer 3, Processor Trigger (Software trigger), Priority 0
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);

    // Configure the Step:
    // Read Channel 9 (PE4), Enable Interrupt Flag, End of Sequence
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH9 | ADC_CTL_IE | ADC_CTL_END);

    // Turn on the ADC Sequencer
    ADCSequenceEnable(ADC0_BASE, 3);
    // Clear any leftover flags
    ADCIntClear(ADC0_BASE, 3);

    // 4. TIMER SETUP (The 1-Second Heartbeat)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); // Enable Timer Hardware
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); // Set mode: Periodic (Repeat)

    // Set the Load Value:
    // Since clock is 80MHz, loading 80,000,000 means it counts down to 0 in exactly 1 second.
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet());

    // Enable the "Time-Out" interrupt for Timer 0
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Register the function 'Timer0_ISR' to be called when the timer fires
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0_ISR);

    // Enable the Timer Interrupt in the Master Interrupt Controller (NVIC)
    IntEnable(INT_TIMER0A);

    // Start the Timer counting!
    TimerEnable(TIMER0_BASE, TIMER_A);

    // Master Enable for all interrupts globally
    IntMasterEnable();

    // 5. MAIN LOOP
    // This loop runs forever
    while(1) {
        // Check if the Timer Interrupt set the "Update Screen" flag
        if (g_bUpdateScreen) {

            // Immediately reset the flag to false
            g_bUpdateScreen = false;

            // Read the current sensor value
            uint32_t adc_val = Read_ADC();

            // --- Write Line 1 ---
            LCD_SetCursor(0, 0);       // Move to Top Left
            LCD_Print("BARAA HOSSREH  "); // Print Name

            // --- Write Line 2 ---
            // Format the string nicely:
            // %02d puts a leading zero if number < 10 (e.g., "05")
            // %4d reserves 4 spaces for the ADC value
            sprintf(buffer, "%02d:%02d:%02d A:%4d", g_ui32Hours, g_ui32Minutes, g_ui32Seconds, adc_val);

            LCD_SetCursor(0, 1);       // Move to Bottom Left
            LCD_Print(buffer);         // Print the time string
        }
    }
}
