/*
 * modbus_manager.cpp
 * Modbus RTU over RS485 (Serial1).
 *
 * modbus_loop() automatically cycles through every row in
 * variables_modbus_table, sends a query for each, and prints
 * the response.  It also handles incoming slave-mode queries
 * when no table query is in flight.
 */

#include "modbus_manager.h"
#include <Arduino.h>
#include <string.h>
extern "C" {
#include "data_manager.h"
}

/* ── Endianness ──────────────────────────────────────────────── */
static Endianness_t SetEndianness = Mid_Little_Endian;

/* ── Slave register databases ────────────────────────────────── */
static uint16_t Holding_Registers_Database[50] = {
       0,  1111,  2222,  3333,  4444,  5555,  6666,  7777,  8888,  9999,
   12345, 15432, 15535, 10234, 19876, 13579, 10293, 19827, 13456, 14567,
   21345, 22345, 24567, 25678, 26789, 24680, 20394, 29384, 26937, 27654,
   31245, 31456, 34567, 35678, 36789, 37890, 30948, 34958, 35867, 36092,
   45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,
};

static const uint16_t Input_Registers_Database[50] = {
       0,  1111,  2222,  3333,  4444,  5555,  6666,  7777,  8888,  9999,
   12345, 15432, 15535, 10234, 19876, 13579, 10293, 19827, 13456, 14567,
   21345, 22345, 24567, 25678, 26789, 24680, 20394, 29384, 26937, 27654,
   31245, 31456, 34567, 35678, 36789, 37890, 30948, 34958, 35867, 36092,
   45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,
};

/* ── I/O buffers ─────────────────────────────────────────────── */
static uint8_t RxData[256];
static uint8_t TxData[256];

/* ── Received data (exported via header) ─────────────────────── */
uint16_t Float_Data_03[125];
uint16_t Float_Data_04[125];
bool     Bool_Data_01[125];
bool     Bool_Data_02[125];
uint16_t Data_Size = 0;

/* ── Internal state ──────────────────────────────────────────── */
typedef enum { REQ_NONE, REQ_FC01, REQ_FC02, REQ_FC03, REQ_FC04 } ModbusReq_t;
static ModbusReq_t lastReq    = REQ_NONE;
static bool        QueryAsked = false;   /* set by external ModbusQuery() */


/* ═══════════════════════════════════════════════════════════════
 *  CRC16 — identical lookup-table implementation from crc.ino
 * ════════════════════════════════════════════════════════════ */
static const uint8_t auchCRCHi[] = {
    0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,
    0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,
    0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,
    0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,
    0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,
    0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,
    0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,
    0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,
    0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,
    0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,
    0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,
    0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,
    0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,
    0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x01,0xC0,
    0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,
    0xC0,0x80,0x41,0x00,0xC1,0x81,0x40,0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,
    0x00,0xC1,0x81,0x40,0x01,0xC0,0x80,0x41,0x01,0xC0,0x80,0x41,0x00,0xC1,0x81,
    0x40
};

