extern "C" {
#include "BBSerialTx.h"
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Return;

const BBSerialTx_BaudRate_t BAUD_RATE = 1000u;

//---------- Spy Implementation -----------------------------------------------

// This spy records the bits that were written so they can be read back to
// verify expectations.

// Duration that achieves baud rate bit time with 1 call to write function.
const BBSerialTx_WriteNsec_t WRITE_DURATION_NSEC = 1000000000u / BAUD_RATE;

const size_t STRING_BUFFER_LEN = 10;

const size_t NUM_RECORDED_BITS = BB_SERIAL_TX_CALIBRATION_PREAMBLE_WRITES + 
                                 (BB_SERIAL_TX_CALIBRATION_BYTES * 
                                  BB_SERIAL_TX_BITS_PER_SYMBOL * 
                                  BB_SERIAL_TX_CALIBRATION_WRITES_PER_BIT) +
                                 BB_SERIAL_TX_CALIBRATION_POSTAMBLE_WRITES;

BBSerialTx_Bit_t recorded_bits[NUM_RECORDED_BITS + 1];
size_t writes_per_bit = 0; // Different for calibration vs. tests.
size_t num_recorded_bits = 0;
size_t first_recorded_bit = 0;

// Spy implementation of BBSerialTx_Write_t.
void recorded_write(BBSerialTx_Bit_t bit)
{
    if (num_recorded_bits < NUM_RECORDED_BITS)
    {
        recorded_bits[num_recorded_bits++] = bit;
    }
}

// Helper predicates to validate recorded data.
bool is_valid_bit_count(size_t num_symbols)
{
    return (num_recorded_bits - first_recorded_bit >= BB_SERIAL_TX_BITS_PER_SYMBOL * writes_per_bit * num_symbols);
}

bool is_valid_start_bit()
{
    return (recorded_bits[first_recorded_bit] == BB_SERIAL_TX_START);
}

bool is_valid_stop_bit()
{
    return (recorded_bits[first_recorded_bit + (BB_SERIAL_TX_BITS_PER_SYMBOL * writes_per_bit) - 1] == BB_SERIAL_TX_STOP);
}
bool is_valid_framing()
{
    return (is_valid_bit_count(1u) &&
            is_valid_start_bit() &&
            is_valid_stop_bit());
}

// Helper functions to read recorded data.
uint8_t read_recorded_byte(void)
{
    uint8_t recorded_byte = 0u;

    if (is_valid_framing())
    {
        // Accumulate byte from framed data bits.
        for (size_t bit = 0; bit < BB_SERIAL_TX_BITS_PER_BYTE; bit++)
        {
            // The bits are sent in LSB-MSB order, so shift left to accumulate them.
            recorded_byte |= recorded_bits[first_recorded_bit + ((BB_SERIAL_TX_START_BITS + bit) * writes_per_bit)] << bit;
        }

        // Advance to the next symbol to read next byte.
        first_recorded_bit += BB_SERIAL_TX_BITS_PER_SYMBOL * writes_per_bit;
    }

    return recorded_byte;
}

char *read_recorded_string(char *buffer, size_t buf_len, size_t read_len)
{
    size_t recorded_string_len = 0;

    if ((buffer != NULL) &&
        (read_len > 0) &&
        (buf_len > read_len) &&
         is_valid_bit_count(read_len))
    {
        for (recorded_string_len = 0; recorded_string_len < read_len; recorded_string_len++)
        {
            buffer[recorded_string_len] = read_recorded_byte();
        }

        buffer[recorded_string_len] = '\0';
    }
    return buffer;
}

size_t read_bit_run(BBSerialTx_Bit_t value)
{
    size_t length = 0;

    for (; (first_recorded_bit < num_recorded_bits) && (recorded_bits[first_recorded_bit] == value); first_recorded_bit++)
    {
        length++;
    }

    return length;
}

size_t read_byte_run(uint8_t value)
{
    size_t length = 0;

    for (; (first_recorded_bit < num_recorded_bits) && (read_recorded_byte() == value); )
    {
        length++;
    }

    return length;
}

//---------- Given_UnopenedBBSerialTx -----------------------------------------

class Given_UnopenedBBSerialTx: public ::testing::Test {
public:
    void SetUp( ) {
        writes_per_bit = 1u;
        num_recorded_bits  = 0u;
        first_recorded_bit = 0u;
    }

    void TearDown( ) {
        BBSerialTx_Close();
    }
};

TEST_F(Given_UnopenedBBSerialTx, When_Opened_And_NullFunctionPointer_Then_ShouldNotOpen)
{
    EXPECT_FALSE(BBSerialTx_Open(BAUD_RATE, NULL, WRITE_DURATION_NSEC));
}

TEST_F(Given_UnopenedBBSerialTx, When_Opened_And_NonNullFunctionPointer_Then_ShouldOpen)
{
    EXPECT_TRUE(BBSerialTx_Open(BAUD_RATE, recorded_write, WRITE_DURATION_NSEC));
}

TEST_F(Given_UnopenedBBSerialTx, When_Instantiated_Then_ShouldNotWriteBit)
{
    EXPECT_EQ(num_recorded_bits, 0u);
}

TEST_F(Given_UnopenedBBSerialTx, When_Opened_Then_ShouldWriteIdle)
{
    EXPECT_TRUE(BBSerialTx_Open(BAUD_RATE, recorded_write, WRITE_DURATION_NSEC));

    EXPECT_EQ(num_recorded_bits, 1u);
    EXPECT_EQ(recorded_bits[num_recorded_bits - 1], BB_SERIAL_TX_IDLE);
}

//---------- Given_OpenBBSerialTx ---------------------------------------------

class Given_OpenBBSerialTx: public ::testing::Test {
public:
    void SetUp( ) {
        BBSerialTx_Open(BAUD_RATE, recorded_write, WRITE_DURATION_NSEC);
        writes_per_bit = 1u;
        num_recorded_bits  = 0u;
        first_recorded_bit = 0u;
        recorded_string_buffer[0] = '\0';
    }

    void TearDown( ) {
        BBSerialTx_Close();
    }

    char recorded_string_buffer[STRING_BUFFER_LEN + 2];
};

TEST_F(Given_OpenBBSerialTx, When_Closed_Then_ShouldNotWriteData)
{
    BBSerialTx_Close();
    BBSerialTx_WriteByte(0u);
    EXPECT_EQ(num_recorded_bits, 0u);
}

TEST_F(Given_OpenBBSerialTx, When_Calibrating_Then_ShouldWriteTestPattern)
{
    BBSerialTx_Calibrate();
    writes_per_bit = BB_SERIAL_TX_CALIBRATION_WRITES_PER_BIT;
    EXPECT_EQ(num_recorded_bits, NUM_RECORDED_BITS);
    EXPECT_EQ(read_bit_run(BB_SERIAL_TX_IDLE), BB_SERIAL_TX_CALIBRATION_PREAMBLE_WRITES);
    EXPECT_EQ(read_byte_run(BB_SERIAL_TX_CALIBRATION_BYTE), BB_SERIAL_TX_CALIBRATION_BYTES);
    EXPECT_EQ(read_bit_run(BB_SERIAL_TX_IDLE), BB_SERIAL_TX_CALIBRATION_POSTAMBLE_WRITES);
}

TEST_F(Given_OpenBBSerialTx, When_WritingByte_Then_ShouldWriteStartAndStopBits)
{
    BBSerialTx_WriteByte(0u);
    EXPECT_TRUE(is_valid_framing());
}

TEST_F(Given_OpenBBSerialTx, When_WritingByte_Then_ShouldWriteBinaryData)
{
    BBSerialTx_WriteByte(0x1bu);
    EXPECT_EQ(read_recorded_byte(), 0x1bu);
}

TEST_F(Given_OpenBBSerialTx, When_WritingString_And_NullPointer_Then_ShouldWriteNothing)
{
    BBSerialTx_WriteString(NULL, 0);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 4u), "");
}

