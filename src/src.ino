/*
    LilyGo Ink Screen Series
        - Created by Lewis he , Last updated 3/16/2021
*/

// According to the board, cancel the corresponding macro definition
#define LILYGO_T5_V213
// #define LILYGO_T5_V22
// #define LILYGO_T5_V24
// #define LILYGO_T5_V28

#include <SD.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <cJSON.h>

#include <boards.h>
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeSans9pt7b.h>


#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AceButton.h>

#include "utilities.h"
#include "defConfig.h"


//Select the header file corresponding to the screen.

// #include <GxGDEW0154Z04/GxGDEW0154Z04.h>  // 1.54" b/w/r 200x200
// #include <GxGDEW0154Z17/GxGDEW0154Z17.h>  // 1.54" b/w/r 152x152
// #include <GxGDEH0154D67/GxGDEH0154D67.h>  // 1.54" b/w

// #include <GxDEPG0150BN/GxDEPG0150BN.h>    // 1.51" b/w   form DKE GROUP

// #include <GxGDEW027C44/GxGDEW027C44.h>    // 2.7" b/w/r
// #include <GxGDEW027W3/GxGDEW027W3.h>      // 2.7" b/w

// #include <GxGDEW0213Z16/GxGDEW0213Z16.h>  // 2.13" b/w/r

// old panel
//#include <GxGDEH0213B72/GxGDEH0213B72.h>  // 2.13" b/w old panel
//#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w old panel

#include <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP

// #include <GxGDEM0213B74/GxGDEM0213B74.h>  // 2.13" b/w  form GoodDisplay 4-color

// #include <GxDEPG0266BN/GxDEPG0266BN.h>    // 2.66" b/w  form DKE GROUP

// #include <GxGDEH029A1/GxGDEH029A1.h>      // 2.9" b/w
// #include <GxQYEG0290BN/GxQYEG0290BN.h>    // 2.9" b/w new panel
// #include <GxDEPG0290B/GxDEPG0290B.h>      // 2.9" b/w    form DKE GROUP

// #include <GxGDEW029Z10/GxGDEW029Z10.h>    // 2.9" b/w/r
// #include <GxDEPG0290R/GxDEPG0290R.h>      // 2.9" b/w/r  form DKE GROUP

// #include <GxDEPG0750BN/GxDEPG0750BN.h>    // 7.5" b/w    form DKE GROUP

enum {
    BADGE_PAGE1,
    BADGE_PAGE2,
    BADGE_PAGE_MAX,
};

enum {
    GxEPD_ALIGN_RIGHT,
    GxEPD_ALIGN_LEFT,
    GxEPD_ALIGN_CENTER,
};

typedef struct {
    char content[256];
} Badge_Info_t;

using namespace         ace_button;
AsyncWebServer          server(80);
GxIO_Class              io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class             display(io, EPD_RSET, EPD_BUSY);
AceButton               *btnPtr = NULL;

Badge_Info_t            info;
const char              *path[2] = {DEFALUT_AVATAR_BMP, DEFALUT_QR_CODE_BMP};

const uint8_t           btns[] = BUTTONS;
const uint8_t           handle_btn_nums = sizeof(btns) / sizeof(*btns);

uint8_t                 lastSelect = 0;

extern void drawBitmap(GxEPD &display, const char *filename, int16_t x, int16_t y, bool with_color);
static void displayBadgePage(uint8_t num);

/****************************************************
                   ____        _   _
     /\           |  _ \      | | | |
    /  \   ___ ___| |_) |_   _| |_| |_ ___  _ __
   / /\ \ / __/ _ \  _ <| | | | __| __/ _ \| '_ \
  / ____ \ (_|  __/ |_) | |_| | |_| || (_) | | | |
 /_/    \_\___\___|____/ \__,_|\__|\__\___/|_| |_|

****************************************************/

static void socEnterSleepMode(void)
{
    esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
    Serial.println("Going to sleep now");
    delay(2000);
    esp_deep_sleep_start();
}


static void singleButtonHandleCb(uint8_t event)
{
    if (event == AceButton::kEventClicked) {
        displayBadgePage(lastSelect++);
        lastSelect %= BADGE_PAGE_MAX;
    } else if (event == AceButton::kEventLongPressed) {
        socEnterSleepMode();
    }
}

#ifdef BUTTON_2
static void threeButtonHandleCb(uint8_t pin, uint8_t event)
{
    if (event != AceButton::kEventClicked) {
        return ;
    }
    switch (pin) {
    case BUTTON_1:
        socEnterSleepMode();
        break;

    case BUTTON_2:
        //TODO:
        break;

    case BUTTON_3:
        displayBadgePage(lastSelect++);
        lastSelect %= BADGE_PAGE_MAX;
        break;
    default:
        break;
    }
}
#endif

