
/***************************************************************************
  
................................................................................................................................................
READ ME
................................................................................................................................................

  INFO:
  Author: Zachary Ervin
  Date: 3/20/22
  Hardware: 'Boneless: Hot Buffalo' flight computer, created by Zachary Ervin

  DESCRIPTION:
  This program is a recovery flight computer for high powered rockets. It is designed to deploy two black-powder charges, 
  one for a drogue parachute and another for the main recovery parachute. The first charge, the drogue charge, will fire
  at apogee or a specified delay after apogee. The second charge, the main charge, will fire at the configured altitude 
  above the ground no less than one second after the drogue charge has fired. The flight computer should be powered on
  before the circuit is completed to the E-matches used to ignite the powder charges. Once the computer has reached the 
  armed state the circuits can be completed. Once armed, the computer will wait until the rocket has risen above the 
  ground a certain configured distance to advance the flight program. 


  SETUP:

  adjustable parameters:
  
  SEALEVELPRESSURE_HPA  - current location and time sea level pressure in HPA. 
                          Get current sea level pressure from: https://weather.us/observations/pressure-qnh.html
  
  hz - frequncy that the altitude reading refreshes. 10 is a good default. 
  
  TAKEOFF_ALTITUDE - altitude above the ground in meters that initiates the flight sequence. 30 is a good default. 
  
  MAIN_CHUTE_ALTITUDE - altitude above the ground in meters that the main chute will deploy after apogee is reached. 
  
  DROGUE_DELAY -  drogue delay time after apogee in milliseconds. Use 0 if this is the main flight recovery computer. 
                  Use a delay if this is the backup flight computer. 

  MACH_DELAY -  delay time in milliseconds that the charges won't fire after launch is detected. Set this time to at least 
                the expected burn time of the motor if your are planning on going mach 0.8 or greater. Otherwise set to 0 
                for no delay (for subsonic flight).


  Configure the adjustable parameters through a configuration file on the attached SD card wiith the file name "config.txt". 
  This file must present to operate the flight computer.

  The configure file should contain a line for each parameter, organized as "<parameterName>=<value>".

  example config.txt content:
  "
  SEALEVELPRESSURE_HPA=1016.0
  hz=10
  TAKEOFF_ALTITUDE=30
  MAIN_CHUTE_ALTITUDE=150
  DROGUE_DELAY=0
  MACH_DELAY=0
  "
  

  MODE INDICATORS:

  
  ARMED AND READY TO LAUNCH
  -Beeps 3 times

  RECOVERY MODE
  -Slow long beeps
  

  
  TROUBLESHOOTING:
  -SD Card error:       LED blinks and Buzzer beeps fast 
  -FILE error:          LED and Buzzer stay on
  -Barometer error:     Buzzer stays on, NO LED
  -Moving on Startup:   Bilnks and Buzzer beeps once

................................................................................................................................................

  
 ***************************************************************************/


//LIBRARIES

#include <Wire.h> //version 1.0
#include "Adafruit_BMP3XX.h" //version 2.1.0
#include <SD.h> //version 1.2.4



//VARIABLES

  int counter_apogee = 0; //counter for apogee detection buffer
  int counter_landed = 0; //counter for landing detection buffer

  float x_current = 0; //current vertical position above ground in meters
  float x_previous = 0; //previous vertical position above ground in meters
  float init_pressure = 0; //initial pressure value
  float init_altitude = 0; //initial altitude value in meters
  
  unsigned long t_previous = 0; //previous clock time in milliseconds
  unsigned long t_current = 0; //current clock time in milliseconds
  unsigned long t_apogee = 0; //time apogee is detected
  unsigned long t_drogue = 0; //time the drogue deploys
  unsigned long t_main = 0; //time the main deploys

  //variables to be defined from config.txt file from SD card:
  float SEALEVELPRESSURE_HPA = 1016.00; //sea level pressure reading in HPA
  int hz = 10; //frequncy that the altitude reading refreshes
  int TAKEOFF_ALTITUDE = 30; //altitude above the ground in meters that triggers the detect_take_off function
  int MAIN_CHUTE_ALTITUDE = 150; //altitude above the ground in meters that the main chute will deploy
  int DROGUE_DELAY = 0; //drogue delay time after apogee in milliseconds
  int MACH_DELAY = 0; //delay time in milliseconds that the charges won't fire after launch is detected.
  
  //DEFINE PIN NUMBERS
  #define BUZZER_PIN  15
  #define LED_PIN  5

  //FIRE PINS
  #define DROGUE_FIRE_PIN  2
  #define MAIN_FIRE_PIN  3

  // barometer attachment
  Adafruit_BMP3XX bmp;

  File logFile;




