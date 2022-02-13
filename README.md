# 60Hz
Measure and record 60 Hz power line frequency using GPS 1 pps as a reference.

Use an Arduino Teensy to accurately measure the 60 Hz AC powerline frequency using 1 pps calibration from a GPS and 60 Hz interrupts from an AC transformer (which also powers the system).
A Raspberry Pi Zero W running Python reads the serial data stream from the Teensy every 5 s and appends it to a log file; it also outputs to a terminal.

A 9 VAC 300 mA transformer powers the system. The transformer powers a bridge rectifier, filter capacitor and simple DCDC converter to deliver 5 V to the Raspberry Pi and the Teensy. Thje PCB link between the USB and 5 V pin on the Teensy is cut to allow the Teensy to be programmed via USB even while the system is powered.

A 2-stage R.C filter with a 3.3V zener clamp is driven from one AC terminal of the transformer. The other is connected to DC GND via 1 kohm. This is important as while the AC waveform is less than the bridge rectifier output, the common-mode level of the transformer is undefined. The resultant signal (looks like a smoothed half-wave rectified signal) drives the comparator in the Teensy which squares it up (with hysteresis) and drives the Teensy's FTM (Flex Timer) module interrupts. These are counted against the Teensy's internal 48 MHz crystal.

Separately, a precise 1 pulse-per-second (pps) signal from the GPS (about 20 ns jitter) also drives interrupts to another FTM channel. These 1 Hz interrupts calibrate the 48 MHz clock to give a precsise timebase. A slow filter ( 16 s time constant) filters this. If the 1 pps is lost, the last good calibration is used until the 1 pps is restored. 

The Teensy outputs serial ASCII data on USB and its serial port. Each line contains the number of 48 MHz clocks, number of power line cycles, calculated 60Hz frequency, calibrated 48 MHz, cumulated cycles, filter and 60 Hz phase information in response to a 'request' (a single blank line). If the 'request' contains any additional character, the Teensy outputs a header and resets the cumulative counter.

The Teensy's serial port is connected to the Raspberry Pi. A small Python program sends a request every 5 s (based on the Raspberry Pi's clock), and logs the time, Teensy's output and a simple ASCII plot to the terminal. Data is also appended to a (networked) log file.

The Pi's clock is only used to timestamp the data; the precision of 60 Hz measurement comes from the 1 pps GPS signal. Care is taken in the interrupt and processing routines that NO 60 Hz cycles are lost. This can result in some 5 s intervals containing 299, 300 or 301 cycles. Irrespectively, the 60 Hz calculation is correct over that precise number of cycles.


The initial Teensy output looks like this:
2022-02-12 21:01:50.072 #
2022-02-12 21:01:50.072 #    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz
2022-02-12 21:01:50.151    4002125,      5, 59.966893838,  47999001,         5,  16,   122, 118
2022-02-12 21:01:55.005  232924802,    291, 59.966602440,  47999000,       296,  16,    62,  10
2022-02-12 21:02:00.005  240125542,    300, 59.967381562,  47999000,       596,  16,     3,  12
2022-02-12 21:02:05.005  240109872,    300, 59.971295141,  47999000,       896,  16,   311,  14
2022-02-12 21:02:10.005  239281816,    299, 59.978235036,  47999000,      1195,  16,   272,   3
2022-02-12 21:02:15.005  240051332,    300, 59.985921261,  47999001,      1495,  16,   247,   4
2022-02-12 21:02:20.005  240035730,    300, 59.989820266,  47999001,      1795,  16,   228,   5

The first few measurements are not over 5 s as the Python program synchs to 5 s intervals.

The R Pi uses systemd to mount a network recorder drive, and to start the 60Hz service. TMUX on the Pi doesn't work with a simple systemd service, so is wrapper in a shell script.


Here's an example of the data also showing the plot:
This correlates precisely with University of Tennessee's FNET service at http://fnetpublic.utk.edu/tabledisplay.html#WECC 

