/* 
  ****** Humidity Controler *******
  
   LCD connection:
 * LCD VSS pin to ground
 * LCD VDD pin to 5V
 * LCD V0 pin through resistor 100 ohm (brown, black, brown, gold) to pin 9
 * LCD RS pin to digital pin 12
 * LCD R/W pin to ground
 * LCD E (Enable) pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD A pin to digital pin 10
 * LCD K pin to ground
 
 DHT connections
 * DHT + to 5V
 * DHT out to digital pin 8
 * DHT - to ground

 Button connections
 * one side of each button connect the ground through resistor 50 k Ohm (yellow, purple, orange, gold) and then:
 *  - The light button to digital pin 7 (ground - resistor - button - pin 7)
 *  - The settings button to digital pin 1 (ground - resistor - button - pin 1)
 * the other side of each button connnect to 5v 

 Relay Module (2-channel) using external power supply
 * RELAY IN1 to digital pin 6
 * RELAY put the jumper off and connect to othet source 5V to JD and the ground to GND (at the same set of pins where JD is)
 * RELAY VCC (the one in the set of pins where all the signal inputs are) connect to atmega 328p pin 7 (VCC) [or 5V in case of arduino]
*/

// include libraries:
#include <LiquidCrystal.h> // The LiquidCrystal library works with all LCD displays that are compatible with the Hitachi HD44780 driver.
#include "DHT.h"
#include <EEPROM.h>
#include<string.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


#define buttonUp 0        // pin to navigate UP
#define buttonSettings 1  // pin turning screen in settings mode
// (...)                  // pins 2,3,4,5 are taken by lcd
#define relayFan 6        // which pin is controlling the relay
#define buttonLight 7     // button turning the light ON or OFF
#define DHTPIN 8          // what digital pin we're connected to
#define contrast 9        // pin controlling the lcd screen contrast
#define bri 10            // pin controlling the lcd screen brightness
// pins 11 and 12 have been taken by lcd
#define buttonDown 13     // pin to navigate Down

#define DHTTYPE DHT22     // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor.

bool light = false;                        // light is OFF (false) or ON (true)
bool lockFan = false;                      // lock the fan (prevent turning on and off many times)
long lockStart;                            // counter for the fan lock
unsigned int counter = 0;                  // main loop counter
bool modeDHT = true;                       // default mode
bool modeSettings = false;                 // if we are in setting mode or not
String currentSetting = "";                // what setting we are in at the moment.
const unsigned int dhtDataInterval = 5000; // number of millisecs between reading DHT data
byte previousButtonState = LOW;            // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateSettings = LOW;    // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustUp = LOW;    // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustDown = LOW;  // the previous button state as a default has to be LOW because of the pull-down resistor
byte storedSettings;                       // stored settings in EEPROM
unsigned long debounceTime = 50;           // milliseconds
unsigned long buttonPressTime;             // when the switch last changed state

// an array of settings names
char* settings[]={
  "Brightness", "Contrast", "Humidity", "Default light", "Fan lock"
};

// An array of settings saved in eeprom.
unsigned int eepromSettings[sizeof(settings)];

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  dht.begin();

  // buttons
  pinMode(buttonLight, INPUT);
  pinMode(buttonSettings, INPUT);
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);

  // relay
  pinMode(relayFan, OUTPUT);
  digitalWrite(relayFan, HIGH);
  
  // EEPROM
  if (EEPROM.read(0) == 255) {
	  EEPROM.write(0, 5);      // brightness
	  EEPROM.write(1, 110);    // contrast
	  EEPROM.write(2, 50);     // Humidity
	  EEPROM.write(3, false);  // light
	  EEPROM.write(4, 1);      // time to lock the fan
  }
  
  // populate the array of settings from EEPROM
  for (int i = 0; i<sizeof(settings); i++){
	eepromSettings[i] = EEPROM.read(i);
  }
  
  // brightness settings
  pinMode(bri, OUTPUT); //Set pin 10 to OUTPUT
  if (eepromSettings[3] == true) {
	analogWrite(bri, eepromSettings[0]);
	light = true;
  }
  else{
    analogWrite(bri, 0); // light is OFF
	light = false;
  }
  
  
  // contrast settings
  pinMode(contrast, OUTPUT); //Set pin 10 to OUTPUT
  analogWrite(contrast, eepromSettings[1]); // from 0 up to 255
  
}

