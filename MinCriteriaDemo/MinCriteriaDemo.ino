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
= Rotary Encoder uses INT4(pin 19)A INT5(pin 18)B
= Pins 22-33 are used for relays for pumps and valves.
= Pin 34 is alarm.
= Analog 
= 
= TO DO (no particular order):
= 	-Add buttons for control
= 	-Add calibration for pressure sensors
=	-Add file IO
=	-Add recipe parsing
=	-Add time tracking
=	-Add oneWire sensor data (addresses and locations)
=	-Add sensor read functionality
=	-Add auto mode following
= 	-Add settings support
=	-Add actuation of relays
=	-Add manual mode
=	-Replace man mode in menu with demo mode.
=	-AHHHHHHHHHHH
*/

//================ Libraries ================
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidTWI.h>
#include <OneWire.h>
#include <SdFat.h>
#include <Button.h>
#include <RTClib.h>


#define SERIAL_OUTPUT_DEBUG 1
#define READ_FROM_SD_CARD 0
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
const prog_char PROGMEM SPLASH1[] = "Assistive Brewing";
const prog_char PROGMEM SPLASH2[] = "Device";
const prog_char PROGMEM SPLASH3[] = "...Preparing to brew";
const prog_char PROGMEM LOAD_RECIPES[] = "Loading Recipes";
const prog_char PROGMEM CHOOSE_MODE[] = "Pick a mode:"
const prog_char PROGMEM AUTO_BREW_MODE[] = "Auto Brewing";
const prog_char PROGMEM MANUAL_BREW_MODE[] = "Manual Brewing";
const prog_char PROGMEM SETTINGS_MODE[] = "Settings";
const prog_char PROGMEM CREDITS_OPT[] = "Credits";
const prog_char PROGMEM CREDITS_TITLE[] = "Awesome Team Members:";
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
const prog_char PROGMEM YN_YES[]=">YES< NO ";          
const prog_char PROGMEM YN_NO[]=" YES >NO<";          
const char *yn_items[]= {YN_YES, YN_NO};		//Used to easily go between Yes and No dialog selections
const char LED_address[] = {0x71, 0x72, 0x73};	//I2C addresses of LED Displays
const prog_char PROGMEM HLT_TEMP[] = {0x28, 0x13, 0xCE, 0x5C, 0x04, 0x00, 0x00, 0xC8};
const prog_char PROGMEM MLT_TEMP[] = {0x28, 0xB4, 0x04, 0x5D, 0x04, 0x00, 0x00, 0x01};
const prog_char PROGMEM BK_TEMP[] = {0x28, 0x61, 0x22, 0x5D, 0x04, 0x00, 0x00, 0x4A};
const char *temp_sensors[] = {HLT_TEMP, MLT_TEMP, BK_TEMP};


