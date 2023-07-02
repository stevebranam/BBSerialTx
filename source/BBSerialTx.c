/**
 * @file BBSerialTx.c
 * @brief Bit-banged serial transmitter (8N1)
 */
#include "BBSerialTx.h"

/** The write function provided to BBSerialTx_Open. */
static BBSerialTx_WriteFunc_t Write_Output = NULL;

/** The number of writes per bit computed from baud rate and calibration. */
static BBSerialTx_BitWriteCount_t Writes_Per_Bit = 0u;

/**
 * @brief Predicate to check if BBSerialTx channel is open.
 */
static inline bool is_open()
{
    return (Write_Output != NULL);
}

/**
 * @brief Output a one-baud bit value to the open BBSerialTx channel.
 * 
 * @param[in] bit The bit value to output.
 */
static void write_bit(BBSerialTx_Bit_t bit)
{
    if (is_open())
    {
        for (BBSerialTx_BitWriteCount_t write = 0; write < Writes_Per_Bit; write++)
        {
            Write_Output(bit);
        }
    }
}

/**
 * @brief Opens the BBSerialTx channel.
 *
 * Sets the write function and timing to generate the serial 8N1 output at the
 * specified baud rate.
 *
 * @param[in] baud_rate The baud rate for the output.
 * @param[in] write_function Pointer to a function that writes the output signal.
 * @param[in] write_duration_nsec The duration of a single call to the write function.
 *
 * @return Result of parameter validation.
 * @retval True on successful validation of parameters.
 * @retval False on failure.
 */
bool BBSerialTx_Open(BBSerialTx_BaudRate_t baud_rate,
                     BBSerialTx_WriteFunc_t write_function,
                     BBSerialTx_WriteNsec_t write_duration_nsec)
{
    if ((baud_rate == 0u) ||
        (write_function == NULL) ||
        (write_duration_nsec == 0u))
    {
        return false;
    }
    else
    {
        /* Compute number of times to write output to achieve baud rate. */
        BBSerialTx_WriteNsec_t nsec_per_bit = 1000000000u / baud_rate;
        Writes_Per_Bit = nsec_per_bit / write_duration_nsec;

        Write_Output = write_function;

        write_bit(BB_SERIAL_TX_IDLE);
        return true;
    }
}

/**
 * @brief Closes the BBSerialTx channel.
 */
void BBSerialTx_Close(void)
{
    Write_Output = NULL;
}

/**
 * @brief Outputs a calibration pattern on the BBSerialTx channel.
 *
 * The pattern can be measured with a logic analyzer or oscilloscope. It
 * consists of a preamble of 1000 writes of idle values (logic 1), 1000 writes
 * of alternating 1's and 0's computed from a known input byte, and a postamble
 * of 1000 idles. This makes it easy to isolate on the analyzer trace.
 *
 * Measure the width of the alternating region in usec and divide the time by
 * 1000 to get the number of nsec per write; pass this value to the
 * BBSerialTx_Open() function as the write_duration_nsec argument.
 *
 * The write operation consists of both computation for the bits of the byte,
 * and GPIO hardware control. Depending on the processor architecture, the
 * calculated nsec value may be very precise, or it may be coarse. You may need
 * to adjust the value by up to 5% to get more accurate baud rate control and
 * avoid framing errors.
 *
 * Calibration is typically very platform-specific. A different MCU or board
 * design may require a different calibration value.
 */
void BBSerialTx_Calibrate(void)
{
    if (is_open())
    {
        /* Temporarily set the calibration to a known value. */
        BBSerialTx_BitWriteCount_t old_calibration = Writes_Per_Bit;
        Writes_Per_Bit = BB_SERIAL_TX_CALIBRATION_WRITES_PER_BIT;

        /* 
         * Write a series of idles as a preamble and postamble with a
         * calibration byte pattern in between. These delimit the pattern,
         * making it easy to see on the logic analyzer or oscilloscope.
         */
        
        for (size_t idles = 0; idles < BB_SERIAL_TX_CALIBRATION_PREAMBLE_WRITES; idles++)
        {
            Write_Output(BB_SERIAL_TX_IDLE);
        }

        for (size_t bytes = 0; bytes < BB_SERIAL_TX_CALIBRATION_BYTES; bytes++)
        {
            BBSerialTx_WriteByte(BB_SERIAL_TX_CALIBRATION_BYTE);
        }

        for (size_t idles = 0; idles < BB_SERIAL_TX_CALIBRATION_POSTAMBLE_WRITES; idles++)
        {
            Write_Output(BB_SERIAL_TX_IDLE);
        }

        Writes_Per_Bit = old_calibration;
    }
}

/**
 * @brief Outputs a raw binary byte on the open BBSerialTx channel.
 *
 * @param[in] byte The byte value to output.
 */
