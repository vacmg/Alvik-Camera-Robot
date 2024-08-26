#include "driver/gpio.h"

#include "app_button.hpp"
#include "app_camera.hpp"
#include "app_lcd.hpp"
#include "app_led.hpp"
#include "app_face.hpp"

extern "C" void app_main()
{
    QueueHandle_t xQueueFrame_0 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from camera to face
    QueueHandle_t xQueueFrame_1 = xQueueCreate(2, sizeof(camera_fb_t *)); // Union from face to lcd

    AppButton *key = new AppButton();
    AppCamera *camera = new AppCamera(PIXFORMAT_RGB565, FRAMESIZE_240X240, 2, xQueueFrame_0); // TODO use xQueueFrame_0
    AppFace *face = new AppFace(key, xQueueFrame_0, xQueueFrame_1);
    AppLCD *lcd = new AppLCD(key, xQueueFrame_1);
    AppLED *led = new AppLED(GPIO_NUM_3, key);

    key->attach(face);
    key->attach(led);
    key->attach(lcd);

    lcd->run();
    face->run();
    camera->run();
    key->run();
}
