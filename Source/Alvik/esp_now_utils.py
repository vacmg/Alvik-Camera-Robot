import network
import espnow

sta = network.WLAN(network.STA_IF)
ap = network.WLAN(network.AP_IF)
esp = espnow.ESPNow()

def init_esp_now(wifi_channel = 1):
  sta.active(True)
  ap.active(False)
  
  while not sta.active():
    time.sleep(0.1)
  sta.disconnect()
  while sta.isconnected():
    time.sleep(0.1)
  
  sta.config(channel=wifi_channel)
  esp.active(True)

def deinit_esp_now():
  sta.active(False)
  esp.active(False)
  ap.active(False)

def get_mac_address():
  return sta.config('mac')

def mac_address_to_string(mac):
  return ':'.join('%02x' % b for b in mac)
