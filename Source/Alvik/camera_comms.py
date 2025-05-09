from esp_now_utils import *
from time import sleep_ms, ticks_diff, ticks_ms


def connect_to_camera(connection_timeout):
  broadcast_MAC = b'\xff' * 6
  local_MAC = get_mac_address()
  connected = False
  start_millis = ticks_ms()

  while not connected:
    if ticks_diff(ticks_ms(), start_millis) > connection_timeout:
      raise OSError(errno.ETIMEDOUT,'Timeout connecting to the camera module over ESP_NOW')

    print('Sending mac address...')
    esp.send(broadcast_MAC, 'ARDUINO_ALVIK_CAMERA_ROBOT_:D')

    mac, msg = esp.irecv(1000)
    if mac is not None:
      print(f'Got a message from MAC: {mac_address_to_string(mac)}')
      if msg == b'ARDUINO_ALVIK_CAMERA_FACEDETECTOR_:P\x00':
        print('ESP32S3-EYE CONNECTED')
        connected = True


def start_camera_comms(connection_timeout = 120000):
  try:
    deinit_esp_now()
  except Exception as e:
    print(e)
  
  init_esp_now()
  
  broadcast_MAC = b'\xff' * 6
  esp.add_peer(broadcast_MAC) # add broadcast MAC to allowed send peers

  connect_to_camera(connection_timeout)


def poll_camera(timeout_ms = 50, connection_timeout = 120000):
  mac, msg = esp.irecv(timeout_ms)
  if mac is not None:
    null_index = msg.find(b'\x00')
    dataStr = msg[:null_index].decode('utf-8')
    if dataStr == 'ARDUINO_ALVIK_CAMERA_FACEDETECTOR_:P':
      print('Camera requesting reconnect')
      connect_to_camera(connection_timeout)
    else:
      data = dataStr.split(',')
      horizontalRotation = float(data[0])
      verticalRotation = float(data[1])
      displacementSpeed = float(data[2])
      return [horizontalRotation, verticalRotation, displacementSpeed]
  return None
