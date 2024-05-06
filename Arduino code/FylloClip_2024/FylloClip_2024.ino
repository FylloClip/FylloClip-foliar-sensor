/*
 *     code written by Martin Thalheimer
 *     capacitance measurement based on: https://wordpress.codewrite.co.uk/pic/2014/01/21/cap-meter-with-arduino-uno/
 */
#include <TinyLoRa.h>  //TinyLora by Adafruit
#include <SPI.h>
#include "LowPower.h"  //Low-Power by Rocket Scream Electronics
#include <EEPROM.h>


byte node_ID = 1; //node ID


// LoRaWAN NwkSKey, network session key
uint8_t NwkSkey[16] =  { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };  //replace with your NwkSKey

// LoRaWAN AppSKey, application session key
uint8_t AppSkey[16] = { 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99 };   //replace with your AppSKey

// Device Address (MSB)
uint8_t DevAddr[4] = { 0xAA, 0xAA, 0xAA, 0xAA }; // <-- Change this address for every node!



unsigned int frame_counter=0;

int alarm_period=75; //duration of sleep period  65 -> 10 minutes


const int OUT_PIN_1 = 4;
const int IN_PIN_1 = A2;

const int OUT_PIN_2 = 3;
const int IN_PIN_2 = A3;

int SF;  //spreading factor

const float IN_STRAY_CAP_TO_GND = 30; //presumed board stray capacitance

int batt_val;
byte LC_1;
byte LC_2;


int i;
int j;
int set_value; //value of sleep period in minutes returned over serial port
unsigned int value;
int L1; //variable for light measurement
int L2; //variable for light measurement
int adr_hi;
int adr_low;

const int batt_v = 5; //AD port for battery
const int photodiode_1=0; //AD port for light sensor
const int photodiode_2=1; //AD port for light sensor
const int RAD_1=8;  //digital pin as GND for photodiode
const int RAD_2=7;  //digital pin as GND for photodiode

uint8_t mydata [30];

// Pinout
TinyLoRa lora = TinyLoRa(6, 10, 9); //Dio0, nss, rst
  

void setup() { 
  delay(500);
Serial.begin(9600);

pinMode(RAD_1, INPUT); //disconnect GND 
pinMode(RAD_2, INPUT); //disconnect GND 

  pinMode(OUT_PIN_1, OUTPUT);
  pinMode(IN_PIN_1, OUTPUT);
  pinMode(OUT_PIN_2, OUTPUT);
  pinMode(IN_PIN_2, OUTPUT);
  
  delay (1000); 
  frame_count_retrieve(); //retrieve value of rx_counter from eeprom 

get_parameters();  //retrieve spreading factor settings
sleep_period_retrieve(); //retrieve sleep period value from eeprom  

if (set_value==1) { 
  alarm_period=6;
}

if (set_value==5) { 
  alarm_period=32;
}


if (set_value==10) { 
  alarm_period=75;
}


if (set_value==15) { 
  alarm_period=99;
}

if (set_value==20) { 
  alarm_period=132;
}


if (alarm_period==198) {
  set_value=30;
}

if (set_value==30) { 
  alarm_period=198;
}

if (set_value==60) { 
  alarm_period=398;
}
   


   // Initialize LoRa
  Serial.print("Starting LoRa...");
  // define multi-channel sending
 lora.setChannel(MULTI);
  //  lora.setChannel(CH0);

   implement_SF();

  if(!lora.begin())
  {
    Serial.println(F("Failed"));
    Serial.println(F("Check your radio"));
    while(true);
  }
  Serial.println(F("ok"));
lora.setPower(20);  //max. value: 20

Serial.println(F("new start")); 
saymyname();

  menu();
 for (i = 0; i < 300; i ++)  //wait some seconds for key to be pressed
    { 
      if (Serial.available() > 0) {
        checkrx();
        break;   //exit loop if serial character was received
      } 
      delay(10);
    } 
   sleepNow();
  delay(100);
}



void loop()
{

  
measure_and_send ();


delay(1000);
  sleepNow();

  delay(100); 
  Serial.println();  //return
  Serial.println(F("waking up.."));


}


 void sleepNow()     {    // here we put the arduino to sleep

                                  
    Serial.println(F("sleep... "));
    delay(100);


for (j=0; j <= alarm_period; j++){  //set length of sleep period
            
   LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
     }
       
}


