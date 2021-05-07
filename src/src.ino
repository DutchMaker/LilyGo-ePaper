/*
* LilyGo ePaper Display application
* 
* Description:  Hosts a web page that allows plain text to be displayed on the epaper display.
* Usage:        After powerup, the WiFi network is exposed.
*               Connect to the network and browse to http://HOSTNAME.local to update the display text.
*               Turn off the device after updating the text to preserve battery power.
*/

#define WIFI_NAME       "R&M Fridge %02X:%02X"
#define HOSTNAME        "fridge"
#define DATA_FILENAME   "display.json"    // TODO: Leading slash may be required

#define LILYGO_T5_V213
#define FILESYSTEM SPIFFS

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
#include <Fonts/Org_01.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>

enum {
    GxEPD_ALIGN_RIGHT,
    GxEPD_ALIGN_LEFT,
    GxEPD_ALIGN_CENTER,
};

typedef struct {
    char title[14];
    char content[256];
} Content_t;

AsyncWebServer  server(80);
GxIO_Class      io(SPI, EPD_CS, EPD_DC, EPD_RSET);
GxEPD_Class     display(io, EPD_RSET, EPD_BUSY);

Content_t       info;

/****************************************************
* S A V E   &   L O A D   C O N T E N T
****************************************************/

void saveContent(Content_t *info)
{
    File file = FILESYSTEM.open(DATA_FILENAME, FILE_WRITE);
    
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }
    
    cJSON *root =  cJSON_CreateObject();
    cJSON_AddStringToObject(root, "title", info->title);
    cJSON_AddStringToObject(root, "content", info->content);
    
    const char *str =  cJSON_Print(root);
    file.write((uint8_t *)str, strlen(str));
    file.close();
    
    cJSON_Delete(root);
}

void loadDefaultContent()
{
    strlcpy(info.title, "List title", sizeof(info.title));
    strlcpy(info.content, "empty", sizeof(info.content));
    saveContent(&info);
}

bool loadContent(Content_t *info)
{
    if (!FILESYSTEM.exists(DATA_FILENAME)) {
        Serial.println("Load configure fail");
        return false;
    }
    
    File file = FILESYSTEM.open(DATA_FILENAME);
    
    if (!file) {
        Serial.println("Open fail -->");
        return false;
    }
    
    cJSON *root =  cJSON_Parse(file.readString().c_str());
    
    if (!root) {
        return false;
    }
    if (cJSON_GetObjectItem(root, "title")->valuestring) {
        strlcpy(info->title, cJSON_GetObjectItem(root, "title")->valuestring, sizeof(info->title));
    }
    if (cJSON_GetObjectItem(root, "content")->valuestring) {
        strlcpy(info->content, cJSON_GetObjectItem(root, "content")->valuestring, sizeof(info->content));
    }
    
    file.close();
    cJSON_Delete(root);
    
    return true;
}

void initFileSystem()
{
  if (!FILESYSTEM.begin()) {
    Serial.println("FILESYSTEM initialization error.");
    Serial.println("FILESYSTEM format ...");
    
    display.setCursor(0, 16);
    display.println("FILESYSTEM initialization error.");
    display.println("FILESYSTEM format ...");
    display.update();
    
    FILESYSTEM.begin(true);
  }
}

/****************************************************
* W I F I  &  W E B   S E R V E R
****************************************************/

void setupWiFi()
{
  uint8_t mac[6];
  char    apName[64];
  
  WiFi.mode(WIFI_AP);
  WiFi.macAddress(mac);
  sprintf(apName, WIFI_NAME, mac[4], mac[5]);
  WiFi.softAP(apName);
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
    
    if (name == "title") {
      strlcpy(info.title, params.c_str(), sizeof(info.title));
    }
    
    if (name == "content") {
      strlcpy(info.content, params.c_str(), sizeof(info.content));
    }
  }
  
  saveContent(&info);
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
    server.on(DATA_FILENAME, HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(FILESYSTEM, DATA_FILENAME, "application/json");
    });
    server.on("/data", HTTP_POST, asyncWebServerDataPostCb);

    server.onNotFound(asyncWebServerNotFoundCb);

    MDNS.begin(HOSTNAME);
    MDNS.addService("http", "tcp", 80);

    server.begin();
}

/****************************************************************
* D I S P L A Y   C O N T E N T
****************************************************************/

void displayText(const char *str, int16_t y, uint8_t align)
{
  int16_t x = 0, x1 = 0, y1 = 0;
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

void displayContent()
{
  display.fillScreen(GxEPD_WHITE);
  
  display.setFont(&FreeSans9pt7b);
  displayText(info.title, 15, GxEPD_ALIGN_LEFT);
  
  display.setFont(&Org_01);
  displayText(info.content, 25, GxEPD_ALIGN_LEFT);
  
  display.update();
}

void initDisplay()
{
  display.init();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
}

/****************************************************************
*  M A I N
****************************************************************/

void setup()
{
  Serial.begin(115200);
  
  SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
  
  initDisplay();
  initFileSystem();
  
  if (!loadContent(&info)) {
    loadDefaultContent();
  }
  
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    displayContent();
  }
  
  setupWiFi();
  setupWebServer();
}

void loop()
{
}
