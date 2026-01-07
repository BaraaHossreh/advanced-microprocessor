// ============================================================================
//                                 LIBRARIES
// ============================================================================
#include <stdint.h>  // Standard Integers (uint32_t, etc.)
#include <stdbool.h> // Boolean (true/false)
#include <stdlib.h>  // Standard Library (used for 'atoi' string conversion)
#include <stdio.h>   // Standard I/O (used for 'sprintf' text formatting)

// Hardware definition files (Addresses of registers)
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h" // Needed for direct register access (HWREG)

// Driver libraries (Functions to control peripherals)
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"     // Serial communication
#include "driverlib/pin_map.h"
#include "driverlib/timer.h"    // Hardware Timers
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"      // Analog to Digital Converter

// ============================================================================
//                             HARDWARE MAPPING
// ============================================================================
// LCD Connections: Using Port B
#define LCD_PORT_BASE GPIO_PORTB_BASE
#define RS GPIO_PIN_0 // Register Select
#define E  GPIO_PIN_1 // Enable
// Data Pins (4-bit mode)
#define D4 GPIO_PIN_4
#define D5 GPIO_PIN_5
#define D6 GPIO_PIN_6
#define D7 GPIO_PIN_7

// ============================================================================
//                             GLOBAL VARIABLES
// ============================================================================
// 'volatile' is used because these variables change inside Interrupts
volatile int hours = 0, minutes = 0, seconds = 0;
volatile uint32_t adcValue[1]; // Array to store ADC result

// Default message to show on LCD until changed by PC
char lcd_custom_msg[8] = "---";

// Flag: Timer sets this to TRUE every second. Main loop reads it.
volatile bool send_report_flag = false;

// Flag: Fixes the issue where fast button presses are missed.
// If we press the button at 0.5s, the main loop remembers it until the 1.0s report.
bool button_latch = false;

// Text buffers for formatting strings
char l1[64];    // Line 1 buffer
char l2[64];    // Line 2 buffer
char txBuf[64]; // Transmit (UART) buffer

// ============================================================================
//                             LCD DRIVER
// ============================================================================
// Toggles the Enable pin to latch data
void LCD_Pulse_Enable() {
    GPIOPinWrite(LCD_PORT_BASE, E, E); // High
    SysCtlDelay(40000);                // Wait
    GPIOPinWrite(LCD_PORT_BASE, E, 0); // Low
    SysCtlDelay(40000);                // Wait
}

// Sends 4 bits to the LCD data pins
void LCD_Write_4Bit(unsigned char data) {
    // Write data to pins 4-7.
    // (data << 4) shifts the value to match the pin positions (PB4-PB7).
    GPIOPinWrite(LCD_PORT_BASE, D4|D5|D6|D7, (data << 4));
    LCD_Pulse_Enable();
}

// Sends a Command (RS = 0)
void LCD_Cmd(unsigned char cmd) {
    GPIOPinWrite(LCD_PORT_BASE, RS, 0); // RS Low = Command
    LCD_Write_4Bit(cmd >> 4);           // Send Upper Nibble
    LCD_Write_4Bit(cmd & 0x0F);         // Send Lower Nibble
    SysCtlDelay(80000);                 // Wait for command to process
}

// Sends Data/Characters (RS = 1)
void LCD_Data(unsigned char data) {
    GPIOPinWrite(LCD_PORT_BASE, RS, RS); // RS High = Data
    LCD_Write_4Bit(data >> 4);           // Send Upper Nibble
    LCD_Write_4Bit(data & 0x0F);         // Send Lower Nibble
    SysCtlDelay(80000);                  // Wait
}

// Initializes the LCD
void LCD_Init() {
    // Enable Port B
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    // Set pins as Output
    GPIOPinTypeGPIOOutput(LCD_PORT_BASE, 0xFF);
    SysCtlDelay(1000000); // Wait for power up

    // "Magic" sequence to force 4-bit mode
    LCD_Write_4Bit(0x03); SysCtlDelay(100000);
    LCD_Write_4Bit(0x03); SysCtlDelay(100000);
    LCD_Write_4Bit(0x03); SysCtlDelay(100000);
    LCD_Write_4Bit(0x02); // 4-bit mode set

    // Configure Display
    LCD_Cmd(0x28); // 4-bit, 2 lines
    LCD_Cmd(0x0C); // Display ON
    LCD_Cmd(0x06); // Cursor Auto-Increment
    LCD_Cmd(0x01); // Clear Screen
    SysCtlDelay(200000);
}

// Prints a full string
void LCD_Print(char *str) {
    while(*str) LCD_Data(*str++);
}

// ============================================================================
//                             TIMER INTERRUPT
// ============================================================================
// This function runs automatically once per second
void Timer0IntHandler(void) {
    // Clear the interrupt flag
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Increment Time
    if(++seconds >= 60) {
        seconds = 0;
        if(++minutes >= 60) {
            minutes = 0;
            if(++hours >= 24) hours = 0;
        }
    }
    // Tell the main loop: "1 Second has passed, please update everything."
    send_report_flag = true;
}