static const uint8_t auchCRCLo[] = {
    0x00,0xC0,0xC1,0x01,0xC3,0x03,0x02,0xC2,0xC6,0x06,0x07,0xC7,0x05,0xC5,0xC4,
    0x04,0xCC,0x0C,0x0D,0xCD,0x0F,0xCF,0xCE,0x0E,0x0A,0xCA,0xCB,0x0B,0xC9,0x09,
    0x08,0xC8,0xD8,0x18,0x19,0xD9,0x1B,0xDB,0xDA,0x1A,0x1E,0xDE,0xDF,0x1F,0xDD,
    0x1D,0x1C,0xDC,0x14,0xD4,0xD5,0x15,0xD7,0x17,0x16,0xD6,0xD2,0x12,0x13,0xD3,
    0x11,0xD1,0xD0,0x10,0xF0,0x30,0x31,0xF1,0x33,0xF3,0xF2,0x32,0x36,0xF6,0xF7,
    0x37,0xF5,0x35,0x34,0xF4,0x3C,0xFC,0xFD,0x3D,0xFF,0x3F,0x3E,0xFE,0xFA,0x3A,
    0x3B,0xFB,0x39,0xF9,0xF8,0x38,0x28,0xE8,0xE9,0x29,0xEB,0x2B,0x2A,0xEA,0xEE,
    0x2E,0x2F,0xEF,0x2D,0xED,0xEC,0x2C,0xE4,0x24,0x25,0xE5,0x27,0xE7,0xE6,0x26,
    0x22,0xE2,0xE3,0x23,0xE1,0x21,0x20,0xE0,0xA0,0x60,0x61,0xA1,0x63,0xA3,0xA2,
    0x62,0x66,0xA6,0xA7,0x67,0xA5,0x65,0x64,0xA4,0x6C,0xAC,0xAD,0x6D,0xAF,0x6F,
    0x6E,0xAE,0xAA,0x6A,0x6B,0xAB,0x69,0xA9,0xA8,0x68,0x78,0xB8,0xB9,0x79,0xBB,
    0x7B,0x7A,0xBA,0xBE,0x7E,0x7F,0xBF,0x7D,0xBD,0xBC,0x7C,0xB4,0x74,0x75,0xB5,
    0x77,0xB7,0xB6,0x76,0x72,0xB2,0xB3,0x73,0xB1,0x71,0x70,0xB0,0x50,0x90,0x91,
    0x51,0x93,0x53,0x52,0x92,0x96,0x56,0x57,0x97,0x55,0x95,0x94,0x54,0x9C,0x5C,
    0x5D,0x9D,0x5F,0x9F,0x9E,0x5E,0x5A,0x9A,0x9B,0x5B,0x99,0x59,0x58,0x98,0x88,
    0x48,0x49,0x89,0x4B,0x8B,0x8A,0x4A,0x4E,0x8E,0x8F,0x4F,0x8D,0x4D,0x4C,0x8C,
    0x44,0x84,0x85,0x45,0x87,0x47,0x46,0x86,0x82,0x42,0x43,0x83,0x41,0x81,0x80,
    0x40
};

static uint16_t CRC16(uint8_t *puchMsg, uint16_t usDataLen)
{
    uint8_t uchCRCHi = 0xFF;
    uint8_t uchCRCLo = 0xFF;
    unsigned uIndex;
    while (usDataLen--) {
        uIndex   = uchCRCHi ^ *puchMsg++;
        uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
        uchCRCLo = auchCRCLo[uIndex];
    }
    return (uint16_t)(uchCRCHi << 8 | uchCRCLo);
}

/* ═══════════════════════════════════════════════════════════════
 *  TRANSMIT (appends CRC, drives DE/RE)
 * ════════════════════════════════════════════════════════════ */
static void sendData(uint8_t *data, int size)
{
    uint16_t crc   = CRC16(data, size);
    data[size]     = (crc >> 8) & 0xFF;  /* CRC HIGH — matches original sendData */
    data[size + 1] =  crc       & 0xFF;  /* CRC LOW  */

    digitalWrite(MODBUS_DE_RE_PIN, HIGH);
    Serial1.write(data, size + 2);
    Serial1.flush();
    digitalWrite(MODBUS_DE_RE_PIN, LOW);
}

/* ═══════════════════════════════════════════════════════════════
 *  SLAVE — exception response
 * ════════════════════════════════════════════════════════════ */
static void modbusException(uint8_t code)
{
    TxData[0] = RxData[0];
    TxData[1] = RxData[1] | 0x80;
    TxData[2] = code;
    sendData(TxData, 3);
}

/* ═══════════════════════════════════════════════════════════════
 *  SLAVE — FC03: read holding registers
 * ════════════════════════════════════════════════════════════ */
