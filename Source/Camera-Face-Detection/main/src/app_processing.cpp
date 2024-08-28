//
// Created by victor on 28/08/24.
//

#include <esp_log.h>
#include "app_processing.hpp"

static const char TAG[] = "App/Processing";

AppProcessing::AppProcessing(AppButton* key, AppFace* face, SemaphoreHandle_t mutex,
                             QueueHandle_t queue_o_movement_orders) : key(key), face(face), mutex(mutex),
                                                                    queue_o_movement_orders(queue_o_movement_orders)
{

}

#include "who_ai_utils.hpp" // TODO remove this

static void task(AppProcessing* self)
{
    ESP_LOGD(TAG, "Start");

    while(true)
    {
        if(!self->face)
            break;
        if(!self->mutex)
            break;

        xSemaphoreTake(self->mutex, portMAX_DELAY);
        if(self->face->detect_results_ptr != nullptr)
        {
            auto &inputData = *(self->face->detect_results_ptr);
            self->face->detect_results_ptr = nullptr;

            print_detection_result(inputData); // TODO process this data
        }
        xSemaphoreGive(self->mutex);
    vTaskDelay(10 / portTICK_PERIOD_MS); // TODO think for a better way to give back CPU1 time
    }

    ESP_LOGD(TAG, "Stop");
    vTaskDelete(nullptr);
}

void AppProcessing::run()
{
    xTaskCreatePinnedToCore((TaskFunction_t)task, TAG, 5 * 1024, this, 5, nullptr, 1);
}