TEST_F(Given_OpenBBSerialTx, When_WritingString_And_EmptyString_Then_ShouldWriteNothing)
{
    BBSerialTx_WriteString("", 0);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 4u), "");
}

TEST_F(Given_OpenBBSerialTx, When_WritingString_Then_ShouldWriteCharData)
{
    BBSerialTx_WriteString("Test", 0);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 4u), "Test");
}

TEST_F(Given_OpenBBSerialTx, When_WritingString_Then_ShouldWriteTrailingSpaces)
{
    BBSerialTx_WriteString("Test", STRING_BUFFER_LEN);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), STRING_BUFFER_LEN), "Test      ");
}

TEST_F(Given_OpenBBSerialTx, When_WritingUint8_Then_ShouldWriteAsciiHexData)
{
    BBSerialTx_WriteUint8(0x1bu);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 2u), "1B");
}

TEST_F(Given_OpenBBSerialTx, When_WritingUint16_Then_ShouldWriteAsciiHexData)
{
    BBSerialTx_WriteUint16(0x2a4cu);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 4u), "2A4C");
}

TEST_F(Given_OpenBBSerialTx, When_WritingUint32_Then_ShouldWriteAsciiHexData)
{
    BBSerialTx_WriteUint32(0xdeadbeefu);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 8u), "DEADBEEF");
}

TEST_F(Given_OpenBBSerialTx, When_WritingUint32_Then_ShouldWriteLeadingZeroes)
{
    BBSerialTx_WriteUint32(0x1u);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 8u), "00000001");
}

TEST_F(Given_OpenBBSerialTx, When_WritingDecimal_Then_ShouldWriteAsciiDecimalData)
{
    BBSerialTx_WriteDecimal(1234, 0u);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 4u), "1234");
}

TEST_F(Given_OpenBBSerialTx, When_WritingNegativeDecimal_Then_ShouldWriteMinusSign)
{
    BBSerialTx_WriteDecimal(-1234, 0);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 5u), "-1234");
}

TEST_F(Given_OpenBBSerialTx, When_WritingZeroDecimal_Then_ShouldWriteAsciiZero)
{
    BBSerialTx_WriteDecimal(0, 0u);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), 1u), "0");
}

TEST_F(Given_OpenBBSerialTx, When_WritingDecimal_Then_ShouldWriteLeadingSpaces)
{
    BBSerialTx_WriteDecimal(1234, STRING_BUFFER_LEN);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), STRING_BUFFER_LEN), "      1234");
}

TEST_F(Given_OpenBBSerialTx, When_WritingNegativeDecimal_Then_ShouldWriteLeadingSpacesAndMinusSign)
{
    BBSerialTx_WriteDecimal(-1234, STRING_BUFFER_LEN);
    EXPECT_STREQ(read_recorded_string(recorded_string_buffer, sizeof(recorded_string_buffer), STRING_BUFFER_LEN), "     -1234");
}

//---------- Main program -----------------------------------------------------

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
