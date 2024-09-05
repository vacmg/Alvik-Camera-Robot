#include <cstring>
#include "app_transmission.hpp"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"

#define TRANSMISSION_MIN_DELAY 100

static const char TAG[] = "App/Transmission";

static bool dest_mac_set = false;
static uint8_t dest_mac[ESP_NOW_ETH_ALEN];

AppTransmission::AppTransmission(uint32_t channel, QueueHandle_t queue_i_movement_orders) : queue_i_movement_orders(queue_i_movement_orders), channel(channel)
{
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    char buff[ESP_NOW_MAX_DATA_LEN+1];
    if (recv_info == nullptr || data == nullptr || len <= 0 || len > ESP_NOW_MAX_DATA_LEN)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    uint8_t * mac_addr = recv_info->src_addr;

    ESP_LOGI(TAG, "Received a ESP-NOW message from mac: " MACSTR " with len: %d", MAC2STR(mac_addr), len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG);

    memcpy(buff, data, len);
    buff[len] = '\0';

    if(strcmp(buff, "ARDUINO_ALVIK_CAMERA_ROBOT_:D") == 0)
    {
        ESP_LOGI(TAG, "Arduino Alvik MAC broadcast detected");
        esp_now_peer_info_t peer_info;
        memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
        peer_info.ifidx = static_cast<wifi_interface_t>(ESP_IF_WIFI_STA);

        for (int i = 0; i < ESP_NOW_ETH_ALEN; i++)
        {
            peer_info.peer_addr[i] = mac_addr[i];
            dest_mac[i] = mac_addr[i];
        }

        if(!dest_mac_set)
        {
            esp_now_add_peer(&peer_info);
            dest_mac_set = true;
            ESP_LOGI(TAG, "Destination MAC set to " MACSTR, MAC2STR(dest_mac));
        }

        ESP_ERROR_CHECK( esp_now_send(dest_mac, (uint8_t *)"ARDUINO_ALVIK_CAMERA_FACEDETECTOR_:P", sizeof("ARDUINO_ALVIK_CAMERA_FACEDETECTOR_:P")) );
        ESP_LOGI(TAG, "Connected to Arduino Alvik");
    }
}

static void task(AppTransmission *self)
{
    ESP_LOGD(TAG, "Start");

    dest_mac_set = false;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(self->channel, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );

    ESP_LOGI(TAG, "ESP-NOW ready");

    movement_orders_t orders;

    TickType_t last_wake_time = xTaskGetTickCount();
    while (true)
    {
        if (self->queue_i_movement_orders == nullptr)
        {
            break;
        }

        if (xQueueReceive(self->queue_i_movement_orders, &orders, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Received Movement - horizontalRotationAmount: %f", orders.horizontalRotationAmount);

            if (dest_mac_set && xTaskGetTickCount() - last_wake_time >= pdMS_TO_TICKS(TRANSMISSION_MIN_DELAY))
            {
                last_wake_time = xTaskGetTickCount();

                char buff[ESP_NOW_MAX_DATA_LEN+1];
                ESP_LOGD(TAG, "Sending movement orders to " MACSTR, MAC2STR(dest_mac));
                int size = std::max(snprintf(buff, ESP_NOW_MAX_DATA_LEN, "%f", orders.horizontalRotationAmount), ESP_NOW_MAX_DATA_LEN);
                ESP_ERROR_CHECK( esp_now_send(dest_mac, (uint8_t *)buff, size) );
            }
        }
    }

    esp_now_deinit();

    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_LOGD(TAG, "Stop");
    vTaskDelete(nullptr);
}

void AppTransmission::run()
{
    xTaskCreatePinnedToCore((TaskFunction_t)task, TAG, 4 * 1024, this, 5, nullptr, 1);
}
