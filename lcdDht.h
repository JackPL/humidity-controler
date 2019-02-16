/* 
  ****** Humidity and Light Controler *******
  
   LCD connection:
 * LCD VSS pin to ground
 * LCD VDD pin to 5V
 * LCD V0 pin through resistor 100 ohm (brown, black, brown, gold) to digital pin D11
 * LCD RS pin to digital pin D10
 * LCD R/W pin to ground
 * LCD E (Enable) pin to digital pin D9
 * LCD D4 pin to digital pin D7
 * LCD D5 pin to digital pin D4
 * LCD D6 pin to digital pin D3
 * LCD D7 pin to digital pin D2
 * LCD A pin to digital pin D5
 * LCD K pin to ground
 
 
 DHT connections
 * DHT + to 5V
 * DHT out to digital pin D0
 * DHT - to ground

 
 Button connections
 * one side of each button connect to the ground through resistor 50 k Ohm (yellow, purple, orange, gold) and then:
 *  - The fan button to analog pin A5 (ground - resistor - button - pin A5)
 *  - The light button to analog pin A4 (ground - resistor - button - pin A4)
 *  - The settings button to analog pin 3 (ground - resistor - button - A3)
 *  - The UP button to analog pin A2 (ground - resistor - button - pin A2)
 *  - The DOWN button to analog pin 1 (ground - resistor - button - A1)
 * the other side of each button connnect to 5v 

 
 PIR sensor connections
 * VCC to 5V
 * GND to ground
 * OUT to analog pin A0
 
 
 Relay Module (2-channel) using external power supply
 * RELAY JD-VCC (remove the jumper) and connect to the other source 5V
 * RELAY VCC leave unconected
 * RELAY GND (at the same set of pins where JD-VCC is) connect to the other source GND
 *
 * RELAY IN1 to digital pin 1
 * RELAY IN2 to digital pin 8
 * RELAY VCC (the one in the set of pins where all the signal inputs are) connect to 5V (of the power supply of atmega or arduino)
 * RELAY GND (the one in the set of pins where all the signal inputs are) leave unconected - DO NOT connect it to anything.
 
 External Cristal
 * this program will work only with external 16Mhz cristal due to DHT22 requirements.
 * connect the external cristal to XLAT1 and XLAT2 pins of atmega328P
*/

// include libraries:
#include <LiquidCrystal.h> // The LiquidCrystal library works with all LCD displays that are compatible with the Hitachi HD44780 driver.
#include "DHT.h"
#include <EEPROM.h>
#include<string.h>

// define atmega328 pins
#define relayFan 1        // which pin is controlling the relay
#define buttonFan 19      // pin to turn the fan ON             (Analog in A5)
#define buttonLight 18    // button turning the light ON or OFF (Analog in A4)
#define buttonSettings 17 // pin to settings                    (Analog in A3)
#define buttonUp 16       // pin to navigate UP                 (Analog in A2)
#define buttonDown 15     // pin to navigate Down               (Analog in A1)
#define DHTPIN 0          // what digital pin we're connected to
#define contrast 11       // pin controlling the lcd screen contrast 
#define bri 5             // pin controlling the lcd screen brightness
#define DHTTYPE DHT22     // DHT 22  (AM2302), AM2321




// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE); 

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 10, en = 9, d4 = 7, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// contrast values should not exceed maxContrast or be less than minContrast because those values are not visible on LCD screen
byte maxContrast = 120;
byte minContrast = 0;


byte fanForced = 0;							// mode 0-> normal; 1->fan forced to run; 2->fan forced to stop.
long fanStopTime = 0;						// the time when the fan was turned OFF
long fanWorkingTimeAllowed;					// the maximum time for the fan to run
bool light = false;                        	// light is OFF (false) or ON (true)
bool lockFan = false;                      	// lock the fan (prevent turning on and off many times)
long lockStart = 0;                         // counter for the fan lock
long fanSaveTime = 0;						// when the fan was turned off in order to cool
long forceTimer = 0;						// count the time of fan forced to run
unsigned int counter = 0;                  	// main loop counter
bool modeDHT = true;                       	// default mode
bool modeSettings = false;                 	// if we are in setting mode or not
bool fanProtect = false;					// fan protection prevents from running the fan for too long.
String currentSetting = "";                	// what setting we are in at the moment.
const unsigned int dhtDataInterval = 5000; 	// number of millisecs between reading DHT data
byte previousButtonState = LOW;            	// the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateFan = LOW;
byte previousButtonStateSettings = LOW;    	// the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustUp = LOW;    	// the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustDown = LOW;  	// the previous button state as a default has to be LOW because of the pull-down resistor
byte storedSettings;                       	// stored settings in EEPROM
unsigned long debounceTime = 70;           	// milliseconds
unsigned long buttonPressTime;             	// when the switch last changed state
unsigned long fanButtonPressTime;			// when the fan ON/OFF button has been pressed.
unsigned long lastPirSensorRead;			// the time of pir sensor reading

