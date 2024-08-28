#pragma once

#include "__base__.hpp"
#include "app_button.hpp"
#include "app_face.hpp"

class AppProcessing
{
private:
    AppButton *key;
public:
    AppFace *face;
    SemaphoreHandle_t mutex;
    QueueHandle_t queue_o_movement_orders;

    explicit AppProcessing(AppButton *key,
                  AppFace *face,
                  SemaphoreHandle_t mutex = nullptr,
                  QueueHandle_t queue_o_movement_orders = nullptr);

    void run();
};
