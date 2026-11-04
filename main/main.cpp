#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "esp-bsp.h"
#include "display.h"
#include "bsp_board_extra.h"

#include "esp_brookesia.hpp"
#include "apps.h"

extern "C" void app_main(void)
{
    bsp_power_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bsp_extra_codec_init());

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    lv_display_t *display = bsp_display_start_with_config(&cfg);
    bsp_display_lock(0);
    bsp_display_rotate(display, LV_DISPLAY_ROTATION_90);
    bsp_display_unlock();

    bsp_rgb_led_duty_set(0, 100);
    bsp_rgb_led_duty_set(1, 0);
    bsp_rgb_led_duty_set(2, 0);

    bsp_display_backlight_on();

    bsp_display_lock(0);

    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone();
    assert(phone != nullptr && "Failed to create phone");

    ESP_Brookesia_PhoneStylesheet_t *phone_stylesheet = new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_1280_800_DARK_STYLESHEET();
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone_stylesheet, "Create phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(*phone_stylesheet), "Add phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(*phone_stylesheet), "Activate phone stylesheet failed");

    assert(phone->begin() && "Failed to begin phone");

    AppSettings *app_settings = new AppSettings();
    assert(app_settings != nullptr && "Failed to create app_settings");
    assert((phone->installApp(app_settings) >= 0) && "Failed to begin app_settings");

    BrightnessApp *brightness_app = new BrightnessApp(app_settings->getId());
    assert(brightness_app != nullptr && "Failed to create light meter");
    assert((phone->installApp(brightness_app) >= 0) && "Failed to install light meter");

    ESP_Brookesia_CoreAppEventData_t event = {};
    event.type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START;
    event.id = brightness_app->getId();

    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->sendAppEvent(&event), "Autostart failed");

    bsp_display_unlock();
}