// an array of settings names
char* settings[]={
	"Brightness", "Contrast", "Humidity", "Default light", "Fan lock", "Fan max run time", "Fan time to rest", "Light lock"
};

// An array of settings saved in eeprom.
unsigned int eepromSettings[sizeof(settings)];


/* --------------- PIR SENSOR -------------------------------------------- */   
#define pirPin 14     		//PIR out (Analog in A0)
int ledPin = 8;          	//the led light pin (the light is ON or OFF)
boolean lowLock = false;
long unsigned lowInTime; //the time when the sensor outputs a low impulse
bool readPirSensor = false;
/* --------------- EOF: PIR SENSOR ------------------------------------------*/


void setup() {
	// set up the LCD's number of columns and rows:
	lcd.begin(16, 2);
	dht.begin();

	// buttons
	pinMode(buttonFan, INPUT);
	pinMode(buttonLight, INPUT);
	pinMode(buttonSettings, INPUT);
	pinMode(buttonUp, INPUT);
	pinMode(buttonDown, INPUT);

	// relay
	pinMode(relayFan, OUTPUT);
	digitalWrite(relayFan, HIGH);

	// EEPROM
	if (EEPROM.read(0) == 255) {
		EEPROM.write(0, 7);		// brightness
		EEPROM.write(1, 90);    // contrast
		EEPROM.write(2, 37);    // Humidity
		EEPROM.write(3, true);  // lcd light
		EEPROM.write(4, 1);     // time to lock the fan		
		EEPROM.write(5,2);		// fan max running time
		EEPROM.write(6,1);		// fan time to cool down		
		EEPROM.write(7, 5);		// time to lock the light
	}

	// populate the array of settings from EEPROM
	for (int i = 0; i<sizeof(settings); i++){
		eepromSettings[i] = EEPROM.read(i);
	}

	// brightness settings
	pinMode(bri, OUTPUT); //Set pin as OUTPUT
	if (eepromSettings[3] == true) {
		analogWrite(bri, eepromSettings[0]);
		light = true;
	}
	else{
		analogWrite(bri, 0); // light is OFF
		light = false;
	}
	
	
	// fan variables
	fanWorkingTimeAllowed = eepromSettings[5]*60000;  // in miliseconds

	// contrast settings
	pinMode(contrast, OUTPUT); //Set the pin as OUTPUT
	analogWrite(contrast, eepromSettings[1]); // from 0 up to 255
	
	// PIR SENSOR
	pinMode(pirPin, INPUT);
	digitalWrite(pirPin, HIGH);
	pinMode(ledPin, OUTPUT);
	digitalWrite(ledPin, HIGH);
}

void loop() {
  
	/* Check what we are doing at the moment by checking button actions */
	
	// forceTimer
	forcedFanTimer();
	
 	// turn the fan ON or OFF 
	updateFan();
	
	// turn the light ON or OFF 
	updateLight();

	// put the program IN or OUR the settings mode
	updateSettings();
		

	// DHT
	if (modeSettings == false){
	
		if(counter > dhtDataInterval) {   
		  getDhtSensorData(); // get and print DHT data on lcd monitor.
		  // reset counters
		  counter = 0; 
		}
	}
	else {
		// settings adjustment  
		adjustSettings(); 
	}
	
	// read pir sensor every one second.
	if (counter % 1000 == 0) {
		readPirSensor = true;
	}
	
	// PIR sensor
	pirSensor();

	// delay loop
	delay(1); // loop goes every 1 milliseconds
	counter++;
}


/*
 * Turn the fan ON or OFF
 */
