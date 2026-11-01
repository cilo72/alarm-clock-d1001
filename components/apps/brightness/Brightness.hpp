#pragma once

#include <atomic>
#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class BrightnessApp : public ESP_Brookesia_PhoneApp {
public:
    explicit BrightnessApp(int settings_id);
    ~BrightnessApp() override;
    bool init() override;
    bool run() override;
    bool pause() override { return true; }
    bool resume() override { return true; }
    bool back() override { return notifyCoreClosed(); }
    bool close() override;

private:
    static void captureTask(void *arg);
    static void refreshLabel(lv_timer_t *timer);
    void capture();

    const int _settings_id;
    TaskHandle_t _task = nullptr;
    SemaphoreHandle_t _done = nullptr;
    lv_obj_t *_value = nullptr;
    lv_obj_t *_detail = nullptr;
    std::atomic<bool> _stop{false};
    std::atomic<int> _percent{-1};
    std::atomic<unsigned> _samples{0};
    std::atomic<const char *> _error{nullptr};
};
