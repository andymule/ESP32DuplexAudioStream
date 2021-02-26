import socket
import struct

import numpy as np
import time

import wave
# import simpleaudio as sa

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 4444))
mreq = struct.pack("=4sl", socket.inet_aton("224.51.105.104"), socket.INADDR_ANY)

sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

cs = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

counter = 0

# only used if we want to send noise data as a test
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

# This code will stream a looping test 8k8bit wav file
if True:
  wav = wave.Wave_read("8bit8k.wav")
  while True:
    if wav.tell() + 800 > wav.getnframes(): #might drop some frames, but is a test
      wav.rewind()
    wavbytes = wav.readframes(800)
    # play_obj = sa.play_buffer(wavbytes, 1, 1, 8000) #uncomment to play test wav locally
    cs.sendto(wavbytes, ('192.168.1.137', 4445))
    time.sleep(.1) #send 10 packets or 800 samples per second, 8khz 8bit audio wow

# will take data from ESP and loopback back to it
while True:
  a = sock.recv(1600)
  cs.sendto(a, ('192.168.1.137', 4445))
  # print(len(a))
  print(counter)
  counter += 1
  if counter % 10 == 0:
    print("+SECOND")