void updateFan(){
	
	// see if whether the button is pressed or not
	byte fanButtonState = digitalRead(buttonFan);
	
		// prevent button debouncing
	if (millis() - fanButtonPressTime >= debounceTime) {

		if (fanButtonState == HIGH) {

			// remember when the button has been pressed
			fanButtonPressTime = millis();

			// the button is pressed now
			// execute this statement only once - it prevents from multi switching on and off
			if (previousButtonStateFan != fanButtonState) {

				// remember state
				previousButtonStateFan = fanButtonState;

				// turn the fan ON or OFF
				// if it's ON it will be in that state for as long as the lock time (delclared in settings)
				
				
				// 0 -> normal mode
				// 1 -> forced ON
				// 2 -> forced OFF
				if (fanForced == 1) {
					fanForced = 2;
				}
				else if(fanForced == 2) {
					fanForced = 1;
				}
				else {
				
					if (!lockFan || fanProtect) {
						fanForced = 1;
					}
					else {
						fanForced = 2;
					}	
				}

				forceTimer = 0;
				fanControl(lockFan);	
			}
		}
		
		else if (previousButtonStateFan != fanButtonState) {

			// the button has just been released
			// reset previous button state to LOW
			previousButtonStateFan = fanButtonState;  
		}

	}
}

/*
  Button to turn the light ON or OFF
*/
void updateLight(){
  
	// see if the switch is open or closed
	byte buttonState = digitalRead(buttonLight);

	// prevent button debouncing
	if (millis() - buttonPressTime >= debounceTime) {

		if (buttonState == HIGH) {

			// remember when the button has been pressed
			buttonPressTime = millis();

			// the button is pressed now
			// execute this statement only once
			if (previousButtonState != buttonState) {

				// remember state
				previousButtonState = buttonState;

				// turn the light ON or OFF
				if (light == false) {
				  // turn the light ON
				  analogWrite(bri, eepromSettings[0]); // from 0 up to 255
				  light = true;
				}
				else {
				  // turn the light OFF
				  analogWrite(bri, 0); // from 0 up to 255
				  light = false; 
				}
			}
		}
		else if (previousButtonState != buttonState) {

			// the button has just been released
			// reset previous button state to LOW
			previousButtonState = buttonState;  
		}
	}
}


//  Put the program in settings mode
void updateSettings(){
  
	// see if the switch is open or closed
	byte buttonState = digitalRead(buttonSettings);

	// prevent button debouncing
	if (millis() - buttonPressTime >= debounceTime) {

		if (buttonState == HIGH) {

			// remember when the button has been pressed
			buttonPressTime = millis();

			// the button is pressed now
			// execute this statement only once
			if (previousButtonStateSettings != buttonState) {

				// remember the button state
				previousButtonStateSettings = buttonState;

				// do any action we want after the button has been pressed
				chooseFromSettings();
			}
		}
		else if (previousButtonStateSettings != buttonState) {

			// the button has just been released
			// reset previous button state to LOW
			previousButtonStateSettings = buttonState;  
		}
	}
}

/*
  When the button Settings is pressed
  Choose settings from the array of settings (Brightness, Contrast, Humidity and so on)
  and display the value of the setting
*/
void chooseFromSettings() {
  
	lcd.clear();

	// mode settings is already active
	if (modeSettings == true) {

		// change settings
		if (currentSetting == settings[0]) {    // if we were on the first setting 
			currentSetting = settings[1];       // then jump to the next one.
			lcd.print(currentSetting);
			storedSettings = eepromSettings[1]; // contrast
			lcd.setCursor(0,1);
			lcd.print(maxContrast-storedSettings);
		}
		else if (currentSetting == settings[1]) {
			currentSetting = settings[2];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[2];  // humidity
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+"%");
		}
		else if (currentSetting == settings[2]) {
			currentSetting = settings[3];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[3];  // light
			lcd.setCursor(0,1);

			// shift the value ON and OFF
			if (storedSettings == true) {
				lcd.print("ON");
			}
			else if (storedSettings == false) {
				lcd.print("OFF");
			}
		}
		else if (currentSetting == settings[3]) {
			currentSetting = settings[4];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[4];  // fan lock
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+" min");
		}
		else if (currentSetting == settings[4]) {
			currentSetting = settings[5];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[5];  // fan max run time
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+" min");
		}
		else if (currentSetting == settings[5]) {
			currentSetting = settings[6];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[6];  // fan time to get rest
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+" min");
		}		
		else if (currentSetting == settings[6]) {
			currentSetting = settings[7];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[7];  // light lock
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+" min");
		}

		else if (currentSetting == settings[7]) {
		  
			// it was the last option, so we are living settings mode and need to UPDATE EEPROM
			// the setting will be written to the EEPROM if they differs from previous.
			for (int i = 0; i < sizeof(settings); i++) {
				// By updating EEPROM we write it only if the value differs from the one already saved at the same address. 
				EEPROM.update(i,eepromSettings[i]);
			}

			// we are exiting the settings mode
			modeSettings = false;
			currentSetting = "";
			lcd.clear();
			lcd.print("Saving");
			lcd.setCursor(0,1);
			lcd.print("settings...");
		}

	}
	// currentSetting is empty string. It means the mode settins is no active yet
	// the button 'settings' has just been presssed (for the first time)
	// turn ON the settings mode and give the first setting from the array of settings 
	else {
		modeSettings = true;           // turn ON the settings mode
		currentSetting = settings[0];  // give the first setting from array of settings to configure.
		lcd.clear();

		// print settings and values
		lcd.print(currentSetting);    
		storedSettings = eepromSettings[0]; // read the first setting value
		lcd.setCursor(0,1);
		lcd.print(storedSettings);          // print the first setting  value
	}
}