//Relay Schedule: 0 = OFF, 1 = ON, 2 = DON'T CARE (INTERPRET AS 0 FOR NOW)
//Could possibly use a bit field structure. (http://www.sgi.com/tech/stl/bit_vector.html for bit<vector>
//bit field structs: http://stackoverflow.com/questions/1772395/c-bool-array-as-bitfield 
const prog_uint8_t PROGMEM HLT_FILL[] = 	{1,0,1,0,1,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM HLT_RECIRC[] = 	{1,0,0,1,1,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM STRIKE_TRANS[] = {1,0,0,1,0,1,0,0,0,0,0,0};
const prog_uint8_t PROGMEM MASH[] = 		{1,1,0,1,1,0,1,0,1,0,0,0};
const prog_uint8_t PROGMEM SPARGE_IN_ON[] = {1,1,0,1,0,1,1,0,0,1,0,0};
const prog_uint8_t PROGMEM SPARGE_IN_OFF[]= {0,1,0,0,0,0,1,0,0,1,0,0};
const prog_uint8_t PROGMEM REFILL_HLT[] = 	{1,2,1,0,1,0,2,2,2,2,2,2};	//For Refilling during boil at the moment
const prog_uint8_t PROGMEM DRAIN_MLT[] =	{2,1,2,2,2,2,1,0,0,0,1,0};
const prog_uint8_t PROGMEM BOIL[] = 		{0,0,0,0,0,0,0,0,0,0,0,0};
const prog_uint8_t PROGMEM COOL[] = 		{1,1,0,1,1,0,0,1,1,0,0,1};
const prog_uint8_t PROGMEM FILL_FERM[] =  	{0,1,0,0,0,0,0,1,0,0,1,0};
const prog_uint8_t PROGMEM DRAIN_HLT[] =  	{1,1,0,1,0,1,0,1,0,0,1,1};
const prog_uint8_t PROGMEM ERROR[] =		{0,0,0,0,0,0,0,0,0,0,0,0};	//Hopefully not needed
const uint8_t chipSelect = 53;	// SD chip select pin

//Object creation
SdFat sd;
SdFile file;
FatReader root;
ArduinoOutStream cout(Serial);	//Create a serial output stream for SDFat
RTC_DS1307 RTC;
OneWire  ds(10);  				//OneWire devices connected to pin #10 (to change pin, change number)
LiguidTWI lcd(0);				//Create LCD screen object
int pulses, A_SIG=0, B_SIG=0;	//Rotary Encoder Variables


//Process Enumeration. Using because it increases readability of code
enum STEP {ST_MENU, ST_READY, ST_FILL, ST_STRIKE, ST_MASH_IN, ST_STEP_MASH_IN, ST_MASH, ST_SPARGE, 
		   ST_BOIL, ST_COOL, ST_FILLFERM, ST_DONE, ST_DRAIN, ST_DRAIN_MLT, ST_DRAIN_HLT, ST_CIP, ST_ERROR};
enum MODE {AUTO = 1, MANUAL, SETTINGS};
bool SD_Present = false;
bool SD_ERROR = false;			//Possibly use
int pulses, A_SIG=0, B_SIG=0;	//Rotary Encoder Variables


//System Params (need to store in EEPROM later for settings menu)
float mash_out_temp = 168.0;	//Mashout temp in Farenheight
float vessel_ht = 17.0;			//Height of vessel in inches
float vessel_diam = 16.0;		//Diameter of vessel in inches
//Store default step times and temps for manual mode 


//Recipe Variables
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
void Init_Disp();
void Splash_screen();
void Main_Menu(); 
int Auto_Brew();
void Settings_Menu();
bool Parse_Recipe_List();
void Parse_Recipe();
void Actuate_Relays(char*&);
void Update_LED();
void Read_Sensors(byte sensors);
void CalcVolume(int, float);		//By each tank, or all at once?
float CalcTempF(float);
void A_RISE();
void A_FALL();
void B_RISE();
void B_FALL();


//************* MAIN PROGRAM ************
void setup()
{
	Serial.begin(9600);
	//Init_Disp();
	lcd.begin(20, 4);		// Define LCD size
	lcd.setBacklight(HIGH);	// Turn on the LCD backlight
	
	Wire.begin();
	RTC.begin();	
	if (! RTC.isrunning()) {
		Serial.println("RTC is NOT running!");
		// following line sets the RTC to the date & time this sketch was compiled
		RTC.adjust(DateTime(__DATE__, __TIME__)); //Need RTC for timekeeping
	}
    // Initialize each LED Display
    for (int i = 0; i < 3; i++){
        Wire.beginTransmission(LED_address[i]);  // Select which display to begin communication with
        Wire.write(0x7A); // Brightness control command
        Wire.write(100);  // Set brightness level: 0% to 100%
        Wire.write('v');  // Clear Display, setting cursor to digit 0
        Wire.endTransmission();  // End communication with this selected module.
    }
	
	
	//Splash_screen();
	//Disp name of project, and other info.
	lcd.clear();
	lcd.setCursor(1,0);//x,y
	lcd.print(SPLASH1);
	lcd.setCursor(6,1);
	lcd.print(SPLASH2);
	lcd.setCursor(0,3)
	lcd.print(SPLASH3);
	

	//Setup pins for pumps, solenoid valves, and alarm.
	pinMode(22, OUTPUT);	//PA0 -Water pump
	pinMode(23, OUTPUT);	//PA1 -Wort pump
	pinMode(24, OUTPUT);	//PA2
	pinMode(25, OUTPUT);	//PA3
	pinMode(26, OUTPUT);	//PA4
	pinMode(27, OUTPUT);	//PA5
	pinMode(28, OUTPUT);	//PA6
	pinMode(29, OUTPUT);	//PA7
	pinMode(30, OUTPUT);	//PC7
	pinMode(31, OUTPUT);	//PC6
	pinMode(32, OUTPUT);	//PC5
	pinMode(33, OUTPUT);	//PC4
	pinMode(34, OUTPUT);	//PC3 -Alarm
	pinMode(53, OUTPUT);	// Chip Select on MEGA for  SD card. Must be an output.
	
	if (!sd.begin(chipSelect, SPI_HALF_SPEED))	//TODO: FIX HALFSPEED HERE
		sd.initErrorHalt(); //Later, throw dialog error and ask for SD insert on LCD

}
void loop()
{
	//MENU SELECTIONS
	//MODE = Main_Menu();
	bool selectionMade = false;
	bool cursorState = true;
	byte blinkCount = 0;
	byte cursorLoc = 1;
	
	
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
		/*
			Control Cursor movement between the 3 options in response to up/down buttons.
			Possibly blink "cursor" on each loop (alternate b/w printing ">" and printing " ")
			Check for confirm button press (and subsequent release if down)
		*/
	}
	//Parse menu selection
	if (MODE == AUTO){
		
	} else if (MODE == MANUAL){
		
	} else if (MODE == SETTINGS){
		
	}
}



/*//************** INITIALIZE TWI ****************
void Init_LEDS(){		// Initialize all TWI devices (LCD, RTC, and LEDs)
	lcd.begin(20, 4);		// Define LCD size
	lcd.setBacklight(HIGH);	// Turn on the LCD backlight
	
	Wire.begin();  
	RTC.begin();
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
	lcd.setCursor(1,0);//x,y
	lcd.print(SPLASH1);
	lcd.setCursor(6,1);
	lcd.print(SPLASH2);
	lcd.setCursor(0,3)
	lcd.print(SPLASH3);
}
*/

//********** MENUS AND CONTROL LOOPS **********
void Main_Menu(){
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
		/*
			Control Cursor movement between the 3 options in response to up/down buttons.
			Possibly blink "cursor" on each loop (alternate b/w printing ">" and printing " ")
			Check for confirm button press (and subsequent release if down)
		*/
	}
	return MODE;
}

