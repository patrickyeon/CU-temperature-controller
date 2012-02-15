/*
Chuck Temp Controller
Martin Friedl
100790201

Used to control the temperature of a vacuum chuck using a PID 
controlled Peltier cooler controlled by a Mosfet. Includes 
temperature inputs for the chuck side and heat sync side of the 
peltier cooler.
*/
//LIBRARIES
#include <SubMenuItem.h>
#include <SubMenu.h>
#include <MenuItem.h>
#include <Menu.h>
#include <LiquidCrystal.h>
#include <Button.h>
#include <PID_v1.h>
//CONSTANTS
#define ChuckThermPIN 4                     // Analog Pin 0
#define HSyncThermPIN 5                     // Analog Pin 1
#define Peltier 10                           // Digital Pin 10 ****THIS PIN CONTROLS THE PELTIER COOLER USING PWM!!!
//LCD
LiquidCrystal lcd(7,6,5,4,3,2);     // lcd initialization [LiquidCrystal(rs, enable, d4, d5, d6, d7)]
//MENUITEM LIBRARY
int numItems = 4;
Menu menu = Menu(menuUsed,menuChanged); 
  MenuItem menuItem1 = MenuItem();
  MenuItem menuItem2 = MenuItem();
  MenuItem menuItem3 = MenuItem();
  MenuItem overheat = MenuItem();
  MenuItem menusetpid = MenuItem();