/*
  Button Up or Down has been pressed.
  Adjusting chosen settings UP or DOWN
  If the button will be pressed without releasing it then do the incrementation on every half a second.
*/
void adjustSettings() {
  
	//button UP 

	// see if the switch is open or closed
	byte buttonState = digitalRead(buttonUp);

	// prevent button debouncing
	if (millis() - buttonPressTime >= 500) {

		if (buttonState == HIGH) {

			// remember when the button has been pressed
			buttonPressTime = millis();
      
			// the SETTINGS button is pressed now
			// execute this statement only once
			if (previousButtonStateSettings != buttonState) {

				// remember the button state
				previousButtonStateAdjustUp = buttonState;
		
			
				// do any action we want after the button has been pressed
				if (currentSetting == settings[0]) {
					writeSettings(0,1,true);      
				}
				else if (currentSetting == settings[1]) {
					writeSettings(1,10,false); // writeSettings(address,value,increment or decrement)
				}
				else if (currentSetting == settings[2]) {
					writeSettings(2,1,true);
				}
				else if (currentSetting == settings[3]) {
					writeSettings(3,1,false);
				}
				else if (currentSetting == settings[4]) {
					writeSettings(4,1,true);
				}
				else if (currentSetting == settings[5]) {
					writeSettings(5,1,true);
				}
				else if (currentSetting == settings[6]) {
					writeSettings(6,1,true);
				}
				else if (currentSetting == settings[7]) {
					writeSettings(7,1,true);
				}
			}
		}
		else if (previousButtonStateAdjustUp != buttonState) {
    
			// the button has just been released
			// reset previous button state to LOW
			previousButtonStateAdjustUp = buttonState;  
		}
	}
  
  
	// button down

    // see if the switch is open or closed
	buttonState = digitalRead(buttonDown);

	// prevent button debouncing
	if (millis() - buttonPressTime >= 500) {

		if (buttonState == HIGH) {

			// remember when the button has been pressed
			buttonPressTime = millis();
      
			// the button is pressed now
			// execute this statement only once
			if (previousButtonStateSettings != buttonState) {

				// remember the button state
				previousButtonStateAdjustDown = buttonState;
    
        
				// do any action we want after the button has been pressed
				if (currentSetting == settings[0]) {
					writeSettings(0,1,false);     // writeSettings(address,value,increment or decrement)
				}
				else if (currentSetting == settings[1]) {
					writeSettings(1,10,true);
				}
				else if (currentSetting == settings[2]) {
					writeSettings(2,1,false);
				}
				else if (currentSetting == settings[3]) {
					writeSettings(3,1,false);
				}
				else if (currentSetting == settings[4]) {
					writeSettings(4,1,false);
				}
				else if (currentSetting == settings[5]) {
					writeSettings(5,1,false);
				}
				else if (currentSetting == settings[6]) {
					writeSettings(6,1,false);
				}
				else if (currentSetting == settings[7]) {
					writeSettings(7,1,false);
				}
			}
		}
		else if (previousButtonStateAdjustDown != buttonState) {
    
			// the button has just been released
			// reset previous button state to LOW
			previousButtonStateAdjustDown = buttonState;  
		}
	} 
}

