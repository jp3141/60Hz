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
    TeensyFreq.reset_input_buffer()
    TeensyFreq.close()
    LOGFile.close()
    exit(0)

signal(SIGINT, handler)

######################################### main ##################################

LineNum = 0
TeensyFreq = serial.Serial(baudrate=57600)
Interval = 5.0 # time between frequency requests
#Interval = 2.0 # time between frequency requests

#TeensyFreq.port = '/dev/ttyS5'
TeensyFreq.port = '/dev/serial0'
#TeensyFreq.port = 'COM5'
TeensyFreq.timeout = 0.5
TeensyFreq.open()
TeensyCMD = b' \n'
LOGName = "60Hz.log"
#LOGName = "/home/pi/networkdrive/" + LOGName
LOGName = "/home/pi/60HzDrive/" + LOGName
LOGFile = open(LOGName, 'a', 1)  # buffered by line

TeensyFreq.reset_input_buffer()
TeensyFreq.reset_output_buffer()

# This is the header. includes a leading \n
TeensyFreq.write(b' \n')  # write a space to make Teensy initialize
TimeStamp = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] + ","
TeensyFreq.readline() # This is a blank line

F0 = 60.0
deltaF = 0.025
PlotSpan = 25
#print("# Plot resolution = %6.3f Hz                                                                    < %+5.3f Hz ..............0.............. %+5.3f Hz >" % (deltaF/PlotSpan, -deltaF, deltaF))
print("# Plot resolution = %6.3f Hz                                                                    < %+5.3f Hz ··············0·············· %+5.3f Hz >" % (deltaF/PlotSpan, -deltaF, deltaF))
#print("#                                                                                               < %+5.3f Hz ..............0.............. %+5.3f Hz >" % (-deltaF, deltaF))
#print("#                                                                                               <--------------------------------------------------->")

#LinePlot = "-" + PlotSpan * "." + "0" + PlotSpan * "." + "+"
LinePlot = "-" + PlotSpan * "·" + "0" + PlotSpan * "·" + "+"
TotalTimeError = 0.0
#lastStep = datetime.now()
lastPlotFreq=0


print(TimeStamp, '#')
print(TimeStamp, '#', file = LOGFile)

FreqString=TeensyFreq.readline().decode("ascii", errors = "replace")[:-1]

print(TimeStamp, FreqString)
print(TimeStamp, FreqString, file = LOGFile)

starttime = int((time.time()+(Interval-1))/10)*10  # on whole 5 s intervals

while True:
  LineNum += 1

  TeensyFreq.write(b'\n')
  TimeStamp = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] + ","

  FreqString=TeensyFreq.readline().decode("ascii", errors = "replace")[:-1]
#  print(TimeStamp, FreqString)
#  print(TimeStamp, FreqString, file = LOGFile)
  
  ThisFreq=float(FreqString.split(", ")[2])
  try:
    PlotFreq=round(PlotSpan*(float(ThisFreq)-F0)/deltaF)
  except ValueError:
    print("# Missing Data %s" % TimeStamp)
    #print("# Missing Data %s" % TimeStamp, file=LOGFile)
    LineNum -= 1
    TotalTimeError = 0
    continue

  PlotChar = "|"
  if (PlotFreq > lastPlotFreq):
    PlotChar = "\\"
  if (PlotFreq < lastPlotFreq):
    PlotChar = "/"
  lastPlotFreq = PlotFreq

  if (PlotFreq < -(PlotSpan+1)):
    PlotFreq = -(PlotSpan+1)
    PlotChar = "<"
  if (PlotFreq > (PlotSpan+1)):
    PlotFreq = (PlotSpan+1)
    PlotChar = ">"
  #print("%4i, %9.4f " % (PlotFreq, CounterString), end='')

  ThisPlot=LinePlot[:(PlotSpan+PlotFreq+1)] + PlotChar + LinePlot[(PlotSpan+PlotFreq+2):]

#  deltaT = (ThisStep-lastStep).total_seconds()
#  lastStep = ThisStep
#  TotalTimeError += (float(CounterString)-F0)*deltaT/F0

#  print("%8d, %s, %11s, %10.6f"     % (LineNum, now, CounterString, TotalTimeError), file=LOGFile)

  print(TimeStamp, FreqString+",", ThisPlot)
  print(TimeStamp, FreqString, file = LOGFile)


# should not be any line available to read
  FreqString=TeensyFreq.readline().decode("ascii", errors = "replace")[:-1]
                                    # should get a timeout here; avoids resetting buffer and losing partial lines.
                                    # should never actually get a string
  if (FreqString != ""):            # but print anyway if one is read
    print(TimeStamp, "#" + FreqString[1:])
    print(TimeStamp, "#" + FreqString[1:], file = LOGFile)

  time.sleep(Interval - ((time.time() - starttime) % Interval))

  LOGFile.flush()


TeensyFreq.close()
LOGFile.close()
print("Done")