//BUTTON LIBRARY
Button bRight = Button(9,PULLUP);
Button bLeft = Button(12,PULLUP);
Button bUp = Button(11,PULLUP);
Button bDown = Button(8,PULLUP);
//GENERAL VARIABLES
long lastDebounceTime;
long debounceDelay = 200;                    // delay so the program won't detect inputs less than 200mS appart to prevent button bounce inputs (in mS)
long holdDelay = 2000;                       // the length of time the user has to hold a button down before the scroll speed increases (in mS)
long holdSpeed = 100;                         // the scroll speed for when a button is held down longer than holdDelay (in mS)
long inputDelay = debounceDelay;             // delay that controls how far appart inputs can be, starts at debounceDelay, but can change to holdSpeed when a button is held down to inscrease scroll speed (in mS)
long lastUpdate = 0;                         // saves the time of the last update
boolean held = false;                        // if any button is held, this will be true
long heldTime;                               // the time at which a button gets held
int refreshRate = 300;                       // time between screen and variable refreshes (in mS)
//THERMISTOR SETUP
float chuckPad = 9870;                       // balance/pad resistor values, set these to the measured resistance of your pad resistors
float hSyncPad = 10200;                       // "
float thermr = 10000;                        // thermistor nominal resistance
int B = 3950;                                // thermistor "beta" value
float chuckTemps = 0;                        // temp addtion variable, used to later find average
float hSyncTemps = 0;                        // "
double avgChuckTemp;                         // averaged temperature value, updated every refresh cycle 
double avghSyncTemp;                         // "
int numTemps = 1;                            // counter for number of temperature values added up so far, used to find the average
double setTemp = 25;                         // the desired chuck temperature set by the user
//PELTIER COOLER SETUP
double peltierDuty = 0; 					 // sets duty cycle of peltier cooler (0-100)
PID peltierPID(&avgChuckTemp, &peltierDuty, &setTemp,0.0,0.0,0.0, REVERSE); //specify the links and initial tuning parameters
//Default PID Parameters: 2.0, 5.0, 1.0
double lastPeltierUpdate = 0;
boolean overheating;
int settingpid = 0;
//=================
void setup(){
  Serial.begin(115200); // setup serial data baud rate
  chuckTemps = Thermistor(analogRead(ChuckThermPIN), chuckPad); // initialize temperature readings
  hSyncTemps = Thermistor(analogRead(HSyncThermPIN), hSyncPad); // "
  overheating = abs(hSyncTemps - chuckTemps) > 50; // check if peltier cooler is overheating
  lcd.begin(24, 2); // initialize LCD
  //configure menus
  menu.addMenuItem(menuItem1);
  menu.addMenuItem(menuItem2);
  menu.addMenuItem(menuItem3);
  menu.addMenuItem(overheat);
  menu.addMenuItem(menusetpid);
  menu.select(0); // select the menu that is shown initially
  lastDebounceTime = 0; // initialize debounce time variable
  // initialize PID library
  peltierPID.SetMode(AUTOMATIC); // PID control is automatic
  // peltierPID.SetOutputLimits(0,100); // set PID output limited from 0-100
  peltierPID.SetSampleTime(200); // refresh the PID controller a maximum of once every 200mS, but normally will be only calling it every 300mS (refreshRate), so this setting doesn't matter much
  //Serial.println("#S|CPTEST|[]#");// for debugging using computer
}
// returns temperature from a thermistor
float Thermistor(int RawADC, float pad) { // where pad is resistance of pad resistor
  long Resistance;  
  float Temp;  // dual-purpose variable to save space.

  Resistance =((1024 * pad / RawADC) - pad); // calculate resistance of thermistor
  Temp = log(Resistance/thermr); // saving the Log(resistance) so not to calculate  it 4 times later
  Temp = 1 /((1/(298.15)) + (Temp/B));
  Temp = Temp - 273.15;  // convert Kelvin to Celsius                      

  return Temp;// return temperature
}
void loop(){ 
  // once no buttons are being pushed, reset the delays
  if (!bRight.isPressed() && !bLeft.isPressed() && !bUp.isPressed() && !bDown.isPressed() && held){
    inputDelay = debounceDelay;
    held = false;
  }
  // right button press  
  if (bRight.isPressed() && ((millis() - lastDebounceTime) > inputDelay)) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      if(!held){
        held = true;
        heldTime = millis();
      }
      // move menus right or loop back to the beginning of the menus
      if(menu.getCurrentIndex() == 2)
        menu.select(4);
	  else if(menu.getCurrentIndex() == 4)
		menu.select(0);
      else
        menu.up();
      lastDebounceTime = millis();
  }
  // left button press
  if (bLeft.isPressed() && ((millis() - lastDebounceTime) > inputDelay)) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      if(!held){
        held = true;
        heldTime = millis();
      }
      // move menus left or loop back to the end of the menus
      if(menu.getCurrentIndex() == 0)
        menu.select(4);
	  else if(menu.getCurrentIndex() == 4){
			if(--settingpid < 0)
				settingpid = 2;
		}
      else
        menu.down();
      lastDebounceTime = millis();
  }
  // up button press
  if (bUp.isPressed() && ((millis() - lastDebounceTime) > inputDelay)) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      if(!held){
        held = true;
        heldTime = millis();
      }
      // if the button has been held for longer than the set delay, increase the scroll speed
      if((millis() - heldTime) > holdDelay)
        inputDelay = holdSpeed;
      // depending on what the current menu is, do different things      
      switch(menu.getCurrentIndex()){
        case 0:// increase the set temp
          if (setTemp < 50)
            setTemp = setTemp + 1;
            updateVariables();
          break; 
				case 4:
					double pidvals[3] = {peltierPID.GetKp(),
															 peltierPID.GetKi(),
															 peltierPID.GetKd()}
					pidvals[settingpid] += 0.25;
					peltierPID.SetTunings(pidavls[0], pidvals[1], pidvals[2]);
					break;
        case 1:// do nothing
          break;
        case 2:// do nothing
          break;       
      }
      lastDebounceTime = millis();
  }
  if (bDown.isPressed() && ((millis() - lastDebounceTime) > inputDelay)) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      if(!held){
        held = true;
        heldTime = millis();
      }
      // if the button has been held for longer than the set delay, increase the scroll speed
      if((millis() - heldTime) > holdDelay)
        inputDelay = holdSpeed;
      // depending on what the current menu is, do different things           
      switch(menu.getCurrentIndex()){
        case 0:// decrease the set temp
          if (setTemp > 0)
            setTemp = setTemp - 1;
            updateVariables();
          break; 
				case 4:
					double pidvals[3] = {peltierPID.GetKp(),
															 peltierPID.GetKi(),
															 peltierPID.GetKd()}
					pidvals[settingpid] -= 0.25;
					peltierPID.SetTunings(pidavls[0], pidvals[1], pidvals[2]);
					break;
        case 1:// do nothing
          break;
        case 2:// do nothing
          break;          
      }
      lastDebounceTime = millis();
  }
  
  // every cycle add sensor temperature reading to a variable to later be averaged
  chuckTemps = chuckTemps + Thermistor(analogRead(ChuckThermPIN), chuckPad);
  hSyncTemps = hSyncTemps + Thermistor(analogRead(HSyncThermPIN), hSyncPad);
  numTemps = numTemps + 1;
  
  // update the PID controller function
  peltierPID.Compute();
  
  // update the LCD and variables 
  if ((millis() - lastUpdate > refreshRate)){
    updateVariables();
    //logData(setTemp*10,avgChuckTemp*10,avghSyncTemp*10); // for logging data using computer (debugging)
    
    overheating = abs(avgChuckTemp - avghSyncTemp) >= 50; // check if the Peltier cooler is overheating
    if(overheating)
      coolDownLoop();       
    else
      analogWrite(Peltier, peltierDuty); 

  }
  
}
void coolDownLoop(){ // puts program into cooldown loop to avoid peltier cooler overheating
  analogWrite(Peltier, 0);// turn peltier cooler off
  menu.select(3); // select overheating screen
  while(abs(avgChuckTemp - avghSyncTemp) > 40){ // wait for temperature difference to fall below 40degC
    chuckTemps = chuckTemps + Thermistor(analogRead(ChuckThermPIN), chuckPad);
    hSyncTemps = hSyncTemps + Thermistor(analogRead(HSyncThermPIN), hSyncPad);
    numTemps = numTemps + 1;  
    if ((millis() - lastUpdate > refreshRate)){ // update variables
      updateVariables();
    } 
  }
  overheating = false;
  menu.select(0);
}
void updateVariables(){ // updates all temperature values + peltier duty cycle and outputs these to the LCD
    // take average of all the previous temperature readings
    if (numTemps > 0){
      avgChuckTemp = chuckTemps/numTemps;
      avghSyncTemp = hSyncTemps/numTemps;
    }
    chuckTemps = 0;
    hSyncTemps = 0;
    numTemps = 0;
    // update the screen based on what screen the user is currently viewing 
    if (menu.getCurrentIndex() == 0){ // set temp screen
      lcd.setCursor(3, 1); 
      lcd.print(avgChuckTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");
      
      lcd.setCursor(16, 1);
      lcd.print(setTemp,0);
      lcd.print((char)223);
      lcd.print("C  ");       
    }
    if (menu.getCurrentIndex() == 1){ // heatsync temp screen
      lcd.setCursor(9, 1); 
      lcd.print(avghSyncTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");
    }
    if (menu.getCurrentIndex() == 2){ // peltier cooler duty screen
      lcd.setCursor(9, 1); 
      lcd.print(peltierDuty);
      lcd.print((char)37);
      lcd.print("  "); 
    }
    if (menu.getCurrentIndex() == 3){ // overheating screen
      lcd.setCursor(3, 1); 
      lcd.print(avgChuckTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");
      
      lcd.setCursor(16, 1); 
      lcd.print(avghSyncTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");      
    }     
		if(menu.getCurrentIndex() == 4){
			lcd.setCursor(3, 1);
			int i;
			char pidchars[3] = {'p', 'i', 'd'};
			double pidvals[3] = {peltierPID.getKp(),
													 peltierPID.getKi(),
													 peltierPID.getKd()}
			for(i = 0; i < 3; i++){
				lcd.print(settingpid == i ? '*' : ' ');
				lcd.print('K');
				lcd.print(pidchars[i]);
				lcd.print(": ");
				lcd.print(pidvals[i]);
			}
    
    lastUpdate = millis();
}
// when user changes to a new menu, this function is called
void menuChanged(ItemChangeEvent event){
  // depending on new menu selected, change the screen and update the new values to the new screen
  if( event == &menuItem1 ){    
    lcd.clear();
    //         012345678901234567890123
    lcd.print("  Cur Temp    Set Temp  ");
    updateVariables();
  }else if( event == &menuItem2){
    lcd.clear();
    //         012345678901234567890123
    lcd.print("     Heat Sync Temp     ");
    updateVariables();
  }else if( event == &menuItem3 ){
    lcd.clear();
    //         012345678901234567890123
    lcd.print("  Peltier Cooler Duty   ");
    updateVariables();
	}else if(event == &menusetpid){
		lcd.clear();
		updateVariables();
  }else if ( event == &overheat ){
    lcd.clear();
    //         012345678901234567890123
    lcd.print("**PELTIER OVERHEATING!**");    
    updateVariables();  
  }
}

void menuUsed(ItemUseEvent event){
  if( event == &menuItem1 ){
  }else if( event == &menuItem2 ){
  }else if( event == &menuItem3 ){
    //Serial.println("\tmenuItem3 used"); //user feedback
  }
}

void logData(double value1, double value2, double value3) // used in debugging when graphing data in excel
{
   char buffer[5];
   Serial.print("#S|LOGTEST|[");
   Serial.print(itoa((value1), buffer, 10));
   Serial.print(";");
   Serial.print(itoa((value2), buffer, 10));
   Serial.print(";");
   Serial.print(itoa((value3), buffer, 10));
   Serial.println("]#");
} 

