/*
= Author: Eric Monroe
= 
= Assitive Inebriation Device
= 
= This is the main program for CFV Senior Design Team.
= All work will be done here and then split out into separate files.
= 
= 
======= Pins Required:
= SD Card uses SPI: pins 50(MISO), 51(MOSI), 52(SCK), 53(SS)
= I2C for screens and clock uses pins 20(SDA) and 21(SCL)
= Rotary Encoder uses INT4(19)A INT5(18)B
= Pins 22-33 are used for relays for pumps and valves.
= 
*/

//================ Libraries ================
#include <LiquidTWI.h>
#include <OneWire.h>
#include <Wire.h>
#include <SD.h>


#define SERIAL_OUTPUT_DEBUG 1
#define HEATING_PRESENT 0		//0 until we add heating capabilities
#define MAX_RECIPE_NUM 5		//Change to allocate more space to store recipe names
#define MAX_INGREDIENT_LEN 30	//Length of ingredient amount and name 
#define MAX_INGREDIENT_NUM 15	//Change to allocate more space for the ingredient list
#define MAX_MASH_ADDS 5		
#define MAX_BOIL_ADDS 5		
#define MAX_STEEP_ADDS 5

#define PI 3.14159

//Volume Conversion Factors
const float galToCubicInch = 231.0;
const float cubicInchToGal = 0.004329;
const float qtToCubicInch = 57.75;
const float cubicInchToQt = 0.017316;

//Put these in the Flash Memory to save space in the SRAM
const prog_char PROGMEM RECIPES[] = "Recipes";
const prog_char PROGMEM CHOOSE_MODE[] = "Pick a mode:"
const prog_char PROGMEM AUTO_BREW_MODE[] = "Auto Brewing";
const prog_char PROGMEM MANUAL_BREW_MODE[] = "Manual Brewing";
const prog_char PROGMEM SETTINGS_MODE[] = "Settings";
const prog_char PROGMEM CREDITS_OPT[] = "Credits";
const prog_char PROGMEM CREDITS_TITLE[] = "";
const prog_char PROGMEM CHOOSE_RECIPE[] = "Choose recipe:"
const prog_char PROGMEM CUR_STEP[] = "Current Step:";
const prog_char PROGMEM MASH_PH[] = "Desired Mash PH:";
const prog_char PROGMEM PAGE[] = "Page ";
const prog_char PROGMEM NEXT[] = " NEXT";
const prog_char PROGMEM NEXT_SEL[] = ">NEXT";
const prog_char PROGMEM PREV[] = " PREV";
const prog_char PROGMEM PREV_SEL[] = ">PREV";
const prog_char PROGMEM CONFIRM[] = " CONFIRM";
const prog_char PROGMEM CONFIRM_SEL[] = ">CONFIRM";
const prog_char PROGMEM YN_NO[]=" YES >NO<";          
const prog_char PROGMEM YN_YES[]=">YES< NO ";          
const char *yn_items[]= {yn_00,yn_01};


