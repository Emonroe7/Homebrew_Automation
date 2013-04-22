/*
= @author Eric Monroe
= @contact 
= Assitive Inebriation Device
= 
= This program is written for the SPSU MTRE 4400 CFV Senior Design Team.
= 	-Members: Jesse Dunn, Joshua Canady, Robert Baldwin, Lisa Mathews, and Eric Monroe
= 	-Sponsor: Jim Fink
= 	-Professor: Dr. Chan Ham
= 
======= Pins Required:
= SD Card uses SPI: pins 50(MISO), 51(MOSI), 52(SCK), 53(SS)
= I2C for screens and clock uses pins 20(SDA) and 21(SCL)
= Rotary Encoder uses INT0(pin 2)A INT1(pin 3)B
= Pins 4, 5, and 6 are confirm, dnBtn, and upBtn respectively
= Pins 22-33 are relays for pumps and valves.
= Pin 34 is alarm.
= Analog 0,1,2 for pressure sensor inputs
= 
= 
= 
= TODO: Add buttons for control -Done
= TODO: Add calibration for pressure sensors
= TODO: Add file IO
= TODO: Add recipe parsing
= TODO: Add time tracking
= TODO: Add sensor read functionality
= TODO: Add auto mode following recipe
= TODO: Add settings support
= TODO: Add manual mode
= TODO: Replace man mode in menu with demo mode.
= TODO: AHHHHHHHHHHH
= 
= Future Improvements:
= 	-Find better way to handle ingredient names in recipes
= 	
= 
*/

//===========================================
//================ Libraries ================
//===========================================
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidTWI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SdFat.h>
#include <Button.h>
#include <RTClib.h>


#define DEMO_RECIPE 1
#define SERIAL_OUTPUT_DEBUG 1
#define TEMPERATURE_PRECISION 9	//Precision to use for Dallas 1 wire devices
#define READ_FROM_SD_CARD 0
#define HEATING_PRESENT 0		//0 until we add heating capabilities
#define MAX_RECIPE_NUM 5		//Change to allocate more space to store recipe names
#define MAX_INGREDIENT_LEN 30	//Length of ingredient amount and name 
#define MAX_INGREDIENT_NUM 15	//Change to allocate more space for the ingredient list
#define MAX_MASH_ADDS 5		
#define MAX_BOIL_ADDS 5		
#define MAX_STEEP_ADDS 5
#define PI 3.14159
#define WATER_PUMP 22
#define WORT_PUMP 23

{//REMOVE BRACKET -STRINGS
//Volume Conversion Factors
const float galToCubicInch = 231.0;
const float cubicInchToGal = 0.004329;
const float qtToCubicInch = 57.75;
const float cubicInchToQt = 0.017316;

//Put these in the Flash Memory to save space in the SRAM
const prog_char PROGMEM SPLASH1[] = "Assistive Brewing";
const prog_char PROGMEM SPLASH2[] = "Device";
const prog_char PROGMEM SPLASH3[] = "...Preparing to brew";
const prog_char PROGMEM CHOOSE_MODE[] = "Pick a mode:";
const prog_char PROGMEM AUTO_BREW_OPT[] = "Auto Brewing";
const prog_char PROGMEM MANUAL_BREW_OPT[] = "Manual Brewing";
const prog_char PROGMEM SETTINGS_OPT[] = "Settings";
const prog_char PROGMEM CREDITS_OPT[] = "Credits";
const prog_char PROGMEM CREDITS_TITLE[] = "Awesome Team Members:";
const prog_char PROGMEM J_DUNN[] = "Jesse Dunn";
const prog_char PROGMEM R_BALDWIN[] = "Robert Baldwin";
const prog_char PROGMEM L_MATTHEWS[] = "Lisa Matthews";
const prog_char PROGMEM J_CANADY[] = "Joshua Canady";
const prog_char PROGMEM E_MONROE[] = "Eric Monroe";
const prog_char PROGMEM LOADING[] = "....Loading....";
//const prog_char PROGMEM R_LIST_NAME[] = "rList.txt"
const prog_char PROGMEM NUM_REC_FOUND[] = "# Recipes Found: ";
const prog_char PROGMEM CHOOSE_RECIPE1[] = "Choose a recipe";
const prog_char PROGMEM CHOOSE_RECIPE2[] = "from the next menu";
const prog_char PROGMEM PAGE_LINE[] = "Pg  of    Prev  Next";
const prog_char PROGMEM LOADING_R[] = "Loading Recipe";
const prog_char PROGMEM CONFIRM_CONT[] = ">Confirm to continue";
const prog_char PROGMEM CUR_STEP[] = "Current Step:";
const prog_char PROGMEM MASH_PH[] = "Desired Mash PH:";
//const prog_char PROGMEM PAGE[] = "Page ";
const prog_char PROGMEM NEXT[] = " NEXT";
//const prog_char PROGMEM NEXT_SEL[] = ">NEXT";
const prog_char PROGMEM PREV[] = " PREV";
//const prog_char PROGMEM PREV_SEL[] = ">PREV";
const prog_char PROGMEM CONFIRM[] = " CONFIRM";
//const prog_char PROGMEM CONFIRM_SEL[] = ">CONFIRM";
const prog_char PROGMEM YN_YES[]=">YES< NO ";          
const prog_char PROGMEM YN_NO[]=" YES >NO<";          
const char *yn_items[]= {YN_YES, YN_NO};		//Used to easily go between Yes and No dialog selections


//Setup array to access from within program
const char *string_table[] = {
	SPLASH1,			//0
	SPLASH2,			//1
	SPLASH3,			//2
	CHOOSE_MODE,		//3
	MANUAL_BREW_OPT,	//4
	SETTINGS_OPT,		//5
	CREDITS_OPT,		//6
	CREDITS_TITLE,		//7
	J_DUNN,				//8
	R_BALDWIN,			//9
	L_MATTHEWS,			//10
	J_CANADY,			//11
	E_MONROE,			//12
	LOADING,			//13
	NUM_REC_FOUND,		//14
	CHOOSE_RECIPE1,		//15
	CHOOSE_RECIPE2,		//16
	PAGE_LINE,			//17
	LOADING_R,			//18
	CONFIRM_CONT,		//19
	CUR_STEP,			//20
	MASH_PH,			//21
	CONFIRM,			//22
	PREV,				//23
	NEXT				//24
	
};
//Extract data with: strcpy_P(mem_cpy, (char*)pgm_read_word(&(string_table[0])));
//where i = index of desired string
char mem_cpy[21];
}//REMOVE

