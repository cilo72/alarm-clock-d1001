#include "Brightness.hpp"
#include "brightness_percent.h"
#include "camera/app_video.h"
#include "esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_init.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

LV_IMG_DECLARE(img_app_camera);

static const char *TAG = "Brightness";

static ESP_Brookesia_CoreAppData_t brightnessCoreData()
{
    ESP_Brookesia_CoreAppData_t data = ESP_BROOKESIA_CORE_APP_DATA_DEFAULT("Light meter", &img_app_camera, true);
    data.flags.enable_resize_visual_area = false;
    return data;
}

static ESP_Brookesia_PhoneAppData_t brightnessPhoneData()
{
    ESP_Brookesia_PhoneAppData_t data = ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(&img_app_camera, false, false);
    // Keep system bars hidden even when swiping along the display edges.
    data.flags.enable_navigation_gesture = false;
    return data;
}

BrightnessApp::BrightnessApp(int settings_id) :
    ESP_Brookesia_PhoneApp(brightnessCoreData(), brightnessPhoneData()),
    _settings_id(settings_id)
{
}

BrightnessApp::~BrightnessApp()
{
    if (_done) {
        vSemaphoreDelete(_done);
    }
}

bool BrightnessApp::init()
{
    _done = xSemaphoreCreateBinary();
    return _done != nullptr;
}

bool BrightnessApp::run()
{
    const lv_area_t &area = getVisualArea();
    const lv_coord_t width = area.x2 - area.x1 + 1;
    const lv_coord_t height = area.y2 - area.y1 + 1;
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_size(screen, lv_disp_get_hor_res(getCore()->getDisplayDevice()),
                    lv_disp_get_ver_res(getCore()->getDisplayDevice()));
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Umgebungshelligkeit per Kamera");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, area.x1 + 40, area.y1 + 32);

    _value = lv_label_create(screen);
    lv_label_set_text(_value, "-- %");
    lv_obj_set_style_text_font(_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_value, lv_color_hex(0x8FE3C0), 0);
    lv_obj_align(_value, LV_ALIGN_TOP_MID, 0, area.y1 + height / 3);

    _detail = lv_label_create(screen);
    lv_label_set_text(_detail, "Kamera startet ...");
    lv_obj_set_style_text_font(_detail, &lv_font_montserrat_22, 0);
    lv_obj_align(_detail, LV_ALIGN_TOP_MID, 0, area.y1 + height / 3 + 80);

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Relativer RAW8-Wert, keine Lux-Messung\nFeste Belichtung: 10 ms, minimale Verstaerkung");
    lv_obj_set_width(hint, width - 80);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_20, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, area.y1 + height / 3 + 140);

    lv_obj_t *settings = lv_btn_create(screen);
    lv_obj_set_size(settings, 240, 64);
    lv_obj_align(settings, LV_ALIGN_TOP_RIGHT, -40, area.y2 - 88);
    lv_obj_t *settings_label = lv_label_create(settings);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_center(settings_label);
    lv_obj_add_event_cb(settings, [](lv_event_t *event) {
        auto *app = static_cast<BrightnessApp *>(lv_event_get_user_data(event));
        ESP_Brookesia_CoreAppEventData_t start = {};
        start.id = app->_settings_id;
        start.type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START;
        app->getCore()->sendAppEvent(&start);
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *home = lv_btn_create(screen);
    lv_obj_set_size(home, 160, 64);
    lv_obj_align(home, LV_ALIGN_TOP_LEFT, area.x1 + 40, area.y2 - 88);
    lv_obj_t *home_label = lv_label_create(home);
    lv_label_set_text(home_label, LV_SYMBOL_HOME " Home");
    lv_obj_center(home_label);
    lv_obj_add_event_cb(home, [](lv_event_t *event) {
        static_cast<BrightnessApp *>(lv_event_get_user_data(event))->back();
    }, LV_EVENT_CLICKED, this);

    _stop = false;
    _percent = -1;
    _samples = 0;
    _error = nullptr;
    xSemaphoreTake(_done, 0);
    if (xTaskCreate(captureTask, "light_capture", 6144, this, 3, &_task) != pdPASS) {
        _task = nullptr;
        _error = "Kamera-Task konnte nicht starten";
    }
    lv_timer_create(refreshLabel, 200, this);
    ESP_LOGI(TAG, "Light meter started: %dx%d, update every 2000 ms", width, height);
    return true;
}

bool BrightnessApp::close()
{
    _stop = true;
    if (_task && xSemaphoreTake(_done, pdMS_TO_TICKS(3000)) != pdTRUE) {
        ESP_LOGE(TAG, "Camera did not stop; keep app resources alive");
        return false;
    }
    _task = nullptr;
    _value = nullptr;
    _detail = nullptr;
    return true;
}

void BrightnessApp::refreshLabel(lv_timer_t *timer)
{
    auto *app = static_cast<BrightnessApp *>(timer->user_data);
    if (const char *error = app->_error.load()) {
        lv_label_set_text(app->_value, "-- %");
        lv_label_set_text(app->_detail, error);
    } else if (app->_percent.load() >= 0) {
        lv_label_set_text_fmt(app->_value, "%d %%", app->_percent.load());
        lv_label_set_text_fmt(app->_detail, "Messung %u  |  Aktualisierung alle 2 Sekunden", app->_samples.load());
    }
}

void BrightnessApp::captureTask(void *arg)
{
    auto *app = static_cast<BrightnessApp *>(arg);
    app->capture();
    xSemaphoreGive(app->_done);
    vTaskDelete(nullptr);
}

static bool setCameraControl(int fd, uint32_t id, uint32_t ctrl_class, int value)
{
    v4l2_ext_control control = {};
    control.id = id;
    control.value = value;
    v4l2_ext_controls controls = {};
    controls.ctrl_class = ctrl_class;
    controls.count = 1;
    controls.controls = &control;
    return ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) == 0;
}

