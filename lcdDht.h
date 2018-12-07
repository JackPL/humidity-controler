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
unsigned int counter = 0;                  // simulate millis() and is easy to reset
bool modeDHT = true;                       // default mode
bool modeSettings = false;
const unsigned int dhtDataInterval = 5000; // number of millisecs between reading DHT data
byte previousButtonState = LOW;            // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateSettings = LOW;    // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustUp = LOW;    // the previous button state as a default has to be LOW because of the pull-down resistor
byte previousButtonStateAdjustDown = LOW;  // the previous button state as a default has to be LOW because of the pull-down resistor
byte storedSettings;                       // stored settings in EEPROM
unsigned long debounceTime = 50;           // milliseconds
unsigned long buttonPressTime;             // when the switch last changed state
String currentSetting = "";

// array of settings
char* settings[]={
  "Brightness", "Contrast", "Humidity"
};

// An array of settings. 
// At the beginning the array is empty, but will be populated when user got into the SETTINGS MODE
// When user will do any setting then the program will build this array in order to to update EEPROM
// EEPROM will be updated only once, at the moment where user will leave the settings mode.
// This method will save the number of write/erase EEPROM cycles which are limited to 100 000
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
	  EEPROM.write(0, 5);    // brightness
	  EEPROM.write(1, 110);  // contrast
	  EEPROM.write(2, 50);   // Humidity	  
  }
  
  // brightness settings
  pinMode(bri, OUTPUT); //Set pin 10 to OUTPUT
  analogWrite(bri, 0); // as a default the light is OFF
  
  // contrast settings
  pinMode(contrast, OUTPUT); //Set pin 10 to OUTPUT
  analogWrite(contrast, EEPROM.read(1)); // from 0 up to 255
  
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
          analogWrite(bri, EEPROM.read(0)); // from 0 up to 255
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

/*
  
  Put the program in settings mode
*/
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
		if (currentSetting == settings[0]) {
			currentSetting = settings[1];
			lcd.print(currentSetting);
			lcd.setCursor(0,1);
			storedSettings = eepromSettings[1];
			lcd.print(storedSettings);
		}
		else if (currentSetting == settings[1]) {
			currentSetting = settings[2];
			lcd.print(currentSetting);
			lcd.setCursor(0,1);
			storedSettings = eepromSettings[2];
			lcd.print(storedSettings);
		}
		else if (currentSetting == settings[2]) {
			// it was the last option, so we are living settings mode and save to EEPROM the new settings
			saveToEeprom();  // write new settings to the EEPROM if they differs from previous.
			modeSettings = false;
			currentSetting = "";
		}

  }
  // mode settins is no active yet, but the button 'settings' has just been presssed
  // that means we shoud turn ON the settings mode and at the same time give the first setting from the array of settings 
  else {
	modeSettings = true;           // turn ON the settings mode
	currentSetting = settings[0];  // give the first setting from array of settings to configure.
	lcd.clear();
	lcd.print(currentSetting);
	
	// define eepromSettings
	for (int i = 0; i<3; i++){
		eepromSettings[i] = EEPROM.read(i);
	}
			
	storedSettings = eepromSettings[0]; // read the first setting value
	lcd.setCursor(0,1);
    lcd.print(storedSettings);			

  }
}

/*
  Button Up or Down
  adjusting chosen settings up or down
  If the button will be pressed continually it increment the values every half a second.
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
        
      }
    }
    else if (previousButtonStateAdjustUp != buttonState) {
    
      // the button has just been released
      // reset previous button state to LOW
      previousButtonStateAdjustDown = buttonState;  
    }
  }
	
}

void writeSettings(byte addr, byte i, bool inc){
  lcd.clear();
  lcd.print(currentSetting);   // setting name
  lcd.setCursor(0, 1);         // column, row
			
  // button UP (inc = true) od DOWN (inc = false) has been clicked. 
  // Increment od decrement the value of given settings.
  if (inc == true) {
    storedSettings = storedSettings+i;
    //EEPROM.update(addr,storedSettings);
	
	// save it in array
    eepromSettings[addr] = storedSettings;
  }
  else {
    storedSettings = storedSettings-i;
    //EEPROM.update(addr,storedSettings);	
	
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

  lcd.print(storedSettings);
}

/* Update settings saved in EEPROM */
void saveToEeprom() {	
  for (int i = 0; i < sizeof(settings); i++) {
    // By updating EEPROM we write it only if the value differs from the one already saved at the same address. 
    EEPROM.update(i,eepromSettings[i]);
  }
}

/* get data from DHT sensor and print them on lcd */
void getDhtSensorData() {

  float h = dht.readHumidity();     // humadity
  float t = dht.readTemperature();  // temperature as Celsius  
  
  // TODO
  // find another way to update
  lcd.setCursor(0, 0);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    lcd.print("DHT sensor fail!");
    return;
  }
  
  // print temperature
  String temperatura = "";
  temperatura = temperatura+t+(char)223+"C";
  lcd.print(temperatura);
  
  // print humidity
  lcd.setCursor(8, 0); // column, row
  String wilgoc = "  H: ";
  wilgoc = wilgoc+round(h)+"%";
  lcd.print(wilgoc);
  
  // print the fan state.
  lcd.setCursor(0,1);
  if (h > EEPROM.read(2)) {
    lcd.print("Fan is ON       ");
    digitalWrite(relayFan, LOW);
  }
  else {  
    lcd.print("Fan is OFF      ");
    digitalWrite(relayFan, HIGH);
  }
}