void BBSerialTx_WriteByte(uint8_t byte)
{
    write_bit(BB_SERIAL_TX_START);

    /* Output the bits from least significant to most significant (LSB to MSB). */
    for (size_t bit = 0; bit < BB_SERIAL_TX_BITS_PER_BYTE; bit++)
    {
        uint8_t bit_value = (byte >> bit) & 0x1u;
        write_bit((BBSerialTx_Bit_t)bit_value);
    }

    write_bit(BB_SERIAL_TX_STOP);
}

/**
 * @brief Outputs a character string on the open BBSerialTx channel.
 * 
 * Output length is maximum of string length or width argument.
 * 
 * @param[in] string Pointer to the NUL-terminated string to output.
 * @param[in] width Output width, padded with trailing spaces.
 */
void BBSerialTx_WriteString(const char *string, size_t width)
{
    if (string != NULL)
    {
        size_t length;

        /* Save string length. */
        for (length = 0; string[length] != '\0'; length++);

        while (*string != '\0')
        {
            BBSerialTx_WriteByte(*string++);
        }

        /* Output trailing spaces up to any width. */
        while (length < width--)
        {
            BBSerialTx_WriteByte(' ');
        }
    }
}

/**
 * @brief Outputs an ASCII decimal-formatted value on the open BBSerialTx channel.
 * 
 * Output length is maximum of converted digits (with minus sign) or width argument.
 * 
 * @param[in] value Value to convert to ASCII and output.
 * @param[in] width Output width, padded with leading spaces.
 */
void BBSerialTx_WriteDecimal(int32_t value, size_t width)
{
    const size_t MAX_DIGITS = 12;
    char digits[MAX_DIGITS];
    size_t length;

    bool is_zero     = (value == 0);
    bool is_negative = (value < 0);

    if (is_negative)
    {
        value *= -1;
    }

    /* Convert digits in reverse order. */
    for (length = 0; (value != 0) && (length < MAX_DIGITS); length++)
    {
        digits[length] = (value % 10) + '0';
        value /= 10;
    }

    if (is_zero)
    {
        digits[length++] = '0';
    }
    else if (is_negative)
    {
        digits[length++] = '-';
    }

    /* Output leading spaces up to any width. */
    while (length < width--)
    {
        BBSerialTx_WriteByte(' ');
    }

    /* Output digits in correct order. */
    while (length > 0)
    {
        BBSerialTx_WriteByte(digits[--length]);
    }
}

/**
 * @brief Outputs an ASCII hex-formatted UINT8 value on the open BBSerialTx channel.
 * 
 * Output length is 2 digits with leading zeroes.
 * 
 * @param[in] value Value to convert to ASCII and output.
 */
void BBSerialTx_WriteUint8(uint8_t value)
{
    const int BB_SERIAL_TX_NYBBLES_PER_BYTE = 2;
    const int BB_SERIAL_TX_BITS_PER_NYBBLE = BB_SERIAL_TX_BITS_PER_BYTE / BB_SERIAL_TX_NYBBLES_PER_BYTE;
    /* Send upper and lower nybbles of value. */
    for (int i = BB_SERIAL_TX_NYBBLES_PER_BYTE - 1; i >= 0; i--)
    {
        uint8_t nybble = value >> (BB_SERIAL_TX_BITS_PER_NYBBLE * i) & 0xfu;
        uint8_t byte = nybble + (nybble < 10u ? '0' : 'A' - 10u);
        BBSerialTx_WriteByte(byte);
    }
}

/**
 * @brief Outputs an ASCII hex-formatted UINT16 value on the open BBSerialTx channel.
 * 
 * Output length is 4 digits with leading zeroes.
 * 
 * @param[in] value Value to convert to ASCII and output.
 */
void BBSerialTx_WriteUint16(uint16_t value)
{
    for (int i = 0; i < sizeof(value); i++)
    {
        uint8_t byte = (uint8_t)((value >> (BB_SERIAL_TX_BITS_PER_BYTE * ((sizeof(value) - 1) - i))) & 0xff);
        BBSerialTx_WriteUint8(byte);
    }
}

/**
 * @brief Outputs an ASCII hex-formatted UINT32 value on the open BBSerialTx channel.
 * 
 * Output length is 8 digits with leading zeroes.
 * 
 * @param[in] value Value to convert to ASCII and output.
 */
void BBSerialTx_WriteUint32(uint32_t value)
{
    for (int i = 0; i < sizeof(value); i++)
    {
        uint8_t byte = (uint8_t)((value >> (BB_SERIAL_TX_BITS_PER_BYTE * ((sizeof(value) - 1) - i))) & 0xff);
        BBSerialTx_WriteUint8(byte);
    }
}
