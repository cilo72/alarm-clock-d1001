# Alarm Clock for reTerminal D1001

Standalone ESP-IDF firmware project for the Seeed Studio reTerminal D1001,
based on the [Seeed Studio factory firmware](https://github.com/Seeed-Studio/reTerminal-D1001/tree/5074d3b/examples/factory_firmware).
The firmware contains the brightness test page and Settings. The brightness
test starts automatically and uses the CSI camera to measure ambient light.

## Build and flash

Use an ESP-IDF 5.4.4 environment with the ESP32-P4 toolchain installed:

```sh
git clone https://github.com/cilo72/alarm-clock-d1001.git
cd alarm-clock-d1001
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Adjust the serial port to match your device. Exit the monitor with `Ctrl+]`.
The board drivers and local libraries are included in `components/`; the
parent Seeed repository is not required. ESP-IDF downloads registry dependencies
into `managed_components/` automatically. Keep `sdkconfig.defaults` and
`dependencies.lock` in Git; generated build files, `sdkconfig`, downloaded
components, and local IDE settings are ignored. The component manager may
regenerate local paths in `dependencies.lock` when the checkout location changes.

## Included features

- Brightness test page with camera-based light measurement
- Settings for Wi-Fi/LTE, audio volume, display brightness, clock and device information
- LCD, touch, camera and board drivers needed by these two applications

Factory demo applications, their media files and the startup logo have been removed.

## License

Apache 2.0; see [LICENSE](LICENSE). Original copyright and third-party license
notices are retained in the included components.
