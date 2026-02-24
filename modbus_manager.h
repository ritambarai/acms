#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "schema.h"   /* variables_modbus_table, variables_description_table */
#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════════
 *  HARDWARE CONFIG — edit to match your wiring
 * ══════════════════════════════════════════════════════════════ */
#define MODBUS_DE_RE_PIN             21
#define MODBUS_BAUD                  9600
#define MODBUS_RX_PIN                16
#define MODBUS_TX_PIN                17
#define MODBUS_FRAME_TIMEOUT_MS      5    /* inter-frame gap to detect end-of-frame */
#define MODBUS_RESPONSE_TIMEOUT_MS   500  /* max wait for a slave to reply         */
#define MODBUS_POLL_INTERVAL_MS      500 /* pause between full table scans        */

/* ═══════════════════════════════════════════════════════════════
 *  SLAVE CONFIG
 * ══════════════════════════════════════════════════════════════ */
#define MODBUS_SLAVE_ID              1

/* ═══════════════════════════════════════════════════════════════
 *  EXCEPTION CODES
 * ══════════════════════════════════════════════════════════════ */
#define ILLEGAL_FUNCTION             0x01
#define ILLEGAL_DATA_ADDRESS         0x02
#define ILLEGAL_DATA_VALUE           0x03

/* ═══════════════════════════════════════════════════════════════
 *  ENUMS
 * ══════════════════════════════════════════════════════════════ */
typedef enum {
    Big_Endian,
    Mid_Little_Endian,
} Endianness_t;

typedef enum {
    Read_Coil_Status          = 0x01,
    Read_Input_Status         = 0x02,
    Read_Holding_Registers    = 0x03,
    Read_Input_Registers      = 0x04,
    Force_Single_Coil         = 0x05,
    Preset_Single_Register    = 0x06,
    Force_Multiple_Coils      = 0x0F,
    Preset_Multiple_Registers = 0x10,
} Function_Code_t;

/* ═══════════════════════════════════════════════════════════════
 *  RECEIVED DATA — populated by modbus_loop() after each response
 * ══════════════════════════════════════════════════════════════ */
extern uint16_t Float_Data_03[125];   /* FC03 holding register values */
extern uint16_t Float_Data_04[125];   /* FC04 input register values   */
extern bool     Bool_Data_01[125];    /* FC01 coil bits               */
extern bool     Bool_Data_02[125];    /* FC02 input status bits       */
extern uint16_t Data_Size;            /* count of values in last frame */

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════ */

/* Call once from setup() */
void modbus_setup(void);

/* modbus_setup() starts an internal FreeRTOS task that cycles through every
 * row in variables_modbus_table, blocking only within its own task stack so
 * the main loop / web server are never interrupted. */

/* Send an ad-hoc Modbus query (master mode).
 * modbus_loop() will parse the response on the next frame. */
void ModbusQuery(uint16_t slave_id, Function_Code_t f_code,
                 uint32_t start_address, uint32_t length);

/* Convert two consecutive uint16 register values to an IEEE 754 float */
float modbus_regs_to_float(uint16_t hi, uint16_t lo);