//////SETUP//////////SETUP//////////////SETUP///////////////SETUP/////////////SETUP/////////////SETUP//////////SETUP//////////
// Setup Function (runs once)

void setup() {
  //Begin serial comunication
  Serial.begin(9600);
  while (!Serial);

  //Fire pin setup
  pinMode(DROGUE_FIRE_PIN, OUTPUT);
  digitalWrite(DROGUE_FIRE_PIN, LOW);
  pinMode(MAIN_FIRE_PIN, OUTPUT);
  digitalWrite(MAIN_FIRE_PIN, LOW);

  //Light setup
  pinMode(LED_PIN, OUTPUT);

  //BUZZER setup:
  pinMode(BUZZER_PIN, OUTPUT);
  

  
  //SD Card Setup***********************************************

  if (!SD.begin(10)) {
    while(!SD.begin()){
      //blink/buzz fast to indicate error
      turn_on_led(); //Turn on LED
      turn_on_buzzer(); //Turn on buzzer
      delay(200);
      turn_off_led(); //Turn off LED
      turn_off_buzzer(); //Turn off buzzer
      delay(100);
    }
  }


  //Read config.txt File***************************************************

  SEALEVELPRESSURE_HPA = SD_findFloat(F("SEALEVELPRESSURE_HPA")); //sea level pressure reading in HPA
  hz = SD_findInt(F("hz")); //frequncy that the altitude reading refreshes
  TAKEOFF_ALTITUDE = SD_findInt(F("TAKEOFF_ALTITUDE")); //altitude above the ground in meters that triggers the detect_take_off function
  MAIN_CHUTE_ALTITUDE = SD_findInt(F("MAIN_CHUTE_ALTITUDE")); //altitude above the ground in meters that the main chute will deploy
  DROGUE_DELAY = SD_findInt(F("DROGUE_DELAY")); //drogue delay time after apogee in milliseconds
  MACH_DELAY = SD_findInt(F("MACH_DELAY")); //delay time in milliseconds that the charges won't fire after launch is detected.


  //Logging File Setup***************************************************

  open_file(); //File for writing
  if (logFile) {
    } else {
    // if the file didn't open, turn on buzzer/LED to indicate file problem
    turn_on_led(); //Turn on LED
    turn_on_buzzer(); //Turn on buzzer
    while(1){ //run while loop forever
    }
  }


// Barometer setup********************************************************

  while (!bmp.begin_I2C()) {   
    //turn on buzzer if not connecting to barometer
    turn_on_buzzer();
  }
  turn_off_buzzer();

  // Set up oversampling and filter initialization settings
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_1);//no filtering
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);



for(int i = 0; i<10; i++){ //calibrates initial pressure and starting altitude to zero
  read_barometer();//updates barometer data
  delay(50);
  init_pressure = bmp.pressure/100; //used to call read altitude for ground level reference
  init_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  x_previous = bmp.readAltitude(init_pressure);
}
  
  set_header_file(); //sets headers at beginning of file
  
  delay(1000);
  x_current = bmp.readAltitude(init_pressure);

  if (abs(x_current - x_previous) > 3 ){//enters if rocket is moving greater than 3 m/s
    if (logFile) {
      logFile.print(F("ABORTED, ROCKET MOVING")); //logs event
      reopen_file();//saves and reopens file
    }
    
    beep_buzz(1); //beeps and lights once to signal that rocket was detected moving
    while (1){//runs forever
      //update altitude at frequency (hz)
      iterate_altitude();
      log_data();//logs data to sd card
      reopen_file();//saves and reopens file
      
    }
  }


  //sets initial time, t_previous
  t_previous = millis();

  beep_buzz(3);//beep and buzz 3 times to signal setup sequence is over
  
}




