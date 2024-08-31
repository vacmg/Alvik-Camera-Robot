#pragma once

#include "__base__.hpp"

class AppTransmission
{
public:
    QueueHandle_t queue_i_movement_orders;

    AppTransmission(QueueHandle_t queue_i_movement_orders = nullptr);

    void run();
};