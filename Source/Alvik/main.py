from arduino_alvik import ArduinoAlvik
from camera_comms import *
from time import sleep_ms

alvik = ArduinoAlvik()
alvik.begin()

start_camera_comms()

while True:
  if alvik.is_on():
    rotation = poll_camera(1000)
    if rotation is not None:
      print(rotation)
      alvik.drive(0, rotation)
    else:
      print('Target lost')
      alvik.drive(0, 0)