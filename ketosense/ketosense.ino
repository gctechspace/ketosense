// Ketose detector by members of the Gold Coast Techspace (http://gctechspace.org)
// Based on Ketos detector by Jens Clarholm (jenslabs.com)

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <math.h>
//////////////////////////////


// Define inputs/output pins ///
#define DHTPIN 10
#define DHTTYPE           DHT22  
DHT_Unified dht(DHTPIN, DHTTYPE);

//LCD
LiquidCrystal_I2C lcd(39, 20, 4);

//TGS882 sensor
int gasValuePin = 0;

//Buttons
int resetMaxSwitchPin = 13;
int resetSensorSwitchPin = 12;
int toggleModeSwitchPin = 11;
//////////////////////////////

//Variable definitions /////
int GlobalMaxValue;
int GlobalMinValue;
int temperatureScaledValue;

//senstor stability check
int newCalibrationVal;
int calibrationHigh;
int calibrationLow;
float warmupHumidity;
float warmupTemperature;
///////////////////////////

int currentMode=1;



//Initial values for variables
//Starting state for buttons
boolean resetMaxSwitchLastButton = LOW;
boolean resetMaxSwitchCurrentButton = LOW;
boolean resetSensorSwitchLastButton = LOW;
boolean resetSensorSwitchCurrentButton = LOW;
boolean toggleModeLastButton = LOW;
boolean toggleModeCurrentButton = LOW;

// Read gas variables
int tempRead = 0;

// General Var
float R0 = 4500;
int lastPPM = 0;

double currentHumidity;
double currentTemperature;
double scalingFactor;
//////////////////////////////


void setup() {

  //Initiate LCD
  lcd.begin();

  //Define pinmode
  pinMode(gasValuePin,INPUT);
  pinMode(resetMaxSwitchPin, INPUT);
  pinMode(resetSensorSwitchPin, INPUT);
  pinMode(toggleModeSwitchPin, INPUT);

  //Write welcome message
  printToRow1("Hi I'm Ketosense");
  printToRow2("Time to warm up.");
  delay(2000);

  //Initiate DHT11 temperature and humidity sensor and verify it.
  clearLcd();
  delay(2000);

  clearLcd();
  printToRow1("Warmup finished");
  delay(1000);
  clearLcd();
  printToRow1("Blow into mouth-");
  printToRow2("piece to start."); 
}

void loop() {

  //read three times from gas sensor with 5ms between each reading
  readsensor();
  //Check if all readings are the same which indictae a stabile behaviour and if the value is higher or lower update global max and min variables.
  updateNewMaxOrMinWithTempHumidity(tempRead);
  //print result to display if current value is different to previous value and update the highest value.
  if (acetoneResistanceToPPMf(toResistance(temperatureScaledValue)) != lastPPM){
    lastPPM = acetoneResistanceToPPMf(toResistance(temperatureScaledValue));
    updateScreen();
  }

  //read buttons

  //read resetMaxMinSwitch
  resetMaxSwitchCurrentButton = debounce(resetMaxSwitchLastButton, resetMaxSwitchPin);
  if (resetMaxSwitchLastButton == LOW && resetMaxSwitchCurrentButton == HIGH){
    /*    GlobalMaxValue=0;
     GlobalMinValue=0;
     updateScreen();
     */
    clearLcd();
    printToRow1("Result displayed");
    printToRow2("as mmol/l."); 
  }
  resetMaxSwitchLastButton = resetMaxSwitchCurrentButton;


  //read resetSensorSwitch
  resetSensorSwitchCurrentButton = debounce(resetSensorSwitchLastButton, resetSensorSwitchPin);
  if (resetSensorSwitchLastButton == LOW && resetSensorSwitchCurrentButton == HIGH){ 
    clearLcd();
    printToRow1("Reset finished");
    delay(1000);
    clearLcd();
    GlobalMaxValue=0;
    GlobalMinValue=0;
    printToRow1("Blow into mouth-");
    printToRow2("piece to start."); 
  }
  resetSensorSwitchLastButton = resetSensorSwitchCurrentButton;


  toggleModeCurrentButton = debounce(toggleModeLastButton, toggleModeSwitchPin);
  if (toggleModeLastButton == LOW && toggleModeCurrentButton == HIGH){
    if (currentMode == 1){
      currentMode=2;
      clearLcd();
      printToRow1("Result displayed");
      printToRow2("as mmol/l."); 
      delay(1000);
      updateScreen();
    }
    else if (currentMode == 2){
      currentMode=1;
      clearLcd();
      printToRow1("Result displayed");
      printToRow2("as PPM.");
      delay(1000);
      updateScreen();
    }
  }
  toggleModeLastButton = toggleModeCurrentButton;


}
//////////////////////////
/// Methods start here ///
//////////////////////////

