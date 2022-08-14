#include <Wire.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <SoftwareSerial.h> 
//SoftwareSerial MyBlue(9,8); // RX | TX 
SoftwareSerial MyBlue(5,3); // RX | TX 

const unsigned int MAX_MESSAGE_LENGTH = 20;
RF24 radio(6,10);

//Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0D203, 0xE8E8F0F0E103 };

// Normal int counts for the following:
// Away indicator, Away score, Home indicator, Home score, Inning, Ball, Strike, Out
uint8_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void setup() {
  Serial.begin(9600);
  MyBlue.begin(9600); // probbably not 9600 

  radio.begin();
  radio.setChannel(108);
  radio.setAutoAck(false);
  radio.setRetries(15,15);
  radio.setPALevel(RF24_PA_MAX);
  radio.setPayloadSize(9);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_8);
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1,pipes[0]);  
  radio.startListening();
  
  Wire.begin();

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); 
  //pinMode(9, OUTPUT);
  //digitalWrite(9, HIGH);
}

boolean waitForResponse = false;
unsigned long lastRemoteSend = 0;
void writeDataToWireless()
{
  uint8_t send_state[9];
  uint8_t sum = 0;
   
  for(int i=0; i < 8; i++){  
    send_state[i] = state[i];
    sum += state[i];
  }
  send_state[8] = sum;
      
  radio.stopListening();
  bool ok = radio.write(send_state, 9); 
  radio.startListening();
  
  if(ok){
    lastRemoteSend = millis();
    waitForResponse = true;
    Serial.println("SENT DATA!");
  } else {
    Serial.println("blah!");
  }
}

void readDataOnWireless(){
  if(radio.available()){
    uint8_t data[8];
    radio.read(data, sizeof(data) );

    bool isSame = true;
    for (int i = 0; i < 8 ; i++) {
      if (data[i] != state[i]) {
        isSame = false;
      }
    }

    waitForResponse = false;
    if (isSame) {  
      Serial.println("Got GOOD response!");
    } else {
      Serial.println("GOT MALFORMED RESPONSE!");
    }
  }
}

static char message[MAX_MESSAGE_LENGTH];
static unsigned int messagePos = 0;

void readSerialInByte(byte inByte) {
  if (messagePos >= MAX_MESSAGE_LENGTH) {
    Serial.print("In message to long.");  
    messagePos = 0;
    while(messagePos < MAX_MESSAGE_LENGTH) {
        message[messagePos] = '\0';
        messagePos++;
    } 
    messagePos = 0;
    return;
  }

  // Onyl numbers, commas and the '\n' char are allowed here.
  if (!isDigit((char)inByte) && (char)inByte != ',' && inByte != '\n' && inByte != '\r') {
    Serial.print("ILLEGAL char on input: ");
    Serial.println((char)inByte);
    return;
  }

  //Message coming
  if (inByte != '\n')
  {
    message[messagePos] = inByte;
    messagePos++;
  } else {
    //Add null character to string
    message[messagePos] = '\0';

    messagePos++;
    while(messagePos < MAX_MESSAGE_LENGTH) {
        message[messagePos] = '\0';
        messagePos++;
    }
    //Reset for the next message
    messagePos = 0;
    
    //Print the message (or do other things)
    Serial.print("Got data: ");   
    //Serial.println(message);   
  
    int statePos = 0;
    char *singleState = strtok(message, ",");
    
    while(singleState != NULL) {
      Serial.print(singleState);
      Serial.print(" ");
      state[statePos] = atoi(singleState);
      singleState = strtok(NULL, ",");
      statePos++;
    }
    Serial.println("");
  
    if (statePos == 8) {
      writeDataToWireless();
    } else {
      Serial.println("Weird data...");
    }
  }
}

unsigned long lastUpdate = 0;
void loop() {
  if(waitForResponse) {
    readDataOnWireless();
  }

  if (millis() - lastRemoteSend > 150 && waitForResponse) {
    writeDataToWireless();
  }
  
  while (MyBlue.available() > 0) {
    char inByte = MyBlue.read();
    readSerialInByte(inByte);
  }
/*
  while (Serial.available() > 0) {
    char inByte = Serial.read();
    readSerialInByte(inByte);
  }
*/
}
