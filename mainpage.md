@mainpage Bit-Banged Serial Transmitter

@tableofcontents

@section intro_sec Introduction

This module implements a bit-banged serial transmitter over a GPIO line
using the 8N1 protocol (8-bit data, No parity, 1 stop bit, see
https://en.wikipedia.org/wiki/8-N-1), emulating a UART transmitter (see
https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter).
This is suitable for connection to a UART receiver for logging or other
unidirectional communications.

BBSerialTx uses no external functions, timers, interrupts, DMA, UART's, or
other MCU peripherals or resources, so it imposes minimal demands on the MCU or
runtime context. As soon as you can bring up a single GPIO in any context, you
can get serial output.

The only resource consumed is CPU time. This can affect timing-critical
operations, so be sure the system can tolerate the delay imposed by
outputting data. This can create Heisenbugs: the act of observing the system
perturbs it.

The hardware connection is a two-wire connection: TX (the GPIO line) to RX,
and GND to GND.

Note: GPIO is TTL-level, not RS-232 level, so the receiver must be a TTL
UART device. RS-232 uses a different voltage range with inverted logic. If
you need to output to an RS-232 device, you'll need a TTL/RS-232 converter
between the devices. The converter may also need to be configured as a null
modem.

@section license_sec License

MIT License in [LICENSE](../../LICENSE). You can use BBSerialTx as you like
without restriction for commercial and non-commercial use, as long as you
include the LICENSE file. See https://choosealicense.com/licenses/mit/ for
more details.

@section usage_sec Usage

To use BBSerialTx on a target MCU, call BBSerialTx_Open() and provide a
write function that writes to the desired GPIO. This makes the GPIO behave
like a UART transmitter. Perform any required setup of the GPIO before
calling BBSerialTx_Open().

Use BBSerialTx_Calibrate() to generate a test pattern that you can measure
with a logic analyzer or oscilloscope to calculate the duration of a single
call to the write function. Provide this value to BBSerialTx_Open(). Once
you've calibrated the module on a particular MCU, you no longer need the
analyzer; you can send data from that MCU to any standard UART device, such
as a USB-TTL Serial adapter.

Once you've opened the BBSerialTx channel with a calibrated value, you can
output raw binary data, character strings, and ASCII decimal- or
hex-formatted numerical values.

See the Google Test suite in [test/tests.cpp](../../test/tests.cpp) for example
usage, including a write function for off-target testing, and the \ref
arduino_sec below.

@section connection_sec Hardware Connection

This shows a typical hardware connection from the target MCU board running
BBSerialTx to a terminal emulator program (such as minicom) running on a
development host (in this case Ubuntu 20.04), using a USB-TTL Serial Adapter.

The adapter is typically connected via jumper wires either to pins on the MCU
board, or to probes that contact points on the board, such as test points,
solder pads, or leads on parts.

The connection is 2 wires, to the RX and GND of the receiving device. The
receiver's TX line is unused.

@startuml
ditaa
+-----------------+                     +-------------------+
| Target MCU Board|                     |  Development Host |
|                 |                     |                   |
|    +------+     |     +---------+     |                   |
|    |      |     |     |         |     |                   |
|    | GPIO |---->|---->| RX      |     |                   |
|    | cFC2 |     |     |         |     |   +----------+    |
|    +------+     |     | USB TTL |     |   | Terminal |    |
|                 |     | Serial  |---->|-->| Emulator |    |
|    +------+     |     | Adapter | USB |   |      cBLK|    |
|    |      |     |     |         |     |   +----------+    |
|    | GND  |-----|-----| GND     |     |              c806 |
|    | cFC2 |     |     |    c9FF |     +-------------------+
|    +------+     |     +---------+
|            c2B1 |
+-----------------+
@enduml

That results in this communications sequence:

@startuml
participant BBSerialTx as TX
participant minicom as RX

TX -> RX: Serial data
TX -> RX: Serial data
TX -> RX: Serial data
@enduml

Similarly, you can connect the target MCU board to any other TTL serial
receiver, such as a UART on another board.

@section theory_sec Theory Of Operation

The *asynchronous serial 8N1 protocol* consists of a single unidirectional signal
wire in each direction, transmit and receive (TX and RX). Normal communication
is bidirectional via TX and RX.

Normal full communication between the *local* and *remote* devices consists of
both transmitter and receiver functionality, with TX and RX wires
*cross-connected* between them (i.e. local TX to remote RX and vice-versa).

