/*
 * Modbus.c
 *
 *  Created on: Jan 24, 2026
 *      Author: vishal
 */
//#include "Modbus.h"

enum Endianness SetEndianness = Mid_Little_Endian;//Big_Endian;//Mid_Little_Endian;

uint16_t Float_Data_03[125];
uint16_t Float_Data_04[125];
bool Bool_Data_01 [125];
bool Bool_Data_02 [125];


uint16_t Data_Size, Main_Data_Size;

bool QueryAsked = false;
extern ModbusReq lastReq;


extern uint8_t RxData[256];
extern uint8_t TxData[256];

enum Function_Code;

void sendData (uint8_t *data, int size)
{
  // we will calculate the CRC in this function itself
  uint16_t crc = CRC16(data, size);
  data[size] = (crc>>8)&0xFF;  // CRC HIGH
  data[size+1] = crc&0xFF;   // CRC LOW

  digitalWrite(DE_RE,HIGH);
  Serial1.write(data,size + 2);
  Serial1.flush(); 
  digitalWrite(DE_RE,LOW);
//  HAL_UART_Transmit(&huart1, data, size+2, 1000);
}

void modbusException (uint8_t exceptioncode)
{
  //| SLAVE_ID | FUNCTION_CODE | Exception code | CRC     |
  //| 1 BYTE   |  1 BYTE       |    1 BYTE      | 2 BYTES |

  TxData[0] = RxData[0];       // slave ID
  TxData[1] = RxData[1]|0x80;  // adding 1 to the MSB of the function code
  TxData[2] = exceptioncode;   // Load the Exception code
  sendData(TxData, 3);         // send Data... CRC will be calculated in the function
}

float u32_to_float(uint32_t hex)
{
    union {
        uint32_t u32;
        float f;
    } conv;

    conv.u32 = hex;
    return conv.f;
}



void receiveModbusData(uint16_t Size ){
  Data_Size = 0;//(Size-5)/4;

  if(lastReq == REQ_FC03  ){
  for (int i=2; i<Size-3;i=i+2)
  { //                                        A                 B
      Float_Data_03[Data_Size] = ((RxData[i+1]<<8 | RxData[i+2]));
     // Serial.printf("Float_Data[%d] = %d\n", (i-2)/2, Float_Data[(i-2)/2]);

      Data_Size++;
  }
  }
  else if( lastReq == REQ_FC04 ){
  for (int i=2; i<Size-3;i=i+2)
  { //                                        A                 B
      Float_Data_04[Data_Size] = ((RxData[i+1]<<8 | RxData[i+2]));
     // Serial.printf("Float_Data[%d] = %d\n", (i-2)/2, Float_Data[(i-2)/2]);

      Data_Size++;
  }
  }

  else if( lastReq == REQ_FC01 ){
     for (int i=2; i<Size-3;i=i+1)
  { //                                        A                 B
      Bool_Data_01[(i-2)*8] = bool((RxData[i+1]>>0 ) & 0x01);
      Bool_Data_01[(i-2)*8+1] = bool((RxData[i+1]>>1 ) & 0x01);
      Bool_Data_01[(i-2)*8+2] = bool((RxData[i+1]>>2 ) & 0x01);
      Bool_Data_01[(i-2)*8+3] = bool((RxData[i+1]>>3 ) & 0x01);
      Bool_Data_01[(i-2)*8+4] = bool((RxData[i+1]>>4 ) & 0x01);
      Bool_Data_01[(i-2)*8+5] = bool((RxData[i+1]>>5 ) & 0x01);
      Bool_Data_01[(i-2)*8+6] = bool((RxData[i+1]>>6 ) & 0x01);
      Bool_Data_01[(i-2)*8+7] = bool((RxData[i+1]>>7 ) & 0x01);

     // Serial.printf("Bool_Data_01[%d] = %d\n", (i-2), Bool_Data_01[(i-2)]);
    //Data_Size++;
    }
  } 

//REQ_FC02 // reading bites
    else{ 
     for (int i=2; i<Size-3;i=i+1)
      { //                                        A                 B
      Bool_Data_02[(i-2)*8] = bool((RxData[i+1]>>0 ) & 0x01);
      Bool_Data_02[(i-2)*8+1] = bool((RxData[i+1]>>1 ) & 0x01);
      Bool_Data_02[(i-2)*8+2] = bool((RxData[i+1]>>2 ) & 0x01);
      Bool_Data_02[(i-2)*8+3] = bool((RxData[i+1]>>3 ) & 0x01);
      Bool_Data_02[(i-2)*8+4] = bool((RxData[i+1]>>4 ) & 0x01);
      Bool_Data_02[(i-2)*8+5] = bool((RxData[i+1]>>5 ) & 0x01);
      Bool_Data_02[(i-2)*8+6] = bool((RxData[i+1]>>6 ) & 0x01);
      Bool_Data_02[(i-2)*8+7] = bool((RxData[i+1]>>7 ) & 0x01);

     // Serial.printf("Bool_Data_02[%d] = %d\n", (i-2), Bool_Data_02[(i-2)]);
    //Data_Size++;
      }
    }

}

