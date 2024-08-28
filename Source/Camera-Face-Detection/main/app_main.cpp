#include "driver/gpio.h"

#include "app_button.hpp"
#include "app_camera.hpp"
#include "app_lcd.hpp"
#include "app_led.hpp"
#include "app_face.hpp"
#include "app_processing.hpp"
#include "app_transmission.hpp"

extern "C" void app_main()
{
    QueueHandle_t xQueueFrame_0 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from appCamera to appFace
    QueueHandle_t xQueueFrame_1 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from appFace to appLcd

    SemaphoreHandle_t xMutexProcessing = xSemaphoreCreateMutex();
    QueueHandle_t xQueueMovementOrders = xQueueCreate(2, sizeof(movement_orders_t));

    AppButton *key = new AppButton();
    AppLED *led = new AppLED(GPIO_NUM_3, key);
    AppCamera *camera = new AppCamera(PIXFORMAT_RGB565, FRAMESIZE_240X240, 2, xQueueFrame_0);
    AppFace *face = new AppFace(key, xQueueFrame_0, xQueueFrame_1, xMutexProcessing);
    AppProcessing *processing = new AppProcessing(key, face, xMutexProcessing, xQueueMovementOrders);
    // AppTransmission *transmission = new AppTransmission(key,);
    AppLCD *lcd = new AppLCD(key, xQueueFrame_1);

    key->attach(face);
    key->attach(led);
    key->attach(lcd);

    lcd->run();
    face->run();
    processing->run();
    camera->run();
    key->run();
}
