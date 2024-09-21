#define AUTO_ENABLE_FACE_RECOGNITION 0

#include "driver/gpio.h"
#include "esp_log.h"

#include "app_button.hpp"
#include "app_camera.hpp"
#include "app_lcd.hpp"
#include "app_led.hpp"
#include "app_face.hpp"
#include "app_transmission.hpp"

extern "C" void app_main()
{
    esp_log_level_set("camera", ESP_LOG_DEBUG);

    QueueHandle_t xQueueFrame_0 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from appCamera to appFace
    QueueHandle_t xQueueFrame_1 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from appFace to appLcd

    QueueHandle_t xQueueMovementOrders = xQueueCreate(2, sizeof(movement_orders_t));

    vTaskDelay(100 / portTICK_PERIOD_MS);
    AppButton *key = new AppButton();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    AppLED *led = new AppLED(GPIO_NUM_3, key);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    //AppCamera *camera = new AppCamera(PIXFORMAT_RGB565, FRAMESIZE_SVGA, 2, xQueueFrame_0);
    AppCamera *camera = new AppCamera(PIXFORMAT_RGB565, FRAMESIZE_240X240, 2, xQueueFrame_0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    AppFace *face = new AppFace(key, xQueueFrame_0, xQueueFrame_1, xQueueMovementOrders);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    AppTransmission *transmission = new AppTransmission(1, xQueueMovementOrders);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    AppLCD *lcd = new AppLCD(key, xQueueFrame_1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    key->attach(face);
    key->attach(led);
    key->attach(lcd);

    transmission->run();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    lcd->run();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    face->run();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    camera->run();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    key->run();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    #if AUTO_ENABLE_FACE_RECOGNITION
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        key->pressed = BUTTON_MENU;
        key->menu = MENU_FACE_RECOGNITION;
        key->notify();
        key->pressed = BUTTON_IDLE;
    #endif
    vTaskDelete(nullptr);
}
