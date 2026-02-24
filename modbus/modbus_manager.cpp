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

/* ── Table-driven polling state ──────────────────────────────── */
static int      tbl_row     = -1;    /* current table row; -1 = not started */
static bool     tbl_waiting = false; /* true while waiting for response      */
static uint32_t tbl_sent_ms = 0;    /* millis() when query was sent         */

/* ═══════════════════════════════════════════════════════════════
 *  CRC16 (Modbus standard — polynomial 0xA001)
 * ════════════════════════════════════════════════════════════ */
static uint16_t CRC16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════
 *  TRANSMIT (appends CRC, drives DE/RE)
 * ════════════════════════════════════════════════════════════ */
static void sendData(uint8_t *data, int size)
{
    uint16_t crc   = CRC16(data, size);
    data[size]     = (crc >> 8) & 0xFF;
    data[size + 1] =  crc       & 0xFF;

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
    uint16_t check_crc = (RxData[size - 2] << 8) | RxData[size - 1];
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

    } else {  /* REQ_FC02 */
        for (int i = 2; i < size - 3; i++)
            for (int b = 0; b < 8; b++)
                Bool_Data_02[(i - 2) * 8 + b] = (bool)((RxData[i + 1] >> b) & 0x01);
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
    uint32_t len      =  (uint32_t)r->Data_Length;

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

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC: modbus_setup — call once from setup()
 * ════════════════════════════════════════════════════════════ */
void modbus_setup(void)
{
    pinMode(MODBUS_DE_RE_PIN, OUTPUT);
    digitalWrite(MODBUS_DE_RE_PIN, LOW);   /* default: receive mode */
    Serial1.begin(MODBUS_BAUD, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    Serial.println("[Modbus] Ready");
}

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC: modbus_loop — call every loop() iteration
 *
 *  State machine:
 *    ┌─────────┐  send query for tbl_row   ┌─────────┐
 *    │  IDLE   │ ─────────────────────────▶│ WAITING │
 *    └─────────┘                           └─────────┘
 *         ▲   frame received OR timeout         │
 *         └─────────────────────────────────────┘
 *              (print result, advance row)
 *
 *  Priority:
 *    1. Table-driven row response  (tbl_waiting)
 *    2. Ad-hoc external response   (QueryAsked)
 *    3. Slave mode                 (neither flag set)
 * ════════════════════════════════════════════════════════════ */
void modbus_loop(void)
{
    /* ── 1. Accumulate bytes ────────────────────────────────── */
    static uint8_t  rx_buf[256];
    static uint16_t rx_len       = 0;
    static uint32_t last_byte_ms = 0;

    while (Serial1.available() && rx_len < (uint16_t)sizeof(rx_buf)) {
        rx_buf[rx_len++] = (uint8_t)Serial1.read();
        last_byte_ms = millis();
    }

    bool     frame_ready = (rx_len > 0 &&
                            millis() - last_byte_ms >= MODBUS_FRAME_TIMEOUT_MS);
    uint16_t frame_len   = rx_len;

    if (frame_ready) {
        memcpy(RxData, rx_buf, rx_len);
        rx_len = 0;

        if (tbl_waiting) {
            /* ── Table row response ── */
            receiveModbusData(frame_len);
            print_row_result(tbl_row);
            tbl_waiting = false;

        } else if (QueryAsked) {
            /* ── Ad-hoc external response ── */
            receiveModbusData(frame_len);
            QueryAsked = false;

        } else {
            /* ── Slave mode: respond to incoming query ── */
            ReceiveQuery(frame_len);
        }
    }

    /* ── 2. Timeout: give up waiting for this row ───────────── */
    if (tbl_waiting && millis() - tbl_sent_ms > MODBUS_RESPONSE_TIMEOUT_MS) {
        Serial.printf("[Modbus] Row %d timeout  (Slave=%.0f  FC=%.0f)\n",
            tbl_row,
            variables_modbus_table.rows[tbl_row].Slave_ID,
            variables_modbus_table.rows[tbl_row].Function_ID);
        tbl_waiting = false;
    }

    /* ── 3. Send query for the next table row ───────────────── */
    if (!tbl_waiting && !QueryAsked && variables_modbus_table.count > 0) {
        tbl_row = (tbl_row + 1) % variables_modbus_table.count;
        send_query_for_row(tbl_row);
        tbl_waiting = true;
        tbl_sent_ms = millis();
    }
}
