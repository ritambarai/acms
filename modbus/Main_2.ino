#include "Modbus.h"

enum ModbusReq {
  REQ_FC01,
  REQ_FC02,
  REQ_FC03,
  REQ_FC04
};

ModbusReq reqState = REQ_FC01;
ModbusReq lastReq;

#define UART1_TX 17  
#define UART1_RX 16  
#define DE_RE 21

#define FC01_No_Of_Add 8
#define FC02_No_Of_Add 8
#define FC03_No_Of_Add 15
#define FC04_No_Of_Add 4

unsigned long lastPollTime = 0;
unsigned long lastRxByteTime = 0;

const uint32_t POLL_INTERVAL = 2000;   // 2 seconds
const uint32_t FRAME_GAP_MS = 5;        // ~3.5 char time @ 9600 baud

extern bool QueryAsked;
extern uint16_t Float_Data_03[125];
extern uint16_t Float_Data_04[125];
extern bool Bool_Data_01[125];
extern bool Bool_Data_02[125];


uint8_t RxData[256];
uint8_t TxData[256];

volatile uint16_t rxLen = 0;
unsigned long lastRxTime = 0;

void onUartRx()
{
  while (Serial1.available())
  {
    RxData[rxLen++] = Serial1.read();
    lastRxByteTime = millis();
  }
}

void decodeBits(){

    if(lastReq == REQ_FC01  ){
   
    Serial.printf("Modbus response 1 bit data: %d \n", rxLen);
    for (int i = 0; i < (rxLen-5)*8 ; i++)
    {
      Serial.printf("Bool_Data_01[%d] = %d\n", i, Bool_Data_01[i]);
    }

    rxLen = 0;   
    QueryAsked = false;

    }

    else{
    
    Serial.printf("Modbus response 1 bit data: %d \n", rxLen);
    for (int i = 0; i < (rxLen-5)*8 ; i++)
    {
      Serial.printf("Bool_Data_02[%d] = %d\n", i, Bool_Data_02[i]);
    }

    rxLen = 0;   
    QueryAsked = false;
    }

}

void decodeRegisters(){

    if(lastReq == REQ_FC03  ){
    Serial.printf("Modbus response 16 bit data: %d \n", rxLen);
    for (int i = 0; i < (rxLen-5)/2; i++)
    {
      Serial.printf("Float_Data_03[%d] = %d\n", i, Float_Data_03[i]);
    }

    rxLen = 0;   // ready for next cycle
    QueryAsked = false;
    }
    
    else {
    Serial.printf("Modbus response 16 bit data: %d \n", rxLen);
    for (int i = 0; i < (rxLen-5)/2; i++)
    {
      Serial.printf("Float_Data_04[%d] = %d\n", i, Float_Data_04[i]);
    }

    rxLen = 0;   // ready for next cycle
    QueryAsked = false;
    }

}


void setup() {
  pinMode(DE_RE,OUTPUT);
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, UART1_RX, UART1_TX);  // UART1 using GPIO35 (RX) and GPIO13 (TX) //VM
  Serial1.onReceive(onUartRx);
  digitalWrite(DE_RE,LOW);
  lastPollTime = millis();

}

void loop() {

   unsigned long now = millis();

  /* ---------- SEND MODBUS QUERY EVERY 2 SEC ---------- */
if ((now - lastPollTime >= POLL_INTERVAL) && !QueryAsked)
{
  rxLen = 0;

  switch (reqState)
  {
    case REQ_FC01:
      ModbusQuery(1, Read_Coil_Status, 0, FC01_No_Of_Add);              // FC01
      lastReq = REQ_FC01;
      reqState = REQ_FC02;
      break;

    case REQ_FC02:
      ModbusQuery(1, Read_Input_Status, 0, FC02_No_Of_Add);    // FC02
      lastReq = REQ_FC02;
      reqState = REQ_FC03;
      break;

    case REQ_FC03:
      ModbusQuery(1, Read_Holding_Registers, 20000, FC03_No_Of_Add);  // FC03
      lastReq = REQ_FC03;
      reqState = REQ_FC04;
      break;

    case REQ_FC04:
      ModbusQuery(1, Read_Input_Registers, 66 , FC04_No_Of_Add);    // FC04
      lastReq = REQ_FC04;
      reqState = REQ_FC01;
      break;
  }

  QueryAsked = true;
  lastPollTime = now;
}

  /* ---------- CHECK IF RESPONSE FRAME COMPLETE ---------- */
if (rxLen > 0 && (millis() - lastRxByteTime) > FRAME_GAP_MS)
{
  receiveModbusData(rxLen);

  switch (lastReq)
  {
    case REQ_FC01:
      Serial.println("FC01 Coils:");
      decodeBits();   // bit-level decode
      break;

    case REQ_FC02:
      Serial.println("FC02 Discrete Inputs:");
      decodeBits();
      break;

    case REQ_FC03:
      Serial.println("FC03 Holding Registers:");
      decodeRegisters();
      break;

    case REQ_FC04:
      Serial.println("FC04 Input Registers:");
      decodeRegisters();
      break;
  }

  rxLen = 0;
  QueryAsked = false;
}



}
