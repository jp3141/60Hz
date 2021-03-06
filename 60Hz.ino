// Teensy 3.2 FTM capture -  pin 20 and pin 16
// Clock = 48 MHz
// Put 60 Hz on pin 20 (FTM0),  1 Hz on pin 16 (FTM1)
// comparator input pin 23, output pin 10 -> pin 20

/* Teensy Connections
 * GND
 *  0 RX1 to R Pi
 *  1 TX1 to R Pi
 * 10 Comparator output to #20, FTM1 input
 * GND
 * 16 GPS 1 pps input to comparator
 * 23 Comparator input
 * 3.3V output 
 * GND
 * VIN 5 V -- cut VIN/USB connection
 * 
 */

#define Target_Hz 60
#define LED 13

// all connected to +3.3V
#define BLULED 7
#define GRNLED 8
#define REDLED 9

// Green LED on at 1 Hz edge; off ~100 ms later
// Red LED on if Serial request, off @ write out
// Blue on at Clear command, off 300 ms later


// Starting from http://shawnhymel.com/681/learning-the-teensy-lc-input-capture/

//FTM0
volatile uint32_t Cycles60Hz=0, Cycles60Hz_Complete=0;
volatile uint32_t Ticks60Hz_Complete, Cycles60Hz_Total = 0;
volatile uint8_t Flag60Hz = 0;
String COMMAND;

//FTM1
volatile int32_t F_BUS_Cal = (F_BUS); // needs to be signed so filtering works correctly
volatile int iFilter = 1;  // start with fast time constant
volatile int32_t Overflow_1Hz_Counter = 0;


elapsedMicros PhaseMicros;
volatile uint32_t Phase;
volatile uint32_t PhaseMicrosARM, PhaseMicrosARM0=0; // From internal counter; not really microseconds. 


void setup() {
  Serial.begin(9600); // while (!Serial); Serial.println("Starting...");
  Serial1.begin(57600); while (!Serial1); Serial.println("Starting...");
  Serial1.println("\n#    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz");

  tone(2, F_BUS/1000, 0);   // test60 Hz on pin 2
  pinMode(LED, OUTPUT);    digitalWrite(LED,   HIGH);   
  pinMode(REDLED, OUTPUT); digitalWrite(REDLED, LOW); delay(200); digitalWrite(REDLED, HIGH);
  delay(200);
  pinMode(GRNLED, OUTPUT); digitalWrite(GRNLED, LOW); delay(200); digitalWrite(GRNLED, HIGH);
  delay(200);
  pinMode(BLULED, OUTPUT); digitalWrite(BLULED, LOW); delay(200); digitalWrite(BLULED, HIGH);
  delay(200);

  ComparatorSetup(32); // about 1.5 V
  
  ARM_DEMCR |= ARM_DEMCR_TRCENA;
  ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;


  // The order of setting the TPMx_SC, TPMx_CNT, and TPMx_MOD
  // seems to matter. You must clear _SC, set _CNT to 0, set _MOD
  // to the desired value, then you can set the bit fields in _SC.

  FTM0_SC = FTM1_SC = 0;

  // Reset FTMx_CNT counter 
  FTM0_CNTIN = FTM1_CNTIN = 0;
  FTM0_CNT = FTM1_CNT = 0;

  // Set overflow value (modulo)
  FTM0_MOD = FTM1_MOD = 0xFFFF;

  // Set FTMx_SC register
  // Bits | Va1ue | Description
  //  8   |    0  | DMA: Disable DMA
  //  7   |    1  | TOF: Clear Timer Overflow Flag
  //  6   |    1  | TOIE: Enable Timer Overflow Interrupt
  //  5   |    0  | CPWMS: TPM in up counting mode
  // 4-3  |   01  | CMOD: Counter incrememnts every TPM clock
  // 2-0  |  000  | PS: Prescale = 1
  FTM0_SC = 0b011001000;
  FTM1_SC = 0b011001000;


  // Set TPMx_CnSC register
  // As per the note on p. 575, must disable the channel
  // first before switching channel modes. Also introduce
  // a 1 us delay to allow the new value to take.
  // Bits | Va1ue | Description
  //  7   |    0  | CHF: Do nothing
  //  6   |    1  | CHIE: Enable Channel Interrupt
  // 5-4  |   00  | MS: Input capture
  // 3-2  |   01  | ELS: Capture on rising  edge
  //  1   |    0  | Reserved
  //  0   |    0  | DMA: Disable DMA
  FTM0_C5SC = FTM1_C0SC = 0;
  delayMicroseconds(1);
  FTM0_C5SC = FTM1_C0SC = 0b01000100;

  // Set PORTD_PCR5 register (Teensy 3.2 - pin 20)
  // Bits | Value | Description
  // 10-8 |   100 | MUX: Alt 4 attach to FTM0_CH5
  //  7   |     0 | Reserved
  //  6   |     0 | DSE: Low drive strength
  //  5   |     0 | Reserved
  //  4   |     0 | PFE: Disable input filter
  //  3   |     0 | Reserved
  //  2   |     0 | SRE: Fast slew rate if output
  //  1   |     0 | PE: Enable pull-up/down
  //  0   |     0 | PS: Internal pull-up
  //  PORTD_PCR5 = 0b10000000000;
  *portConfigRegister(20) = PORT_PCR_MUX(4);
  NVIC_SET_PRIORITY(IRQ_FTM0, 32);  // higher priority for 60 Hz counter

  *portConfigRegister(16) = PORT_PCR_MUX(3);  // pin 16, A2
  NVIC_SET_PRIORITY(IRQ_FTM1, 64);

  NVIC_ENABLE_IRQ(IRQ_FTM0); // 60 Hz
  NVIC_ENABLE_IRQ(IRQ_FTM1); //  1 Hz

  //OSC0_CR = 4;

  // Skip 1st result; note doesn't require 1 Hz
  Flag60Hz = 1; // flag request
  while (Flag60Hz);  // loop until ISR clears this
  Cycles60Hz_Total = 0;


} // Setup