void ReceiveQuery(uint16_t Size){
  //CRC Verification
  uint16_t crc = CRC16(RxData, Size-2);
  uint16_t Check_crc = RxData[Size-2]<<8 | RxData[Size-1];
  if (crc == Check_crc && RxData[0] == SLAVE_ID){
    //Response
    if (RxData[1] == Read_Holding_Registers){ //Read_Holding_Registers
      readHoldingRegs();

    }
    else if (RxData[1] == Read_Input_Registers){ //Read_Input_Registers
      readInputRegs();
    }
  }
}

uint8_t readHoldingRegs (void)
{
  uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

  uint16_t numRegs = ((RxData[4]<<8)|RxData[5]);   // number to registers master has requested
  if ((numRegs<1)||(numRegs>125))  // maximum no. of Registers as per the PDF
  {
    modbusException (ILLEGAL_DATA_VALUE);  // send an exception
    return 0;
  }

  uint16_t endAddr = startAddr+numRegs-1;  // end Register
  if (endAddr>49)  // end Register can not be more than 49 as we only have record of 50 Registers in total
  {
    modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
    return 0;
  }
  // Prepare TxData buffer

  //| SLAVE_ID | FUNCTION_CODE | BYTE COUNT | DATA      | CRC     |
  //| 1 BYTE   |  1 BYTE       |  1 BYTE    | N*2 BYTES | 2 BYTES |

  TxData[0] = SLAVE_ID;  // slave ID
  TxData[1] = RxData[1];  // function code
  TxData[2] = numRegs*2;  // Byte count
  int indx = 3;  // we need to keep track of how many bytes has been stored in TxData Buffer

  for (int i=0; i<numRegs; i++)   // Load the actual data into TxData buffer
  {
    if(SetEndianness == Mid_Little_Endian){
      //CDAB
      TxData[indx++] = (Holding_Registers_Database[startAddr])&0xFF;   // extract the lower byte
      TxData[indx++] = (Holding_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
    }
    else{
      //ABCD
      TxData[indx++] = (Holding_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
      TxData[indx++] = (Holding_Registers_Database[startAddr])&0xFF;   // extract the lower byte
    }

    startAddr++;  // increment the register address
  }

  sendData(TxData, indx);  // send data... CRC will be calculated in the function itself
  return 1;   // success
}


uint8_t readInputRegs (void)
{
  uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

  uint16_t numRegs = ((RxData[4]<<8)|RxData[5]);   // number to registers master has requested
  if ((numRegs<1)||(numRegs>125))  // maximum no. of Registers as per the PDF
  {
    modbusException (ILLEGAL_DATA_VALUE);  // send an exception
    return 0;
  }

  uint16_t endAddr = startAddr+numRegs-1;  // end Register
  if (endAddr>49)  // end Register can not be more than 49 as we only have record of 50 Registers in total
  {
    modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
    return 0;
  }
  // Prepare TxData buffer

  //| SLAVE_ID | FUNCTION_CODE | BYTE COUNT | DATA      | CRC     |
  //| 1 BYTE   |  1 BYTE       |  1 BYTE    | N*2 BYTES | 2 BYTES |

  TxData[0] = SLAVE_ID;  // slave ID
  TxData[1] = RxData[1];  // function code
  TxData[2] = numRegs*2;  // Byte count
  int indx = 3;  // we need to keep track of how many bytes has been stored in TxData Buffer

  for (int i=0; i<numRegs; i++)   // Load the actual data into TxData buffer
  {
    if(SetEndianness == Mid_Little_Endian){
      //CDAB
      TxData[indx++] = (Input_Registers_Database[startAddr])&0xFF;   // extract the lower byte
      TxData[indx++] = (Input_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
    }
    else{
      //ABCD
      TxData[indx++] = (Input_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
      TxData[indx++] = (Input_Registers_Database[startAddr])&0xFF;   // extract the lower byte
    }

    startAddr++;  // increment the register address
  }

  sendData(TxData, indx);  // send data... CRC will be calculated in the function itself
  return 1;   // success
}

void ModbusQuery(uint16_t slave_id, uint16_t f_code, uint32_t start_address, uint32_t length)
{
  TxData[0] = slave_id;  // slave address

  TxData[1] = f_code;  // Function code for Read Holding Registers

  TxData[2] = (start_address>>8) & 0xFF; //higher byte
  TxData[3] = start_address & 0xFF;      //lower byte

  TxData[4] = (length>>8) & 0xFF;       //higher byte
  TxData[5] = length & 0xFF;            //lower byte

  sendData(TxData, 6);
  QueryAsked = true;
//  BSP_LED_On(LED_RED);
  return;
}
