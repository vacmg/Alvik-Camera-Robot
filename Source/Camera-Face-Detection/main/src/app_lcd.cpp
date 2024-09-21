#include "app_lcd.hpp"

#include <string.h>

#include "esp_log.h"
#include "esp_camera.h"
#include "dl_image.hpp"

#include "arduino_community_logo_240_240.h"

static const char TAG[] = "App/LCD";

AppLCD::AppLCD(AppButton *key,
               QueueHandle_t queue_i,
               QueueHandle_t queue_o,
               void (*callback)(camera_fb_t *)) : Frame(queue_i, queue_o, callback),
                                                  key(key),
                                                  panel_handle(NULL),
                                                  switch_on(false),
                                                  paper_drawn(false),
                                                  black_drawn(false)
{

        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t bus_conf = {
            .mosi_io_num = BOARD_LCD_MOSI,
            .miso_io_num = BOARD_LCD_MISO,
            .sclk_io_num = BOARD_LCD_SCK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t),
        };
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_conf, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = BOARD_LCD_CS,
            .dc_gpio_num = BOARD_LCD_DC,
            .spi_mode = 0,
            .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = BOARD_LCD_CMD_BITS,
            .lcd_param_bits = BOARD_LCD_PARAM_BITS,
        };
        // Attach the LCD to the SPI bus
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

        // ESP_LOGI(TAG, "Install ST7789 panel driver");
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = BOARD_LCD_RST,
            .rgb_endian = LCD_RGB_ENDIAN_RGB,
            .bits_per_pixel = 16,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        esp_lcd_panel_invert_color(panel_handle, true);// Set inversion for esp32s3eye

        // turn on display
        esp_lcd_panel_disp_on_off(panel_handle, true);

        this->draw_color(0x000000);
        vTaskDelay(pdMS_TO_TICKS(500));
        this->draw_wallpaper();
        vTaskDelay(pdMS_TO_TICKS(1000));
}

void AppLCD::draw_wallpaper()
{
    uint16_t *pixels = (uint16_t *)heap_caps_malloc((arduino_community_logo_240x240_lcd_width * arduino_community_logo_240x240_lcd_height) * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (nullptr == pixels)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }
    memcpy(pixels, arduino_community_logo_240x240_lcd, (arduino_community_logo_240x240_lcd_width * arduino_community_logo_240x240_lcd_height) * sizeof(uint16_t));
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, arduino_community_logo_240x240_lcd_width, arduino_community_logo_240x240_lcd_height, (uint16_t *)pixels);
    heap_caps_free(pixels);

    this->paper_drawn = true;
}

void AppLCD::draw_color(int color) const
{
    uint16_t *buffer = (uint16_t *)malloc(BOARD_LCD_H_RES * sizeof(uint16_t));
    if (NULL == buffer)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
    }
    else
    {
        for (size_t i = 0; i < BOARD_LCD_H_RES; i++)
        {
            buffer[i] = color;
        }

        for (int y = 0; y < BOARD_LCD_V_RES; y++)
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, BOARD_LCD_H_RES, y+1, buffer);
        }

        free(buffer);
    }
}

void AppLCD::update()
{
    if (this->key->pressed > BUTTON_IDLE)
    {
        if (this->key->pressed == BUTTON_MENU)
        {
            this->switch_on = this->key->menu != MENU_STOP_WORKING;
            this->black_drawn = false;
            ESP_LOGD(TAG, "%s", this->switch_on ? "ON" : "OFF");
        }
    }

    if (!this->switch_on)
    {
        this->paper_drawn = false;
    }
}

static void task(AppLCD *self)
{
    ESP_LOGD(TAG, "Start");
    uint32_t constexpr frame_pixels_buff_size = (BOARD_LCD_H_RES * BOARD_LCD_V_RES) * sizeof(uint16_t);
    ESP_LOGI(TAG, "allocating %lu bytes for frame_pixels_buff", frame_pixels_buff_size);
    uint16_t *frame_pixels_buff = (uint16_t *)heap_caps_malloc(frame_pixels_buff_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if(nullptr == frame_pixels_buff)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        vTaskDelete(nullptr);
    }

    camera_fb_t *frame = nullptr;
    while (true)
    {
        if (self->queue_i == nullptr)
            break;

        if (xQueueReceive(self->queue_i, &frame, portMAX_DELAY))
        {
            if (self->switch_on)
            {
                if(!self->black_drawn)
                {
                    self->draw_color(0x000000);
                    self->black_drawn = true;
                }

                if(frame->height == BOARD_LCD_V_RES && frame->width == BOARD_LCD_H_RES)
                {
                    esp_lcd_panel_draw_bitmap(self->panel_handle, 0, 0, frame->width, frame->height, (uint16_t *)frame->buf);
                }
                else
                {
                    double aspectRatio = static_cast<double>(frame->width) / frame->height;
                    int destHRes = BOARD_LCD_H_RES;
                    int destVRes = BOARD_LCD_V_RES;
                    if(aspectRatio > 1) // width > height
                    {
                        destVRes = static_cast<int>(static_cast<double>(BOARD_LCD_V_RES) / aspectRatio);
                    }
                    else if(aspectRatio < 1) // height > width
                    {
                        destHRes = static_cast<int>(static_cast<double>(BOARD_LCD_H_RES) * aspectRatio);
                    }

                    ESP_LOGD(TAG, "Resizing image from %dx%d to %dx%d (aspect ratio: %f)", frame->width, frame->height, destHRes, destVRes, aspectRatio);
                    dl::image::resize_image_nearest((uint16_t*)frame->buf, {static_cast<int>(frame->height), static_cast<int>(frame->width), 1}, frame_pixels_buff, {destVRes, destHRes, 1});
                    esp_lcd_panel_draw_bitmap(self->panel_handle, 0, 0, destHRes, destVRes, frame_pixels_buff);
                }
            }

            else if (!self->paper_drawn)
                self->draw_wallpaper();

            if (self->queue_o)
                xQueueSend(self->queue_o, &frame, portMAX_DELAY);
            else
                self->callback(frame);
        }
    }
    ESP_LOGD(TAG, "Stop");
    self->draw_wallpaper();
    heap_caps_free(frame_pixels_buff);
    vTaskDelete(nullptr);
}

void AppLCD::run()
{
    xTaskCreatePinnedToCore((TaskFunction_t)task, TAG, 4 * 1024, this, 5, nullptr, 1);
}