static void aceButtonHandleEventCb(AceButton *b, uint8_t event, uint8_t state)
{
    Serial.printf("Pin:%d event:%u state:%u\n", b->getPin(), event, state);

#ifdef BUTTON_2
    threeButtonHandleCb(b->getPin(), event);
#else
    singleButtonHandleCb(event);
#endif
}

static void setupButton(void)
{
    if (btnPtr) {
        return;
    }
    btnPtr = new  AceButton [handle_btn_nums];
    for (int i = 0; i < handle_btn_nums; ++i) {
        pinMode(btns[i], INPUT);
        btnPtr[i].init(btns[i]);
        ButtonConfig *buttonConfig = btnPtr[i].getButtonConfig();
        buttonConfig->setEventHandler(aceButtonHandleEventCb);
        buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
        buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
        buttonConfig->setFeature(ButtonConfig::kFeatureClick);
    }
}

static void aceButtonLoop(void)
{
    if (!btnPtr) {
        return;
    }
    for (int i = 0; i < handle_btn_nums; ++i) {
        btnPtr[i].check();
    }
}

/****************************************************
  ____            _              _____        __
 |  _ \          | |            |_   _|      / _|
 | |_) | __ _  __| | __ _  ___    | |  _ __ | |_ ___
 |  _ < / _` |/ _` |/ _` |/ _ \   | | | '_ \|  _/ _ \
 | |_) | (_| | (_| | (_| |  __/  _| |_| | | | || (_) |
 |____/ \__,_|\__,_|\__, |\___| |_____|_| |_|_| \___/
                     __/ |
                    |___/
****************************************************/
void saveBadgeInfo(Badge_Info_t *info)
{
    File file = FILESYSTEM.open(BADGE_CONFIG_FILE_NAME, FILE_WRITE);
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }
    cJSON *root =  cJSON_CreateObject();
    cJSON_AddStringToObject(root, "content", info->content);
    const char *str =  cJSON_Print(root);
    file.write((uint8_t *)str, strlen(str));
    file.close();
    cJSON_Delete(root);
}

void loadDefaultInfo(void)
{
    strlcpy(info.content, "empty list", sizeof(info.content));
    saveBadgeInfo(&info);
}

bool loadBadgeInfo(Badge_Info_t *info)
{
    if (!FILESYSTEM.exists(BADGE_CONFIG_FILE_NAME)) {
        Serial.println("load configure fail");
        return false;
    }
    File file = FILESYSTEM.open(BADGE_CONFIG_FILE_NAME);
    if (!file) {
        Serial.println("Open Fial -->");
        return false;
    }
    cJSON *root =  cJSON_Parse(file.readString().c_str());
    if (!root) {
        return false;
    }
    if (cJSON_GetObjectItem(root, "content")->valuestring) {
        strlcpy(info->content, cJSON_GetObjectItem(root, "content")->valuestring, sizeof(info->content));
    }
    file.close();
    cJSON_Delete(root);
    return true;
}

/****************************************************
 __          ___ ______ _
 \ \        / (_)  ____(_)
  \ \  /\  / / _| |__   _
   \ \/  \/ / | |  __| | |
    \  /\  /  | | |    | |
     \/  \/   |_|_|    |_|

****************************************************/

void setupWiFi(bool apMode)
{
    if (apMode) {
        uint8_t mac[6];
        char    apName[64];
        WiFi.mode(WIFI_AP);
        WiFi.macAddress(mac);
        sprintf(apName, "R&M Fridge %02X:%02X", mac[4], mac[5]);
        WiFi.softAP(apName);
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        while (WiFi.waitForConnectResult() != WL_CONNECTED) {
            Serial.print(".");
            esp_restart();
        }
        Serial.println(F("WiFi connected"));
        Serial.println("");
        Serial.println(WiFi.localIP());
    }
}

/****************************************************************
 __          __  _     _____
 \ \        / / | |   / ____|
  \ \  /\  / /__| |__| (___   ___ _ ____   _____ _ __
   \ \/  \/ / _ \ '_ \\___ \ / _ \ '__\ \ / / _ \ '__|
    \  /\  /  __/ |_) |___) |  __/ |   \ V /  __/ |
     \/  \/ \___|_.__/_____/ \___|_|    \_/ \___|_|

****************************************************************/
static void asyncWebServerFileUploadCb(AsyncWebServerRequest *request, const String &filename,
                                       size_t index, uint8_t *data, size_t len, bool final)
{
    static File file;
    static int pathIndex = 0;
    if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        file = FILESYSTEM.open(path[pathIndex], FILE_WRITE);
        if (!file) {
            Serial.println("Open FAIL");
            request->send(500, "text/plain", "hander error");
            return;
        }
    }
    if (file.write(data, len) != len) {
        Serial.println("Write fail");
        request->send(500, "text/plain", "hander error");
        file.close();
        return;
    }

    if (final) {
        Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
        file.close();
        request->send(200, "text/plain", "");
        if (++pathIndex >= 2) {
            pathIndex = 0;
            displayBadgePage(BADGE_PAGE1);
        }
    }
}