void Auto_Brew(){
	switch(STEP)
	{
		case ST_MENU:
			lcd.clear();
			lcd.setCursor(2,1);
			lcd.print(LOAD_RECIPES);
			if(!Parse_Recipe_List())
			{
				//Scan
			}
				
			/*
			Scan SD card and find all files
			load file menu stuff (another function?)
			*/
		break;
		
		case ST_READY:
			/*
			Select and parse all recipe vars into data structure
			Display all ingredients.
			*/
		break;
		
		case ST_FILL:
		//Fill HLT till at till at level in file
		break;
		
		case ST_STRIKE;
		//Heat up HLT, add water additives
		break;
		
		case ST_MASH_IN;
		//Transfer, recirc., 
		break;
		
		case ST_STEP_MASH_IN;
		//Transfer in interval if mash in temp above a certain amount, recirc., 
		break;
		
		case ST_MASH:
		//Follow Mash Schedule
		break;
		
		case ST_SPARGE: 
			//Pump out slowly, pump in as needed switching between 2 relay profiles
			//Stop pumping when BK level reaches calculated volume with pre-boil gravity
		break; 
		
		case ST_BOIL:
			//Boil and sound alarm and update LCD following schedule
		break;
		
		case ST_STEEP:
			//Display names of add on LCD
			//Wait, but allow user to continue at press of confirm button
		break;
		
		case ST_COOL:
			//Cool till at cool temp
		break;
		
		case ST_FERMFILL:
			//Don't drain until confirm (possibly flush first from HLT)
		break;
		
		case ST_DRAIN:
			//Wait till confirm to drain
		break;
		
		default:
			while(1)//Halt, error
		break;	
		}
	
}