void BrightnessApp::capture()
{
    int fd = -1;
    bool initialized = false;
    bool streaming = false;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint8_t *frames[2] = {};
    size_t lengths[2] = {};
    v4l2_format format = {};
    v4l2_requestbuffers request = {};
    int64_t next_sample = 0;

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
    _error = "Fuer die Messung muss die Belichtungsautomatik deaktiviert sein";
    return;
#endif

    if (app_video_main(bsp_i2c_0_get_handle()) != ESP_OK) {
        _error = "Kamera konnte nicht initialisiert werden";
        return;
    }
    initialized = true;
    fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        _error = "Kamera konnte nicht geoeffnet werden";
        goto cleanup;
    }
    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        _error = "Kameraformat konnte nicht gelesen werden";
        goto cleanup;
    }
    // esp_video 1.2's initial G_FMT response does not retain the caller's type.
    format.type = type;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
    format.fmt.pix.bytesperline = format.fmt.pix.width;
    format.fmt.pix.sizeimage = format.fmt.pix.width * format.fmt.pix.height;
    if (ioctl(fd, VIDIOC_S_FMT, &format) != 0 || ioctl(fd, VIDIOC_G_FMT, &format) != 0 ||
            format.fmt.pix.pixelformat != V4L2_PIX_FMT_SBGGR8) {
        _error = "RAW8-Kameraformat wird nicht unterstuetzt";
        goto cleanup;
    }
    // V4L2 exposure units are 100 us: 100 = 10 ms. Gain is a menu index;
    // index zero is the SC2356's lowest available gain.
    if (!setCameraControl(fd, V4L2_CID_EXPOSURE_ABSOLUTE, V4L2_CTRL_CLASS_CAMERA, 100) ||
            !setCameraControl(fd, V4L2_CID_GAIN, V4L2_CTRL_CLASS_USER, 0)) {
        _error = "Feste Kamerabelichtung konnte nicht eingestellt werden";
        goto cleanup;
    }
    request.type = type;
    request.memory = V4L2_MEMORY_MMAP;
    request.count = 2;
    if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0 || request.count != 2) {
        _error = "Kamerapuffer konnten nicht angelegt werden";
        goto cleanup;
    }
    for (unsigned i = 0; i < 2; ++i) {
        v4l2_buffer buffer = {};
        buffer.type = type;
        buffer.memory = request.memory;
        buffer.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            _error = "Kamerapuffer konnten nicht gelesen werden";
            goto cleanup;
        }
        frames[i] = static_cast<uint8_t *>(mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                               MAP_SHARED, fd, buffer.m.offset));
        lengths[i] = buffer.length;
        if (!frames[i] || frames[i] == MAP_FAILED || ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
            _error = "Kamerapuffer konnten nicht aktiviert werden";
            goto cleanup;
        }
    }
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        _error = "Kamerastream konnte nicht starten";
        goto cleanup;
    }
    streaming = true;
    next_sample = esp_timer_get_time() + 2000000;
    while (!_stop.load()) {
        v4l2_buffer buffer = {};
        buffer.type = type;
        buffer.memory = request.memory;
        if (ioctl(fd, VIDIOC_DQBUF, &buffer) != 0) {
            _error = "Kamera liefert keine Bilder";
            break;
        }
        const int64_t now = esp_timer_get_time();
        if (now >= next_sample && buffer.index < 2 && !(buffer.flags & V4L2_BUF_FLAG_ERROR)) {
            const unsigned stride = format.fmt.pix.bytesperline ? format.fmt.pix.bytesperline : format.fmt.pix.width;
            const size_t used = buffer.bytesused < lengths[buffer.index] ? buffer.bytesused : lengths[buffer.index];
            const int percent = brightness_percent(frames[buffer.index], used,
                                                  format.fmt.pix.width, format.fmt.pix.height, stride);
            if (percent < 0) {
                _error = "Ungueltiges Kamerabild";
                break;
            }
            _percent = percent;
            ++_samples;
            ESP_LOGI(TAG, "Brightness: %d%% (sample %u)", percent, _samples.load());
            next_sample = now + 2000000;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
            _error = "Kamerapuffer konnte nicht zurueckgegeben werden";
            break;
        }
    }

cleanup:
    if (const char *error = _error.load()) {
        ESP_LOGE(TAG, "%s", error);
    }
    if (streaming) {
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    if (fd >= 0) {
        ::close(fd);
    }
    // esp_video owns the MMAP buffers; deinit releases them after capture stops.
    if (initialized) {
        esp_video_deinit();
    }
}