void loop() {
  if ((Overflow_1Hz_Counter > F_BUS/(1<<16)/20) && (Overflow_1Hz_Counter < 700))  digitalWrite(GRNLED, HIGH); //in case 1 Hz while overflow still at 732
  //if (Overflow_1Hz_Counter > F_BUS/(1<<16)/20) digitalWrite(GRNLED, HIGH); 
  if (Cycles60Hz_Complete > Target_Hz/3)        digitalWrite(BLULED, HIGH);

  if ( Serial.available() > 0 ) {
    Flag60Hz = 1; // flag request as soon as a character appears
    digitalWrite(REDLED, LOW);
    while (Flag60Hz);  // loop until ISR clears this
    COMMAND = Serial.readStringUntil('\n'); // read rest of string
    if (COMMAND.length() > 0) { // clearing command
      Cycles60Hz_Total = 0;
      Ticks60Hz_Complete = 1;
      Cycles60Hz_Complete = 0;
      digitalWrite(BLULED, LOW);
      Serial.println ("#" + COMMAND + "\n#    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz");
      Serial1.println("#" + COMMAND + "\n#    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz");
    } else
    PrintHz();
  }

  if ( Serial1.available() > 0 ) {
    Flag60Hz = 1; // flag request as soon as a character appears
    digitalWrite(REDLED, LOW);
    while (Flag60Hz);  // loop until ISR clears this
    COMMAND = Serial1.readStringUntil('\n');
    if (COMMAND.length() > 0) { // clearing command
      Cycles60Hz_Total = 0;
      Ticks60Hz_Complete = 1;
      Cycles60Hz_Complete = 0;
      digitalWrite(BLULED, LOW);
      Serial.println ("#" + COMMAND + "\n#    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz");
      Serial1.println("#" + COMMAND + "\n#    Ticks, Cycles,    Frequency,      FBUS, Total Cyc, Flt, Phase, 1Hz");
    } else
    PrintHz();
  }
  
  if (Cycles60Hz >= Target_Hz*20-1) { // Force a read at least every 20 s
    // 20 s delay; counters would overflow; need to read at least every 20 s
    // -1 since this loop will wait until the NEXT cycle completes
    Flag60Hz = 1; // flag request
    while (Flag60Hz);  // loop until ISR clears this
    PrintHz();
  }

} // loop

