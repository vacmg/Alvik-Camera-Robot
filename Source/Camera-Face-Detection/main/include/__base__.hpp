#pragma once

#include <list>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"

typedef enum
{
    MENU_STOP_WORKING = 0,
    MENU_DISPLAY_ONLY = 1,
    MENU_FACE_RECOGNITION = 2,

    MENU_MAX
} command_word_t;

typedef struct movement_orders_struct_t // For the program to work, all members must be default initialized to NO_MOVEMENT.
{
    double horizontalRotationAmount = 0; // in deg/s
} movement_orders_t;

class Observer
{
public:
    virtual void update() = 0;
};

class Subject
{
private:
    std::list<Observer *> observers;

public:
    void attach(Observer *observer)
    {
        this->observers.push_back(observer);
    }

    void detach(Observer *observer)
    {
        this->observers.remove(observer);
    }

    void detach_all()
    {
        this->observers.clear();
    }

    void notify()
    {
        for (auto observer : this->observers)
            observer->update();
    }
};

class Frame
{
public:
    QueueHandle_t queue_i;
    QueueHandle_t queue_o;
    void (*callback)(camera_fb_t *);

    Frame(QueueHandle_t queue_i = nullptr,
          QueueHandle_t queue_o = nullptr,
          void (*callback)(camera_fb_t *) = nullptr) : queue_i(queue_i),
                                                       queue_o(queue_o),
                                                       callback(callback) {}

    void set_io(QueueHandle_t queue_i, QueueHandle_t queue_o)
    {
        this->queue_i = queue_i;
        this->queue_o = queue_o;
    }
};
