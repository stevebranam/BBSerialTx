/**
 * @file BBSerialTx.h
 * @brief Header for bit-banged serial transmitter (8N1)
 */
#ifndef BB_SERIAL_TX_H
#define BB_SERIAL_TX_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** The standard async serial bit logic levels on the wire (TTL). */
typedef enum {
    BB_SERIAL_TX_SPACE,                      /**< Logic level for binary 0: 0. */
    BB_SERIAL_TX_MARK,                       /**< Logic level for binary 1: 1. */
    BB_SERIAL_TX_IDLE  = BB_SERIAL_TX_MARK,  /**< Logic level for idle line: 1. */
    BB_SERIAL_TX_START = BB_SERIAL_TX_SPACE, /**< Logic level for start bit: 0. */
    BB_SERIAL_TX_STOP  = BB_SERIAL_TX_MARK   /**< Logic level for stop bit: 1. */
} BBSerialTx_Bit_t;

/** A type for the baud rate. */
typedef uint32_t BBSerialTx_BaudRate_t;

/** A type for the write function duration in nsec. */
typedef uint32_t BBSerialTx_WriteNsec_t;

/** A type for the number of times to call the write function for one baud. */
typedef uint32_t BBSerialTx_BitWriteCount_t;

/** A function that writes the serial output line to set the bit logic level. */
typedef void (*BBSerialTx_WriteFunc_t)(BBSerialTx_Bit_t bit);

const size_t BB_SERIAL_TX_START_BITS = 1;
const size_t BB_SERIAL_TX_STOP_BITS = 1;
const size_t BB_SERIAL_TX_BITS_PER_BYTE = 8;
const size_t BB_SERIAL_TX_OVERHEAD_BITS = BB_SERIAL_TX_START_BITS + BB_SERIAL_TX_STOP_BITS;
const size_t BB_SERIAL_TX_BITS_PER_SYMBOL = BB_SERIAL_TX_BITS_PER_BYTE + BB_SERIAL_TX_OVERHEAD_BITS;

const size_t BB_SERIAL_TX_CALIBRATION_PREAMBLE_WRITES = 1000;
const size_t BB_SERIAL_TX_CALIBRATION_POSTAMBLE_WRITES = 1000;
const size_t BB_SERIAL_TX_CALIBRATION_WRITES_PER_BIT = 10;
const size_t BB_SERIAL_TX_CALIBRATION_BYTES = 10;
const size_t BB_SERIAL_TX_CALIBRATION_BYTE = 0x55u; /** Alternating ones and zeroes. */

bool BBSerialTx_Open(BBSerialTx_BaudRate_t baud_rate,
                     BBSerialTx_WriteFunc_t write_function,
                     BBSerialTx_WriteNsec_t write_duration_nsec);
void BBSerialTx_Close(void);
void BBSerialTx_Calibrate(void);
void BBSerialTx_WriteByte(uint8_t byte);
void BBSerialTx_WriteString(const char *string, size_t width);
void BBSerialTx_WriteDecimal(int32_t value, size_t width);
void BBSerialTx_WriteUint8(uint8_t value);
void BBSerialTx_WriteUint16(uint16_t value);
void BBSerialTx_WriteUint32(uint32_t value);
#endif