FASTRUN void PrintHz(void) {
    uint32_t xTicks60Hz_Complete;
    uint32_t xCycles60Hz_Complete;
    int32_t xF_BUS_Cal;
    uint32_t xCycles60Hz_Total;

    noInterrupts(); // get a self-consistent set of values from 60 Hz interrupt
    xTicks60Hz_Complete = Ticks60Hz_Complete;
    xCycles60Hz_Complete = Cycles60Hz_Complete;
    xF_BUS_Cal = F_BUS_Cal;
    xCycles60Hz_Total = Cycles60Hz_Total;
    interrupts(); // get a self-consistent set of values

  Serial.printf ("%10lu, %6i, %12.9f, %9ld, %9lu,  %2i,   %3lu, %3i\n", 
  xTicks60Hz_Complete, xCycles60Hz_Complete, (double) xF_BUS_Cal / xTicks60Hz_Complete * (double) xCycles60Hz_Complete, xF_BUS_Cal, xCycles60Hz_Total, iFilter, Phase, 
  (iFilter == 16) ? Overflow_1Hz_Counter : -1 );
  Serial1.printf("%10u, %6i, %12.9f, %9ld, %9lu,  %2i,   %3lu, %3i\n", 
  xTicks60Hz_Complete, xCycles60Hz_Complete, (double) xF_BUS_Cal / xTicks60Hz_Complete * (double) xCycles60Hz_Complete, xF_BUS_Cal, xCycles60Hz_Total, iFilter, Phase, 
  (iFilter == 16) ? Overflow_1Hz_Counter : -1 );
  // overflow is 48M/2^16 = 1.365 ms
  delay(50); // to flash the RED
  digitalWrite(REDLED, HIGH);


}


FASTRUN void ftm0_isr(void) { // 60 Hz
  uint32_t C5V_60Hz;
  static uint32_t start_C5V_60Hz = 0;
  static uint32_t Overflow_60Hz_Counter = 0;
  static uint32_t Ticks60Hz = 0;  // ticks for this 60 Hz cycle

  if ( FTM0_SC & (1 << 7) ) {
    FTM0_SC &= ~(1 << 7);   // T3.2
    Overflow_60Hz_Counter++;
  }

  // If here from the input capture, clear channel flag.
  if ( FTM0_C5SC & (1 << 7) ) {
    FTM0_C5SC &= ~(1 << 7);    // T3.2
    if (Overflow_60Hz_Counter < ((F_BUS) >> 16) / Target_Hz - 2) return; // ignore if edges are too close
      // get ~ 12.2 overflows/60 Hz
    Cycles60Hz_Total++; // got a 60 Hz edge; total lifetime 60 Hz edges
    Cycles60Hz++;       // number of edges in this group

    // Retrieve the counter value; include overflows.
    C5V_60Hz = FTM0_C5V;
    Ticks60Hz += (C5V_60Hz - start_C5V_60Hz) + (Overflow_60Hz_Counter << 16);

    start_C5V_60Hz = C5V_60Hz;
    Overflow_60Hz_Counter = 0;
    PhaseMicros = 0;
    PhaseMicrosARM0 = ARM_DWT_CYCCNT;

    // now Cycles60Hz and Ticks60Hz are consistent. 
    // overflows can still accumulate

    if(Flag60Hz) { // flag set by main loop to request data
      Ticks60Hz_Complete = Ticks60Hz; // FTM counts
      Ticks60Hz = 0;
      Cycles60Hz_Complete = Cycles60Hz;
      Cycles60Hz = 0;
      Flag60Hz = 0; // handshake
    }    
  }
}

FASTRUN void ftm1_isr(void) { // 1 Hz; updates F_BUS_Cal with # FBUS cycles per 1 Hz; filtered
  uint32_t C0V_1Hz;
  static uint32_t start_C0V_1Hz = 0;
  static int32_t F_BUS_1pps = (F_BUS);  // initialize to FBUS

  if (FTM1_SC & (1 << 7) ) { 
    FTM1_SC &= ~(1 << 7);   // T3.2
    Overflow_1Hz_Counter++; // would overflow in 67 days
    if (Overflow_1Hz_Counter > (F_BUS >> 16) + 1) {
      iFilter = 1 ;  // reset filter if 1 pps lost
      digitalWrite(LED, HIGH); // LED on ==> missing 1 Hz
    }
  }

  // If from input capture, clear flag.
  if (FTM1_C0SC & (1 << 7) ) { // 1 Hz pin interrupt 
    FTM1_C0SC &= ~(1 << 7);    
    C0V_1Hz = FTM1_C0V;
    digitalWrite(GRNLED, LOW);  // Turn on Green LED at 1 Hz edge

    F_BUS_1pps = (C0V_1Hz - start_C0V_1Hz) + (Overflow_1Hz_Counter << 16);

    if (abs(F_BUS_1pps - (F_BUS)) < 10000) {  // 10000 clocks is 0.01 % of 48 MHz  
    //  Makes truncation round to closest, no matter if + or - value
      if (F_BUS_1pps != F_BUS_Cal) {
        if (F_BUS_1pps > F_BUS_Cal) 
          F_BUS_Cal += (F_BUS_1pps - F_BUS_Cal + iFilter - 1)/iFilter; // rounds up
        else 
          F_BUS_Cal += (F_BUS_1pps - F_BUS_Cal - iFilter + 1)/iFilter; // rounds towards neg
      } //  !=  
      if (iFilter < 16) iFilter++;   // slowly increase when good 1 pps arrives
      digitalWrite(LED, LOW);   
      // Serial.printf("%i, %i, %i, %i\n", F_BUS_Cal, F_BUS_1pps, iFilter, F_BUS_Cal- F_BUS_1pps);
    } else { // Error too large; reset filter
      iFilter = 1 ;  // reset filter if 1 pps lost
      digitalWrite(LED, HIGH);     
    }
    start_C0V_1Hz = C0V_1Hz; // save for next 1 Hz pulse
    Phase = PhaseMicros * 360 * Target_Hz / 1000000;
    Phase = (ARM_DWT_CYCCNT-PhaseMicrosARM0) * 360LL * Target_Hz / F_BUS; // final result is 0..359; intermediate could be 64 bit (48M*360), so LL forces 64 bit
    Overflow_1Hz_Counter = 0;


  }
}

