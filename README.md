NeatAMP : Tests & Development software
======================================

This software is aimed at easing tests and setup of Class D audio amplifier
NeatAMP based on Texas Instrument TAS3251 chip which embed a DSP.

### Hardware

NeatAMP amplifier design process is logged into a DIYAudio thread where schematics and
PCB can be found :

>   <https://www.diyaudio.com/forums/class-d/325826-design-log-neat-2x170w-i2s-i2c-controlled-integrated-dsp-amp-tas3251.html>

A basic User Interface made of a rotary encoder and some LEDs could be added to
NeatAMP main board. Connection diagram can be found in /Hardware folder.
Software could run without it as most command are available via the serial monitor.

### Software environment

Project made with ST CubeIDE V1.11.0, using CubeMX 6.7 and STM32CubeF0 Firmware Package
V1.11.3

### Software description

#### Main goal:

This software is designed to ease development of a target use case of the
TAS3251 without the TI EVM board (TI Evaluation Module Board). As the TAS3251 is
highly configurable and contains dozens of registers to setup, development will
mainly consist of designing and tuning the configuration of the DSP of the TAS,
which will involve many design/tests/updates cycles: changing many times the
dozens of registers of the DSP.

Designing and tuning DSP is easily feasible with TI Pure Path Console 3
software, but without a TI EVM board, PPC3 software can't connect to the TAS and
interact with it. PPC3 can produce an output file which content has to be
loaded into TAS registers. This is a text file (a C .h header file). The board
software allows to download this file via a serial link, then to process
it and store it in the board EEPROM. This software allows to easily load those files
, to store up to five of them in the board EEPROM and to choose the one to play in order
to do some comparisons and choices. This avoids having to recompile and upload your code
every time you want to change a DSP parameter, which is the case when the DSP parameters
are stored in your source code as data.

###### File handling by the software

.h files produced by PPC3 contains 5 main blocks, only 3 are used:

-   first block is C code example (ignored),

-   second block is configuration data for the TAS,

-   third block is DSP configuration data : FilterSet data for the TAS,

-   fourth block is swap command required by the DSP to activate the FilterSet
    data (ignored),

-   fifth block is additional configuration data.

The software allow to download in NetAMP the PPC3 generated files. While
downloading the file, the software catch the configuration data and store them
into one of the five Configuration data memory in the EEPROM. It also catch the
FilterSet data and store them in one off the five FilterSet data memory in the
EEPROM. Thus Configuration and FilterSet data are dissociated, this allows
software to play any combination on the two. Configuration data contains
clocking setup, audio stream parameter (I2S, TDM,…, \# of bits, etc…), error
detection, reset management, etc..., those are mainly in & out interface data
and DAC data. The FilterSet data contains DSP data: the various coefficients.

In order to catch Config and FilterSet data, the software parse on line by line
basis the incoming file while receiving it. It uses the tags described below to take actions.
Tags are group of 4 characters. An empty line is also interpreted as a specific tag.

Format of the.h file change with the kind of speaker configuration you choose in PPC3
(2.0 vs other). That added some complexification in the tags and the processing made by the software.

|    Tag    | Description                                    |
|-----------|------------------------------------------------|
| \@cfn     | Set the name of the Config preset in memory    |
| \@fln     | Set the name of the FilterSet preset in memory |
| //wr      | Start of FilterSet data block                  |
| //Co      | Start of FilterSet data block (Alias of //wr)  |
| YM h      | Start of Subwoofer on switch                   |
| //Sa      | Start of clock configuration data              |
| cfg\_     | Start of first Config data block               |
| /pro      | Confirm first Config data block detection      |
| { 0x      | Load a couple register / data                  |
| //sw      | Start of Block 4 (swap command)                |
| emptyline | End of FilterSet data Block                    |

#### Additional goals:

The software also provide additional housekeeping functions:

It manage volume, balance, mono/stereo, left-right swap and various TAS faults
described in TI datasheet.

It contain a basic UI with a rotary encoder, a push button and a multicolour
led.

A serial monitor implement various serial commands.

#### Typical usage of the software:

-   Straight from PPC3:
    -   Generate a .h file with PPC3 (use burst=1)
    -   Download it, tell NeatAMP to use it.

It also feasible to :

-   Edit the file
    -   before block 2 add \@cfn [name on 12 char] and \@fln [name on 12 char]
        tags to name the preset in board EEPROM (otherwise file name will be
        used)
    -   drop block 1 
    -   edit registers and biquad values
-   Download it, tell NeatAMP to use it.
    
    
-   Create a file from scratch :
    -   add @cfn and @fln tags to set the names
    -   add cfg\ tag on next line
    -   add /pro tag on the following line
    -   add you Config datas with { 0x00, 0x00 } format.
    -   add //wr tag on next line
    -   add you FilterSet Data with { 0x00, 0x00 } format.    
    -   Download it, tell NeatAMP to use it.

Tuning consist of :
-   Listen / measure
-   Adjust your setup in PPC3, regenerate the file
-   Copy/Paste the chunk of the new file in the previous
-   Download it,…and repeat this iterative design process

#### Remark

At first startup, or when an issue is encounter with EEPROM, EEPROM content is
reinitialized. By default the software contains no configuration data and is not
able to initialize the TAS3251.

NeatAmp should be loaded with a configuration file in order to start the TAS.

A default basic configuration file running I2S, 4 wires, stereo, 48kHz can be
found in /PPC3_file folder.

### Configuration required

The software communicate to the host PC with a serial link :

-   Serial link configuration: 115 200 bauds, 8bits, 1 Stop, No parity, No flow
    control

-   File transfer protocol : YMODEM

-   Terminal emulator tested: Ok with ExtraPuTTY
    ([www.extraputty.com](http://www.extraputty.com)) but KO with  
    Tera Term.

    -   Enable VT100

    -   Set screen size to 80\*40

### Commands and UI

###### Following commands can be passed over the serial link:

-   ?...Command list
-   m...Memory usage
-   l...Preset to load
-   p...Preset to play
-   e...Preset to erase
-   d...Download preset
-   s...Status
-   b...Out Crossbar L&R
-   c...Out Crossbar L&Sub
-   *...Volume default
-   +...Volume +
-   -...Volume -
-   f...Force EEPROM reinitialization


###### User Interface:

-   Rotary encoder turn left/right : Volume

-   Rotary encoder push : Save current volume in order to be applied at next
    startup, then flash Led.

-   Green Led: Amp Ok, output stage Off (toggle with ‘s’ command)

-   Blue Led : Amp Ok, output stage On (toggle with ‘s’ command). Default state

-   While playing :

    -   Green Led flash : Amp clipping.

    -   Green Led flash continuously : Amp continuously clipping, software will
        reduce volume slowly.

    -   Red Led flash:

        -   Transient power supply under voltage : software will reduce will
            reduce volume slowly.

        -   Chip over temperature Level 1 (125 °C) : software will reduce will
            reduce volume.

    -   Red Led Blink:

        -   Major under voltage detected by chip : software will shut down TAS.

        -   Major over temperature (155°C) detected by chip : software will shut
            down TAS.

        -   Power cycle required to restart the amp. Amp won’t restart if fault
            condition still present

-   On board Led blink : software is running.
