/*
Chuck Temp Controller
Martin Friedl, Patrick Yeon

Used to control the temperature of a vacuum chuck using a PID
controlled Peltier cooler controlled by a Mosfet. Includes
temperature inputs for the chuck side and heatsink side of the
peltier cooler.
*/

#include <SubMenuItem.h>
#include <SubMenu.h>
#include <MenuItem.h>
#include <Menu.h>
#include <LiquidCrystal.h>
#include <Button.h>
#include <PID_v1.h>

// Thermistor to the chuck (Analog Pin 4)
#define ChuckThermPIN 4
// Thermistor to the heatsink (Analog Pin 5)
#define HSinkThermPIN 5
// Output control line for the peltier device (must be a digital PWM pin)
#define Peltier 10
#define PeltierDir 13
#define PeltierEn 1
#define MAXTEMP 70

//LCD [LiquidCrystal(rs, enable, d4, d5, d6, d7)]
LiquidCrystal lcd(7,6,5,4,3,2);

//MENUITEM LIBRARY
Menu menu = Menu(menuUsed,menuChanged);
  MenuItem menuSetTemp = MenuItem();
  MenuItem menuHeatSink = MenuItem();
  MenuItem menuPeltierDuty = MenuItem();
  // debug menus
  MenuItem menusetpid = MenuItem();
int maxMenuItem = 2;

//BUTTON LIBRARY
Button bRight = Button(9,PULLUP);
Button bLeft = Button(12,PULLUP);
Button bUp = Button(11,PULLUP);
Button bDown = Button(8,PULLUP);

//GENERAL VARIABLES
long lastDebounceTime;
// delay so the program won't detect inputs less than 200mS appart to prevent
// button bounce inputs (in mS)
long debounceDelay = 200;
// the length of time the user has to hold a button down before the scroll speed
// increases (in mS)
long holdDelay = 2000;
// the scroll speed for when a button is held down longer than holdDelay (in mS)
long holdSpeed = 100;
// delay that controls how far appart inputs can be, starts at debounceDelay,
// but can change to holdSpeed when a button is held down to inscrease scroll
// speed (in mS)
long inputDelay = debounceDelay;
long lastUpdate = 0; // saves the time of the last update
boolean held = false; // if any button is held, this will be true
long heldTime; // the time at which a button gets held
int refreshRate = 300; // time between screen and variable refreshes (in mS)

//THERMISTOR SETUP
float chuckPad = 9870; // measured chuck pad resistor value
float hSinkPad = 10200; // measure heatsink pad resistor value
float thermr = 10000; // thermistor nominal resistance
int B = 3950; // thermistor "beta" value
float chuckTemps = 0; // temp addtion variable, used to later find average
float hSinkTemps = 0; // "
double avgChuckTemp; // averaged temperature value, updated every refresh cycle
double avghSinkTemp; // "
int numTemps = 1; // counter for number of temperature values added up so far,
                  // used to find the average
double setTemp = 25; // the desired chuck temperature set by the user