/////////MAIN PROGRAM///////////////MAIN PROGRAM//////////////MAIN PROGRAM///////////////MAIN PROGRAM////////////////MAIN PROGRAM////////////////MAIN PROGRAM///////////////MAIN PROGRAM/////////
//BEGIN LOOP MAIN PROGRAM

void loop() { 

// ***********************STANDBY MODE************************************

while(!detect_take_off()){
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
}

  log_data();//logs data to sd card

  MACH_DELAY = MACH_DELAY + t_current;


// ***********************ASCENDING MODE************************************


while(MACH_DELAY > t_current){ //runs until MACH_DELAY is reached
  
  //update altitude at frequency (hz)
  iterate_altitude();
  log_data();//logs data to sd card

}

  if (logFile) {
    logFile.print(F("MACH DELAY REACHED\t")); //logs event
    reopen_file();//saves and reopens file
  }

  
while(!detect_apogee()){
  
  //update altitude at frequency (hz)
  iterate_altitude();
  log_data();//logs data to sd card
}

  t_apogee = t_current; //saves apogee time

  if (logFile) {
    logFile.print(F("APOGEE DETECTED\t")); //logs event
    reopen_file();//saves and reopens file
  }



  

// ***********************FIRE 1 (DROGUE CHUTE)************************************


  while(t_current - t_apogee < DROGUE_DELAY){//logs date during drogue delay
    //update altitude at frequency (hz)
    iterate_altitude();
    log_data();//logs data to sd card
  }
  
  t_drogue = t_current; //saves drogue fire time

  digitalWrite(DROGUE_FIRE_PIN, HIGH); //turns on drogue relay (IGN 1)
  
  if (logFile) {
  logFile.print(F("FIRE DROGUE\t")); //logs event
  reopen_file();//saves and reopens file
  }
  
  
  while(t_current < t_drogue + 1000){// logs data for one second while pin remains high
    //update altitude at frequency (hz)
    iterate_altitude();
    log_data();//logs data to sd card

  }

  digitalWrite(DROGUE_FIRE_PIN, LOW); //turns off drogue relay (IGN 1)





// ***********************DESCENDING MODE 1************************************

while(!detect_main()){// logs data for one second 
  
  //update altitude at frequency (hz)
  iterate_altitude();
  log_data();//logs data to sd card

}


  

// ***********************FIRE 2 (MAIN CHUTE)************************************

  t_main = t_current; //saves main chute fire time

  if (logFile) {
  logFile.print(F("FIRE MAIN")); //logs event
  reopen_file();//saves and reopens file
  }
  
  digitalWrite(MAIN_FIRE_PIN, HIGH); //turns on main relay (IGN 2)
  
  while(t_current < t_main + 1000){// logs data for one second while pin remains high
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  digitalWrite(MAIN_FIRE_PIN, LOW); //turns off main relay (IGN 2)






// ***********************DESCENDING MODE 2************************************

while(!detect_landing()){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  if (logFile) {
  logFile.print(F("LANDED")); //logs event
  close_file();
  }




// ***********************RECOVERY MODE************************************
  
  recovery_beeps();

}//end of main program















//******FUNCTIONS********FUNCTIONS********** FUNCTIONS **********FUNCTIONS*********FUNCTIONS**********FUNCTIONS*************

//***********LED FUNCTIONS***********

void turn_on_led(){ // Turns on LED
  digitalWrite(LED_PIN, HIGH);
}

void turn_off_led(){ // Turns off LED
  digitalWrite(LED_PIN, LOW);
}


//***********BUZZER FUNCTIONS***********

void turn_on_buzzer(){ // Turns on LED
  digitalWrite(BUZZER_PIN, HIGH);
}

void turn_off_buzzer(){ // Turns off LED
  digitalWrite(BUZZER_PIN, LOW);
}

void beep_buzz(int num){ // Blinks LED and Beeps Buzzer for num times
  for(int i = 0; i < num; i++){
    turn_on_buzzer();
    turn_on_led();
    delay(300);
    turn_off_buzzer();
    turn_off_led();
    delay(300);
  }
}

void recovery_beeps(){//Continuous Slow long beeps for recovery
  while(1){ //does not exit loop
  turn_on_buzzer();
  delay(1000); //beep delay
  turn_off_buzzer();
  delay(5000); //pause delay
  }
}


//***********BAROMETER FUNCTIONS***********

void read_barometer(){//takes barometer reading
  while (! bmp.performReading()) {//performs barometer reading
    //could not perform reading
  }
}

float read_altitude(){//reads and returns barometer altitude reading
  float altitudeReading;
  do {
    read_barometer(); 
    altitudeReading = bmp.readAltitude(init_pressure);
  } while (abs(altitudeReading - x_current)/(t_current - t_previous) > 1200); 
  //keeps reading altimeter if speed vertical speed is more than 1200 m/s (should never actually be that fast).
  return altitudeReading; // returns current altitude reading
}

void iterate_altitude(){ //reads altitude at frequency (hz)
  do {
  t_current = millis();
  }while(t_current - t_previous < 1000/hz);//runs until reaches frequency period

  x_previous = x_current; //updates previous position variable for landing detection
  x_current = read_altitude(); //updates altitude
  t_previous = t_current;  //updates new time
}


//***********FILE FUNCTIONS***********

void open_file(){// opens the file for writing
  logFile = SD.open("flight.txt", FILE_WRITE);
}

void set_header_file(){//sets headers at begining of file
  logFile.print(F("Flight Log:\t\t"));
  logFile.print(F("SEALEVELPRESSURE:\t"));
  logFile.print(SEALEVELPRESSURE_HPA); logFile.print(F(" [HPA]\t")); 
  logFile.print(F("hz:\t"));
  logFile.print(hz); logFile.print(F(" [hz]\t")); 
  logFile.print(F("Initial Altitude:\t"));
  logFile.print(init_altitude); logFile.print(F(" [m]\t")); 
  logFile.print(F("TAKEOFF_ALTITUDE:\t"));
  logFile.print(TAKEOFF_ALTITUDE); logFile.print(F(" [m]\t")); 
  logFile.print(F("MAIN_CHUTE_ALTITUDE:\t"));
  logFile.print(MAIN_CHUTE_ALTITUDE); logFile.print(F(" [m]\t")); 
  logFile.print(F("DROGUE_DELAY:\t"));
  logFile.print(DROGUE_DELAY); logFile.print(F(" [millis]\t")); 
  logFile.print(F("MACH_DELAY:\t"));
  logFile.print(MACH_DELAY); logFile.println(F(" [millis]\t")); 
  
  logFile.print(F("Time:\tAltitude:\tEvents:\t"));



  
}

void close_file(){
  logFile.close(); //closes file
}

void reopen_file(){//closes then reopens the file, saving data up to this point
  if (logFile){
  close_file();//closes the file
  }
  open_file();//opens the file
}

void log_data(){//saves current data to sd card
  if (logFile) {
  logFile.print(F("\n"));
  logFile.print(t_current); logFile.print(F("\t"));       //logs current time
  logFile.print(x_current); logFile.print(F("\t"));       //logs current position
  }
}

int SD_findInt(const __FlashStringHelper * key) {
  char value_string[30];
  int value_length = SD_findKey(key, value_string);
  return HELPER_ascii2Int(value_string, value_length);
}

float SD_findFloat(const __FlashStringHelper * key) {
  char value_string[30];
  int value_length = SD_findKey(key, value_string);
  return HELPER_ascii2Float(value_string, value_length);
}

int SD_findKey(const __FlashStringHelper * key, char * value) {
  File configFile = SD.open("config.txt");

  if (!configFile) {
    // if the file didn't open, turn on buzzer/LED to indicate file problem
    turn_on_led(); //Turn on LED
    turn_on_buzzer(); //Turn on buzzer
    while(1){ //run while loop forever
    }
  }

  char key_string[30];
  char SD_buffer[30 + 30 + 1]; // 1 is = character
  int key_length = 0;
  int value_length = 0;

  // Flash string to string
  PGM_P keyPoiter;
  keyPoiter = reinterpret_cast<PGM_P>(key);
  byte ch;
  do {
    ch = pgm_read_byte(keyPoiter++);
    if (ch != 0){
      key_string[key_length++] = ch;
    }
  } while (ch != 0);

  // check line by line
  while (configFile.available()) {
    int buffer_length = configFile.readBytesUntil('\n', SD_buffer, 100);
    if (SD_buffer[buffer_length - 1] == '\r'){
      buffer_length--; // trim the \r
    }

    if (buffer_length > (key_length + 1)) { // 1 is = character
      if (memcmp(SD_buffer, key_string, key_length) == 0) { // equal
        if (SD_buffer[key_length] == '=') {
          value_length = buffer_length - key_length - 1;
          memcpy(value, SD_buffer + key_length + 1, value_length);
          break;
        }
      }
    }
  }

  configFile.close();  // close the file
  return value_length;
}

int HELPER_ascii2Int(char *ascii, int length) {
  int sign = 1;
  int number = 0;

  for (int i = 0; i < length; i++) {
    char c = *(ascii + i);
    if (i == 0 && c == '-'){
      sign = -1;
    }
    else {
      if (c >= '0' && c <= '9'){
        number = number * 10 + (c - '0');
      }
    }
  }

  return number * sign;
}

float HELPER_ascii2Float(char *ascii, int length) {
  int sign = 1;
  int decimalPlace = 0;
  float number  = 0;
  float decimal = 0;

  for (int i = 0; i < length; i++) {
    char c = *(ascii + i);
    if (i == 0 && c == '-'){
      sign = -1;
    }
    else {
      if (c == '.'){
        decimalPlace = 1;
      }
      else if (c >= '0' && c <= '9') {
        if (!decimalPlace){
          number = number * 10 + (c - '0');
        }
        else {
          decimal += ((float)(c - '0') / pow(10.0, decimalPlace));
          decimalPlace++;
        }
      }
    }
  }

  return (number + decimal) * sign;
}




//***********PHYSICS FUNCTIONS***********



bool detect_take_off(){//returns 1 if take off is detected, otherwise 0
  if (x_current >= TAKEOFF_ALTITUDE){
    return 1;
  } else {
    return 0;
  }
}

bool detect_apogee(){//looks for apogee and returns 1 if detected, 0 if not.
  if (x_current > x_previous){//enters if current position is higher than saved apogee value
    counter_apogee = 0;      //resets apogee counter
  } else {
    counter_apogee++;        //increments apogee counter if latest value is less than recorded apogee
  }
  if (counter_apogee > 3){    //enters if last 4 positions are lower than recorded apogee
    return true;   //returns 1 for apogee detected
  } else {           
    return false;   //returns 0 for apogee not reached
  }
}


bool detect_main(){//returns 0 while above MAIN_CHUTE_ALTITUDE altitude, 0 other wise
  if (x_current <= MAIN_CHUTE_ALTITUDE){
    return true;
  } else {
    return false;
  }
}

bool detect_landing(){//returns 1 if touchdown is detected, otherwise returns 0.
  if (x_current - x_previous < 1 && x_current - x_previous > -1){ //enters if velocity is almost 0
    counter_landed++;    //increments counter
  } else {
    counter_landed = 0;  //resets counter
  }
  if (counter_landed > 5){  //enters if counted 5 velocities in a row close to 0
    return true; //returns 1 if touchdown detected
  } else {
    return false;  //returns 0 if touchdown not detected
  }
}