{//REMOVE BRACKET -SCHEDULES AND ADR
//Relay Schedule: 0 = OFF, 1 = ON, 2 = DON'T CARE (INTERPRET AS 0 FOR NOW)
//Could possibly use a bit field structure, bit <vector> or just 2 bytes if we need to save space here.
const byte HLT_FILL[] = 	{1,0,1,0,1,0,0,0,0,0,0,0};	//Previously prog_uint8_t PROGMEM, not byte
const byte HLT_RECIRC[] = 	{1,0,0,1,1,0,0,0,0,0,0,0};
const byte STRIKE_TRANS[] = {1,0,0,1,0,1,0,0,0,0,0,0};
const byte MASH[] = 		{1,1,0,1,1,0,1,0,1,0,0,0};
const byte SPARGE_IN_ON[] = {1,1,0,1,0,1,1,0,0,1,0,0};
//const byte SPARGE_IN_OFF[]= {0,1,0,0,0,0,1,0,0,1,0,0};
const byte REFILL_HLT[] = 	{1,0,1,0,1,0,0,0,0,0,0,0};	//For Refilling during boil at the moment
const byte DRAIN_MLT[] =	{0,1,0,0,0,0,1,0,0,0,1,0};
const byte BOIL[] = 		{0,0,0,0,0,0,0,0,0,0,0,0};
const byte COOL[] = 		{1,1,0,1,1,0,0,1,1,0,0,1};
const byte FILL_FERM[] =  	{0,1,0,0,0,0,0,1,0,0,1,0};
const byte DRAIN_HLT[] =  	{1,1,0,1,0,1,0,1,0,0,1,1};
const byte FULL_CLOSE[] =	{0,0,0,0,0,0,0,0,0,0,0,0};	//Same as BOIL, but makes code clearer
const byte chipSelect = 53;	// SD chip select pin
const byte blinkMax = 100;
const char LED_adr[] = {0x71, 0x72, 0x73};	//I2C addresses of LED Displays
DeviceAddress hlt_temp, mlt_temp, bk_temp;	// Creating 1Wire address variables
hlt_temp = {0x28, 0x61, 0x22, 0x5D, 0x04, 0x00, 0x00, 0x4A};
mlt_temp = {0x28, 0xB4, 0x04, 0x5D, 0x04, 0x00, 0x00, 0x01};
bk_temp = {0x28, 0x13, 0xCE, 0x5C, 0x04, 0x00, 0x00, 0xC8};

}//REMOVE

{//REMOVE BRACKET -GLOBAL VARS
//Object creation
SdFat sd;
SdFile file;
FatReader root;
ArduinoOutStream cout(Serial);		//Create a serial output stream for SDFat. Can use cout instead of Serial.write();
RTC_DS1307 RTC;						//Real time clock object
OneWire  ds(10);  					//OneWire devices connected to pin #10 (to change pin, change number)
DallasTemperature temp_sense(&ds);	//Extends and simplifies the OneWire library
LiquidTWI lcd(0);					//Create LCD screen object
Button confirm = Button(4, PULLUP);	//Confirm btn object wired to rot. enc. btn
Button dnBtn = Button(5, PULLUP);	//Menu Nav Down btn object
Button upBtn = Button(6, PULLUP);	//Menu Nav Up btn object
int pulses, A_SIG=0, B_SIG=1;		//Rotary Encoder Variables
//int millisDelay = 100;				//Delay time for btn debouncing
//int confirmMil = 0;					//Vars to store last actuation milli time for debouncing btns
//int upBtnMil = 0; 
//int dnBtnMil = 0;
//boolean confState=0, dnBtnState=0, upBtnState=0;
//boolean confPrevState=0, dnBtnPrevState=0, upBtnPrevState=0;	



//Process Enumeration. Increases readability of code. Have extras here, will clean out later
enum STEP {ST_MENU, ST_READY, ST_FILL, ST_STRIKE, ST_MASH_IN, ST_STEP_MASH_IN, ST_MASH, ST_MASH_OUT, ST_SPARGE, 
		   ST_BOIL, ST_COOL, ST_FILLFERM, ST_DONE, ST_DRAIN, ST_DRAIN_MLT, ST_DRAIN_HLT, ST_CIP, ST_ERROR};
enum MODE {AUTO = 1, MANUAL, SETTINGS};

//Program Global Variables
boolean SD_Present = FALSE;
boolean SD_ERROR = FALSE;	//Possibly use
boolean step_done = FALSE;	
int HLT_pres, MLT_pres, BK_pres;	//RAW pressor sensor readings
float HLT_ht, MLT_ht, BK_ht;
float des_HLT_ht, des_MLT_ht, des_BK_ht;
float HLT_degC, MLT_degC, BK_degC;
float HLT_degF, MLT_degF, BK_degF;
boolean selectionMade;
//boolean rec_selected = FALSE;
byte num_found = 0;			//Number of found recipes
byte tot_page_num = 0;
byte curr_page_num = 0;
byte blinkCnt;
byte cursorLoc;
byte cursorX, cursorY;
boolean blinkState;
char fName[13];				//Max length of a file name is 8+1(.)+3(ext)+null = 13
boolean at_temp = FALSE;
int step_sec_remain = 0;
byte step_min_remain = 0;
DateTime now;


//System Params (need to store in EEPROM later for settings menu)
float step_in_threshold = 130.0;//Theshold at whach to use stepped mash input
float mash_out_temp = 168.0;	//Mashout temp in Farenheight
float vessel_ht = 17.0;			//Height of vessel in inches
float vessel_diam = 16.0;		//Diameter of vessel in inches
int press_sensor_ht = 1;		//Pressure sensor height above tank bottom (in inches)
int act_time = 6000;			//Time it takes for slowest valve to actuate (in ms)
float boil_temp_set = 210.9;	//Boild temp setting because it changes based on altitude.
int boil_power = 512;			//How vigorous should the boil be. (0-1023 or 0-100?)
float ht_hysteresis = 0.5;		//Margin of error form filling
//TODO: Store default step times and temps for manual mode in EEPROM

}//REMOVE

