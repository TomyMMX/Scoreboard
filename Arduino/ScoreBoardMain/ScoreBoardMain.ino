#include <Wire.h>
#include "nRF24L01.h"
#include "RF24.h"

RF24 radio(6,10);

//Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xE8E8F0F0E103, 0xF0F0F0F0D203 };

// Normal int counts for the following:
// Away indicator, Away score, Home indicator, Home score, Inning, Ball, Strike, Out
uint8_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};
boolean updateDisplay = true;

void setup() {
  Serial.begin(9600);

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
}

// Select I2C BUS
void selectBoard(uint8_t bus){
  Wire.beginTransmission(0x70);  // TCA9548A address
  Wire.write(1 << bus);          // send byte to select bus
  Wire.endTransmission();
}

const byte numTable[11] = {0x88, 0xDB, 0xA2, 0x92, 0xD1, 0x94, 0x84, 0xDA, 0x80, 0x90, 0xFF};

// Used to turn the first relay on (used for inning top/bottom indicator)
const byte relay0 = 1 << 7;

int curNo = 0;
void writeNumber(uint8_t bus, int no, boolean indicator = false) {
  byte relayState = numTable[no];

  if (indicator) {
    relayState = ~(~relayState | relay0);
  }

  writeByteToRelay(bus, relayState);
}

void writeByteToRelay(uint8_t bus, byte no) {
  selectBoard(bus);
  Wire.beginTransmission(0x20);
  Wire.write(no);
  Wire.endTransmission();

  Serial.print("On bus ");
  Serial.print(bus);
  Serial.print(": ");
  Serial.println(no);
}

void readDataOnWireless(){
  if(radio.available()){
    uint8_t got_state[9];
    radio.read(got_state, sizeof(got_state));

    Serial.println("Got new DATA!"); 
    uint8_t sum = 0;
    for(int i=0; i < sizeof(got_state); i++){
      Serial.print(got_state[i]);
      Serial.print(" ");  
      if (i < 8) {
        sum += got_state[i];
      }
    }
    Serial.print("Sum: ");
    Serial.println(sum);

    Serial.print("Sum erls: ");
    Serial.println(got_state[8]);
    if ( got_state[8] == sum && sum != 0) {
      for(int i=0; i < 8; i++){  
        state[i] = got_state[i];
      }
      updateDisplay = true;
    } else {
      Serial.println("Shit data.");  
    }

    radio.stopListening();
    bool ok = radio.write(got_state, sizeof(got_state) );  
    radio.startListening(); 
    
    if(ok){
      Serial.println("SENT Reply!");        
    }
  }
}

void setResult(uint8_t data, boolean setIndicator, uint8_t busFirst, uint8_t busSecond) {
  uint8_t first = 10;
  uint8_t second = 10;

  if (data >= 0) {
     second = data % 10; 
  }

  if (data >= 10 && data < 100) {
    data = data / 10;
    first = data % 10;
  }
  // Temporarily until the numbers are able to show something else than 1.
  if (first > 1 && first < 10) {
    writeByteToRelay(busFirst, 0xFB);
  } else {
    writeNumber(busFirst, first);  
  }

  // hide tob/bottom if inning 0
  if (state[4] == 0) {
    setIndicator = false;
  }

  // Turn numbers off when game is reset to 0.
  if (state[4] == 0 && state[1] == 0 && state[3] == 0 && (state[0] != 0 || state[2] !=0)) {
    second = 10;
  }
  writeNumber(busSecond, second, setIndicator);
}


unsigned long lastUpdate = 0;
void loop() {
  readDataOnWireless();
/*
  if(millis() - lastUpdate > 10000){
    lastUpdate = millis();
    updateDisplay = true;
  } 
 */ 
  if (updateDisplay) {
    Serial.println("Update scoreboard");
    updateDisplay = false;

    boolean setGuestIndicator = false;
    boolean setHomeIndicator = false;

    byte count = 0x0;

    for(int i=0; i < sizeof(state); i++){
      uint8_t data = state[i];
      switch (i) {
        case 0:
          // Guest inning indicator
          if (data > 0) {
            setGuestIndicator = true;
          }
          break;
        case 1:
          setResult(data, setGuestIndicator, 5, 4);
          break;
        case 2:
          // Home inning indicator
          if (data > 0) {
            setHomeIndicator = true;
          }
          break;
        case 3:
          // Home result
          setResult(data, setHomeIndicator, 2, 1);
          break;
        case 4:
          // Current inning
          if (data > 0 && data < 10) {
            writeNumber(3, data);
          } else {
            writeNumber(3, 10);
          }
          break;
        case 5:
          // Ball count
          if (data > 0) {
            count = count | (1 << 6);
          }
          if (data > 1) {
            count = count | (1 << 5);
          }
          if (data > 2) {
            count = count | (1 << 4);
          }
          break;
        case 6:
          // Strike count
          if (data > 0) {
            count = count | (1 << 3);
          }
          if (data > 1) {
            count = count | (1 << 2);
          }
          break;
        case 7:
          // Out count
          if (data > 0) {
            count = count | (1 << 1);
          }
          if (data > 1) {
            count = count | 1;
          }
          break;
        default:
          break;
      }
    }

    Serial.print("Count byte: ");
    Serial.println(count);
    writeByteToRelay(0, ~count);
  }  
}
