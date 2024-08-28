#pragma once

#include "sdkconfig.h"

#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_tool.hpp"
#if CONFIG_MFN_V1
#if CONFIG_S8
#include "face_recognition_112_v1_s8.hpp"
#elif CONFIG_S16
#include "face_recognition_112_v1_s16.hpp"
#endif
#endif

#include "__base__.hpp"
#include "app_camera.hpp"
#include "app_button.hpp"

class AppFace : public Observer, public Frame
{
private:
    AppButton *key;

public:
    HumanFaceDetectMSR01 detector;
    HumanFaceDetectMNP01 detector2;

    face_info_t recognize_result;
    SemaphoreHandle_t detection_data_mutex;

    std::list<dl::detect::result_t>* volatile detect_results_ptr;
    bool switch_on;

    AppFace(AppButton *key,
            QueueHandle_t queue_i = nullptr,
            QueueHandle_t queue_o = nullptr,
            SemaphoreHandle_t mutex = nullptr,
            void (*callback)(camera_fb_t *) = esp_camera_fb_return);

    void update();
    void run();
};