/* Button UP or DOWN has been clicked.
   Increment od decrement the value of given settings. 
 */
void writeSettings(byte addr, byte i, bool inc){

	lcd.clear();
	lcd.print(currentSetting);   // setting name


	// Increment od decrement the value of given settings.
	//inc = true for button UP 
	//inc = false for button DOWN
	if (inc == true) {
		
		// increment current settings
		storedSettings = storedSettings+i;
		
		// reset contrast values 
		// these values should not exceed maxContrast or be less than minContrast because those values are not visible on LCD screen
		if (addr == 1){
			if (storedSettings > maxContrast) {
				storedSettings = maxContrast;
			}
		}

		// save it in array
		eepromSettings[addr] = storedSettings;
	}
	
	
	else if (addr == 1 && inc == false) { // dąży do najsilniejszego kontrastu równego 0
	
		// decrement current settings
		storedSettings = storedSettings-i;
		
		// if we crossed do zero downwards it will leap into the max value (255) and the contrast will be unvisible.
		// prevent it by seetting the storedSettings value as minContrast.
		if (storedSettings > maxContrast) {
			storedSettings = minContrast;
		}
		
		// save it in array
		eepromSettings[addr] = storedSettings;
	}
	else if (addr == 3 && inc == false) {
		// there is no incrementation
		// it is only true or false
		if (storedSettings == true) {
			storedSettings = false;
		}
		else{
			storedSettings = true;
		}
		// save it in array
		eepromSettings[addr] = storedSettings;
	}
	else {
		storedSettings = storedSettings-i;    
		// save it in array
		eepromSettings[addr] = storedSettings;
	}


	// print the value of given setting
	lcd.setCursor(0, 1); // column, row 
	
	if (addr == 1){
		lcd.print(maxContrast - storedSettings);
	}
	else if (addr == 2) {
		lcd.print((String)eepromSettings[2]+"%");
	}
	else if (storedSettings == true && addr == 3) {
		lcd.print("ON");
	}
	else if (storedSettings == false && addr == 3) {
		lcd.print("OFF");
	}
	else if (addr == 4) {
		lcd.print((String)eepromSettings[4]+" min");
	}
	else if (addr == 5) {
		lcd.print((String)eepromSettings[5]+" min");
	}
	else if (addr == 6) {
		lcd.print((String)eepromSettings[6]+" min");
	}
	else if (addr == 7) {
		lcd.print((String)eepromSettings[7]+" min");
	}
	else{
		lcd.print(storedSettings);
	}
	
	
	// set the light and contrast immediately in order to see how it works
	switch (addr) {
		case 0:
		analogWrite(bri, storedSettings); // from 0 up to 255
		break;
		case 1:
		analogWrite(contrast, storedSettings); // from 0 up to 255
		break;
	}
}

/*
 * get data from DHT sensor and print them on lcd
 */ 
void getDhtSensorData() {

	  float h = dht.readHumidity();     // humadity
	  float t = dht.readTemperature();  // temperature as Celsius
	  
	  // clear lcd
	  lcd.clear();
	  


	  // Check if any reads failed and exit early (to try again).
	  if (isnan(h) || isnan(t)) {
		lcd.print("DHT sensor fail!");
		return;
	  }
	  
	  // print temperature
	  lcd.print((String)t+(char)223+"C");
	  
	  // print humidity
	  lcd.setCursor(10, 0); // column, row
	  lcd.print("H: "+String(round(h))+"%");
	  
	  // set the fan lock ON or OFF (true or false)
	  
	  // HUMIDITY has risen to high
	  if (h > eepromSettings[2] && lockFan == false && eepromSettings[4] != 0) {  
		lockFan = true;
		lockStart = millis();
	  }
	  // HUMIDITY is not high any more, but lock is active. Release the lock.
	  else if (h <= eepromSettings[2] && lockFan == true && millis()-lockStart > eepromSettings[4]*60000){  
		lockFan = false;
	  }
	  
	  // timer
	  fanTimer(lockFan);

	  // turn the fan ON or OFF
	  fanControl(lockFan);

}

/* 
 * Count the time of the fun turned on.
 * - parameter bool fan: 
 * --  true: fan is running
 * --  false: fan is NOT running
 */ 