void measure_and_send () {
  
 measure_light();
 get_capacitance();
 get_voltage();

  
   pinMode(RAD_1, INPUT); //disconnect GND 
   pinMode(RAD_2, INPUT); //disconnect GND  

mydata[0]=node_ID;
mydata[1]=lowByte(L1);
mydata[2]=highByte(L1);
mydata[3]=lowByte(L2);
mydata[4]=highByte(L2); 
mydata[5]=LC_1;
mydata[6]=LC_2;
mydata[7]=lowByte(batt_val);
mydata[8]=highByte(batt_val); 


frame_counter=frame_counter+1; 
if (frame_counter>65000) {
  frame_counter=0;
}

Serial.print (F("frame-counter: "));
Serial.println (frame_counter);

frame_count_write();

Lora_send ();

}



void checkrx()
{
  if (Serial.read()==120) {
    menu();
  }
  else {
    Serial.println("no access");
  } 
 
}

void menu()
{
  Serial.println(F("******************"));
  Serial.println(F("s - status"));
  Serial.println(F("r - reset rx counter"));
  Serial.println(F("z - sendData"));
  Serial.println(F("a - interval"));  
  Serial.println(F("f - set SF"));
  Serial.println(F("******************"));
  for (i = 0; i < 500; i ++)  {   //wait some seconds for key to be pressed
    if (Serial.available() > 0) {

    value = Serial.read();

  if (value == 's') {  //a-status
    actual_read();
      break;   //exit loop if serial character was received
  }
 
      
   if (value == 'z') {  //a-alarm period
         measure_and_send ();
              break;   //exit loop if serial character was received
      }
      
  if (value == 'a') {  //set length of sleep period between 2 measurements
    set_sleep_period();
  }     

     if (value == 'r') {  //r - reset rx counter
         reset_frame_counter ();
              break;   //exit loop if serial character was received
      }
          
    if (value == 'f') {  //s-SF
        set_SF();
      }         
      
  }
  delay(10);
  }
} 

void actual_read() {  // actual values

  Serial.println(F("actual values:"));
    measure_light();
   
  Serial.println(F("PAR: "));
  Serial.println(L1);
  Serial.println(L2);

  get_capacitance();
  Serial.println(F("leaf capacitance: "));
  Serial.println(LC_1);
  Serial.println(LC_2);

  pinMode(RAD_1, INPUT); //disconnect GND 
  pinMode(RAD_2, INPUT); //disconnect GND 
  
    get_voltage();
  Serial.print(F("Batt: "));
  Serial.println(batt_val);

  Serial.print(F("alarm period: "));
  Serial.println(set_value);
  Serial.print(F("node_ID: "));
  Serial.println(node_ID);
 saymyname ();
}


void make_number(){
  int a;
  int b;
  while (Serial.available() == 0){
  }  //wait for data from serial port
  if (Serial.available()>0) {
    a = Serial.read();
  }
  Serial.print(a-48);

  while (Serial.available() == 0){
  }  //wait for data from serial port
  if (Serial.available()>0) {
    b = Serial.read();
  }
  Serial.println(b-48);
  value=((a-48)*10+b-48);
}


void saymyname() {
      Serial.println(F("FylloClip_2024.ino"));
}

void Lora_send () {

   Serial.println(F("Sending LoRa Data..."));

 lora.sendData(mydata, 9, frame_counter);

}


void emptyBuffer() {
  while (Serial.available()) {
    byte temp = Serial.read();
    Serial.println(temp);
  }
}