//PELTIER COOLER SETUP
double peltierDuty = 0; // sets duty cycle of peltier cooler (-255 to 255)
PID peltierPID(&avgChuckTemp, &peltierDuty, &setTemp, 400, 4, 0, REVERSE
// (input, output, target, Kp, Ki, Kd, direction)
double lastPeltierUpdate = 0;

boolean overheating; // Differential across the peltier device is too high
int settingpid = 0;
boolean debugmode = false; // Used for tuning the PID loop

void setup(){
  // Initialize the board
  //Serial.begin(115200); // setup serial data baud rate

  // initialize temperature readings
  chuckTemps = Thermistor(analogRead(ChuckThermPIN), chuckPad);
  hSinkTemps = Thermistor(analogRead(HSinkThermPIN), hSinkPad);
  // 50 C is too hot!
  overheating = abs(hSinkTemps - chuckTemps) > MAXTEMP;

  lcd.begin(24, 2); // initialize LCD

  //configure menus
  menu.addMenuItem(menuSetTemp);
  menu.addMenuItem(menuHeatSink);
  menu.addMenuItem(menuPeltierDuty);
  // Allow debug mode if the right button is held down during boot up
  if(bLeft.isPressed()){
    debugmode = true;
    menu.addMenuItem(menusetpid);
    maxMenuItem += 1;
    peltierPID.SetTunings(300, 0, 0);
  }
  menu.select(0); // select the menu that is shown initially

  lastDebounceTime = 0;

  // initialize PID library
  peltierPID.SetMode(AUTOMATIC); // PID control is automatic
  peltierPID.SetSampleTime(200);
  peltierPID.SetOutputLimits(-255, 255);
  // refresh the PID controller a maximum of once every 200mS
  // (normally will be only calling it every 300mS (refreshRate), so this
  // setting doesn't matter much)
  
  pinMode(PeltierEn, OUTPUT);
  pinMode(PeltierDir, OUTPUT);
  pinMode(Peltier, OUTPUT);
  
  digitalWrite(PeltierEn, HIGH);
  //Serial.println("#S|CPTEST|[]#");// for debugging using computer
}

float Thermistor(int RawADC, float pad) {
  // returns temperature from a thermistor, pad is the associated resistance
  long Resistance;
  float Temp; // dual-purpose variable to save space.

  Resistance =((1024 * pad / RawADC) - pad); // resistance of thermistor
  Temp = log(Resistance/thermr); // saving the Log(resistance)
                                 // so not to calculate  it 4 times later
  Temp = 1 /((1/(298.15)) + (Temp/B));
  Temp = Temp - 273.15; // convert Kelvin to Celsius

  return Temp;
}

void loop(){
  // main execution loop

  // once no buttons are being pushed, reset the delays
  if(!bRight.isPressed() && !bLeft.isPressed() &&
      !bUp.isPressed() && !bDown.isPressed()){
    if(held){
      inputDelay = debounceDelay;
      held = false;
    }
  }
  else if((millis() - lastDebounceTime) > inputDelay){
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    if(!held){
      held = true;
      heldTime = millis();
    }

    // right button press used to power down the peltier device for
    // noise-sensitive measurements
    if(bRight.isPressed()){
      measurementLoop(15);
    }
  
    // left button press used to cycle through menus
    else if(bLeft.isPressed()){
      // special control for PID loop tuning
      if(debugmode && menu.getCurrentIndex() == 3){
        settingpid++;
        if(settingpid > 2){
          settingpid = 0;
          menu.down();
        }
      }
      // move menus left or loop back to the end of the menus
      else{
        if(menu.getCurrentIndex() == 0)
          menu.select(maxMenuItem);
        else
          menu.down();
      }
    }
  
    // up button press
    else if(bUp.isPressed()){
      // if the button has been held for long enough, increase the scroll speed
      if((millis() - heldTime) > holdDelay)
        inputDelay = holdSpeed;
  
      double pidvals[3] = {peltierPID.GetKp(),
                           peltierPID.GetKi(),
                           peltierPID.GetKd()};
      double pidincs[3] = {5, 0.25, 5};
      // depending on what the current menu is, do different things
      switch(menu.getCurrentIndex()){
        case 0:// increase the set temp
          if (setTemp < MAXTEMP)
            setTemp = setTemp + 1;
            updateVariables();
          break;
        case 3:
          pidvals[settingpid] += pidincs[settingpid];
          peltierPID.SetTunings(pidvals[0], pidvals[1], pidvals[2]);
          break;
        default: // do nothing
          break;
      }
    }
  
    else if(bDown.isPressed()){
      // if the button has been held long enough, increase the scroll speed
      if((millis() - heldTime) > holdDelay)
        inputDelay = holdSpeed;
  
      double pidvals[3] = {peltierPID.GetKp(),
                           peltierPID.GetKi(),
                           peltierPID.GetKd()};
      double pidincs[3] = {5, 0.25, 5};
      // depending on what the current menu is, do different things
      switch(menu.getCurrentIndex()){
        case 0:// decrease the set temp
          if (setTemp > 0)
            setTemp = setTemp - 1;
            updateVariables();
          break;
        case 3:
          pidvals[settingpid] -= pidincs[settingpid];
          peltierPID.SetTunings(pidvals[0], pidvals[1], pidvals[2]);
          break;
        default:// do nothing
          break;
      }
    }
    lastDebounceTime = millis();
  }

  // every cycle record sensor temperature reading to later be averaged
  chuckTemps = chuckTemps + Thermistor(analogRead(ChuckThermPIN), chuckPad);
  hSinkTemps = hSinkTemps + Thermistor(analogRead(HSinkThermPIN), hSinkPad);
  numTemps = numTemps + 1;

  
  // update the LCD and variables
  if ((millis() - lastUpdate > refreshRate)){
    updateVariables();
    //logData(setTemp * 10, avgChuckTemp * 10, avghSinkTemp * 10);
    // for logging data using computer (debugging)
    
    // update the PID controller function
    peltierPID.Compute();

    // check for overheat
    overheating = abs(avgChuckTemp - avghSinkTemp) >= MAXTEMP;
    if(overheating)
      coolDownLoop();
    else{
      if(peltierDuty > 0){
        digitalWrite(PeltierDir, LOW);
        analogWrite(Peltier, peltierDuty);
      }
      else{
        digitalWrite(PeltierDir, HIGH);
        analogWrite(Peltier, peltierDuty + 255);
      }
    }
  }
}

void coolDownLoop(){
  // puts program into cooldown loop to avoid peltier cooler overheating
  analogWrite(Peltier, 0);// turn peltier cooler off
  digitalWrite(PeltierEn, LOW);
  // post a warning to the LCD
  lcd.clear();
  //         012345678901234567890123
  lcd.print("**PELTIER OVERHEATING!**");

  while(abs(avgChuckTemp - avghSinkTemp) > (MAXTEMP - 10)){
    // wait for temperature difference to fall by 10deg C
    chuckTemps = chuckTemps + Thermistor(analogRead(ChuckThermPIN), chuckPad);
    hSinkTemps = hSinkTemps + Thermistor(analogRead(HSinkThermPIN), hSinkPad);
    numTemps = numTemps + 1;
    if ((millis() - lastUpdate > refreshRate)){ // update variables
      updateVariables();
    }
  }
  overheating = false;
  digitalWrite(PeltierEn, HIGH);
  // reset the menu screen
  menu.select(menu.getCurrentIndex());
}

void measurementLoop(int secs){
  // Maximum measurement time: 99 seconds
  // Mostly for ease of implementation, but temperature will drift alot in that time already
  if(secs > 99)
    secs = 99;
  if(secs < 0)
    return;
  digitalWrite(PeltierEn, LOW);
  lcd.clear();
  //         012345678901234567890123
  lcd.print("      Peltier  off      ");
  lcd.setCursor(0, 1);
  //         012345678901234567890123
  lcd.print("Temp: XX.X*C  On in: XXs");

  for(; secs > 0; secs--){
    float chuckTemp = Thermistor(analogRead(ChuckThermPIN), chuckPad);
    lcd.setCursor(6, 1);
    lcd.print(chuckTemp, 1);
    lcd.print((char)223);
    lcd.print("C ");
    
    lcd.setCursor(21, 1);
    if(secs < 10)
      lcd.print(" ");
    lcd.print(secs, 10);
    delay(1000);
  }
  digitalWrite(PeltierEn, HIGH);
  menu.select(menu.getCurrentIndex());
}

void updateVariables(){
  // updates all temperature values + peltier duty cycle and outputs to the LCD
  // take average of all the previous temperature readings
  if (numTemps > 0){
    avgChuckTemp = chuckTemps/numTemps;
    avghSinkTemp = hSinkTemps/numTemps;
  }
  chuckTemps = 0;
  hSinkTemps = 0;
  numTemps = 0;

  if(overheating){
    lcd.setCursor(3, 1);
    lcd.print(avgChuckTemp,1);
    lcd.print((char)223);
    lcd.print("C  ");

    lcd.setCursor(16, 1);
    lcd.print(avghSinkTemp,1);
    lcd.print((char)223);
    lcd.print("C  ");

    return;
  }

  char pidchars[3] = {'p', 'i', 'd'};
  double pidvals[3] = {peltierPID.GetKp(),
                       peltierPID.GetKi(),
                       peltierPID.GetKd()};
  // update the screen based on what screen the user is currently viewing
  switch(menu.getCurrentIndex()){
    case 1: // heatsink temp screen
      lcd.setCursor(9, 1);
      lcd.print(avghSinkTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");
      break;
    case 2: // peltier cooler duty screen
      lcd.setCursor(9, 1);
      lcd.print(peltierDuty * (100.0/255.0));
      lcd.print((char)37);
      lcd.print("  ");
      break;
    case 3:
      lcd.setCursor(0, 1);
      int i;
      for(i = 0; i < 3; i++){
        // print out each loop control variable's value
        lcd.print(settingpid == i ? '*' : ' '); // * means selected
        lcd.print(pidchars[i]);
        lcd.print(pidvals[i]);
      }
      break;
    default:
    case 0: // set temp screen
      lcd.setCursor(3, 1);
      lcd.print(avgChuckTemp,1);
      lcd.print((char)223);
      lcd.print("C  ");

      lcd.setCursor(16, 1);
      lcd.print(setTemp,0);
      lcd.print((char)223);
      lcd.print("C  ");
      break;
  }

  lastUpdate = millis();
}

// when user changes to a new menu, this function is called
void menuChanged(ItemChangeEvent event){
  // change the screen and update the new values to the new screen
  lcd.clear();
  if( event == &menuSetTemp ){
    //         012345678901234567890123
    lcd.print("  Cur Temp    Set Temp  ");
  }else if( event == &menuHeatSink){
    //         012345678901234567890123
    lcd.print("     Heat Sink Temp     ");
  }else if( event == &menuPeltierDuty ){
    //         012345678901234567890123
    lcd.print("  Peltier Cooler Duty   ");
  }
  updateVariables();
}

void menuUsed(ItemUseEvent event){
  if( event == &menuSetTemp ){
  }else if( event == &menuHeatSink ){
  }else if( event == &menuPeltierDuty ){
  //Serial.println("\tmenuPeltierDuty used"); //user feedback
  }
}

void logData(double value1, double value2, double value3){
  // used in debugging when graphing data in excel
  char buffer[5];
  Serial.print("#S|LOGTEST|[");
  Serial.print(itoa((value1), buffer, 10));
  Serial.print(";");
  Serial.print(itoa((value2), buffer, 10));
  Serial.print(";");
  Serial.print(itoa((value3), buffer, 10));
  Serial.println("]#");
}