static void asyncWebServerNotFoundCb(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

static void asyncWebServerDataPostCb(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", "");
    for (int i = 0; i < request->params(); i++) {
        String name = request->getParam(i)->name();
        String params = request->getParam(i)->value();
        Serial.println(name + " : " + params);

        if (name == "content") {
            strlcpy(info.content, params.c_str(), sizeof(info.content));
        }
    }

    saveBadgeInfo(&info);
}

static void setupWebServer(void)
{
    server.serveStatic("/", FILESYSTEM, "/").setDefaultFile("index.html");

    server.on("css/main.css", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(FILESYSTEM, "css/main.css", "text/css");
    });
    server.on("js/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(FILESYSTEM, "js/jquery.min.js", "application/javascript");
    });
    server.on("js/tbdValidate.js", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(FILESYSTEM, "js/tbdValidate.js", "application/javascript");
    });
    server.on("/data", HTTP_POST, asyncWebServerDataPostCb);

    //server.onFileUpload(asyncWebServerFileUploadCb);

    server.onNotFound(asyncWebServerNotFoundCb);

    MDNS.begin("fridge");

    MDNS.addService("http", "tcp", 80);

    server.begin();
}

/****************************************************************
  ____            _              _____  _           _
 |  _ \          | |            |  __ \(_)         | |
 | |_) | __ _  __| | __ _  ___  | |  | |_ ___ _ __ | | __ _ _   _
 |  _ < / _` |/ _` |/ _` |/ _ \ | |  | | / __| '_ \| |/ _` | | | |
 | |_) | (_| | (_| | (_| |  __/ | |__| | \__ \ |_) | | (_| | |_| |
 |____/ \__,_|\__,_|\__, |\___| |_____/|_|___/ .__/|_|\__,_|\__, |
                     __/ |                   | |             __/ |
                    |___/                    |_|            |___/
****************************************************************/
static void displayText(const char *str, int16_t y, uint8_t align)
{
    int16_t x = 0;
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    display.setCursor(x, y);
    display.getTextBounds(str, x, y, &x1, &y1, &w, &h);
    switch (align) {
    case GxEPD_ALIGN_RIGHT:
        display.setCursor(display.width() - w - x1, y);
        break;
    case GxEPD_ALIGN_LEFT:
        display.setCursor(0, y);
        break;
    case GxEPD_ALIGN_CENTER:
        display.setCursor(display.width() / 2 - ((w + x1) / 2), y);
        break;
    default:
        break;
    }
    display.println(str);
}

static void displayBadgePage(uint8_t num)
{
    display.fillScreen(GxEPD_WHITE);
    displayText(info.content, 15, GxEPD_ALIGN_LEFT);
    display.update();
}

/****************************************************************
 __  __       _
|  \/  |     (_)
| \  / | __ _ _ _ __
| |\/| |/ _` | | '_ \
| |  | | (_| | | | | |
|_|  |_|\__,_|_|_| |_|
****************************************************************/
void setup()
{
    Serial.begin(115200);

    // IP5306 has been removed in the new version
    // Wire.begin(I2C_SDA, I2C_SCL);
    // bool ret = setPowerBoostKeepOn(1);
    // Serial.printf("Power KeepUp %s\n", ret ? "PASS" : "FAIL");

    SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
    display.init();
    display.setRotation(0);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(DEFALUT_FONT);

    if (!FILESYSTEM.begin()) {
        Serial.println("FILESYSTEM initialization error.");
        Serial.println("FILESYSTEM formart ...");

        display.setCursor(0, 16);
        display.println("FILESYSTEM initialization error.");
        display.println("FILESYSTEM formart ...");
        display.update();
        FILESYSTEM.begin(true);
    }

    if (!loadBadgeInfo(&info)) {
        loadDefaultInfo();
    }

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        displayBadgePage(BADGE_PAGE1);
    }

    setupButton();

    setupWiFi(DEFAULE_WIFI_MODE);

    setupWebServer();

}

void loop()
{
    aceButtonLoop();
}
