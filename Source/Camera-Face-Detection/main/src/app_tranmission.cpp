#include "app_transmission.hpp"

#include "esp_log.h"

static const char TAG[] = "App/Transmission";

AppTransmission::AppTransmission(QueueHandle_t queue_i_movement_orders) : queue_i_movement_orders(queue_i_movement_orders)
{
}

static void task(AppTransmission *self)
{
    ESP_LOGD(TAG, "Start");
    movement_orders_t orders;

    while (true)
    {
        if (self->queue_i_movement_orders == nullptr)
        {
            break;
        }

        if (xQueueReceive(self->queue_i_movement_orders, &orders, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGW(TAG, "Received Movement - horizontalRotationAmount: %f", orders.horizontalRotationAmount);
        }
    }

    ESP_LOGD(TAG, "Stop");
    vTaskDelete(nullptr);
}

void AppTransmission::run()
{
    xTaskCreatePinnedToCore((TaskFunction_t)task, TAG, 4 * 1024, this, 5, nullptr, 1);
}
