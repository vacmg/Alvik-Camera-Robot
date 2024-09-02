from camera_comms import *
from time import sleep_ms

start_camera_comms()

while True:
  print(poll_camera())
  sleep_ms(100)