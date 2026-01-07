// Detailed connection diagram is provided in the comments below:
// Data Lines (Sending characters): PB4, PB5, PB6, PB7
// Control Lines (Managing traffic): PE1 (RS), PE2 (RW), PE3 (EN)

/*
** LCD Pinlerinin Stellaris LaunchPad Pinleri ile Baglantisi        **
** D7 - D6  - D5  - D4                     **
** Veri hatti ==> PB7 - PB6 - PB5 - PB4    MSB:YUKSEK ANLAMLI BIT    **
** MSB             LSB    LSB:DUSUK ANLAMLI BIT    **
** **
** RS biti ==> PE1                            **
** R/W biti ==> PE2                            **
** EN biti ==> PE3                            **
*/
//===========================================================================

// "Include Guard": Prevents this file from being included twice in the same compilation
#ifndef _LCD_H
#define _LCD_H

// Hardware and Driver definitions (same as main.c)
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"

// Define readable names for the Control Pins on Port E
#define RS  GPIO_PIN_1 // Register Select: 0 = Command, 1 = Data
#define RW  GPIO_PIN_2 // Read/Write: Usually kept Low (0) for Write
#define EN  GPIO_PIN_3 // Enable: The "Enter" key to process data

// Define readable names for the Data Pins on Port B
#define D4  GPIO_PIN_4
#define D5  GPIO_PIN_5
#define D6  GPIO_PIN_6
#define D7  GPIO_PIN_7

// Global variable for delay duration (50,000 loop cycles)
long sure = 50000;

// Function Prototypes (telling the compiler these functions exist)
void baslangic(void);
void komut_yaz(void);
void LCD_sil(void);
void veri_yaz(void);
void satir_sutun(unsigned char satir, unsigned char sutun);
void veri(char deger);
void printf(char* s);

//===========================================================================
// FUNCTION: baslangic (Start/Initialize)
// Sets up the ports and sends the specific startup sequence the LCD needs.
//===========================================================================
void baslangic(void)
{
    // Enable the clocks for Port B (Data) and Port E (Control)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Set Control pins (RS, RW, EN) as Output
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, RS | RW | EN);
    // Set Data pins (D4-D7) as Output
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, D7 | D6 | D5 | D4);

    // Initialize Control pins to Low (0)
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x00);

    // Wait for LCD to power up (>20ms)
    SysCtlDelay(sure);

    // --- Start of Initialization Sequence (Magic Numbers) ---
    // The LCD needs specific commands to wake up and enter 4-bit mode.

    // Send 0x20 (Function Set: 4-bit mode)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x20);
    komut_yaz(); // Latch the command

    // Send 0x20 again (Hardware requirement)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x20);
    komut_yaz();

    // Send 0x80 (Lines/Font settings)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x80);
    komut_yaz();
    SysCtlDelay(sure);

    // Send 0x00 then 0xD0 (Display ON, Cursor ON, Blink ON)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x00);
    komut_yaz();
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0xD0);
    komut_yaz();
    SysCtlDelay(sure);

    // Send 0x00 then 0x10 (Clear Display command)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x00);
    komut_yaz();
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x10);
    komut_yaz();
    SysCtlDelay(sure);

    // Send 0x00 then 0x20 (Entry Mode Set)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x00);
    komut_yaz();
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x20);
    komut_yaz();
}

//===========================================================================
// FUNCTION: komut_yaz (Write Command)
// Pulses the Enable pin (EN) to tell the LCD "Read the instruction on the data lines now."
//===========================================================================
void komut_yaz(void)
{
    // Set EN (Bit 3) HIGH. RS is Low (Command Mode).
    // 0x08 = Binary 0000 1000 (Pin 3 High)
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x08);

    // Small delay to let the signal stabilize
    SysCtlDelay(10000);

    // Set EN LOW. The falling edge (High -> Low) triggers the LCD to read.
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x00);
}

//===========================================================================
// FUNCTION: satir_sutun (Row_Column)
// Moves the cursor to a specific location.
// satir = Row (1 or 2), sutun = Column (1 to 16)
//===========================================================================
void satir_sutun(unsigned char satir, unsigned char sutun)
{

    char total;

    // Calculate the memory address for the cursor.
    // Row 1 starts at address 0x00 (0x80 command + 0x00).
    // Row 2 starts at address 0x40 (0x80 command + 0x40 = 0xC0).

    // If Row 1: Base becomes 0x7F (because 0x7F + 1 = 0x80 command start)
    if(satir == 1) { satir = 0x7F; }
    // If Row 2: Base becomes 0xBF (because 0xBF + 1 = 0xC0 command start)
    if(satir == 2) { satir = 0x0BF; }

    // Add the column number to find exact position
    total = satir + sutun;

    // Reset control pins to 0 (Command Mode)
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x00);

    // SEND UPPER NIBBLE (Top 4 bits)
    // Mask with 0xF0 to keep only top bits.
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, (0xF0 & total));
    komut_yaz(); // Pulse Enable

    // SEND LOWER NIBBLE (Bottom 4 bits)
    // Mask bottom bits (0x0F) and shift them LEFT by 4 positions so they line up with pins D4-D7.
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, ((total & 0x0F) << 4));
    komut_yaz(); // Pulse Enable
}

//===========================================================================
// FUNCTION: LCD_sil (Clear LCD)
// Sends the specific command to wipe the screen blank.
//===========================================================================
void LCD_sil(void)
{
    // Reset control pins
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x00);
    SysCtlDelay(sure);

    // Send "0"
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x00);
    SysCtlDelay(sure);
    komut_yaz();

    // Send "1" (0x01 is the standard "Clear Display" command)
    // Shifted logic: 0x10 is sent to the upper pins to represent 1 in the lower nibble.
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, 0x10);
    SysCtlDelay(sure);
    komut_yaz();
}

//===========================================================================
// FUNCTION: printf
// Prints a full string of text to the LCD.
// NOTE: This overrides the standard C 'printf'.
//===========================================================================
void printf(char* s)
{
    // Set RS High (0x02 = Binary 0000 0010).
    // RS High means we are sending DATA (letters), not commands.
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x02);

    // Loop through the string 's' until the end (null terminator)
    while(*s)
    {
        // Send the current character and move to the next one
        veri(*s++);
    }
}

//===========================================================================
// FUNCTION: veri (Data)
// Sends a single character to the LCD in two parts (4-bit mode).
//===========================================================================
void veri(char deger)
{
    // Send Upper 4 bits
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, (0xF0 & deger));
    veri_yaz(); // Pulse Enable for Data

    // Send Lower 4 bits (Shifted left to match pins)
    GPIOPinWrite(GPIO_PORTB_BASE, D7 | D6 | D5 | D4, ((deger & 0x0F) << 4));
    veri_yaz(); // Pulse Enable for Data
}

//===========================================================================
// FUNCTION: veri_yaz (Write Data Latch)
// Pulses Enable (EN) while maintaining RS High (Data mode).
//===========================================================================
void veri_yaz(void)
{
    // Set EN High (Bit 3) AND RS High (Bit 1). Binary 1010 = 0x0A.
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x0A);

    // Wait
    SysCtlDelay(10000);

    // Set EN Low, keep RS High. Binary 0010 = 0x02.
    // Falling edge processes the data.
    GPIOPinWrite(GPIO_PORTE_BASE, RS | RW | EN, 0x02);
}

#endif
