#pragma once

#include "__base__.hpp"
#include "esp_now.h"

class AppTransmission
{
public:
    QueueHandle_t queue_i_movement_orders;
    uint32_t channel;

    AppTransmission(uint32_t channel, QueueHandle_t queue_i_movement_orders = nullptr);

    void run();
};