static uint8_t readHoldingRegs(void)
{
    uint16_t startAddr = (RxData[2] << 8) | RxData[3];
    uint16_t numRegs   = (RxData[4] << 8) | RxData[5];

    if (numRegs < 1 || numRegs > 125)        { modbusException(ILLEGAL_DATA_VALUE);   return 0; }
    if (startAddr + numRegs - 1 > 49)        { modbusException(ILLEGAL_DATA_ADDRESS); return 0; }

    TxData[0] = MODBUS_SLAVE_ID;
    TxData[1] = RxData[1];
    TxData[2] = numRegs * 2;
    int idx = 3;

    for (int i = 0; i < numRegs; i++) {
        uint16_t val = Holding_Registers_Database[startAddr++];
        if (SetEndianness == Mid_Little_Endian) {
            TxData[idx++] =  val       & 0xFF;
            TxData[idx++] = (val >> 8) & 0xFF;
        } else {
            TxData[idx++] = (val >> 8) & 0xFF;
            TxData[idx++] =  val       & 0xFF;
        }
    }
    sendData(TxData, idx);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  SLAVE — FC04: read input registers
 * ════════════════════════════════════════════════════════════ */
static uint8_t readInputRegs(void)
{
    uint16_t startAddr = (RxData[2] << 8) | RxData[3];
    uint16_t numRegs   = (RxData[4] << 8) | RxData[5];

    if (numRegs < 1 || numRegs > 125)        { modbusException(ILLEGAL_DATA_VALUE);   return 0; }
    if (startAddr + numRegs - 1 > 49)        { modbusException(ILLEGAL_DATA_ADDRESS); return 0; }

    TxData[0] = MODBUS_SLAVE_ID;
    TxData[1] = RxData[1];
    TxData[2] = numRegs * 2;
    int idx = 3;

    for (int i = 0; i < numRegs; i++) {
        uint16_t val = Input_Registers_Database[startAddr++];
        if (SetEndianness == Mid_Little_Endian) {
            TxData[idx++] =  val       & 0xFF;
            TxData[idx++] = (val >> 8) & 0xFF;
        } else {
            TxData[idx++] = (val >> 8) & 0xFF;
            TxData[idx++] =  val       & 0xFF;
        }
    }
    sendData(TxData, idx);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  SLAVE — dispatch incoming query
 * ════════════════════════════════════════════════════════════ */
static void ReceiveQuery(uint16_t size)
{
    if (size < 4) return;
    uint16_t crc       = CRC16(RxData, size - 2);
    uint16_t check_crc = RxData[size - 2] | ((uint16_t)RxData[size - 1] << 8); /* low byte first */
    if (crc != check_crc || RxData[0] != MODBUS_SLAVE_ID) return;

    if      (RxData[1] == Read_Holding_Registers) readHoldingRegs();
    else if (RxData[1] == Read_Input_Registers)   readInputRegs();
}

/* ═══════════════════════════════════════════════════════════════
 *  MASTER — parse response into data arrays
 * ════════════════════════════════════════════════════════════ */
static void receiveModbusData(uint16_t size)
{
    Data_Size = 0;

    if (lastReq == REQ_FC03) {
        for (int i = 2; i < size - 3; i += 2)
            Float_Data_03[Data_Size++] = (uint16_t)((RxData[i + 1] << 8) | RxData[i + 2]);

    } else if (lastReq == REQ_FC04) {
        for (int i = 2; i < size - 3; i += 2)
            Float_Data_04[Data_Size++] = (uint16_t)((RxData[i + 1] << 8) | RxData[i + 2]);

    } else if (lastReq == REQ_FC01) {
        for (int i = 2; i < size - 3; i++)
            for (int b = 0; b < 8; b++)
                Bool_Data_01[(i - 2) * 8 + b] = (bool)((RxData[i + 1] >> b) & 0x01);
        Data_Size = 1;   /* one coil per row — bit 0 of first data byte */

    } else {  /* REQ_FC02 */
        for (int i = 2; i < size - 3; i++)
            for (int b = 0; b < 8; b++)
                Bool_Data_02[(i - 2) * 8 + b] = (bool)((RxData[i + 1] >> b) & 0x01);
        Data_Size = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  TABLE-DRIVEN: send query for one table row (no QueryAsked side-effect)
 * ════════════════════════════════════════════════════════════ */
static void send_query_for_row(int row_idx)
{
    variables_modbus_row_t *r = &variables_modbus_table.rows[row_idx];
    uint16_t slave_id =  (uint16_t)r->Slave_ID;
    uint8_t  f_code   =  (uint8_t)(int)r->Function_ID;
    uint32_t addr     =  (uint32_t)r->Start_Address;
    uint32_t len      =  1;   /* Data_Length is always 1; default anything else to 1 */
    (void)r->Data_Length;

    TxData[0] =  slave_id;
    TxData[1] =  f_code;
    TxData[2] = (addr >> 8) & 0xFF;
    TxData[3] =  addr       & 0xFF;
    TxData[4] = (len  >> 8) & 0xFF;
    TxData[5] =  len        & 0xFF;
    sendData(TxData, 6);

    switch (f_code) {
        case Read_Coil_Status:       lastReq = REQ_FC01; break;
        case Read_Input_Status:      lastReq = REQ_FC02; break;
        case Read_Holding_Registers: lastReq = REQ_FC03; break;
        case Read_Input_Registers:   lastReq = REQ_FC04; break;
        default:                     lastReq = REQ_NONE; break;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  TABLE-DRIVEN: print result for one table row
 * ════════════════════════════════════════════════════════════ */
static void print_row_result(int row_idx)
{
    variables_modbus_row_t *r = &variables_modbus_table.rows[row_idx];

    /* Look up the matching description row for Class/Name if available */
    const char *cls  = (row_idx < variables_description_table.count)
                       ? variables_description_table.rows[row_idx].Class : nullptr;
    const char *name = (row_idx < variables_description_table.count)
                       ? variables_description_table.rows[row_idx].Name  : nullptr;

    Serial.printf("[Modbus] Row %d  Slave=%.0f  FC=%.0f  Addr=%.0f  Len=%.0f",
        row_idx,
        r->Slave_ID, r->Function_ID, r->Start_Address, r->Data_Length);

    if (cls || name)
        Serial.printf("  (%s.%s)", cls ? cls : "?", name ? name : "?");

    Serial.printf("  -> %d value(s)\n", Data_Size);

    for (uint16_t i = 0; i < Data_Size; i++) {
        if      (lastReq == REQ_FC03) Serial.printf("  [%u] = %u\n",  i, Float_Data_03[i]);
        else if (lastReq == REQ_FC04) Serial.printf("  [%u] = %u\n",  i, Float_Data_04[i]);
        else if (lastReq == REQ_FC01) Serial.printf("  [%u] = %d\n",  i, (int)Bool_Data_01[i]);
        else if (lastReq == REQ_FC02) Serial.printf("  [%u] = %d\n",  i, (int)Bool_Data_02[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC: ad-hoc master query (external callers)
 * ════════════════════════════════════════════════════════════ */
void ModbusQuery(uint16_t slave_id, Function_Code_t f_code,
                 uint32_t start_address, uint32_t length)
{
    TxData[0] =  slave_id;
    TxData[1] =  f_code;
    TxData[2] = (start_address >> 8) & 0xFF;
    TxData[3] =  start_address       & 0xFF;
    TxData[4] = (length >> 8)        & 0xFF;
    TxData[5] =  length              & 0xFF;
    sendData(TxData, 6);

    switch (f_code) {
        case Read_Coil_Status:       lastReq = REQ_FC01; break;
        case Read_Input_Status:      lastReq = REQ_FC02; break;
        case Read_Holding_Registers: lastReq = REQ_FC03; break;
        case Read_Input_Registers:   lastReq = REQ_FC04; break;
        default:                     lastReq = REQ_NONE; break;
    }
    QueryAsked = true;
}

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC: convert two uint16 register values to IEEE 754 float
 * ════════════════════════════════════════════════════════════ */
float modbus_regs_to_float(uint16_t hi, uint16_t lo)
{
    union { uint32_t u32; float f; } conv;
    conv.u32 = ((uint32_t)hi << 16) | lo;
    return conv.f;
}

static void modbus_task(void *pvParameters);   /* forward declaration */

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC: modbus_setup — call once from setup()
 * ════════════════════════════════════════════════════════════ */
void modbus_setup(void)
{
    pinMode(MODBUS_DE_RE_PIN, OUTPUT);
    digitalWrite(MODBUS_DE_RE_PIN, LOW);   /* default: receive mode */
    Serial1.begin(MODBUS_BAUD, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    Serial.printf("[Modbus] Table has %d row(s)\n", variables_modbus_table.count);
    xTaskCreate(modbus_task, "modbus", 4096, NULL, 1, NULL);
    Serial.println("[Modbus] Ready");
}

/* ═══════════════════════════════════════════════════════════════
 *  INTERNAL: poll one table row — blocking, runs inside FreeRTOS task
 * ════════════════════════════════════════════════════════════ */
static void modbus_poll_row(int row)
{
    variables_modbus_row_t *r = &variables_modbus_table.rows[row];
    Serial.printf("[Modbus] Querying row %d  Slave=%d  FC=%d  Addr=%d  Len=%d\n",
        row, (int)r->Slave_ID, (int)r->Function_ID,
        (int)r->Start_Address, (int)r->Data_Length);

    /* ── 1. Send query ───────────────────────────────────────── */
    while (Serial1.available()) Serial1.read();   /* flush any stale RX bytes */
    send_query_for_row(row);

    /* ── 2. Wait for response frame ─────────────────────────── */
    uint8_t  rx_buf[256];
    uint16_t rx_len       = 0;
    uint32_t start_ms     = millis();
    uint32_t last_byte_ms = 0;
    bool     got_frame    = false;

    while (millis() - start_ms < MODBUS_RESPONSE_TIMEOUT_MS) {
        while (Serial1.available() && rx_len < sizeof(rx_buf)) {
            rx_buf[rx_len++] = (uint8_t)Serial1.read();
            last_byte_ms = millis();
        }
        if (rx_len > 0 && millis() - last_byte_ms >= MODBUS_FRAME_TIMEOUT_MS) {
            got_frame = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));   /* yield to other FreeRTOS tasks */
    }

    /* ── 3. Parse, print, and update data manager (only on success) ── */
    if (got_frame) {
        memcpy(RxData, rx_buf, rx_len);
        receiveModbusData(rx_len);
        print_row_result(row);

        /* ── 4. Write received value into description table and notify DM ── */
        variables_modbus_row_t *r = &variables_modbus_table.rows[row];
        if (r->value_ptr != NULL) {
            float val;
            if      (lastReq == REQ_FC01) val = (float)Bool_Data_01[0];
            else if (lastReq == REQ_FC02) val = (float)Bool_Data_02[0];
            else if (lastReq == REQ_FC03) val = (float)Float_Data_03[0];
            else                          val = (float)Float_Data_04[0];
            *r->value_ptr = val;
            update_variable(r->value_ptr);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  FreeRTOS task: cycles all rows, then waits MODBUS_POLL_INTERVAL_MS
 * ════════════════════════════════════════════════════════════ */
static void modbus_task(void *pvParameters)
{
    for (;;) {
        for (int i = 0; i < variables_modbus_table.count; i++) {
            modbus_poll_row(i);
        }
        vTaskDelay(pdMS_TO_TICKS(MODBUS_POLL_INTERVAL_MS));
    }
}