//Recipe Data Structure
struct recipe{
	String name;
	float srm;
	float ibu;
	float og;
	float batch_size;
	int ingredient_num;
//	char ingredients [MAX_INGREDIENT_LEN][MAX_INGREDIENT_NUM];
	float strike_vol;	//gal
	float strike_temp;
	float ph_desire;
	int num_mash_steps;
	float mash_step_temps[MAX_MASH_ADDS];
	int mash_step_times[MAX_MASH_ADDS];
	char mash_step_name [MAX_INGREDIENT_LEN][MAX_MASH_ADDS];
	float sac_temp;
	int sac_time;
	boolean mash_out;
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
} myRecipe;		//Creating recipe struct myRecipe here b/c we will only have 1 recipe at a time.


//=======================================================
//================= Function Prototypes =================
//=======================================================
//void Init_Disp();
//void Splash_screen();
//void Main_Menu(); 
int Auto_Brew();
void Settings_Menu();
void Man_Brew();
byte Get_Num_Recipes();
byte Load_Page(byte);
void Get_Rec_fName(byte, byte);
void Parse_Recipe();
void Actuate_Relays(byte);
void Update_LED();
void Read_Sensors(byte);
//void CalcVolume(int, float);		//By each tank, or all at once?	//Working with Height, not vol.
float CalcTempF(float);
void A_RISE();
void A_FALL();
void B_RISE();
void B_FALL();


//========================================================
//==================== MAIN PROGRAM ======================
//========================================================
//   ######  ######## ######## ##     ## ########  
//  ##    ## ##          ##    ##     ## ##     ## 
//  ##       ##          ##    ##     ## ##     ## 
//   ######  ######      ##    ##     ## ########  
//        ## ##          ##    ##     ## ##        
//  ##    ## ##          ##    ##     ## ##        
//   ######  ########    ##     #######  ##  
void setup() {
	Serial.begin(9600);
	//Init_Disp();
	lcd.begin(20, 4);		// Define LCD size
	lcd.setBacklight(HIGH);	// Turn on the LCD backlight
	lcd.noBlink();
	lcd.noCursor();
	lcd.noAutoscroll();
	
	
	//Called before comms to all TWI devices except lcd display.
	Wire.begin();
	
	RTC.begin();	//Start comms with rtc
	if (!RTC.isrunning()) {	//Ensure clock is ticking so we can keep track of time
		Serial.println("RTC is NOT running! Setting time");
		// following line sets the RTC to the date & time this sketch was compiled
		RTC.adjust(DateTime(__DATE__, __TIME__)); //Need RTC for timekeeping
	}
	
    // Initialize each LED Display
    for (int i = 0; i < 3; i++){
        Wire.beginTransmission(LED_adr[i]);  // Select which display to begin communication with
        Wire.write(0x7A); // Brightness control command
        Wire.write(100);  // Set brightness level: 0% to 100%
        Wire.write('v');  // Clear Display, setting cursor to digit 0
        Wire.endTransmission();  // End communication with this selected module.
    }

	//Setup the OneWire Temp sensors.
	temp_sense.begin();
	//Set Resolution (9 bit should be plenty)
	temp_sense.setResolution(hlt_temp, TEMPERATURE_PRECISION);
	temp_sense.setResolution(mlt_temp, TEMPERATURE_PRECISION);
	temp_sense.setResolution(bk_temp, TEMPERATURE_PRECISION);
	//Output debug msg over serial
	Serial.print("Device 0 Resolution: ");
	Serial.print(temp_sense.getResolution(hlt_temp), DEC); 
	Serial.println();
	Serial.print("Device 1 Resolution: ");
	Serial.print(temp_sense.getResolution(mlt_temp), DEC); 
	Serial.println();
	Serial.print("Device 2 Resolution: ");
	Serial.print(temp_sense.getResolution(bk_temp), DEC); 
	Serial.println();
	
	//Splash_screen();
	//Disp name of project, and other info.
	lcd.clear();
	lcd.setCursor(1,0);	//x,y
	strcpy_P(mem_cpy, (char*)pgm_read_word(&(string_table[0])));	//Const. strings stored in flash prog mem
	lcd.print(mem_cpy);	//SPLASH1
	lcd.setCursor(6,1);
	strcpy_P(mem_cpy, (char*)pgm_read_word(&(string_table[1])));
	lcd.print(mem_cpy);	//SPLASH2
	lcd.setCursor(0,3)
	strcpy_P(mem_cpy, (char*)pgm_read_word(&(string_table[2])));
	lcd.print(mem_cpy);	//SPLASH3
	

	//Setup pins for pumps, solenoid valves, alarm, btns, etc. Dedicated, 1 way I/O, no comms lines
//	pinMode(4, INPUT);		//Confirm Btn
//	pinMode(5, INPUT);		//Nav Down Btn
//	pinMode(6, INPUT);		//Nav Up Btn
	pinMode(22, OUTPUT);	//Water pump
	pinMode(23, OUTPUT);	//Wort pump
	pinMode(24, OUTPUT);	//Water Supply valve
	pinMode(25, OUTPUT);	//
	pinMode(26, OUTPUT);	//
	pinMode(27, OUTPUT);	//
	pinMode(28, OUTPUT);	//
	pinMode(29, OUTPUT);	//
	pinMode(30, OUTPUT);	//
	pinMode(31, OUTPUT);	//
	pinMode(32, OUTPUT);	//
	pinMode(33, OUTPUT);	//HLT Coil output 3 way valve.
	pinMode(34, OUTPUT);	//Alarm
	pinMode(53, OUTPUT);	//Chip Select on MEGA for  SD card. Must be an output.
	
	//Not currently using, but need for manual mode
	//attachInterrupt(0, A_RISE, RISING);
	//attachInterrupt(1, B_RISE, RISING);
	
	//Set pumps to off. This shouldn't be needed, but just in case. We can afford to do this here
	digitalWrite(WATER_PUMP, LOW);
	digitalWrite(WORT_PUMP, LOW);
	
/* 	//Check to see if SD card is present
	if (!sd.begin(chipSelect, SPI_HALF_SPEED))
		sd.initErrorHalt(); //TODO: Throw dialog error and ask user to insert SD card on LCD
*/

	delay(2000);	//Give time to read the Splash Screen.
}