Each device sends data to its peer by transmitting data on the TX wire to the
RX wire on the other end, where the peer receives the data. This provides
*point-to-point connection* between them (vs. *multi-point* or *broadcast
connection*, where multiple devices may communicate with each other).

BBSerialTx implements only the transmit side, and expects that the remote peer
device implements at least the the receive side.

The TX side signals bits by toggling the signal between high and low states,
one bit at a time. This makes it *serial* (vs. *parallel*, which signals multiple
bits at a time over multiple wires). The transmitter adds framing bits to the
data bits to delimit them on the wire and provide error detection and recovery.
These consist of start and stop bits (and optional parity bits).

The bit-time (the duration of each bit) is determined by the *baud rate* (the
bit rate). Since there are 1 billion nsec (nanoseconds) in a second, the
equation for bit-time in nsec is

> bit-time = 1,000,000 nsec / baud-rate

For example, for the common baud rates 9600 and 115200, the bit-times are

> bit-time = 1,000,000 nsec / 9,600 baud = 104,166.67 nsec

> bit-time = 1,000,000 nsec / 115,200 baud = 8,680.56 nsec

A complete transmission of a framed data byte is a *symbol*. The *symbol rate*
is the number of symbols, or framed data bytes, per second.

The 8N1 protocol consist of 8 data bits framed by a start bit, no parity bits,
and 1 stop bit per symbol. Other combinations are possible, such 7N2 (7 data
bits, no parity, 2 stop bits), 7E1 (for *even parity*, where the parity bit
ensures that the number of 1-bits in the symbol is even), but 8N1 is virtually
universal in modern use.

Since 8N1 adds two framing bits to 8 data bits, a symbol is 10 bits, so the
symbol rate is 1/10th the baud rate. For example, 9600 and 115200 baud result
in symbol rates of 960 and 11520 bytes, or characters, per second.

The bit value logic levels 0 and 1 (low and high) are known as *space* and
*mark*, respectively. When the transmitter is not sending any data, it sets the
line to the *idle* state, which is the mark level (i.e. 1, high). The start bit
is the space level (i.e. 0, low), so that the line transitions from high to low
to signal the start of a frame. The stop bit is the mark level, so that the
line transitions back to high for at least one bit-time. It remains at high,
idle, for a random period of time, until the transmitter sends the next frame.

Data bits are transmitted from LSB to MSB (Least Significant Bit to Most
Signficant Bit), so they appear in reverse order in time on the wire.

This timing diagram shows sending the data byte 0x0D, bits 00001101, 10 bits
total:

@code
    Idle    |Start|Data                                          |Stop |Idle
                  |  1     0     1     1     0     0     0    0  |

    --------+     +-----+     +-----+-----+                      +-----+--------
TX          |     |     |     |           |                      |   
            +-----+     +-----+           +-----+-----+-----+----+   
Bit Time       0     1     2     3     4     5     6     7     8    9
@endcode

The RX side recovers data by monitoring the line for the transition from idle
to start, 1 to 0. Since this could occur randomly at any unknown time, it is
*asynchronous*. That completes the definition of *asynchronous serial 8N1*.

The start bit synchronizes the receiver. Since the RX side is configured for
the same baud rate as the TX side, 8N1, it knows the bit-time. This tells it
how to sample the signal, dividing it up into framing and data bits by time.

It accumulates the arriving bits to reassemble them into the received data
byte. If it doesn't detect a stop bit at the right point in time, it declares a
*framing error* for that symbol.

Once the symbol time has elapsed and the receiver has detected idle, it's ready
to receive the next symbol. That allows it to recover from transient errors,
for instance due to poor physical connection or electrical noise on the line.

But if TX and RX are configured differently, or TX doesn't send properly, or
there is continuous electrical noise on the line, the receiver will declare
continuous errors.

BBSerialTx works by setting the output GPIO line to the mark and space levels
according to the protocol. It uses a write function that's passed in by pointer
(function pointer) to set the line level.

It's also given a baud rate and a *write duration* in nsec; this is the time it
takes to execute the write function and computational overhead on the
particular board where it's running. For timing purpose, it simply calls the
write function repeatedly, as many times as needed to maintain the output
signal for the correct bit-time duration to achieve the desired baud rate.