void Settings_Menu(){	//TODO: Later
	/*
		Find out what kind of settings Jim will want
		These then need to be stored in EEPROM memory to stick around after power down.
		
		Time, Date, Actuation time, Defaults for manual mode.
	*/
}


//*************** FILE HANDLING ***************
int Parse_Recipe_List(){
	int num_recipes = -1;
	/*
		Search SD card for recipe files
		When found, open and read first line to get name.
		Close file and continue searching SD card contents.
		Possibly write file names and recipe names to files to store, then read from this file for recipe list;
		***DON'T FORGET TO SYNC***
		
		see OpenNext example
	*/
	return num_recipes;
}

void Parse_Recipe(){
	/*
		HEADACHE
		Write to data structure
		Use .getLine from sdfat library if using .txt (see sample)
		Decide whether or not using .csv or .txt
	*/
}


//****** SENSOR, ACTUATOR, ETC UPDATES ********
void Actuate_Relays(uint_8* &ptr_schedule){
	/*
		Shut off pumps first
		Load the relay schedule profile into local var (maybe)
		Go through 3-12 and turn on or off each in relation to schedule.
		Actuate pump relays last.
	*/
	
	//Turn off pumps
	digitalWrite(22, LOW);
	digitalWrite(23, LOW);
	
	//Actuate valves for indicated step
	digitalWrite(24, ptr_schedule[2]);
	digitalWrite(25, ptr_schedule[3]);
	digitalWrite(26, ptr_schedule[4]);
	digitalWrite(27, ptr_schedule[5]);
	digitalWrite(28, ptr_schedule[6]);
	digitalWrite(29, ptr_schedule[7]);
	digitalWrite(30, ptr_schedule[8]);
	digitalWrite(31, ptr_schedule[9]);
	digitalWrite(32, ptr_schedule[10]);
	digitalWrite(33, ptr_schedule[11]);
	
	delay(actTime);
	
	
}

void Update_LED(){ //possibly use Pointers to direct LED display vars to the correct vars.
	/*
		HLT Temp, MLT Temp, and Time left in step (at least initially)
		HLT Temp, BK Temp and something else during cooling.
		(possibly turn on little LED indicators to tell what each is displaying.
		
		For floats (temps), if float/100 >= 1, multiply by ten, print 4 nums to display and turn on decimal b/w digit 3 and 4
							if float/100 < 1, multiply by ten, print nothing to digit 1, then three nums, and turn on decimal as above
							
		For times, figure out later.
	*/
}

void Read_Sensors(){
	/*
		Take in 6 (later 7) bools to know which sensors to check.
		Save processing time
		Do ananlog reads on pressure sensors and read temps with oneWire
	*/
}

void CalcVolume(int tankNum, float specificGrav){
	/*
		Use lookup table (or map) for HLT(b/c coil)
		Take into account preboil specific gravity for boil kettle filling.
		
		First convert sensor readings to height in water (sg = 1.000).
	*/
}	

float CalcTempF(float degC){
	float degF = 0;
	degF = degC*1.8 + 32.0;
	return degF;
}


//***** ROTARY ENCODER INTERRUPT HANDLING *****
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