//  ##     ##    ###    #### ##    ## 
//  ###   ###   ## ##    ##  ###   ## 
//  #### ####  ##   ##   ##  ####  ## 
//  ## ### ## ##     ##  ##  ## ## ## 
//  ##     ## #########  ##  ##  #### 
//  ##     ## ##     ##  ##  ##   ### 
//  ##     ## ##     ## #### ##    ## 
void loop()	{	//Contains Main Menu
	//MENU SELECTIONS
	//MODE = Main_Menu();	//Just implemented here. Keeping Function around just in case
	selectionMade = FALSE;
	blinkState = TRUE;	//Was previously named cursorState, changed to assoc. more with blinking
	blinkCnt = 0;
	cursorLoc = 1;
	
	
	//Display Menu Options
	lcd.setCursor(0,0);
	lcd.print(CHOOSE_MODE);	//Const. strings stored in flash prog mem
	lcd.setCursor(1,1);
	lcd.print(AUTO_BREW_OPT);
	lcd.setCursor(1,2);
	lcd.print(MANUAL_BREW_OPT);
	lcd.setCursor(1,3);
	lcd.print(SETTINGS_OPT);	
	//Place Cursor in initial location
	lcd.setCursor(0, cursorLoc);	//Set cursor leftmost pos on line 1 (not 0)
	lcd.print(">");		//Print cursor to screen
	
	//If all menu look similar, could write menu options, then call a func to manipulate menu.
	while (!selectionMade) {
		/* Control Cursor movement between the 3 options in response to up/down buttons.
		 * Possibly blink "cursor" on each loop (alternate b/w printing ">" and printing " ")
		 * Check for confirm button press (and subsequent release if down)
		 */
		
		//Blink Cursor
		if (blinkCnt == blinkMax){
			lcd.setCursor(0,cursorLoc);
			if (blinkState == TRUE){
				lcd.print(" ");
				blinkState = FALSE;
			} else if (blinkState == FALSE) {
				lcd.print(">");
				blinkState = TRUE;
			}
			blinkCnt = 0;
		} else {	
			blinkCnt++;
		}
		
		//Process buttons
		if(confirm.uniquePress() == TRUE) {	
			//TODO: Process debounce if we encounter a bounce problem in testing.
			//if (millis()-pressedStartTime > threshold)
			selectionMade == TRUE;	//Exit menu, go to selected state
		} else if(dnBtn.uniquePress() == TRUE) {
			//Process debounce if problem
			//Set cursorLoc down 1 (cursorLoc ++) and make sure within bounds.
			//Set blinkCnt = blinkMax
			//Set blinkState = FALSE to turn ensure it is turned on during next loop
			//Taking advantage of previously written code to ensure cursor is visible
			lcd.setCursor(0,cursorLoc);	//Clear previous cursor from screen, written in new position by cursor blink script
			lcd.print(" ");
			if(cursorLoc == 3)
				cursorLoc = 1;
			else
				cursorLoc++;
			blinkCnt = blinkMax;
			blinkState = FALSE;
		} else if(upBtn.uniquePress() == TRUE) {
			//Same as above except cursorLoc --, not ++ to 
			lcd.setCursor(0,cursorLoc);	//Clear previous cursor from screen, written in new position by cursor blink script
			if(cursorLoc == 1)
				cursorLoc = 3;
			else
				cursorLoc--;
			blinkCnt = blinkMax;
			blinkState = FALSE;
		}
	}
	//Parse menu selection
	if (cursorLoc == 1){		//MODE == AUTO
		Auto_Brew();
	} else if (cursorLoc == 2){	//MODE == MANUAL
		//Call Man_Brew()
	} else if (cursorLoc == 3){	//MODE == SETTINGS
		//Call Settings_Menu()
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
        Wire.beginTransmission(LED_adr[i]);  // Select which display to begin communication with
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


//********** MENUS AND CONTROL LOOPS **********
void Main_Menu(){
	boolean selectionMade = FALSE;
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
	
	while (!selectionMade){
		//*
		// * Control Cursor movement between the 3 options in response to up/down buttons.
		// * Possibly blink "cursor" on each loop (alternate b/w printing ">" and printing " ")
		// * Check for confirm button press (and subsequent release if down)
		// 
		
	}
	return MODE;
}
*/


//     ###    ##     ## ########  #######      ########  ########  ######## ##      ## 
//    ## ##   ##     ##    ##    ##     ##     ##     ## ##     ## ##       ##  ##  ## 
//   ##   ##  ##     ##    ##    ##     ##     ##     ## ##     ## ##       ##  ##  ## 
//  ##     ## ##     ##    ##    ##     ##     ########  ########  ######   ##  ##  ## 
//  ######### ##     ##    ##    ##     ##     ##     ## ##   ##   ##       ##  ##  ## 
//  ##     ## ##     ##    ##    ##     ##     ##     ## ##    ##  ##       ##  ##  ## 
//  ##     ##  #######     ##     #######      ########  ##     ## ########  ###  ###  
void Auto_Brew(){
	step_done = FALSE;
	byte sense = B00000000;
	STEP = ST_MENU;

	while (!step_done)
	{
		switch(STEP){
			case ST_MENU: {	//Done	//Could break most of this out into other functions later
				selectionMade = FALSE;
				blinkState = TRUE;	//Was previously named cursorState, changed to assoc. more with blinking
				blinkCnt = 0;
				cursorLoc = 0;
				byte entries;	//Place to store number of valid entries on current page
				
				lcd.clear();
				lcd.setCursor(2,1);
				lcd.print(LOADING);
				delay(500);
				num_found = Get_Num_Recipes();	//Scan SD for all recipe files
				if(num_found < 1){	//No valid Recipes found	
					lcd.clear();			//No recipes found error
					lcd.setCursor(1,1);
					lcd.print("No Recipes Found");
					cout << "No Recipes found" << endl;	//Debug to PC over Arduino serial link
					delay(500);
					lcd.setCursor(1,2);
					lcd.print("Returning to Menu");
					delay(5000);
					step_done = TRUE;
					selectionMade = TRUE;
					break;
				}
				
				//If reached, successfully found at least 1 good recipe.
				tot_page_num = ((num_found-1)/3);	//(num_found - 1) so that 3 recipes doesn't result in 2 valid pages. 9rec = 3pages (0-2)
				curr_page_num = 0;
				//Display results
				lcd.clear();
				lcd.setCursor(0,0);
				lcd.print(NUM_REC_FOUND);
				lcd.setCursor(17,0);
				lcd.print(num_found);
				delay(1500);
				//Display instructions
				lcd.setCursor(0,2);
				lcd.print(CHOOSE_RECIPE1);
				lcd.setCursor(0,3);
				lcd.print(CHOOSE_RECIPE2);
				delay(3000);
				
				//Load first menu page
				entries = Load_Page(curr_page_num);
				
				//Menu operation
				while(!selectionMade) {
					//Blink Cursor
					if (blinkCnt == blinkMax) {
						lcd.setCursor(cursorX,cursorY);
						if (blinkState == TRUE){
							lcd.print(" ");
							blinkState = FALSE;
						}else if (blinkState == FALSE) {
							lcd.print(">");
							blinkState = TRUE;
						}
						blinkCnt = 0;
					}else 
						blinkCnt++;	
					
					
					//Button Processing
					if (confirm.uniquePress() == TRUE) {	
						if (cursorLoc <= 3) { //Picked a file
							selectionMade = TRUE;
							lcd.clear();
							lcd.setCursor(2,1);
							lcd.print(LOADING);
							Get_Rec_fName(curr_page_num, cursorLoc);	//Get file name of chosen recipe
						}else if(cursorY == 3) { //Load Prev or Next page
							if (cursorLoc == 4) {	//Previous Page
								Load_Page(curr_page_num--);
							}else if (cursorLoc == 5) {
								Load_Page(curr_page_num++);
							}
						}
					}else if (dnBtn.uniquePress() == TRUE) {
						lcd.setCursor(cursorX,cursorY);	//Clear previous cursor from screen, written in new position by cursor blink script
						lcd.print(" ");
						if ((cursorLoc + 1) <= entries) { 	//Next entry
							cursorX = 0;
							cursorY = cursorLoc;
							cursorLoc ++;
						}else if (cursorLoc == entries) { //Prev
							cursorX = 9;
							cursorY = 3;
							cursorLoc = 4;
						}else if (cursorLoc == 4) {	//Next
							cursorX = 15;
							cursorY = 3;
							cursorLoc = 5;
						}else if (cursorLoc == 5) {	//Top entry
							cursorX = 0;
							cursorY = 0;
							cursorLoc = 1;
						}
						blinkCnt = blinkMax;
						blinkState = FALSE;
						
					}else if (upBtn.uniquePress() == TRUE) {
						lcd.setCursor(cursorX,cursorY);	//Clear previous cursor from screen, written in new position by cursor blink script
						lcd.print(" ");
						if ((cursorLoc - 1) <= 0) {	//Bottom -> Next
							cursorX = 15;
							cursorY = 3;
							cursorLoc = 5;
						}else if (cursorLoc == 5) {	//Prev
							cursorX = 9;
							cursorY = 3;
							cursorLoc = 4;
						}else if (cursorLoc == 4) {	//Bottom entry
							cursorX = 0;
							cursorY = (entries - 1);
							cursorLoc = entries;
						}else if (cursorLoc <= entries) {	//Prev entry
							cursorX = 0;
							cursorY = (cursorLoc - 2);	//-2 b/c off by 1 and decreasing
							cursorLoc--;
						}
						blinkCnt = blinkMax;
						blinkState = FALSE;
					}
				}
				
				STEP = ST_READY;
			} break;
				
			case ST_READY: {				
				/* Select and parse all recipe vars into data structure
				 * Display all ingredients.
				 */
				Parse_Recipe();
				STEP = ST_FILL;
			} break;
			
			case ST_FILL: {
				/* Fill HLT till at level corresponding to myRecipe.strike_vol
				 */
				sense = B00000001;	//Just HLT pressure
				boolean full = FALSE;
				
				des_HLT_ht = 14.5;
				
				lcd.clear();
				lcd.setCursor(4,1);
				lcd.print("Filling HLT");
				
				//Open correct valves
				Actuate_Relays(HLT_FILL);
				delay(5000);		//Allow pump to be primed by water pressure
				digitalWrite(WATER_PUMP, HIGH);
				while (!full){
					/*
					 * Continually check HLT_pres/ht against desired ht derived from strike_vol
					 */
					Read_Sensors(sense);
					
					if(HLT_ht >= (des_HLT_ht - ht_hysteresis)){
						full = true;
					}
				}
				
				STEP = ST_STRIKE;
			} break;
			
			case ST_STRIKE; {
				/* Heat up HLT to myRecipe.strike_temp, add water additives
				 */
				sense = B00001001;	//HLT Pressure and temp
				Actuate_Relays(HLT_RECIRC);	//Turn on correct valves
				
				lcd.clear();
				lcd.setCursor(4,1);
				lcd.print("Mashing in");
				
				digitalWrite(WATER_PUMP,HIGH);	
				//While loop to check temps and setting heat lvl of HLT when heating is added to system
				delay(30000);
				
				if (myRecipe.strike_temp < step_in_threshold ){
					STEP = ST_MASH_IN;
				} else {
					STEP = ST_STEP_MASH_IN;
				}
			} break;
			
			case ST_MASH_IN; {
				/*
				If strike_temp < stepin_threshold -> continue,
				else STEP = ST_STEP_MASH_IN
				
				Pump out of HLT until vol(orginal_ht-now_ht) = strike_vol
				After an initial waiting time (to allow water to reach wort pump)
				turn on wort pump to recirc 
				*/
				boolean full = FALSE;
				//calc des_MLT_ht
				
				lcd.clear();
				lcd.setCursor(1,1);
				lcd.print("Transfering Strike");
				lcd.setCursor(4,2);
				lcd.print("Mashing in");
				
				sense = B00011011;	//HLT and MLT Pressure and heat
				Actuate_Relays(STRIKE_TRANS);
				digitalWrite(WATER_PUMP, HIGH);
				
				//While loop to check temps and setting heat lvl of HLT when heating is added to system
				while (!full) {
					
					Read_Sensors(sense);
					
					if(MLT_ht >= (des_MLT_ht - ht_hysteresis/2)){
						full = true;
					}
					
					
				}
				
				STEP = ST_MASH; //Even if no mash steps, sach step occurs in mash
			} break;
			
			case ST_STEP_MASH_IN; {
				/*
				Transfer in interval if strike_temp above a certain amount, recirc., 
				Turn Water side pump (22) on and of for intervals of time.
				After trans. myRecipe.strike_vol, STEP = ST_MASH
				*/
				sense = B00011011;
				Actuate_Relays(STRIKE_TRANS);
				digitalWrite(WATER_PUMP, HIGH);
				//While loop
				
				STEP = ST_MASH; //Even if no mash steps, sach step occurs in mash
			} break;
			
			case ST_MASH: {
				/*
				Follow Mash Schedule
				-Bring HLT to temp
				
				Continue when curr_mash_step > myRecipe.num_mash_steps;
				-Using > b/c we want to complete each step, advance, see 
				 there isn't corresponding step, then move on.
				-DONT FORGET SACCH!!!! ALWAYS HAVE SACCH as last step.
				
				
				If myRecipe.mash_out == 1 -> STEP = ST_MASH_OUT
				else STEP = ST_SPARGE
				*/
				sense = B00011000;	//Only care about temps
				Actuate_Relays(MASH);
				digitalWrite(WATER_PUMP,HIGH);	//Recirc HLT water to heat coil/Prevent hotspots
				digitalWrite(WORT_PUMP,HIGH);	//Recirc through HLT coil
				
				//while or for loop when following schedule.
				delay(30000);
				
				if (myRecipe.mash_out == 1) {
					STEP = ST_MASH_OUT;
				} else {
					STEP = ST_SPARGE;
				}
			} break;
			
			case ST_MASH_OUT: {
				/*
				Bring HLT and MLT up to myRecipe.mash_out_temp
				Recirc MLT through HLT coil
				After myRecipe.mash_out_time reached, continue to sparge.
				*/
				sense = B00011000; //Only care about temps
				while (!at_temp){
					//Wait for hlt to reach temp
					//TODO: Implement real PID temp logic
					//read sensors
					if (HLT_degF >= mash_out_temp) {
						at_temp = TRUE;
					}
				}
				
				
			} break;
			
			case ST_SPARGE: {
				/*
				Pump out slowly, pump in as needed turing pump 1 on/off as required.
				-(or switching between 2 relay profiles(takes much longer))
				Stop pumping when BK level reaches calculated volume with pre-boil gravity
				*/
				sense = B00011111;	//Check everything except boil kettle temp
				Actuate_Relays(SPARGE_IN_ON);
				digitalWrite(WORT_PUMP, HIGH);
			} break; 
			
			case ST_BOIL: {
				/*
				Boil and sound alarm and update LCD following schedule.
				Also perform drainage op on MLT while pump is unoccupied.
				-Always prompt user to confirm befor operating drain
				*/
				sense = B01100100;
				Actuate_Relays(BOIL);
				//Refill HLT
				delay(10000);
				
				
				STEP = ST_STEEP;
			} break;
			
			case ST_STEEP: {
				/*
				 * Display names of adds on LCD (myRecipe.steep_add_name[][])
				 * Wait till either myRecipe.steep_time reached or confirm btn pressed
				 */
				sense = B00000000;
				
				
				
				STEP = ST_COOL;
			} break;
			
			case ST_COOL: {
				/*
				 * Cool till at myRecipe.cooling_temp
				 * Whirlpooling in Boil Kettle to filter debris happens here.
				 */
				sense = B00101000;
				Actuate_Relays(COOL);
				digitalWrite(WATER_PUMP,HIGH);
				digitalWrite(WORT_PUMP,HIGH);
				
				
				STEP = ST_FERMFILL;
			} break;
			
			case ST_FERMFILL: {
				/*
				 * Don't drain until confirm (possibly flush first from HLT->Cleaned MLT)
				 * Pause upon another confirm after a minimum time passed (5sec or so).
				 * -Offer to 	>Continue Filling
				 * -or			-Next Step-Drain Sys	.
				 * For now, this is final step.
				 */
				
				
			} break;
			
			case ST_DRAIN: {
				/*
				 * Wait till confirm to drain all tanks (implement later)
				 */
				
			} break;
			
			default: {
				/* Display "CRITICAL ERROR" on screen.
				 * Stop pumps, write 0 to at least water supply valve, then stop program.
				 */
				lcd.clear();
				lcd.setCursor(2,1);
				lcd.print("Critical Error");
				lcd.setCursor(2,2);
				lcd.print("Shuttting Down");
				Actuate_Relays(FULL_CLOSE);
				while(1)//Halt, error
			} break;	
		}
	}
}

void Settings_Menu(){	//TODO: Add settings menu
	/*
		Find out what kind of settings Jim will want
		These then need to be stored in EEPROM memory to stick around after power down.
		
		Time, Date, Actuation time, Defaults for manual mode, etc.
	*/
}

void Man_Brew(){	//TODO: Add Manual mode
	
}

//*************** FILE HANDLING ***************
byte Get_Num_Recipes(){	
	int num_recipes = 0;
	
	//Open each file in directory consecutively, check if its valid, increase counter
	while (file.openNext(sd.vwd(), O_READ)) {
		file.printName(&Serial);	
		cout << ' ' << endl;
		if (file.isFile() == TRUE){	//File is a normal text file.
			num_recipes++;
		}
		file.close();
	}
	return num_recipes;
}

byte Load_Page(byte page){  
	byte good_entries = 0;
	bool none_left = false;
	const int buf_size = 20;
	char rec_name_buf[rec_name_buf_size];
	
	//rollover protection here
	if (page > tot_page_num){
		page = 0;
	}else if(page < 0){
		page = tot_page_num;
	}
	
	
	//Get to desired directory location
	if((page < curr_page_num) || (page == 0)) {
		root.rewind();
//		if (page != 0) {	//Following for loop shouldn't run if page == 1, but just in case
		for(int i = 0; i < (page*3);) {	//If page 1 (so 2nd real page) go through 3 valid files that won't be displayed.
			file.openNext(sd.vwd(), O_READ);
			cout << 'Skip file: ';
			file.printName(&Serial);
			cout << endl;
			if (file.isFile() == TRUE){	//Is file is a normal text file?
				i++;	//Increment inside loop. Only want valid files. Bit more robust.
			}
			file.close();
		}
//		}
	}
	
	lcd.clear();
	//Now at correct dir locationto load valid file names
	//Print up to 3 entries to screen.
	while(good_entries < 3 || !none_left){
		if(file.openNext(sd.vwd(), O_READ)){
			if(file.isFile()){
				if(file.getline(rec_name_buf, buf_size, '\n') || file.gcount()){
					if (sdin.fail())
					{	sdin.clear(sdin.rdstate() & ~ios_base::failbit);}
					//If a file can be opened, is valid, and has a line to read, then print 19 chars of first line to correct line
					lcd.setCursor(1,good_entries);
					lcd.print(rec_name_buf);
					good_entries ++;
				}
			}
		}
		else	//No valid files left
		{	none_left = TRUE;	}
	}

	//Add stuff on bottom line. Currently only good for <30 recipes, but should be enough for awhile.
	//TODO: ADD functionality to remove bottom line if 
	lcd.setCursor(0,3);
	lcd.print(PAGE_LINE);
	lcd.setCursor(3,3);
	lcd.print(page + 1);	//Page 0 will show as Page 1
	lcd.setCursor(6,3);
	lcd.print(tot_page_num + 1);	//Account for page 0 not making sense

	//Reset cursor
	cursorX = 0;
	cursorY = 0;
	cursorLoc = 1;
	//lcd.setCursor(cursorX,cursorY);
	//lcd.print(">");
	blinkCnt = blinkMax;
	blinkState = FALSE;
	
	curr_page_num = page;
	return good_entries;
}

void Get_Rec_fName(byte page, byte entry_num){
	byte dir_index = (page * 3) + entry_num;
	
	root.rewind();
	cout << "Getting selected file name." << endl; 
	for(int i = 0; i < dir_index;) {	//Will open all files until the the target file is opened and validated
		file.openNext(sd.vwd(), O_READ);
		cout << 'Skip file: ';
		file.printName(&Serial);
		cout << endl;
		if (file.isFile() == TRUE){	//Is file is a normal text file?
			i++;	//Increment inside loop. Only want valid files. Bit more robust.
		}
		if (i < dir_index) {	//Evaluate after increment for valid file, don't close last file before escaping loop.
			file.close();
		}
	}
	
	//Target file is currently open
	file.getFilename(fName);
	file.close();
	
	return();
}

void Parse_Recipe(){	//Preliminarily DONE
/* 	if(!file.open("test.txt", O_READ)) {
		sd.errorHalt("Opening recipe file for read failed");
	} */
	
	if (DEMO_RECIPE == 1) {
		//Not reading from file due to time constraint for programming and debugging
		//Not part of minimum success criteria;
		myRecipe.name = "Demo Recipe";
		myRecipe.srm = 0.0;
		myRecipe.ibu = 0.0;
		myRecipe.og = 1.00;
		myRecipe.batch_size = 5.00;
		myRecipe.ingredient_num = 1;
		//Display recipe overview
		lcd.clear();
		lcd.setCursor(0,0);
		lcd.print(myRecipe.name);
		lcd.setCursor(0,1);
		lcd.print("SRM:      IBU:      ");
		lcd.setCursor(5,1);
		lcd.print(myRecipe.srm);
		lcd.setCursor(14,1);
		lcd.print(myRecipe.ibu);
		lcd.setCursor(0,2);
		lcd.print("OG:     Batch:     g");
		lcd.setCursor(4,2);
		lcd.print(myRecipe.og);
		lcd.setCursor(15,2);
		lcd.print(myRecipe.batch_size);
		lcd.setCursor(0,3);
		lcd.print(CONFIRM_CONT);
		while(!confirm.uniquePress()){ }
		lcd.clear();
		delay(500);
		//Display ingredients
		lcd.setCursor(0,1);
		lcd.print("Ing1: Water");
		lcd.setCursor(0,3);
		lcd.print(CONFIRM_CONT);
		while(!confirm.uniquePress()){ }
		lcd.clear();
		lcd.setCursor(2,1);
		lcd.print(LOADING_R);
		//Resume loading ingredients
		myRecipe.strike_vol = 3.5;
		myRecipe.strike_temp = 0;
		myRecipe.ph_desire =5.0;
		myRecipe.num_mash_steps = 0;
		myRecipe.sac_temp = 0;
		myRecipe.sac_time = 0.30;
		myRecipe.mash_out = 1;
		myRecipe.mash_out_time = 0.30;
		myRecipe.sparge_vol = 4.00;
		myRecipe.sparge_temp = 0;
		myRecipe.pre_boil_grav = 1.00;
		myRecipe.boil_vol = 6.0;
		myRecipe.boil_time_total = 0.30;
		myRecipe.num_boil_adds = 0;
		myRecipe.num_steep_adds = 0;
		myRecipe.steep_time = 0.10;
		myRecipe.cooling_temp = 55.0;
		
	
	}else {	//IN PROGRESS
		//ifstream sdin(fName);
	
		//if (!sdin.is_open()) error("open");
	}
	/*
		HEADACHE
		Uses standard ifstream and oftream objects
		Write to data structure
		Use .getLine from sdfat library if using .txt (see sample)
		Decide whether or not using .csv or .txt
	*/
}


//****** SENSORS AND ACTUATORS UPDATES ********
void Actuate_Relays(byte){	//DONE
	/* Shut off pumps first
	 * Load the relay schedule profile into local var (maybe)
	 * Go through 3-12 and turn on or off each in relation to schedule.
	 * Actuate pump relays last.
	 */
	
	//Turn off pumps
	digitalWrite(22, LOW);
	digitalWrite(23, LOW);
	
	//Actuate valves for indicated step
	digitalWrite(24, schedule[2]);
	digitalWrite(25, schedule[3]);
	digitalWrite(26, schedule[4]);
	digitalWrite(27, schedule[5]);
	digitalWrite(28, schedule[6]);
	digitalWrite(29, schedule[7]);
	digitalWrite(30, schedule[8]);
	digitalWrite(31, schedule[9]);
	digitalWrite(32, schedule[10]);
	digitalWrite(33, schedule[11]);
	
	//Allow time for valves to completely actuate
	delay(act_time);
}

void Update_LED(int ){ //possibly use Pointers to direct LED display vars to the correct vars.
	/*
		HLT Temp, MLT Temp, and Time left in step (at least initially)
		HLT Temp, BK Temp and something else during cooling.
		(possibly turn on little LED indicators to tell what each is displaying.
		
		For floats (temps), if float/100 >= 1, multiply by ten, print 4 nums to display and turn on decimal b/w digit 3 and 4
							if float/100 < 1, multiply by ten, print nothing to digit 1, then three nums, and turn on decimal as above				
		For times, figure out later.
	*/
	//TODO: Add LED Display Handling
}

void Read_Sensors(uint_8_t sensor_read){
	/*
	 * Do ananlog reads on pressure sensors and read temps with oneWire
	 * Take in byte and then check each bit for which sensors to check.
	 * Different steps require different sensors.
	 * TODO: Possibly add Time limiting to readings
	 */
	if (bitRead(sensor_read,0) == 1){	//HLT Pressure
		HLT_pres = analogRead(A0);
		//Convert to height
		//HLT_ht = map(0,1023,0,1000); //height in mm.  Map not ideal
		//could also calc flowrates
	}
	if (bitRead(sensor_read,1) == 1){	//MLT Pressure
		MLT_pres = analogRead(A1);
		//Convert to height
	}
	if (bitRead(sensor_read,2) == 1){	//BK Pressure
		BK_pres = analogRead(A1);
		//Convert to height
	}
	if (bitRead(sensor_read,3) == 1){	//HLT Temp
		//Read HLT Temp
	}
	if (bitRead(sensor_read,4) == 1){	//MLT Temp
		//Read MLT Temp
	}
	if (bitRead(sensor_read,5) == 1){	//BK Temp
		//Read BK Temp
	}
	if (bitRead(sensor_read,6) == 1){	//Ultrasonic sensor
		//TODO: Implement ultrasonic sensor to read height of froth in boil
	}
}

void CalcVolume(int tankNum, float specificGrav){
	/*
		Use lookup table (or map) for HLT(b/c coil)
		Take into account preboil specific gravity for boil kettle filling.
		
		First convert sensor readings to height in water (sg = 1.000).
	*/
}	

float CalcTempF(float degC){ //Use DallasTemperature::toFahrenheit(tempC)
	float degF = 0;
	degF = degC*1.8 + 32.0;
	return degF;
}


//***** ROTARY ENCODER INTERRUPT HANDLING *****
void A_RISE(){
	detachInterrupt(0);
	A_SIG=1;
	
	if(B_SIG==0)
		pulses++;	//Clockwise
	if(B_SIG==1)
		pulses--;	//Counter-Clockwise
	Serial.println(pulses);
	attachInterrupt(0, A_FALL, FALLING);
}
void A_FALL(){
	detachInterrupt(0);
	A_SIG=0;
	
	if(B_SIG==1)
		pulses++;	//Clockwise
	if(B_SIG==0)
		pulses--;	//Counter-Clockwise
	Serial.println(pulses);
	attachInterrupt(0, A_RISE, RISING);
}
void B_RISE(){
	detachInterrupt(1);
	B_SIG=1;
	
	if(A_SIG==1)
		pulses++;	//Clockwise
	if(A_SIG==0)
		pulses--;	//Counter-Clockwise
	Serial.println(pulses);
	attachInterrupt(1, B_FALL, FALLING);
}	
void B_FALL(){
	detachInterrupt(1);
	B_SIG=0;
	
	if(A_SIG==0)
		pulses++;	//Clockwise
	if(A_SIG==1)
		pulses--;	//Counter-Clockwise
	Serial.println(pulses);
	attachInterrupt(1, B_RISE, RISING);
}
