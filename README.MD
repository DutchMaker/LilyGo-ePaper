
# LilyGo ESP32 E-Paper device firmware

This firmware runs on the LilyGo T5 v2.3 (2.13")

## Use Arduino IDE
1. Install the correct serial port driver [CP210X Driver](https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers)
2. Copy the files in the `lib` directory to `~/Arduino/libraries` (windows users copy to `Documents/Arduino/libraries`)
3. Open `src/src.ino` 
4. Make sure `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` is added as board manager URL.
5. Make sure the `esp32` board definitions are installed.
6. Select the `ESP32 Dev Module` board and select the correct port.
7. Upload the sketch.
8. Make sure `ESP32 Sketch data Upload` tool is installed (See: [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases).
9. Run the tool `Tools -> ESP32 Sketch data Upload` to upload the data folder to the ESP32 file system.
10. Connect to the WiFi access point that is broadcasted by the ESP32.
11. Browse to the web page to configure the content (hostname is configured in code).