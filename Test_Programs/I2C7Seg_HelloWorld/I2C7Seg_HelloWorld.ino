/*
=    Eric Monroe
=    February 11, 2013
=
=    This is a sample program to demonstrate the ability to communicate 
=    independently with all of our displays over the I2C protocol, 
=    which only uses 2 wires to communicate with all 4 displays.
=    This code will scroll the values 0 to 11 across all 3 LED
=    displays continuously.
*/

// Included Libraries
#include <Wire.h>
#include <LiquidTWI.h>

// Variable declarations
const char address[] = {0x71, 0x72, 0x73};        // Addresses of LED Displays
const int nums[12] = {0,1,2,3,4,5,6,7,8,9,10,11}; // 10 will display as A, 11 will display as b
int cnt = 0;  

// Create new LCD object
LiquidTWI lcd(0);

void setup(){
    // Setup LCD Disply
    lcd.begin(20, 4);            // Define LCD size
    lcd.setBacklight(HIGH);      // Turn on the LCD backlight
    lcd.print("Hello, world!");  // Print to screen
    
    Wire.begin();  
    
    // Initialize each LED Display
    for (int i = 0; i < 3; i++)
    {
        Wire.beginTransmission(address[i]);  // Select which display to begin communication with
        Wire.write(0x7A); // Brightness control command
        Wire.write(100);  // Set brightness level: 0% to 100%
        Wire.write('v');  // Clear Display, setting cursor to digit 0
        Wire.endTransmission();  // End communication with this selected module.
    }
}

void loop(){
    scrollNums();        // Call the scrollNums function
    if (cnt >= 11)       // Reset the count variable for the offset when count reaches 12.
    {   cnt = 0;    }
    else                 // Else, increment cnt
    {   cnt++;      }
    delay(200);          // Delay to make scrolling action visible, otherwise, scrolling is too quick.
}
    
void scrollNums()
{
    for (int j = 0; j < 12; j++)
    {   // Only begin a new transmission when at a new display
        if (j==0 || j==4 || j==8)
        {   Wire.beginTransmission(address[j/4]);   }
        
        // Get num (j+cnt)%12 (?) out of array and write to position j%4
        Wire.write(0x79);   // Move cursor command
        Wire.write(j%4);    // Cursor position (0-3)
        Wire.write(nums[(j+cnt)%12]); //Write value from the number array offset by cnt
        
        // End a transmission to move to next display after the 4th digit is written to on each display.
        if (j==3 || j==7 || j==11)
        {   Wire.endTransmission();  }
    }
}