// ============================================================================
//                             HARDWARE SETUP
// ============================================================================
void InitHardware() {
    // 1. Clock Setup (Set to 16MHz)
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // 2. UART Setup (PC Communication)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); // Enable UART Module
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); // Enable Port A (Rx/Tx pins)
    GPIOPinConfigure(GPIO_PA0_U0RX);             // Set Pin A0 as RX
    GPIOPinConfigure(GPIO_PA1_U0TX);             // Set Pin A1 as TX
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Activate UART mode

    // Configure UART: 9600 Baud Rate, 8 data bits, 1 stop bit, No parity
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // 3. Timer Setup
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); // Repeat mode
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()); // Load 1 second worth of ticks
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler); // Link ISR function
    IntEnable(INT_TIMER0A); // Enable in NVIC
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Enable Timer Timeout Interrupt

    // 4. ADC Setup (Analog Input)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE); // Using Port E

    // Set Pin PE3 as Analog Input
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    // Configure Sequencer 3 (Single sample mode)
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    // Step 0: Read Channel 0 (PE3), Enable Interrupt, End Sequence
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCIntClear(ADC0_BASE, 3);

    // 5. Button Setup (PF4)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // Unlock Port F (Required for PF0, good practice for Port F generally)
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY; // Unlock with magic password
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;           // Commit the change
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;             // Re-lock

    // Configure PF4 as Input with Pull-Up Resistor (WPU)
    // Pull-Up means default is 1. Button press makes it 0.
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // 6. Start Everything
    IntMasterEnable(); // Enable global interrupts
    TimerEnable(TIMER0_BASE, TIMER_A); // Start the clock
}

// ============================================================================
//                                MAIN LOOP
// ============================================================================
int main(void) {
    InitHardware(); // Run setup
    LCD_Init();     // Run LCD setup

    adcValue[0] = 0; // Reset ADC value

    while (1) {
        // --- PHASE 0: CONTINUOUS POLLING (Button) ---
        // We check this constantly (thousands of times per second).
        // If the button (PF4) reads 0 (Pressed)...
        if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0) {
            // Set the latch to TRUE.
            // Even if you release the button immediately, this stays true
            // until the Timer Interrupt handles it later.
            button_latch = true;
        }

        // --- PHASE 1: RECEIVE COMMANDS (UART) ---
        // Check if PC sent any data
        if (UARTCharsAvail(UART0_BASE)) {
            char cmd = UARTCharGet(UART0_BASE); // Read first letter

            // Command 'S': Set Time (Format: S12:30:45)
            if (cmd == 'S') {
                char b[9]; int i;
                // Read the next 8 characters (12:30:45)
                for(i=0; i<8; i++) {
                    while(!UARTCharsAvail(UART0_BASE)); // Wait for char
                    b[i] = UARTCharGet(UART0_BASE);
                }
                b[8] = '\0'; // Null terminate string

                // Parse string to integers.
                // atoi(b) reads "12"
                // atoi(b+3) skips 3 chars and reads "30"
                hours = atoi(b); minutes = atoi(b + 3); seconds = atoi(b + 6);
            }
            // Command 'M': Set Message (Format: MABC)
            else if (cmd == 'M') {
                int i;
                // Read next 3 characters
                for(i=0; i<3; i++) {
                    while(!UARTCharsAvail(UART0_BASE));
                    lcd_custom_msg[i] = UARTCharGet(UART0_BASE);
                }
                lcd_custom_msg[3] = '\0'; // Terminate string
            }
        }

        // --- PHASE 2: SEND REPORT & UPDATE LCD ---
        // This block runs ONLY when the Timer says so (Once per second)
        if (send_report_flag) {
            send_report_flag = false; // Reset flag

            // 1. Read ADC Hardware
            ADCProcessorTrigger(ADC0_BASE, 3); // Trigger
            while(!ADCIntStatus(ADC0_BASE, 3, false)); // Wait
            ADCIntClear(ADC0_BASE, 3); // Clear flag
            ADCSequenceDataGet(ADC0_BASE, 3, (uint32_t*)adcValue); // Get Data

            // 2. Button State Logic
            // If button_latch is true, set btn=1, otherwise btn=0.
            int btn = button_latch ? 1 : 0;
            button_latch = false; // Reset latch for the next second

            // 3. Send Report to PC (Format: 12:00:00;1024;1)
            sprintf(txBuf, "%02d:%02d:%02d;%u;%d\r\n", hours, minutes, seconds, adcValue[0], btn);
            // Loop through string and send char by char via UART
            char *p = txBuf; while(*p) UARTCharPut(UART0_BASE, *p++);

            // 4. Update LCD Screen
            // Line 1: Time
            sprintf(l1, "Time: %02d:%02d:%02d", hours, minutes, seconds);
            LCD_Cmd(0x80); // Move cursor to Line 1
            LCD_Print(l1);

            // Line 2: ADC value + Custom Message
            sprintf(l2, "ADC:%4u Msg:%s", adcValue[0], lcd_custom_msg);
            LCD_Cmd(0xC0); // Move cursor to Line 2
            LCD_Print(l2);
        }
    }
}
