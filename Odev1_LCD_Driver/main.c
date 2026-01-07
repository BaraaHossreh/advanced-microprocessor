// Standard libraries for integer types (e.g., uint32_t) and boolean values (true/false)
#include <stdint.h>
#include <stdbool.h>

// Hardware memory map: Defines the addresses of the processor's memory and peripherals
#include "inc/hw_memmap.h"

// Hardware types: Defines data types and macros for hardware registers
#include "inc/hw_types.h"

// System Control driver: Used to set up the system clock (speed)
#include "driverlib/sysctl.h"

// GPIO driver: Used to control input/output pins (Turn LEDs on/off, read buttons)
#include "driverlib/gpio.h"

// Pin map: Maps specific hardware pins to functions (not strictly used here but good practice)
#include "driverlib/pin_map.h"

// Custom LCD library: Contains specific functions like 'baslangic' and 'satir_sutun'
#include "lcd.h"

int main(void)
{
    // 1. CLOCK SETUP
    // Sets the system clock to run at 50 MHz using the 16 MHz external crystal.
    // SYSDIV_4 divides the 200 MHz PLL down to 50 MHz.
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // 2. ENABLE PORT F
    // Turns on the clock for GPIO Port F so we can use the LEDs and Switches.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // 3. CONFIGURE OUTPUT PINS (LEDS)
    // Sets Pins 1 (Red LED), 2 (Blue LED), and 3 (Green LED) as OUTPUTS.
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    // 4. CONFIGURE INPUT PIN (SWITCH)
    // Sets Pin 4 (Switch 1) as an INPUT.
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);

    // 5. CONFIGURE PULL-UP RESISTOR
    // Configures Pin 4 with a "Weak Pull-Up" (WPU).
    // This ensures the button reads as "1" (High) when not pressed, and "0" (Low) when pressed.
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);

    // 6. INITIALIZE LCD
    // Calls a custom function to start up the LCD screen (likely clears screen, sets modes).
    baslangic();

    // 7. TURN ON RED LED
    // Writes digital values to the pins. 0x02 is binary 0000 0010.
    // This sends a '1' to Pin 1 (Red LED), turning it ON.
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0x02);

    // 8. WRITE FIRST LINE TO LCD
    // Custom function: Move cursor to Row 1, Column 2.
    satir_sutun(1, 2);
    // Print the letter "a" at that location.
    printf("a");

    // 9. WRITE SECOND LINE TO LCD
    // Custom function: Move cursor to Row 2, Column 1.
    satir_sutun(2, 1);
    // Print the string "MIKRO LCD PROJE".
    printf("MIKRO LCD PROJE");

    // 10. SWITCH TO GREEN LED
    // Writes 0x08 (binary 0000 1000) to the port.
    // This turns Pin 1 (Red) OFF and turns Pin 3 (Green LED) ON.
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0x08);
}
