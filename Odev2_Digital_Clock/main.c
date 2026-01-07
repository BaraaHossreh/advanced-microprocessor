// ============================================================================
//                                 INCLUDES
// ============================================================================
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>               // Standard Input/Output: Needed for 'sprintf'
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"         // Interrupt assignments (e.g., INT_TIMER0A)
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h" // Logic to manage the NVIC (Interrupt Controller)
#include "driverlib/timer.h"     // Logic to manage Hardware Timers

// ============================================================================
//                             PIN DEFINITIONS
// ============================================================================
// LCD Control Pins (Port E)
#define LCD_CTRL_PERIPH     SYSCTL_PERIPH_GPIOE
#define LCD_CTRL_PORT       GPIO_PORTE_BASE
#define LCD_RS_PIN          GPIO_PIN_1 // Register Select
#define LCD_RW_PIN          GPIO_PIN_2 // Read/Write
#define LCD_EN_PIN          GPIO_PIN_3 // Enable
#define LCD_CTRL_PINS       (LCD_RS_PIN | LCD_RW_PIN | LCD_EN_PIN)

// LCD Data Pins (Port B) - Using 4-bit mode (D4, D5, D6, D7)
#define LCD_DATA_PERIPH     SYSCTL_PERIPH_GPIOB
#define LCD_DATA_PORT       GPIO_PORTB_BASE
#define LCD_D4_PIN          GPIO_PIN_4
#define LCD_D5_PIN          GPIO_PIN_5
#define LCD_D6_PIN          GPIO_PIN_6
#define LCD_D7_PIN          GPIO_PIN_7
#define LCD_DATA_PINS       (LCD_D4_PIN | LCD_D5_PIN | LCD_D6_PIN | LCD_D7_PIN)

// LCD Commands (Magic Numbers)
#define LCD_CMD_CLEAR       0x01
#define LCD_CMD_ENTRY_MODE  0x06 // Increment cursor, no shift
#define LCD_CMD_DISPLAY_ON  0x0C // Display ON, Cursor OFF, Blink OFF
#define LCD_CMD_FUNCTION_SET 0x28 // 4-bit data, 2-line display, 5x8 font
#define LCD_CMD_SET_DDRAM   0x80 // Command to set cursor position

// ============================================================================
//                             GLOBAL VARIABLES
// ============================================================================
// Volatile tells the compiler: "These change unexpectedly (inside interrupts)"
volatile uint32_t g_ui32Hours = 12;
volatile uint32_t g_ui32Minutes = 0;
volatile uint32_t g_ui32Seconds = 0;
volatile bool g_bTimeChanged = true; // Flag: ISR sets True -> Main loop reads it

// ============================================================================
//                          HELPER FUNCTIONS
// ============================================================================

// Delay Function: Simple wait loop (Not exact, but good for LCD timing)
// 'us' is microseconds.
void delay_us(uint32_t us)
{
    // SysCtlClockGet() / 3000000 is roughly 1 microsecond loop count
    SysCtlDelay((SysCtlClockGet() / 3000000) * us);
}

// ============================================================================
//                          LCD DRIVER FUNCTIONS
// ============================================================================

// 1. PULSE ENABLE
// Toggles the Enable pin High -> Low to tell LCD "Read the data now"
void LCD_Pulse_EN(void)
{
    GPIOPinWrite(LCD_CTRL_PORT, LCD_EN_PIN, LCD_EN_PIN); // EN = 1
    delay_us(10);
    GPIOPinWrite(LCD_CTRL_PORT, LCD_EN_PIN, 0);          // EN = 0
    delay_us(10);
}

// 2. SEND NIBBLE (4 Bits)
// Puts 4 bits of data onto Port B pins 4-7
void LCD_Send_Nibble(uint8_t nibble)
{
    // Shift the data left by 4 so it lines up with pins 4,5,6,7
    // Example: nibble 0x03 (0000 0011) becomes 0x30 (0011 0000)
    GPIOPinWrite(LCD_DATA_PORT, LCD_DATA_PINS, (nibble << 4));
    LCD_Pulse_EN();
}

// 3. SEND BYTE (8 Bits)
// Splits a full byte into two 4-bit chunks (High nibble, Low nibble)
void LCD_Send_Byte(uint8_t byte, bool is_data)
{
    // Set RS Pin: High (1) for Data (Characters), Low (0) for Commands
    if (is_data)
        GPIOPinWrite(LCD_CTRL_PORT, LCD_RS_PIN, LCD_RS_PIN);
    else
        GPIOPinWrite(LCD_CTRL_PORT, LCD_RS_PIN, 0);

    // Send the top 4 bits (e.g., if byte is 0x41, send 0x4)
    LCD_Send_Nibble(byte >> 4);

    // Send the bottom 4 bits (e.g., if byte is 0x41, send 0x1)
    LCD_Send_Nibble(byte & 0x0F);

    // Wait for LCD to process
    delay_us(50);
}

// Wrappers for clarity
void LCD_Send_Cmd(uint8_t cmd) { LCD_Send_Byte(cmd, false); }
void LCD_Send_Data(uint8_t data) { LCD_Send_Byte(data, true); }