The write duration must first be determined by performing a calibration run.
The produces an output pattern on the GPIO that you can measure with a logic
analyzer or oscilloscope. Once you know the duration of the pattern, you can
calculate the duration of a write.

That architecture allows BBSerialTx to be platform-indepdent, tailorable to a
specific platform.

@section arduino_sec Arduino Example

This example (Arduino_BBSerialTx.ino) uses the builtin yellow LED connected to
pin 13 on an Arduino-compatible board as the GPIO channel. Using jumper wires,
you can connect a USB-TTL Serial adapter to it to receive the output.

The specific calibration value is for an Elegoo MEGA2560 R3 board, which has
an Atmel ATMEGA2560 MCU and 16 MHz crystal.

While this development board has headers for connecting to pin 13 and GND,
solderless probes can be placed on the GND and pin 13 GPIO solder pads on
either side of the yellow LED, as might be required on a production board.

@startuml
ditaa
+--------------------------------+                     +-------------------+
|                                |                     |  Development Host |
|          +------+              |                     |                   |
| +------+ |      | +------+     |     +---------+     |                   |
| |      | |      | | P13  |     |     |         |     |                   |
| | GND  |-| LED  |-| GPIO |---->|---->| RX      |     |                   |
| | cEEE | | cFF0 | | cEEE |     |     |         |     |  +----------+     |
| +------+ |      | +------+     |     | USB TTL |     |  | Terminal |     |
|     |    +------+              |     | Serial  |---->|->| Emulator |     |
|     |                          |     | Adapter | USB |  |      cBLK|     |
|     |                          |     |         |     |  +----------+     |
|     +------------------------->|-----| GND     |     |              c806 |
|                                |     |    c9FF |     +-------------------+
|                                |     +---------+
| c36F        Elegoo MEGA2560 R3 |
+--------------------------------+
@enduml

Arduino uses ".ino" files instead of ".c" files. Add the files BBSerialTx.c
and BBSerialTx.h to the sketch ("Sketch > Add File..." in Arduino IDE 2.1.0)
and rename BBSerialTx.c to BBSerialTx.ino (select the sketch in the
Sketchbook and "Open Folder", then right-click on the file and select
"Rename..."). Then upload the sketch to the board.

DON'T FORGET TO RENAME THE .c FILE to .ino! That causes all kinds of errors
when the Arduino IDE compiles the sketch.

@code{.cpp}
#include "BBSerialTx.h"

#define NEEDS_CALIBRATION 0

const BBSerialTx_BaudRate_t  baud_rate = 9600;
const BBSerialTx_WriteNsec_t calibrated_nsec_per_write = 7100;

uint32_t loop_count;

// Platform-specific function to write desired GPIO line, function pointer
// to be passed to BBSerialTx_Open() as "write_function" parameter.
void builtin_write(BBSerialTx_Bit_t bit)
{
  digitalWrite(LED_BUILTIN, bit);
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  BBSerialTx_Open(baud_rate, builtin_write, calibrated_nsec_per_write);
  loop_count = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
#if NEEDS_CALIBRATION
  BBSerialTx_Calibrate();
#else
  BBSerialTx_WriteString("Hello from Arduino! Up for ", 0);
  BBSerialTx_WriteDecimal(millis(), 8);
  BBSerialTx_WriteString(" msec, loop count 0x", 0);
  BBSerialTx_WriteUint32(loop_count);
  BBSerialTx_WriteString("\r\n", 0);
  loop_count++;
#endif
}

@endcode

Use the following command (or any other terminal emulator) to see the output 
from it (the adapter used here shows up as /dev/ttyUSB0 on Ubuntu 20.04):

@code
minicom -D /dev/ttyUSB0 -B 9600
@endcode

Note that the string being output include "\r\n", which is CR-LF (Carriage
Return and Line Feed). These are typically both required for terminal
emulators in order to get each new line to output at the leftmost column on
the terminal, although they can be configured to automatically expand LF
(aka "newline") into CR-LF. Otherwise, all the output appears at the
right-hand column, where you only see the last character output.

Example output:

@code{.text}
parallels@parallels-Parallels-Virtual-Platform:~/BBSerialTx$ minicom -D /dev/ttyUSB0 -b 9600


Welcome to minicom 2.7.1

OPTIONS: I18n 
Compiled on Dec 23 2019, 02:06:26.
Port /dev/ttyUSB0, 10:47:45