void fanTimer(bool fan){
			
	// fan is resting or is turned OFF; increment the allowed time for the fan to be turned ON while protection time.
	if (fanProtect || !fan || fanForced == 2){
		fanWorkingTimeAllowed = fanWorkingTimeAllowed+dhtDataInterval;
	}
	// fan is working; decrement the allowed time for the fan to be turned ON
	else if (fan) {
		fanWorkingTimeAllowed = fanWorkingTimeAllowed-dhtDataInterval;		
	}

	// make sure do not exceed allowed maximum fan working time.
	if (fanWorkingTimeAllowed >= eepromSettings[5]*60000) {  
		fanWorkingTimeAllowed = eepromSettings[5]*60000;
	}		
	
	// Turn the protection on or off
	if (fanWorkingTimeAllowed <= 0){
		fanProtect = true;
		fanStopTime = millis();
	}
	// wait untill fan cool down
	else if (fanProtect && millis()-fanStopTime < eepromSettings[6]*60000) {
		fanProtect = true;
	}
	// continue resting if the light is OFF
	else if (fanProtect && digitalRead(ledPin) == LOW) {
		fanProtect = true;
	}
	// when the time for to cool down passed reset the allowed max running time
	else if (fanProtect && millis()-fanStopTime >= eepromSettings[6]*60000) {
		fanWorkingTimeAllowed = eepromSettings[5]*60000;
		fanProtect = false;
	}
	// turn OFF the protection
	else {
		fanProtect = false;
	}
}


/*
 * PIR SENSOR
 * Turn the light ON or OFF
 */
void pirSensor(){
 
	if (millis() > 30000) { // pir sensor need that time to calibrate
	
		if (readPirSensor == true) {
		
			// Note the HIGH signal is frozen for at least 3 seconds - depend on potentiometer set.
			if(digitalRead(pirPin) == HIGH){
				digitalWrite(ledPin, HIGH);  // the light is ON
				lowLock = false;
			}

			// Note the LOW signal is frozen for approx 5 seconds
			// and only after this 5 seconds the sensor will be able to detect a new motion
			if(digitalRead(pirPin) == LOW){


				// only if it is the first LOW signal
				if (!lowLock){
					lowInTime = millis();       //save the time of the transition from high to LOW
					lowLock = true;
				}

				// lock time last longer than sustainLight.
				if(lowLock && millis() - lowInTime > eepromSettings[7]*60000){
					digitalWrite(ledPin, LOW);  // turn the light OFF
					
					// display info
					lcd.setCursor(0,1);
					lcd.print("Light is OFF    ");
				}
			}
			
			// reset
			readPirSensor = false;
		}
	}
}

/*
 * Due to safety reasons the fan can NOT be turned on for ever.
 * Count the time how long the fan is forced to run.
 * When the time is out then turn the forcing mode OFF by reseting fanForced variable.
 */ 
void forcedFanTimer(){
	
	// fan is forced ON
	if (fanForced == 1) {
	
		// increment timer
		forceTimer = forceTimer+1;
		
		// turn the forcing OFF
		if (forceTimer >= eepromSettings[5]*60000) {
			fanForced = 0;
		}
	}
}

/*
 * Turn the fan ON or OFF and print the state.
 */ 
void fanControl(bool on){
	
	lcd.setCursor(0,1);
	
	// NORMAL MODE
	if (fanForced == 0) {

			// humidity is high; fan is ready to work; 
		if (on && !fanProtect){		
			// fan is ON
			digitalWrite(relayFan, LOW);
			lcd.print("Fan is ON       ");
			//lcd.print(fanWorkingTimeAllowed/1000);
		}
		// the fan should rest;
		else if (on && fanProtect) {
			digitalWrite(relayFan, HIGH); // turn the fan OFF (protection mode)
			lcd.print("Fan is resting  ");		
		}
		// humidity is low
		else if (!on) {
			digitalWrite(relayFan, HIGH); // turn the fan OFF
			lcd.print("Fan is OFF  ");		
		}
		else {
			// fan is OFF
			digitalWrite(relayFan, HIGH);
			lcd.print("Fan is OFF ???  ");	
			//lcd.print(fanWorkingTimeAllowed/1000);
		}
		
		
	}
	
	// FORCED MODE
	else if (fanForced == 1) {
		digitalWrite(relayFan, LOW); // force the fan to turn ON
		lcd.print("Fan forced ON   "); 
	}
	else{
		digitalWrite(relayFan, HIGH); // force the fan to turn OFF
		lcd.print("Fan forced OFF  ");
	}
}
