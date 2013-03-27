/*
==
==
==
==
==
*/

#include <Wire.h>
#include <LiquidTWI.h>

//=============== Function Declarations =============
void selectLine1();
void selectLine2();
void selectLine3();
void selectLine4();


LiquidTWI lcd(0);


void setup(){
  lcd.begin(20,4);
  lcd.setBacklight(HIGH);
}

void loop(){
  
  
}


//================ Functions ==================
void selectLine1(){          //puts the cursor at line 1 char 0.
   Serial.print(0xFE, BYTE);  //command flag
   Serial.print(128, BYTE);    //position 0 at line 1
}
void selectLine2(){          //puts the cursor at line 2 char 0.
   Serial.print(0xFE, BYTE);  //command flag
   Serial.print(192, BYTE);    //position 0 at line 2
}
void selectLine3(){        //puts the cursor at line 3 char 0
   Serial.print(0xFE, BYTE);   //command flag
   Serial.print(148, BYTE);    //position 0 at line 3
}
void selectLine4(){         //puts the cursor at line 4 char 0
   Serial.print(0xFE, BYTE);  //command flag
   Serial.print(212, BYTE);    //position 0 at line 4
}