Press CTRL-A Z for help on special keys
Hello from Arduino! Up for   128398 msec, loop count 0x00000797
Hello from Arduino! Up for   128464 msec, loop count 0x00000798
Hello from Arduino! Up for   128531 msec, loop count 0x00000799
Hello from Arduino! Up for   128596 msec, loop count 0x0000079A
Hello from Arduino! Up for   128663 msec, loop count 0x0000079B
Hello from Arduino! Up for   128729 msec, loop count 0x0000079C
Hello from Arduino! Up for   128795 msec, loop count 0x0000079D
Hello from Arduino! Up for   128861 msec, loop count 0x0000079E
CTRL-A Z for help | 9600 8N1 | NOR | Minicom 2.7.1 | VT102 | Offline | ttyUSB0
@endcode

@section build_sec Build and Test

(See @ref tools_sec section below for all tools used and setup commands for Ubuntu Linux.)

First run CMake to prepare the build environment, creating the directory @b build. This generates makefiles and
Doxygen configuration.

@code
cd BBSerialTx
cmake -S . -B build
@endcode

To build the Google Test suite for off-target testing:

@code
cmake --build build
@endcode

To run the built test suite:

@code
build/testBBSerialTx
@endcode

To run the suite under GDB for debugging (for instance, if you make changes
and a test fails unexpectedly, or the program crashes):

@code
gdb build/testBBSerialTx
@endcode

To perform code analysis:

@code
scripts/analyze build
@endcode

To rebuild the documents (uses the CMake-generated Doxyfile.doxygen, not the default
Doxyfile):

@code
cmake --build build --target doxygen
@endcode

@section analysis_sec Analysis

The file @b scripts/analyze is a bash script (here using Ubuntu 20.04) that performs
several analysis steps, generating the @b analysis directory.

The script does the following:
- Runs the Google Test suite that was built and uses the output to generate
  the software specification (since the suite was developed using TDD (Test-Driven
  Development) in a BDD (Behavior-Driven Development) style).
- Runs the pmccabe tool to perform MCC analysis.
- Runs the lcov and gcov tools to perform coverage analysis.

@code
parallels@parallels-Parallels-Virtual-Platform:~/BBSerialTx$ scripts/analyze build
Removing previous analysis data.
Create analysis directory.

Generate test-driven specification.

Perform MCC analysis.

Perform coverage analysis.
Capturing coverage data from build/CMakeFiles/BBSerialTx.dir
Found gcov version: 9.4.0
Using intermediate gcov format
Scanning build/CMakeFiles/BBSerialTx.dir for .gcda files ...
Found 1 data files in build/CMakeFiles/BBSerialTx.dir
Processing BBSerialTx.gcda
Finished .info-file creation
Reading data file analysis/lcov.info
Found 1 entries.
Found common filename prefix "/home/parallels/BBSerialTx"
Writing .css and .png files.
Generating output.
Processing file source/BBSerialTx.c
Writing directory view page.
Overall coverage rate:
  lines......: 100.0% (84 of 84 lines)
  functions..: 100.0% (11 of 11 functions)

Coverage results:
File '/home/parallels/BBSerialTx/source/BBSerialTx.c'
Lines executed:100.00% of 84
@endcode

@subsection specification_sec Specification

In file @b analysis/specification.txt:

- PASSED: Given UnopenedBBSerialTx, When Opened And NullFunctionPointer Then ShouldNotOpen
- PASSED: Given UnopenedBBSerialTx, When Opened And NonNullFunctionPointer Then ShouldOpen
- PASSED: Given UnopenedBBSerialTx, When Instantiated Then ShouldNotWriteBit
- PASSED: Given UnopenedBBSerialTx, When Opened Then ShouldWriteIdle
- PASSED: Given OpenBBSerialTx, When Closed Then ShouldNotWriteData
- PASSED: Given OpenBBSerialTx, When Calibrating Then ShouldWriteTestPattern
- PASSED: Given OpenBBSerialTx, When WritingByte Then ShouldWriteStartAndStopBits
- PASSED: Given OpenBBSerialTx, When WritingByte Then ShouldWriteBinaryData
- PASSED: Given OpenBBSerialTx, When WritingString And NullPointer Then ShouldWriteNothing
- PASSED: Given OpenBBSerialTx, When WritingString And EmptyString Then ShouldWriteNothing
- PASSED: Given OpenBBSerialTx, When WritingString Then ShouldWriteCharData
- PASSED: Given OpenBBSerialTx, When WritingString Then ShouldWriteTrailingSpaces
- PASSED: Given OpenBBSerialTx, When WritingUint8 Then ShouldWriteAsciiHexData
- PASSED: Given OpenBBSerialTx, When WritingUint16 Then ShouldWriteAsciiHexData
- PASSED: Given OpenBBSerialTx, When WritingUint32 Then ShouldWriteAsciiHexData
- PASSED: Given OpenBBSerialTx, When WritingUint32 Then ShouldWriteLeadingZeroes
- PASSED: Given OpenBBSerialTx, When WritingDecimal Then ShouldWriteAsciiDecimalData
- PASSED: Given OpenBBSerialTx, When WritingNegativeDecimal Then ShouldWriteMinusSign
- PASSED: Given OpenBBSerialTx, When WritingZeroDecimal Then ShouldWriteAsciiZero
- PASSED: Given OpenBBSerialTx, When WritingDecimal Then ShouldWriteLeadingSpaces
- PASSED: Given OpenBBSerialTx, When WritingNegativeDecimal Then ShouldWriteLeadingSpacesAndMinusSign

@subsection mcc_sec MCC Analysis

In file @b analysis/pmccabe.txt:

@code{.text}
Modified McCabe Cyclomatic Complexity
|   Traditional McCabe Cyclomatic Complexity
|       |    # Statements in function
|       |        |   First line of function
|       |        |       |   # lines in function
|       |        |       |       |  filename(definition line number):function
|       |        |       |       |           |
1	1	1	16	4	source/BBSerialTx.c(16): is_open
3	3	5	26	10	source/BBSerialTx.c(26): write_bit
4	4	7	51	22	source/BBSerialTx.c(51): BBSerialTx_Open
1	1	1	77	4	source/BBSerialTx.c(77): BBSerialTx_Close
5	5	16	103	32	source/BBSerialTx.c(103): BBSerialTx_Calibrate
2	2	7	141	13	source/BBSerialTx.c(141): BBSerialTx_WriteByte
5	5	10	163	21	source/BBSerialTx.c(163): BBSerialTx_WriteString
8	8	20	193	42	source/BBSerialTx.c(193): BBSerialTx_WriteDecimal
3	3	9	243	12	source/BBSerialTx.c(243): BBSerialTx_WriteUint8
2	2	5	263	8	source/BBSerialTx.c(263): BBSerialTx_WriteUint16
2	2	5	279	8	source/BBSerialTx.c(279): BBSerialTx_WriteUint32
@endcode

@subsection coverage_sec Test Coverage Analysis

In [analysis/gcov](../../analysis/gcov), and HTML in [analysis/lcov](../../analysis/lcov/index.html).

Summary in @b analysis/gcov/gcov.txt:

@code{.text}
File '/home/parallels/BBSerialTx/source/BBSerialTx.c'
Lines executed:100.00% of 84
@endcode

@section tools_sec Tools

The following tools and setup commands were used here (Ubuntu 20.04 VM under
Parallels on Mac):

@code{.py}
# Install git:
sudo apt install git

# Install cmake:
sudo apt install cmake 

# Install Google Test:
git clone https://github.com/google/googletest.git -b release-1.11.0
cd googletest
mkdir build
cd build
cmake ..
sudo make install
cd

# Install lcov:
sudo apt install lcov

# Install pmccabe:
sudo apt install pmccabe

# Install tkdiff:
sudo apt install tkcvs

# Install minicom:
sudo apt install minicom

# Install doxygen:
sudo apt install doxygen

# Install dot for doxygen graphics:
sudo apt install graphviz

# Install plantuml:
sudo apt-get install plantuml

# Give user access to USB-connected Arduino and serial adapter via dialout and tty groups
# (the information for usermod says you need to log out and back in to
# apply it, but I had to restart my Ubuntu VM to get it to take effect;
# this is consistent with the experience others have posted online):
sudo usermod -a -G dialout $USER
sudo usermod -a -G tty $USER

# Show groups (after logging back in):
groups
@endcode

Arduino IDE for Linux: https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE

Note that these are not necessarily the latest version of all the tools. For
instance, this installs an older version of plantuml that doesn't support some
of the syntax documented at https://plantuml.com/. However, these versions work
together as used here.