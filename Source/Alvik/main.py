from arduino_alvik import ArduinoAlvik
from camera_comms import *
from time import sleep_ms, ticks_diff, ticks_ms


vertical_servo_position = 90

alvik = ArduinoAlvik()
alvik.begin()
alvik.set_servo_positions(vertical_servo_position, vertical_servo_position) # TODO only use one channel (decide which channel to use)

start_camera_comms()

last_tick_millis = ticks_ms()

def move_servo_at_speed(speed_dgps, upper_limit = 100, lower_limit = 80):
  global last_tick_millis
  global vertical_servo_position
  
  move_value = 1
  if speed_dgps == 0:
    return
  elif speed_dgps < 0:
    move_value = -1
    speed_dgps = -speed_dgps
  tick_period_ms = 1000/speed_dgps
  
  if ticks_diff(ticks_ms(), last_tick_millis) > tick_period_ms:
    last_tick_millis = ticks_ms()
    vertical_servo_position += move_value
    
    if vertical_servo_position > upper_limit:
      vertical_servo_position = upper_limit
    elif vertical_servo_position < lower_limit:
      vertical_servo_position = lower_limit
    
    alvik.set_servo_positions(vertical_servo_position, vertical_servo_position) # TODO only use one channel (decide which channel to use)


while True:
  if alvik.is_on():
    data = poll_camera(2500)
    if data is not None:
      horizontal_rotation = data[0]
      vertical_rotation = data[1]
      displacement_speed = data[2]
      print(f'horizontal_rotation: {horizontal_rotation}\tvertical_rotation: {vertical_rotation}\tdisplacement_speed: {displacement_speed}')
      
      alvik.drive(displacement_speed, horizontal_rotation)
      # alvik.drive(0, horizontal_rotation)
      move_servo_at_speed(vertical_rotation)
    else:
      print('Target lost')
      alvik.drive(0, 0)
      move_servo_at_speed(0)