2022-02-12 22:03:55.005  239995033,    300, 59.999993000,  47999001,    223465,  16,   169,   7, -·························/·························+
2022-02-12 22:04:00.005  239997705,    300, 59.999326244,  47999002,    223765,  16,   168,   7, -························/0·························+
2022-02-12 22:04:05.005  239999145,    300, 59.998966246,  47999002,    224065,  16,   166,   7, -························|0·························+
2022-02-12 22:04:10.005  240002613,    300, 59.998099271,  47999002,    224365,  16,   162,   7, -·······················/·0·························+
2022-02-12 22:04:15.005  240023214,    300, 59.992948432,  47999001,    224665,  16,   150,   8, -··················/······0·························+
2022-02-12 22:04:20.005  240031158,    300, 59.990964173,  47999002,    224965,  16,   134,   8, -················/········0·························+
2022-02-12 22:04:25.005  240037041,    300, 59.989492622,  47999001,    225265,  16,   115,   9, -··············/··········0·························+
2022-02-12 22:04:30.005  240047995,    300, 59.986755149,  47999001,    225565,  16,    91,   9, -············/············0·························+
2022-02-12 22:04:35.005  240059397,    300, 59.983905983,  47999001,    225865,  16,    62,  10, -·········/···············0·························+
2022-02-12 22:04:40.005  240028170,    300, 59.991710973,  47999002,    226165,  16,    47,  11, -·················\·······0·························+
2022-02-12 22:04:45.005  240006390,    300, 59.997153826,  47999001,    226465,  16,    42,  11, -······················\··0·························+
2022-02-12 22:04:50.005  239998958,    300, 59.999012996,  47999002,    226765,  16,    40,  11, -························\0·························+
2022-02-12 22:04:55.005  240000788,    300, 59.998555505,  47999002,    227065,  16,    37,  11, -························|0·························+
2022-02-12 22:05:00.005  239995227,    300, 59.999945749,  47999002,    227365,  16,    37,  11, -·························\·························+
2022-02-12 22:05:05.005  239995759,    300, 59.999812747,  47999002,    227665,  16,    37,  11, -·························|·························+
2022-02-12 22:05:10.005  240001501,    300, 59.998376010,  47999001,    227965,  16,    34,  11, -·······················/·0·························+
2022-02-12 22:05:15.005  239989974,    300, 60.001257803,  47999001,    228265,  16,    36,  11, -·························0\························+
2022-02-12 22:05:20.005  239963438,    300, 60.007894203,  47999002,    228565,  16,    50,  11, -·························0·······\·················+
2022-02-12 22:05:25.005  239950235,    300, 60.011194821,  47999001,    228865,  16,    71,  10, -·························0··········\··············+
2022-02-12 22:05:30.005  239959655,    300, 60.008838986,  47999001,    229165,  16,    86,  10, -·························0········/················+
2022-02-12 22:05:35.005  239963278,    300, 60.007932964,  47999001,    229465,  16,   101,   9, -·························0·······/·················+
2022-02-12 22:05:40.005  239947895,    300, 60.011780057,  47999001,    229765,  16,   122,   8, -·························0···········\·············+
2022-02-12 22:05:45.005  239929564,    300, 60.016365053,  47999001,    230065,  16,   151,   7, -·························0···············\·········+
2022-02-12 22:05:50.005  239919301,    300, 60.018933616,  47999002,    230365,  16,   185,   6, -·························0··················\······+
2022-02-12 22:05:55.005  239916644,    300, 60.019598307,  47999002,    230665,  16,   221,   5, -·························0···················\·····+
2022-02-12 22:06:00.005  239934105,    300, 60.015230432,  47999002,    230965,  16,   248,   4, -·························0··············/··········+
2022-02-12 22:06:05.005  239971193,    300, 60.005954965,  47999002,    231265,  16,   259,   4, -·························0·····/···················+
2022-02-12 22:06:10.005  239984642,    300, 60.002592166,  47999002,    231565,  16,   263,   4, -·························0··/······················+
2022-02-12 22:06:15.005  239961928,    300, 60.008271812,  47999002,    231865,  16,   278,   3, -·························0·······\·················+