//Reads sensor 3 times with 5ms delay between reads and store read values in tempRead1, 2 and 3
float ppmToMmol(int PPM){
  float ppmInmmol = (((float) PPM / 1000) / 58.08);
  ppmInmmol = ppmInmmol * 1000;
  return ppmInmmol;
}

void readsensor(){
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println(" *C");
    tempRead = event.temperature;
  }
}

//Update screen with result
void updateScreen(){
  clearLcd();
  ///// DEBUG start
  printToRow1("H:");
  printIntToCurrentCursorPossition((int)currentHumidity);
  printStringToCurrentCursorPossition(" T:");
  printIntToCurrentCursorPossition((int)currentTemperature);
  // printStringToCurrentCursorPossition(" S:");
  // printFloatToCurrentCursorPossition((float)scalingFactor);
  printStringToCurrentCursorPossition(" R:");
  printIntToCurrentCursorPossition((int)GlobalMaxValue);
  ///// Debug end

  printToRow2("Now:    Max:    ");
  if (currentMode == 2){
    //Result in mmol/l
    lcd.setCursor(4,1);
    printFloatToCurrentCursorPossition(ppmToMmol(acetoneResistanceToPPMf(toResistance(temperatureScaledValue))));
    lcd.setCursor(12,1);
    printFloatToCurrentCursorPossition(ppmToMmol(acetoneResistanceToPPMf(toResistance(GlobalMaxValue))));

  }
  else if (currentMode == 1){
    //result in PPM
    lcd.setCursor(4,1);
    printIntToCurrentCursorPossition(acetoneResistanceToPPMf(toResistance(temperatureScaledValue)));
    lcd.setCursor(12,1);
    printIntToCurrentCursorPossition(acetoneResistanceToPPMf(toResistance(GlobalMaxValue)));
  }
}

//calculate the gas concentration relative to the resistance
int acetoneResistanceToPPMf(float resistance){
  double tempResistance = (double)resistance;
  double PPM; 
  if (tempResistance > 50000){
    double PPM = 0;
  }
  else {
    double logPPM = (log10(tempResistance/R0)*-2.6)+2.7;
    PPM = pow(10, logPPM);
  }
  return (int)PPM;
}



//debounce function
boolean debounce(boolean last, int pin )
{
  boolean current = digitalRead(pin);
  if (last != current)
  {
    delay(5);
    current = digitalRead(pin);
  }
  return current;
}

//check if we have new max or min after temperature and humidity scaling has been done. 
void updateNewMaxOrMinWithTempHumidity(int value){
  temperatureScaledValue = value;

  if (GlobalMaxValue==0){
    GlobalMaxValue = temperatureScaledValue;
  }
  if (GlobalMinValue==0){
    GlobalMinValue = temperatureScaledValue;
  }
  if (temperatureScaledValue < GlobalMinValue){
    GlobalMinValue = temperatureScaledValue;
  }
  if (temperatureScaledValue > GlobalMaxValue){
    GlobalMaxValue = temperatureScaledValue;
  }
}
//check if we have new max or min without temperature and humidity scaling. 
void updateNewMaxOrMin(int value){
  if (GlobalMaxValue==0){
    GlobalMaxValue = value;
  }
  if (GlobalMinValue==0){
    GlobalMinValue = value;
  }
  if (value < GlobalMinValue){
    GlobalMinValue = value;
  }
  if (value > GlobalMaxValue){
    GlobalMaxValue = value;
  }
}

//Convert the 1-1023 voltage value from gas sensor analog read to a resistance, 9800 is the value of the other resistor in the voltage divide.
float toResistance(int reading){
  float resistance = ((5/toVoltage(reading) - 1) * 9800);
  return resistance;
}



//Convert the 1-1023 value from analog read to a voltage.
float toVoltage(int reading){
  //constant derived from 5/1023 = 0.0048875
  float voltageConversionConstant = 0.0048875;
  float voltageRead = reading * voltageConversionConstant;
  return voltageRead;
}

//Clear all text on both lines of LCD
void clearLcd(){
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("                ");
}

//Clears one row of LCD and prints the text sent to the method starting on possition 0 on row 1
void printToRow1(String text){
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,0);
  lcd.print(text);
}

//Clears one row of LCD and prints the text sent to the method starting on possition 0 on row 2
void printToRow2(String text)
{
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print(text);
}

//Prints what ever string is sent to it to current cursor possition
void printStringToCurrentCursorPossition(String text)
{
  lcd.print(text);
}

//Prints what ever intiger is sent to it to current cursor possition
void printIntToCurrentCursorPossition(int text)
{
  lcd.print(text);
}

//Prints what ever float is sent to it to current cursor possition
void printFloatToCurrentCursorPossition(float text)
{
  lcd.print(text);
}








