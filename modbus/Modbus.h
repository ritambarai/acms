#define SLAVE_ID 1

#define ILLEGAL_FUNCTION       0x01
#define ILLEGAL_DATA_ADDRESS   0x02
#define ILLEGAL_DATA_VALUE     0x03


// extern uint8_t RxData[256];
// extern uint8_t TxData[256];

static uint16_t Holding_Registers_Database[50]={
    0000,  1111,  2222,  3333,  4444,  5555,  6666,  7777,  8888,  9999,   // 0-9   40001-40010
    12345, 15432, 15535, 10234, 19876, 13579, 10293, 19827, 13456, 14567,  // 10-19 40011-40020
    21345, 22345, 24567, 25678, 26789, 24680, 20394, 29384, 26937, 27654,  // 20-29 40021-40030
    31245, 31456, 34567, 35678, 36789, 37890, 30948, 34958, 35867, 36092,  // 30-39 40031-40040
    45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 40-49 40041-40050
};

static const uint16_t Input_Registers_Database[50]={
    0000,  1111,  2222,  3333,  4444,  5555,  6666,  7777,  8888,  9999,   // 0-9   40001-40010
    12345, 15432, 15535, 10234, 19876, 13579, 10293, 19827, 13456, 14567,  // 10-19 40011-40020
    21345, 22345, 24567, 25678, 26789, 24680, 20394, 29384, 26937, 27654,  // 20-29 40021-40030
    31245, 31456, 34567, 35678, 36789, 37890, 30948, 34958, 35867, 36092,  // 30-39 40031-40040
    45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 40-49 40041-40050
};

enum Endianness{
  Big_Endian,         //ABCD
  Mid_Little_Endian,  //CDAB
};

enum Function_Code
{
  Read_Coil_Status          = 0x01,
  Read_Input_Status         = 0x02,
  Read_Holding_Registers    = 0x03,
  Read_Input_Registers      = 0x04,
  Force_Single_Coil         = 0x05,
  Preset_Single_Register    = 0x06,
  Force_Multiple_Coils      = 0x0F,
  Preset_Multiple_Registers = 0x10,
};
