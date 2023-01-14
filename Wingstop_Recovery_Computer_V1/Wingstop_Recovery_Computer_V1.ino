
/***************************************************************************
  This program is for Bone-In: Honey BBQ flight computer, Created by Zachary Ervin


................................................................................................................................................
READ ME
................................................................................................................................................

  SETUP:
  
  Set Current Location and Time Sea Level Pressure:
    Get current sea level pressure from: https://weather.us/observations/pressure-qnh.html
    Save under variables, Defined Constant "SEALEVELPRESSURE_HPA" 
  
  Sea Level vs ground reference setup:
    under barometer setup code, uncomment section 1 and comment section 2 for ground reference data. Uncomment section 2 and comment section 1 for ground reference data. 

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

#include <Wire.h>
#include "Adafruit_BMP3XX.h"
//#include <Adafruit_MPU6050.h>

#include <SD.h>



//VARIABLES

  int c = 0; //COUNTER
  float x1 = 0, x2 = 0, landing = 0, apo = 0, apo_act = 0, init_pressure = 0, init_altitude = 0, delta_t = 0; //DATA VARIABLES (decimal)
  unsigned long t1 = 0, t2 = 0, t_droge = 0, t_main = 0; //TIMER VARIABLES (integers)

  
  //DEFINE PIN NUMBERS
  #define BUZZER_PIN  15
  #define LED_PIN  5

  //FIRE PINS
  #define FIRE_PIN_1  2
  #define FIRE_PIN_2  3


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
File myFile;




//////SETUP//////////SETUP//////////////SETUP///////////////SETUP/////////////SETUP/////////////SETUP//////////SETUP//////////
// Setup Function (runs once)

void setup() {
  //Begin serial comunication
  Serial.begin(9600);
  while (!Serial);

  //Fire pin setup
  pinMode(FIRE_PIN_1, OUTPUT);
  digitalWrite(FIRE_PIN_1, LOW);
  pinMode(FIRE_PIN_2, OUTPUT);
  digitalWrite(FIRE_PIN_2, LOW);

  //Light setup
  pinMode(LED_PIN, OUTPUT);

  //BUZZER setup:
  pinMode(BUZZER_PIN, OUTPUT);
  //beep_buzz(1);//beep and buzz 
  

  
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



for(int i = 1; i<10; i++){ //calibrates initial pressure and starting altitude to zero
  read_barometer();//updates barometer data
  delay(50);

  //calibrate readings for altitude measurements. CHOOSE SEALEVEL OR GROUND REFERENCE ALTITUDE READINGS. One section must be commented.

  ///* SECTION 1
  // COMMENT this section to use all sea level data, not ground reference data. Must uncomment second section. 
  init_pressure = bmp.pressure/100;
  init_altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  x1 = read_altitude();
  //*///END SECTION 1 
}
  /* SECTION 2
  // UNCOMMENT this section to use all sea level data. Target altitude will be updated using initial altitude reading and target above ground. Must comment first section. 
  init_pressure = SEALEVELPRESSURE_HPA;
  init_altitude = read_altitude();
  x1 = init_altitude;
  target_altitude += init_altitude;
  *///END SECTION 2 

  


  //print initial sea level altitude to file
  myFile.print(init_altitude);

  

  //sets initial time, t1
  t1 = millis();


  
  beep_buzz(3);//beep and buzz 


  

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

  t_droge = t2; //saves droge fire time
  myFile.print(F("FIRE DROGUE")); //logs event
  digitalWrite(FIRE_PIN_1, HIGH); //turns on droge releay (IGN 1)
  
  while(t2 < t_droge + 1000){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  digitalWrite(FIRE_PIN_1, LOW); //turns off droge releay (IGN 1)





// ***********************DESCENDING MODE 1************************************

while(!detect_main()){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  reopen_file();//saves and reopens file


  

// ***********************FIRE 2 (MAIN CHUTE)************************************

  t_main = t2; //saves droge fire time
  myFile.print(F("FIRE MAIN")); //logs event
  digitalWrite(FIRE_PIN_2, HIGH); //turns on droge releay (IGN 1)
  
  while(t2 < t_main + 1000){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  digitalWrite(FIRE_PIN_2, LOW); //turns off droge releay (IGN 1)






// ***********************DESCENDING MODE 2************************************

while(!detect_landing()){// logs data for one second
  
  //update altitude at frequency (hz)
  iterate_altitude();
  
  log_data();//logs data to sd card

}

  myFile.print(F("LANDED")); //logs event
  
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
  while (! bmp.performReading()) {
    //performs until it gets a valid reading
  }
  return bmp.readAltitude(init_pressure); // returns current altitude reading
}

void iterate_altitude(){ //reads altitude at frequency (hz)
  do{
  t2 = millis();
  }while(t2 - t1 < 1000/hz);//runs until reaches frequency

  landing = x1; //updates landing variable for detection
  x1 = read_altitude(); //updates altitude
  t1 = t2;  //updates new time
}


//***********FILE FUNCTIONS***********

void open_file(){// opens the file for writing
  myFile = SD.open("flight.txt", FILE_WRITE);

  
  if (myFile) {
    //file opened ok
  } else {
    // if the file didn't open, turn on buzzer/LED to indicate file problem
    turn_on_led(); //Turn on LED
    turn_on_buzzer(); //Turn on buzzer
    while(1){ 
      //run while loop forever
    }
  }
}

void set_header_file(){//sets headers at begining of file
  myFile.print(F("Flight Log:\t"));
  myFile.print(hz);
  myFile.println(F("hz"));
  myFile.print(F("Time:\tAlt:\tApo:\tEvents:\t"));
}

void close_file(){
  myFile.close(); //closes file
}

void reopen_file(){//closes then reopens the file, saving data up to this point
  close_file();//closes the file
  open_file();//opens the file
}

void log_data(){//saves current data to sd card
  myFile.print("\n");
  myFile.print(t2); myFile.print(F("\t"));       //logs current time
  myFile.print(x1); myFile.print(F("\t"));       //logs current position
  
}




//***********PHYSICS FUNCTIONS***********



int detect_take_off(){//returns 1 if take off is detected, otherwise 0
  if(x1 >= TAKEOFF_ALTITUDE){
    return 1;
  }
  else{
    return 0;
  }
}

int detect_apogee(){//looks for apogee and returns 1 if detected, 0 if not.
  if(x1 > apo_act){//enters if current position is higher than saved apogee value
    apo_act = x1; //saves new apogee value
    c = 0;      //resets apogee counter
  }
  else{
    c++;        //increments apogee counter if latest value is less than recorded apogee
  }
  if(c > 3){    //enters if last 4 positions are lower than recorded apogee
    c = 0;      //resets counter for landing
    return 1;   //returns 1 for apogee detected
  }
  else{           
    return 0;   //returns 0 for apogee not reached
  }
}


int detect_main(){//returns 0 while above MAIN_CHUTE_ALTITUDE altitude, 0 other wise
  if(x1 <= MAIN_CHUTE_ALTITUDE){
    return 1;
  }
  else{
    return 0;
  }
}

int detect_landing(){//returns 1 if touchdown is detected, otherwise returns 0.
  if (x1 - landing < 1 && x1 - landing > -1){ //enters if velocity is almost 0
    c++;    //increments counter
  } else {
    c = 0;  //resets counter
  }
  if(c > 5){  //enters if counted 5 velocities in a row close to 0
    c = 0;    //resets counter
    return 1; //returns 1 if touchdown detected
  } else {
    return 0;  //returns 0 if touchdown not detected
  }
}



//***********FIRE PIN FUNCTIONS***********

void fire_1(){  //Fires ignition 1 for 1 second
  digitalWrite(FIRE_PIN_1, HIGH); 
  delay(2000);
  digitalWrite(FIRE_PIN_1, LOW);
  //digitalWrite(FIRE_PIN_1, LOW);
}

void fire_2(){  //Fires ignition 1 for 1 second
  digitalWrite(FIRE_PIN_2, HIGH);
  delay(2000);
  digitalWrite(FIRE_PIN_2, LOW);
  //digitalWrite(FIRE_PIN_2, LOW);
}