void ComparatorSetup(int threshold) { // threshold 0..63
  // Output is on Teensy pin 10 (TX2), chip pin 49; port C4
  // Input is pin A9 (Teensy pin 23, chip pin 45; port C2)
  // Reference is 6-bit internal DAC
  // DAC reference VIN1 = VREF (1.19 V), VIN2 = VDD (3.3)
  
  SIM_SCGC4 |= SIM_SCGC4_CMP; //Clock to Comparator
  CORE_PIN10_CONFIG = PORT_PCR_MUX(6); //Alternate function 6: Teensy pin10 = CMP1_OUT, PTC4

  //CMP1_CR0 = 0b00000000; // FILTER_CNT=0; HYSTCTR=0
  CMP1_CR0 = 0b01110011; // FILTER_CNT=7; HYSTCTR=11 (max)
  // hysteresis is higher if VIN common-mode = 0 V
  //CMP1_CR1 = 0b00010111; // SE=0, high power, COUTA, output pin, enable; mode #2A
  CMP1_CR1 = 0b00000111; // SE=0, low power, COUTA, output pin, enable; mode #2A; low power == slow
  // read CMP1_SCR LSB is analog comparator output state
  CMP1_DACCR = 0b11000000+threshold; // enable DAC, VIN2 selected (=VDD), reference = threshold*VDD/64 -- 19 suits X72
  if (threshold==0) CMP1_DACCR = 0; //Disable DAC ==> reference = 0 V
  //PTC2 (pin 23) is default CMP1_IN0
  CMP1_MUXCR = 0b01000111; // Input pins select; plus = IN0 (pin 23), neg = DAC (code 7) 
}

/*
 #!/usr/bin/env python3
import time
import sys
import os
import datetime as dt
import serial

from signal import signal, SIGINT
from sys import exit


def handler(signal_received, frame):
    print('\nCtrl+C detected.')
    TeensyFreq.close()
    LOGFile.close()
    exit(0)

signal(SIGINT, handler)

######################################### main ##################################

LineNum = 0
TeensyFreq = serial.Serial(baudrate=9600)

#TeensyFreq.port = '/dev/ttyS0'
TeensyFreq.port = '/dev/serial0'
TeensyFreq.timeout = 11   # for gate 9.6 s
TeensyFreq.open()
LOGName = "/home/pi/networkdrive/60Hz.log"
#LOGName = "60Hz.log"
LOGFile = open(LOGName, 'a', 1)  # buffered by line 

TeensyFreq.write(b' \n')  # write a space to make Teensy initialize

starttime = int((time.time()+4)/10)*10  # on whole 5 s intervale

while True:
  LineNum += 1

  TeensyFreq.write(b'\n')
  TimeStamp = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
  # FreqString=TeensyFreq.readline().strip().decode()
  FreqString=TeensyFreq.readline().decode("ascii", errors = "replace")[:-1]

  # print( dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3], FreqString)
  print(TimeStamp, FreqString)
  print(TimeStamp, FreqString, file = LOGFile)
  # print( dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3], FreqString, file=LOGFile)
  time.sleep(5.0 - ((time.time() - starttime) % 5.0))

  #LOGFile.flush()


TeensyFreq.close()
LOGFile.close()
print("Done")

 */
