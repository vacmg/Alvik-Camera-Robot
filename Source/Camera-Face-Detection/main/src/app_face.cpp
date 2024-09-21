#include "app_face.hpp"

#include <list>

#include "esp_log.h"
#include "esp_camera.h"

#include "dl_image.hpp"
#include "fb_gfx.h"

#include "who_ai_utils.hpp"

static const char TAG[] = "App/Face";

#define RGB565_MASK_RED 0xF800
#define RGB565_MASK_GREEN 0x07E0
#define RGB565_MASK_BLUE 0x001F

#define FRAME_DELAY_NUM 16

static void rgb_print(camera_fb_t *fb, uint32_t color, const char *str)
{
    fb_gfx_print(fb, (fb->width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(camera_fb_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}

AppFace::AppFace(AppButton *key,
                 QueueHandle_t queue_i,
                 QueueHandle_t queue_o,
                 QueueHandle_t queue_o_movement_orders,
                 void (*callback)(camera_fb_t *)) : Frame(queue_i, queue_o, callback),
                                                    key(key),
                                                    detector(0.3F, 0.3F, 10, 0.3F),
                                                    detector2(0.4F, 0.3F, 10),
                                                    queue_o_movement_orders(queue_o_movement_orders),
                                                    switch_on(false)
{

}

void AppFace::update()
{
    // Parse key
    if (this->key->pressed > BUTTON_IDLE)
    {
        if (this->key->pressed == BUTTON_MENU)
        {
            this->switch_on = (this->key->menu == MENU_FACE_RECOGNITION);
            ESP_LOGD(TAG, "%s", this->switch_on ? "ON" : "OFF");
        }
    }
}

static double fmap(double value, double in_min, double in_max, double out_min, double out_max)
{
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void task(AppFace *self)
{
    ESP_LOGD(TAG, "Start");
    camera_fb_t *frame = nullptr;

    while (true)
    {
        if (self->queue_i == nullptr)
            break;

        if (xQueueReceive(self->queue_i, &frame, portMAX_DELAY))
        {
            if (self->switch_on)
            {
                std::list<dl::detect::result_t>& detect_candidates = self->detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t>& detect_results = self->detector2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);

                if(self->queue_o_movement_orders && !detect_results.empty()) // Process the detection results and send the movement orders
                {
                    int32_t left_offset = static_cast<int32_t>(frame->width); // The minimum value, to be override by the first detection
                    int32_t right_offset = 0; // The maximum value, to be override by the first detection
                    int32_t top_offset = static_cast<int32_t>(frame->height); // The minimum value, to be override by the first detection
                    int32_t bottom_offset = 0; // The maximum value, to be override by the first detection

                    enum box_offset {left_up_x = 0, left_up_y, right_down_x, right_down_y};

                    for(auto data : detect_results)
                    {
                        ESP_LOGD(TAG, "box data: %d, %d, %d, %d", data.box[left_up_x], data.box[left_up_y], data.box[right_down_x], data.box[right_down_y]);

                        if(data.box[left_up_x] < left_offset)
                        {
                            left_offset = data.box[left_up_x];
                        }
                        if(data.box[left_up_y] < top_offset)
                        {
                            top_offset = data.box[left_up_y];
                        }
                        if(data.box[right_down_x] > right_offset)
                        {
                            right_offset = data.box[right_down_x];
                        }
                        if(data.box[right_down_y] > bottom_offset)
                        {
                            bottom_offset = data.box[right_down_y];
                        }
                    }

                    double right_proportion = static_cast<double>(right_offset) / frame->width;
                    double left_proportion = static_cast<double>(left_offset) / frame->width;
                    double top_proportion = static_cast<double>(top_offset) / frame->height;
                    double bottom_proportion = static_cast<double>(bottom_offset) / frame->height;

                    uint32_t area = (right_offset - left_offset) * (bottom_offset - top_offset);
                    double area_proportion = static_cast<double>(area) / (frame->width * frame->height);

                    ESP_LOGW(TAG, "area: %lu, area_proportion: %f,\nleft_proportion: %f, right_proportion: %f, top_proportion: %f, bottom_proportion: %f,\nleft_offset: %lu, right_offset: %lu, top_offset: %lu, bottom_offset: %lu",
                             area, area_proportion, left_proportion, right_proportion, top_proportion, bottom_proportion, left_offset, right_offset, top_offset, bottom_offset);

                    #define TARGET_HORIZONTAL_EXCLUSION_PROPORTION 0.3
                    #define MAX_HORIZONTAL_ROTATION_MOVEMENT 30.0
                    #define MIN_HORIZONTAL_ROTATION_MOVEMENT 1.0

                    static movement_orders_t movementOrders = {};

                    if(left_proportion < TARGET_HORIZONTAL_EXCLUSION_PROPORTION)
                    {
                        movementOrders.horizontalRotationAmount = fmap(left_proportion, 0, TARGET_HORIZONTAL_EXCLUSION_PROPORTION, -MAX_HORIZONTAL_ROTATION_MOVEMENT, -MIN_HORIZONTAL_ROTATION_MOVEMENT);
                    }
                    else if(right_proportion > 1 - TARGET_HORIZONTAL_EXCLUSION_PROPORTION)
                    {
                        movementOrders.horizontalRotationAmount = fmap(right_proportion, 1 - TARGET_HORIZONTAL_EXCLUSION_PROPORTION, 1, MIN_HORIZONTAL_ROTATION_MOVEMENT, MAX_HORIZONTAL_ROTATION_MOVEMENT);
                    }
                    else
                    {
                        movementOrders.horizontalRotationAmount = 0;
                    }

                    // VERTICAL ROTATION UNTESTED
                    #define TARGET_VERTICAL_EXCLUSION_PROPORTION 0.20
                    #define MAX_VERTICAL_ROTATION_MOVEMENT 30.0
                    #define MIN_VERTICAL_ROTATION_MOVEMENT 1.0

                    if(top_proportion < TARGET_VERTICAL_EXCLUSION_PROPORTION)
                    {
                        movementOrders.verticalRotationAmount = fmap(top_proportion, 0, TARGET_VERTICAL_EXCLUSION_PROPORTION, -MAX_VERTICAL_ROTATION_MOVEMENT, -MIN_VERTICAL_ROTATION_MOVEMENT);
                    }
                    else if(bottom_proportion > 1 - TARGET_VERTICAL_EXCLUSION_PROPORTION)
                    {
                        movementOrders.verticalRotationAmount = fmap(bottom_proportion, 1 - TARGET_VERTICAL_EXCLUSION_PROPORTION, 1, MIN_VERTICAL_ROTATION_MOVEMENT, MAX_VERTICAL_ROTATION_MOVEMENT);
                    }
                    else
                    {
                        movementOrders.verticalRotationAmount = 0;
                    }

                    #define TARGET_AREA_PROPORTION 0.15
                    #define TARGET_AREA_PROPORTION_TOLERANCE 0.05
                    #define MAX_FORWARD_MOVEMENT 20.0
                    #define MIN_FORWARD_MOVEMENT 0.5
                    #define MAX_AREA_PROPORTION ((2*TARGET_AREA_PROPORTION) + TARGET_AREA_PROPORTION_TOLERANCE)

                    if(area_proportion < TARGET_AREA_PROPORTION - TARGET_AREA_PROPORTION_TOLERANCE)
                    {
                        movementOrders.forwardDisplacementAmount = fmap(area_proportion, 0, TARGET_AREA_PROPORTION - TARGET_AREA_PROPORTION_TOLERANCE, MAX_FORWARD_MOVEMENT, MIN_FORWARD_MOVEMENT);
                    }
                    else if(area_proportion > TARGET_AREA_PROPORTION + TARGET_AREA_PROPORTION_TOLERANCE)
                    {
                        movementOrders.forwardDisplacementAmount = fmap(area_proportion, TARGET_AREA_PROPORTION + TARGET_AREA_PROPORTION_TOLERANCE, MAX_AREA_PROPORTION, -MIN_FORWARD_MOVEMENT, -MAX_FORWARD_MOVEMENT);
                    }
                    else
                    {
                        movementOrders.forwardDisplacementAmount = 0;
                    }

                    xQueueSend(self->queue_o_movement_orders, &movementOrders, portMAX_DELAY);
                }

                if (!detect_results.empty())
                {
                    draw_detection_result((uint16_t *)frame->buf, frame->height, frame->width, detect_results);
                }
            }

            if (self->queue_o)
                xQueueSend(self->queue_o, &frame, portMAX_DELAY);
            else
                self->callback(frame);
        }
    }
    ESP_LOGD(TAG, "Stop");
    vTaskDelete(nullptr);
}

void AppFace::run()
{
    xTaskCreatePinnedToCore((TaskFunction_t)task, TAG, 8 * 1024, this, 5, nullptr, 1);
}