void measure_light() {
//photodiode_1
  pinMode(RAD_1, OUTPUT); //connect GND 
  digitalWrite(RAD_1, LOW); //connect GND 
 L1 = 0;
  for (j=0; j <= 9; j++){  //take 10 readings and calculate mean value
      L1=L1 + analogRead(photodiode_1);
      delay(10);       
 }   

  L1=L1/10; //mean value 

//photodiode_2
  pinMode(RAD_2, OUTPUT); //connect GND 
  digitalWrite(RAD_2, LOW); //connect GND  
 L2 = 0;
  for (j=0; j <= 9; j++){  //take 10 readings and calculate mean value
      L2=L2 + analogRead(photodiode_2);
      delay(10);       
 }   
  L2=L2/10; //mean value   
  
 }

 void get_capacitance()
{

 int val = analogRead(IN_PIN_1);


   pinMode(IN_PIN_1, INPUT);
  digitalWrite(OUT_PIN_1, HIGH);
   val = analogRead(IN_PIN_1);

  //Clear everything for next measurement
  digitalWrite(OUT_PIN_1, LOW);
  pinMode(IN_PIN_1, OUTPUT);

  //Calculate and print result

  LC_1 = (float)val * IN_STRAY_CAP_TO_GND / (float)(1023 - val);


delay (1000); //seems to reduce noise of sensor 2

  val = analogRead(IN_PIN_2); //seems to reduce noise of sensor 2

  pinMode(IN_PIN_2, INPUT);
  digitalWrite(OUT_PIN_2, HIGH);
  val = analogRead(IN_PIN_2);

  //Clear everything for next measurement
  digitalWrite(OUT_PIN_2, LOW);
  pinMode(IN_PIN_2, OUTPUT);

  //Calculate and print result

  LC_2 = (float)val * IN_STRAY_CAP_TO_GND / (float)(1023 - val);
}

void get_voltage() { //measure battery voltage at AD0
  Serial.print(F(" Batt: "));
  value = 0;
  for (i = 0; i <= 9; i++) { //take 10 readings and calculate mean value
    value = value + analogRead(batt_v);
    delay(100);
  }
  batt_val = value / 10; //mean value of 10 measurements //transformation in volts will take place on the webserver
  Serial.println(batt_val);
}

void set_sleep_period() {
  Serial.print(F("interval minutes:  "));
  Serial.println(set_value);
  Serial.println(F("(new:01-05-10-15-20-30-60)"));
  make_number();

  if (value==1) {
    alarm_period=6; 
    set_value=1;
  }

    if (value==5) {
    alarm_period=32; 
    set_value=5;
  }

    if (value==10) {
    alarm_period=65; 
    set_value=10;
  }

    if (value==15) {
    alarm_period=99; 
    set_value=15;
  }

    if (value==20) {
    alarm_period=132; 
    set_value=20;
  }

    if (value==30) {
    alarm_period=198; 
    set_value=30;
  }

    if (value==60) {
    alarm_period=398; 
    set_value=60;
  }

  Serial.print(F("interval new:  "));
  Serial.println(set_value);  
   EEPROM.write(6,set_value); //save sleep period in EEPROM
}

 void sleep_period_retrieve(){
set_value=EEPROM.read(6);
}  

void frame_count_retrieve() {
   frame_counter=EEPROM.read(4)*256+EEPROM.read(5); 
   }  

   void frame_count_write() {
  adr_hi=frame_counter/256;    //
  adr_low=frame_counter % 256; //
  EEPROM.write(4,adr_hi);
  EEPROM.write(5,adr_low);
  } 

  void reset_frame_counter () {
frame_counter=0;
frame_count_write();
Serial.print(F("frame-counter: "));
Serial.println(frame_counter);
  }

void  set_SF() {
 Serial.print(F("SF (07-12): "));
  make_number();
  if (value > 12) {
    value = 12;
  }
  if (value < 7) {
    value = 7;
  }
  SF = value;
  EEPROM.write(2, value);

  implement_SF();
}

void get_parameters(){

SF=EEPROM.read(2);
}

void implement_SF() {  //set command for SF from 7 to 12
switch (SF) {
    case 7:
      //do something when var equals 7
   lora.setDatarate(SF7BW125);   
      break;
    case 8:
      //do something when var equals 8
      lora.setDatarate(SF8BW125);
      break;
     case 9:
      //do something when var equals 8
      lora.setDatarate(SF9BW125);
      break;
      case 10:
      //do something when var equals 8
      lora.setDatarate(SF10BW125);
      break; 
      case 11:
      //do something when var equals 8
      lora.setDatarate(SF11BW125);
      break;  
      case 12:
      //do something when var equals 8
      lora.setDatarate(SF12BW125);
      break;   
    default:
      // if nothing else matches, do the default
      // default is optional
      lora.setDatarate(SF9BW125);
    break;
  }
}  