void loop() {
  
  /* Check what we are doing at the moment by checking button actions */
  
  // turn the light ON or OFF 
  updateLight(); 
 
  // put program into or out of settings mode
  updateSettings();

  // DHT
  if (modeSettings == false){
    
    if(counter - 0 > dhtDataInterval) {   
      getDhtSensorData(); // get and print DHT data on lcd monitor.
      // reset counters
      counter = 0; 
    }
  }
  else {
  
    // settings adjustment	
	adjustSettings(); 
  }
  
  delay(1); // loop goes every 2 milliseconds
  counter++;
}


/*
  Button to turn the light ON or OFF
*/
void updateLight(){
  
  // see if the switch is open or closed
  byte buttonState = digitalRead(buttonLight);
  
  // prevent button bouncing
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
  Choose settings from the array of settings (Brightness, Contrast, Humidity)
  Display stored settings from the data saved in EEPROM
*/
void chooseFromSettings() {
	
  lcd.clear();
  
  // mode settings is already active
  if (modeSettings == true) {
		
		// change settings
		if (currentSetting == settings[0]) {  // if we were on the first setting 
			currentSetting = settings[1];     // then jump to the next one.
			lcd.print(currentSetting);
			storedSettings = eepromSettings[1];
			lcd.setCursor(0,1);
			lcd.print(storedSettings);
		}
		else if (currentSetting == settings[1]) {
			currentSetting = settings[2];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[2];
			lcd.setCursor(0,1);
			lcd.print(storedSettings);
		}
		else if (currentSetting == settings[2]) {
			currentSetting = settings[3];
			lcd.print(currentSetting);
			storedSettings = eepromSettings[3];
			lcd.setCursor(0,1);
			// TODO
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
			storedSettings = eepromSettings[4];
			lcd.setCursor(0,1);
			lcd.print((String)storedSettings+" min");
		}
		
		else if (currentSetting == settings[4]) {
			
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
  // the the button 'settings' has just been presssed
  // turn ON the settings mode and give the first setting from the array of settings 
  else {
	modeSettings = true;           // turn ON the settings mode
	currentSetting = settings[0];  // give the first setting from array of settings to configure.
	lcd.clear();
	
	// print settings and values
	lcd.print(currentSetting);		
	storedSettings = eepromSettings[0]; // read the first setting value
	lcd.setCursor(0,1);
    lcd.print(storedSettings);
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
      
      // the button is pressed now
      // execute this statement only once
      if (previousButtonStateSettings != buttonState) {

        // remember the button state
        previousButtonStateAdjustUp = buttonState;
		
        
		// do any action we want after the button has been pressed
		if (currentSetting == settings[0]) {
          writeSettings(0,1,true);			
		}
		else if (currentSetting == settings[1]) {
		    writeSettings(1,10,true);
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
          writeSettings(0,1,false);			
		}
		else if (currentSetting == settings[1]) {
		    writeSettings(1,10,false);
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
      }
    }
    else if (previousButtonStateAdjustUp != buttonState) {
    
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
    storedSettings = storedSettings+i;
	
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
  
    // set the light and contrast immediately
  switch (addr) {
	case 0:
      analogWrite(bri, storedSettings); // from 0 up to 255
    break;
    case 1:
      analogWrite(contrast, storedSettings); // from 0 up to 255
    break;
  }
  
  
  // print the value of given setting
  lcd.setCursor(0, 1); // column, row	
  if (storedSettings == true && addr == 3) {
    lcd.print("ON");
  }
  else if (storedSettings == false && addr == 3) {
    lcd.print("OFF");
  }
  else if (addr == 4) {
	lcd.print((String)eepromSettings[4]+" min");
  }
  else{
	lcd.print(storedSettings);
  }
}

// get data from DHT sensor and print them on lcd
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
  if (h > eepromSettings[2] && lockFan == false && eepromSettings[4] != 0) {
    lockFan = true;
	lockStart = millis();
  }
  else if (h < eepromSettings[2] && lockFan == true && millis()-lockStart > eepromSettings[4]*60000){
    lockFan = false;
  }
 
  // turn the fan On or OFF and print the state of fan
  lcd.setCursor(0,1);
  if (lockFan == true){
  lcd.print(millis()-lockStart);
    digitalWrite(relayFan, LOW);
  }
  else {
    lcd.print("Fan is OFF      ");
    digitalWrite(relayFan, HIGH);
  }
}
