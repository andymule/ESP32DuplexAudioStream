# mostly copy/pasted code for reading broadcast usp messages and sending them back to the ESP32
# TODO is this code fine?

import socket
import struct

import numpy as np
import time

import wave

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 4444))
# wrong: mreq = struct.pack("sl", socket.inet_aton("224.51.105.104"), socket.INADDR_ANY)
mreq = struct.pack("=4sl", socket.inet_aton("224.51.105.104"), socket.INADDR_ANY)

sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

cs = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# cs.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# cs.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

counter = 0

noise = []
while len(noise) < 800:
  noise.append(np.uint8(0))
  noise.append(np.uint8(20))

# This code can directly send generated noise to ESP32 instead
if False:
  print(bytearray(noise))
  while True:
    time.sleep(.1) #send 10 packets or 800 samples per second, 8khz 8bit audio wow
    cs.sendto(bytearray(noise), ('192.168.1.137', 4445))

#This code will loop and stream a test 8k8bit wav file
if True:
  wav = wave.Wave_read("8bit8k.wav")
  while True:
    if wav.tell() + 800 > wav.getnframes(): #might drop some frames, but is a test
      wav.rewind()
    wavbytes = wav.readframes(800)
    print(wav.tell())
    time.sleep(.1) #send 10 packets or 800 samples per second, 8khz 8bit audio wow
    cs.sendto(wavbytes, ('192.168.1.137', 4445))

# network loopback to ESP32
while True:
  a = sock.recv(1600)
  cs.sendto(a, ('192.168.1.137', 4445))
  # print(len(a))
  print(counter)
  counter += 1
  if counter % 10 == 0:
    print("+SECOND")
