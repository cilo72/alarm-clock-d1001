set(APPS_DIR ./)
file(GLOB_RECURSE SETTINGS_C_SRCS ${APPS_DIR}/setting/*.c)
set(APPS_C_SRCS ${SETTINGS_C_SRCS} camera/app_video.c brightness/assets/img_app_camera.c)
set(APPS_CPP_SRCS setting/Setting.cpp brightness/Brightness.cpp)

idf_component_register(
    SRCS ${APPS_C_SRCS} ${APPS_CPP_SRCS}
    INCLUDE_DIRS ${APPS_DIR}
    REQUIRES lvgl__lvgl esp_event esp_wifi iot_usbh_modem console ping-cmd nvs_flash esp_mm esp-brookesia bsp_extra esp32_p4_re_terminal_d1001 esp_video esp_lcd_touch_gsl3670)

target_compile_options(
    ${COMPONENT_LIB}
    PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        -Wno-format
        -Wno-incompatible-pointer-types
        -Wno-int-conversion
    >
    $<$<COMPILE_LANGUAGE:CXX>:
        -Wno-format
        -Wno-conversion-null
        -Wno-attributes
        -Wno-write-strings
    >
    -DLV_LVGL_H_INCLUDE_SIMPLE
)