// 4. CLEAR SCREEN
void LCD_Clear(void)
{
    LCD_Send_Cmd(LCD_CMD_CLEAR);
    delay_us(2000); // Clear command takes longer (>1.5ms)
}

// 5. PRINT STRING
void LCD_PrintString(char *str)
{
    while(*str)
    {
        LCD_Send_Data(*str++);
    }
}

// 6. SET CURSOR
// col = 0-15, row = 0-1
void LCD_SetCursor(uint8_t col, uint8_t row)
{
    // Base command is 0x80
    uint8_t cmd = LCD_CMD_SET_DDRAM + col;
    // If row is 1 (the second line), add offset 0x40
    if (row == 1)
        cmd += 0x40;

    LCD_Send_Cmd(cmd);
}

// 7. INITIALIZE LCD
void LCD_Init(void)
{
    // Enable Ports
    SysCtlPeripheralEnable(LCD_CTRL_PERIPH);
    SysCtlPeripheralEnable(LCD_DATA_PERIPH);
    // Wait for hardware to be ready
    while(!SysCtlPeripheralReady(LCD_CTRL_PERIPH));
    while(!SysCtlPeripheralReady(LCD_DATA_PERIPH));

    // Configure Pins as Outputs
    GPIOPinTypeGPIOOutput(LCD_CTRL_PORT, LCD_CTRL_PINS);
    GPIOPinTypeGPIOOutput(LCD_DATA_PORT, LCD_DATA_PINS);

    // Reset all pins to 0
    GPIOPinWrite(LCD_CTRL_PORT, LCD_CTRL_PINS, 0);
    GPIOPinWrite(LCD_DATA_PORT, LCD_DATA_PINS, 0);

    // Wait 50ms for LCD internal power up
    SysCtlDelay(SysCtlClockGet() / 3 / 20);

    // --- "Magic" Initialization Sequence for 4-bit mode ---
    LCD_Send_Nibble(0x03); delay_us(5000);
    LCD_Send_Nibble(0x03); delay_us(200);
    LCD_Send_Nibble(0x03); delay_us(200);
    LCD_Send_Nibble(0x02); delay_us(200); // Switch to 4-bit mode

    // --- Configure Settings ---
    LCD_Send_Cmd(LCD_CMD_FUNCTION_SET); // 2 lines, 5x8 font
    LCD_Send_Cmd(LCD_CMD_DISPLAY_ON);   // Turn display on
    LCD_Clear();                        // Clear garbage data
    LCD_Send_Cmd(LCD_CMD_ENTRY_MODE);   // Auto-increment cursor
}

// ============================================================================
//                          TIMER INTERRUPT HANDLER
// ============================================================================
// This function runs AUTOMATICALLY every 1 second
void Timer0_ISR(void)
{
    // 1. Clear the interrupt flag (Required!)
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // 2. Update Time
    g_ui32Seconds++;
    if (g_ui32Seconds == 60)
    {
        g_ui32Seconds = 0;
        g_ui32Minutes++;
        if (g_ui32Minutes == 60)
        {
            g_ui32Minutes = 0;
            g_ui32Hours++;
            if (g_ui32Hours == 24)
            {
                g_ui32Hours = 0;
            }
        }
    }

    // 3. Notify main loop
    g_bTimeChanged = true;
}

// ============================================================================
//                          TIMER CONFIGURATION
// ============================================================================
void ConfigureTimer(void)
{
    // Enable Timer 0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));

    // Configure: Periodic Timer (Counts down, resets, repeats)
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    // Set Speed: System Clock = 1 Second
    // If Clock is 80MHz, we load 80,000,000.
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet());

    // Enable Interrupts for "Time Out"
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Link the ISR function to the hardware
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0_ISR);

    // Enable Timer0 interrupt in the CPU
    IntEnable(INT_TIMER0A);

    // Start Timer
    TimerEnable(TIMER0_BASE, TIMER_A);
}

// ============================================================================
//                                MAIN
// ============================================================================
int main(void)
{
    char time_buffer[17]; // Buffer for string "Time: 12:00:00"

    // 1. Set System Clock to 80 MHz
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    // 2. Initialize LCD
    LCD_Init();
    LCD_SetCursor(0, 0);
    LCD_PrintString("Timer Clock");
    LCD_SetCursor(0, 1);
    LCD_PrintString("Waiting...");

    // 3. Setup Timer (Interrupts start immediately after this)
    ConfigureTimer();

    // 4. Enable Master Interrupts
    IntMasterEnable();

    // 5. Infinite Loop
    while(1)
    {
        // Check if ISR updated the time
        if (g_bTimeChanged)
        {
            g_bTimeChanged = false; // Reset flag

            // Format the string (HH:MM:SS)
            sprintf(time_buffer, "Time: %02d:%02d:%02d",
                    g_ui32Hours, g_ui32Minutes, g_ui32Seconds);

            // Print to LCD (Row 1, Col 0)
            LCD_SetCursor(0, 1);
            LCD_PrintString(time_buffer);
        }
    }
}