//Relay Schedule: 0 = OFF, 1 = ON, 2 = DON'T CARE (INTERPRET AS 0 FOR NOW)
const prog_uint8_t PROGMEM HLT_FILL[] = 	{1,0,1,0,1,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM HLT_RECIRC[] = 	{1,0,0,1,1,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM STRIKE_TRANS[] = {1,0,0,1,0,1,0,0,0,0,0,0};
const prog_uint8_t PROGMEM MASH[] = 		{1,1,0,1,1,0,1,0,1,0,0,0};
const prog_uint8_t PROGMEM SPARGE_IN_ON =	{1,1,0,1,0,1,1,0,0,1,0,0};
const prog_uint8_t PROGMEM SPARGE_IN_OFF = 	{0,1,0,0,0,0,1,0,0,1,0,0};
const prog_uint8_t PROGMEM REFILL_HLT = 	{1,2,1,0,1,0,2,2,2,2,2,2};	//For Refilling during boil at the moment
const prog_uint8_t PROGMEM DRAIN_MLT = 		{2,1,2,2,2,2,1,0,0,0,1,0};
const prog_uint8_t PROGMEM BOIL = 			{0,0,0,0,0,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM COOL =			{1,1,0,1,1,0,0,1,1,0,0,1};
const prog_uint8_t PROGMEM FILL_FERM = 		{0,1,0,0,0,0,0,1,0,0,1,0};
const prog_uint8_t PROGMEM DRAIN_HLT = 		{1,1,0,1,0,1,0,1,0,0,1,1};
const prog_uint8_t PROGMEM ERROR = 			{0,0,0,0,0,0,0,0,0,0,0,0};	//Hopefully not needed


OneWire  ds(10);  	//OneWire devices connected to pin #10
LiguidTWI lcd(0);	//Create LCD screen object
const char LED_address[] = {0x71, 0x72, 0x73};	//I2C addresses of LED Displays
int pulses, A_SIG=0, B_SIG=0;	//Rotary Encoder Variables

//Process Enumeration. Using because it increases readability of code
enum STEP {ST_MENU, = 0, ST_READY, ST_FILL, ST_STRIKE, ST_STRIKE_TRANS,ST_MASH, ST_SPARGE, 
		   ST_BOIL, ST_COOL, ST_FILLFERM, ST_DONE, ST_DRAIN, ST_DRAIN_MLT, ST_DRAIN_HLT, ST_ERROR};
enum MODE {AUTO = 1, MANUAL, SETTINGS};
bool SD_Present = false;
bool SD_ERROR = false;			//Possibly use


//System Params
float mash_out_temp = 168.0;	//Mashout temp in Farenheight
float vessel_ht = 17.0;			//Height of vessel in inches
float vessel_diam = 16.0;		//Diameter of vessel in inches



//Recipe Variables
//Make a data structure here
struct recipe{
	String name;
	float srm;
	float ibu;
	float og;
	float batch_size;
	int ingredient_num;
	char ingredients [MAX_INGREDIENT_LEN][MAX_INGREDIENT_NUM];
	float strike_vol;
	float strike_temp;
	float ph_desire;
	int num_mash_steps;
	float mash_step_temps[MAX_MASH_ADDS];
	int mash_step_times[MAX_MASH_ADDS];
	char mash_step_name [MAX_INGREDIENT_LEN][MAX_MASH_ADDS];
	float sac_temp;
	int sac_time;
	bool mash_out;
	int mash_out_time;
	float sparge_vol;	//gal
	float sparge_temp;
	float pre_boil_grav;
	float boil_vol;		//gal
	int boil_time_total;
	int num_boil_adds;
	int boil_add_time[MAX_BOIL_ADDS];
	char boil_add_name[MAX_INGREDIENT_LEN][MAX_BOIL_ADDS];
	int num_steep_adds;
	char steep_add_name[MAX_INGREDIENT_LEN][MAX_STEEP_ADDS];
	int steep_time;
	float cooling_temp;
} myRecipe;


//================= Function Prototypes =================
void Init_LEDS();
void Splash_screen();
int Main_Menu(); 
void Settings_Menu();
int Parse_Recipe_List();
void Parse_Recipe();
void Update_LED();
void Read_Sensors();
void CalcVolume();		//By each tank, or all at once?
float CalcTempF(float, int);
void A_RISE();
void A_FALL();
void B_RISE();
void B_FALL();



void setup()
{
    lcd.begin(20, 4);		// Define LCD size
    lcd.setBacklight(HIGH);	// Turn on the LCD backlight
	Init_LEDS();
	
	pinMode(53, OUTPUT);	// Chip Select on MEGA for  SD card. Must be an output.
	
	//Setup pins for pumps and solenoid valves
	pinMode(22, OUTPUT);
	pinMode(23, OUTPUT);
	pinMode(24, OUTPUT);
	pinMode(25, OUTPUT);
	pinMode(26, OUTPUT);
	pinMode(27, OUTPUT);
	pinMode(28, OUTPUT);
	pinMode(29, OUTPUT);
	pinMode(30, OUTPUT);
	pinMode(31, OUTPUT);
	pinMode(32, OUTPUT);
	pinMode(33, OUTPUT);

	if (!SD.begin(53))
	{	SD_Present = false;	}
	else
	{	SD_Present = true;	}
	
	Splash_screen();
	
}
void loop()
{
	//MENU SELECTIONS
	MODE = Main_Menu();
	//Parse menu selection
	
	
/*	while (Auto_Brew)
	{
		switch(STEP)
		{
				case ST_READY:
				//Display all ingredients
				break;
				
				case ST_FILL:
				//Fill HLT till at 
				break;
				
				case ST_STRIKE;
				//Heat up HLT, transfer, recirc., add water additives
				break;
				
				case ST_MASH:
				//Follow Mash Schedule
				break;
				
				case ST_SPARGE: 
				//
				break; 
				
				case ST_BOIL:
				//
				break;
				
				case ST_STEEP:
				//
				break;
				
				case ST_COOL:
				//
				break;
				
				case ST_FERMFILL:
				break;
				
				case ST_DRAIN:
				break;
				
				default:
					while(1)//Halt
				break;	
		}
	}*/
}



void Init_LEDS(){	// Initialize the LED Displays
	Wire.begin();  
    // Initialize each LED Display
    for (int i = 0; i < 3; i++){
        Wire.beginTransmission(LED_address[i]);  // Select which display to begin communication with
        Wire.write(0x7A); // Brightness control command
        Wire.write(100);  // Set brightness level: 0% to 100%
        Wire.write('v');  // Clear Display, setting cursor to digit 0
        Wire.endTransmission();  // End communication with this selected module.
    }
}

void splash_screen(){
	//Disp name of project, and other info.
	lcd.clear();
	lcd.setCursor(0,0);//x,y
	lcd.print("");
	lcd.
	lcd.print("Hey Brew, Lets Brew");
}

int Main_Menu(){
	bool selection_made = false;
	int cursor_loc = 1;
	
	//Display Menu
	lcd.setCursor(0,0);
	lcd.print(CHOOSE_MODE);
	lcd.setCursor(1,1);
	lcd.print(AUTO_BREW_MODE);
	lcd.setCursor(1,2);
	lcd.print(MANUAL_BREW_MODE);
	lcd.setCursor(1,3);
	lcd.print(SETTINGS_MODE);	
	
	lcd.setCursor(0,1);
	lcd.print(">");
	
	while (!selection_made)
	{
		//If confirm pressed
	}
	return MODE;
}

void Settings_Menu(){
	
	
}

int Parse_Recipe_List(){
	int num_recipes = -1;
	//Read int number of recipes, then each recipe (file) name. Output number of recipes.
	return num_recipes;
}

void Parse_Recipe(){
	//HEADACHE
}

void Update_LED(){ //possibly use Pointers to direct LED display vars to the correct vars.
	//HLT Temp, MLT Temp, Time left in step
}

void Read_Sensors(){
	
}

void CalcVolume(/* int tankNum */){

}	

float CalcTempF(float degC, int pot_num){
	float degF = 0;
	return degF;
}

//Rotary Encoder functions
void A_RISE(){
	detachInterupt(4);
	A_SIG=1;
	
	if(B_SIG==0)
		pulses++;	//Clockwise
	if(B_SIG==1)
		pulses--;	//Counter-Clockwise
	if(SERIAL_OUTPUT_DEBUG)
		Serial.println(pulses);
	attachInterrupt(4, A_FALL, FALLING);
}
void A_FALL(){
	detachInterupt(4);
	A_SIG=0;
	
	if(B_SIG==1)
		pulses++;	//Clockwise
	if(B_SIG==0)
		pulses--;	//Counter-Clockwise
	if(SERIAL_OUTPUT_DEBUG)
		Serial.println(pulses);
	attachInterrupt(4, A_RISE, RISING);
}
void B_RISE(){
	detachInterupt(5);
	B_SIG=1;
	
	if(B_SIG==1)
		pulses++;	//Clockwise
	if(B_SIG==0)
		pulses--;	//Counter-Clockwise
	if(SERIAL_OUTPUT_DEBUG)
		Serial.println(pulses);
	attachInterrupt(5, B_FALL, FALLING);
}	
void B_FALL(){
	detachInterupt(5);
	A_SIG=0;
	
	if(B_SIG==0)
		pulses++;	//Clockwise
	if(B_SIG==1)
		pulses--;	//Counter-Clockwise
	if(SERIAL_OUTPUT_DEBUG)
		Serial.println(pulses);
	attachInterrupt(5, B_RISE, RISING);
}