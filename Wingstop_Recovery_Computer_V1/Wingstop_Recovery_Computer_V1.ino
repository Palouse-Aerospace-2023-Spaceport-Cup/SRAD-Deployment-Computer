
/***************************************************************************
  This program is for Bone-In: Honey BBQ flight computer, Created by Zachary Ervin


................................................................................................................................................
READ ME
................................................................................................................................................

  SETUP:
  
  Set Current Location and Time Sea Level Pressure:
    Get current sea level pressure from: https://weather.us/observations/pressure-qnh.html
    Save under variables, Defined Constant "SEALEVELPRESSURE_HPA"  

  Takeoff altitude setup:
    under variables, adjust take off altitude in meters. Reaching this altitude initiates the flight program.
    
  Main chute altitude deployment setup:
    under variables, adjust main chute deployment altitude in meters. Falling below this altitude deploys the main chute.



  MODE INDICATORS:

  
  ARMED AND READY TO LAUNCH
  -BEEPS 3 TIMES

  RECOVERY MODE
  -Slow long Beeps
  

  
  TROUBLESHOOTING:
  -SD Card error:       LED blinks and Buzzer beeps fast 
  -FILE error:          LED and Buzzer stay on
  -Accelerometer error: LED stays on, NO BUZZER
  -Barometer error:     Buzzer stays on, NO LED

................................................................................................................................................

  
 ***************************************************************************/


 
//LIBRARIES

#include <Wire.h> //version 1.0
#include "Adafruit_BMP3XX.h" //version 2.1.0
#include <SD.h> //version 1.2.4



//VARIABLES

  int counter_apogee = 0; //counter for apogee detection buffer
  int counter_landed = 0; //counter for landing detection buffer

  float x_current = 0; //current vertical position 
  float x_previous = 0; //previous vertical position
  float apogee = 0; //apogee altitude
  float init_pressure = 0; //initial pressure value
  float init_altitude = 0; //initial altitude value
  float delta_t = 0; //time between iterations
  
  unsigned long t_previous = 0; //previous clock time 
  unsigned long t_current = 0; //current clock time
  unsigned long t_drogue = 0; //time the drogue deploys
  unsigned long t_main = 0; //time the main deploys
  
  //DEFINE PIN NUMBERS
  #define BUZZER_PIN  15
  #define LED_PIN  5

  //FIRE PINS
  #define DROGUE_FIRE_PIN  2
  #define MAIN_FIRE_PIN  3


// *********SET SEA LEVEL PRESSURE HERE***************
  #define SEALEVELPRESSURE_HPA (1016.00)//set according to location and date
  
// *********SET FREQUENCY HERE IN HZ***************
  #define hz (10)//hz

// *********SET TAKEOFF ALTITUDE HERE IN METERS***************
  #define TAKEOFF_ALTITUDE 30

// *********SET MAIN CHUTE ALTITUDE HERE IN METERS***************
  #define MAIN_CHUTE_ALTITUDE 150
  



// barometer attachment
Adafruit_BMP3XX bmp;

// File for logging
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


  //FILE Setup***************************************************

  open_file(); //File for writing
  set_header_file(); //sets headers at beginning of file


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
  init_pressure = bmp.pressure/100;
  init_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  x_current = read_altitude();
}
  
  //print initial sea level altitude to file
  logFile.print(init_altitude);


  //sets initial time, t_previous
  t_previous = millis();

  
  beep_buzz(3);//beep and buzz 3 times to signal setup sequence is over

  //COMPUTER IS NOW ARMED
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



// ***********************ASCENDING MODE************************************

while(!detect_apogee()){
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  reopen_file();//saves and reopens file

// ***********************FIRE 1 (DROGUE CHUTE)************************************

  t_drogue = t_current; //saves drogue fire time
  logFile.print(F("FIRE DROGUE")); //logs event
  digitalWrite(DROGUE_FIRE_PIN, HIGH); //turns on drogue relay (IGN 1)
  
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

  reopen_file();//saves and reopens file


  

// ***********************FIRE 2 (MAIN CHUTE)************************************

  t_main = t_current; //saves main chute fire time
  logFile.print(F("FIRE MAIN")); //logs event
  digitalWrite(MAIN_FIRE_PIN, HIGH); //turns on drogue relay (IGN 1)
  
  while(t_current < t_main + 1000){// logs data for one second while pin remains high
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  digitalWrite(MAIN_FIRE_PIN, LOW); //turns off drogue relay (IGN 1)






// ***********************DESCENDING MODE 2************************************

while(!detect_landing()){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  logFile.print(F("LANDED")); //logs event
  
  close_file();





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
  read_barometer();
  return bmp.readAltitude(init_pressure); // returns current altitude reading
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
  
  if (logFile) {
    //file opened ok
  } else {
    // if the file didn't open, turn on buzzer/LED to indicate file problem
    turn_on_led(); //Turn on LED
    turn_on_buzzer(); //Turn on buzzer
    while(1){ //run while loop forever
    }
  }
}

void set_header_file(){//sets headers at begining of file
  logFile.print(F("Flight Log:\t"));
  logFile.print(hz);
  logFile.println(F("hz"));
  logFile.print(F("Time:\tAlt:\tEvents:\tInital Altitude:\t"));
}

void close_file(){
  logFile.close(); //closes file
}

void reopen_file(){//closes then reopens the file, saving data up to this point
  close_file();//closes the file
  open_file();//opens the file
}

void log_data(){//saves current data to sd card
  logFile.print("\n");
  logFile.print(t_current); logFile.print(F("\t"));       //logs current time
  logFile.print(x_current); logFile.print(F("\t"));       //logs current position
  
}




//***********PHYSICS FUNCTIONS***********



int detect_take_off(){//returns 1 if take off is detected, otherwise 0
  if (x_current >= TAKEOFF_ALTITUDE){
    return 1;
  } else {
    return 0;
  }
}

bool detect_apogee(){//looks for apogee and returns 1 if detected, 0 if not.
  if (x_current > apogee){//enters if current position is higher than saved apogee value
    apogee = x_current; //saves new apogee value
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
