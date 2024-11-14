#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "main.h"

#define DEBUG_MODE        true
#define CONFIG_FILE_NAME  "/config.json"
#define STATIC_JSON_SIZE  512
#define BINANCE_LINK      "/api/v3/ticker/price?symbol="
WebServer server(80);

void serverRoutine(void *pvParameters);
void binanceRequestRoutine(void *pvParameters);

const char* upload_html = R"(
<!DOCTYPE html>
<html>
  <body>
    <h2>Upload file to FS</h2>
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <input type="file" name="file">
      <input type="submit" value="Upload">
    </form>
  </body>
</html>
)";
const char* host = "api.binance.com";
const int httpsPort = 443;
boolean json_exist = false;

spotter_config conf;

void fetchCoinPrice(const char* coinSymbol) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed!");
    return;
  }

  String startURL = BINANCE_LINK;
  String fullURL = startURL + coinSymbol;

  printf("Going to send request: %s\n", fullURL.c_str());
  client.print(String("GET ") + fullURL + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected() && !client.available()) delay(10);

  String line;
  while (client.available()) {
    line += client.readStringUntil('\n');
  }
  client.stop();

  int jsonStartIndex = line.indexOf("==");
  if (jsonStartIndex == -1) {
    Serial.println("Failed to find JSON data marker '==' in response");
    return;
  }

  String jsonPart = line.substring(jsonStartIndex + 2);

  Serial.printf("Received responce: %s\n", jsonPart.c_str());
  StaticJsonDocument<STATIC_JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, jsonPart);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  const char* symbol = doc["symbol"];
  const char* price = doc["price"];
  Serial.printf("Symbol: %s\n", symbol);
  Serial.printf("Price: %s\n", price);
}

void setDefaultConfig() {
  const char* ssid = "wifiname";
  const char* password = "pass";
  const char* default_coin = "BTCUSDT";

  conf.wifi.ssid = (char*)malloc(strlen(ssid) + 1);
  strcpy(conf.wifi.ssid, ssid);
  conf.wifi.password = (char*)malloc(strlen(password) + 1);
  strcpy(conf.wifi.password, password);

  conf.binance.num_of_coins = 1;
  conf.binance.coin_list = (char**)malloc(conf.binance.num_of_coins * sizeof(char*));
  conf.binance.coin_list[0] = (char*)malloc(strlen(default_coin) + 1);
  strcpy(conf.binance.coin_list[0], default_coin);

  conf.binance.update_interval_s = 30;
  conf.display.brightness = 128;
  conf.display.theme = (char*)malloc(strlen("dark") + 1);
  strcpy(conf.display.theme, "dark");
  conf.display.coin_switch_interval_s = 10;
}

void freeConfig() {
  free(conf.wifi.ssid);
  free(conf.wifi.password);

  for (int i = 0; i < conf.binance.num_of_coins; i++) {
    free(conf.binance.coin_list[i]);
  }
  free(conf.binance.coin_list);

  free(conf.display.theme);
}

void setConfigFromJson(StaticJsonDocument<STATIC_JSON_SIZE> *doc) {
 
  conf.wifi.ssid = (char*)malloc(strlen((*doc)["wifi"]["ssid"])+1);
  strcpy(conf.wifi.ssid, (*doc)["wifi"]["ssid"]);
  conf.wifi.password = (char*)malloc(strlen((*doc)["wifi"]["password"])+1);
  strcpy(conf.wifi.password, (*doc)["wifi"]["password"]);

  conf.binance.num_of_coins = (*doc)["binance"]["symbols"].size();
  conf.binance.coin_list = (char**)malloc(conf.binance.num_of_coins * sizeof(char*));

  for(int i = 0; i < conf.binance.num_of_coins; i++){
    conf.binance.coin_list[i] = (char*)malloc(strlen((*doc)["binance"]["symbols"][i]) + 1);
    strcpy(conf.binance.coin_list[i], (*doc)["binance"]["symbols"][i]);
  }

  conf.binance.update_interval_s = (*doc)["binance"]["update_interval"];
  conf.display.brightness = (*doc)["display"]["brightness"];
  conf.display.theme = (char*)malloc(strlen((*doc)["display"]["theme"]) + 1);
  strcpy(conf.display.theme, (*doc)["display"]["theme"]);
  conf.display.coin_switch_interval_s = (*doc)["display"]["coin_switch_interval"];
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nBinance spotter started");

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  if (DEBUG_MODE) {
    Serial.println("Going to list all files on SPIFFS");
    listSPIFFSFiles();
  }

  File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
  if (!configFile) {
    Serial.printf("Failed to open %s\n", CONFIG_FILE_NAME);
  }

  StaticJsonDocument<STATIC_JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.printf("Failed to parse JSON: %s\n", error.c_str());
  } else {
    json_exist = true;
  }

  configFile.close();

  if (DEBUG_MODE) {
    Serial.printf("Content of %s:\n", CONFIG_FILE_NAME);
    serializeJsonPretty(doc, Serial);
  }

  if (!json_exist) {
    setDefaultConfig();
  } else {
    setConfigFromJson(&doc);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(conf.wifi.ssid, conf.wifi.password);

  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", upload_html);
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "File Uploaded Successfully");
  }, handleFileUpload);

  ElegantOTA.begin(&server);
  server.begin();

  xTaskCreate(serverRoutine, "server & OTA", 2000, NULL, 1, NULL);
  xTaskCreate(binanceRequestRoutine, "reqCoinCurr", 5000, NULL, 5, NULL);
}

void loop() {

}

void binanceRequestRoutine(void *pvParameters) {
  while(true) {
    for (int i = 0; i < conf.binance.num_of_coins; i++) {
      fetchCoinPrice(conf.binance.coin_list[i]);
      vTaskDelay((conf.binance.update_interval_s * 1000) / portTICK_PERIOD_MS);
    }
  }
}

void serverRoutine(void *pvParameters) {
  while (true) {
    server.handleClient();
    ElegantOTA.loop();
  }
}

void listSPIFFSFiles() {
  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("Can't open root dir");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.printf("File: %s  — Size: %d B\n", file.name(), file.size());
    file = root.openNextFile();
  }
}

// TODO: Handle files removal. Maybe via GUI?
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  static File file;

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Upload File Start: %s\n", upload.filename.c_str());
    file = SPIFFS.open(upload.filename.c_str(), "w");
    if (!file) {
      Serial.println("Can't open file for write");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (file) {
      file.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (file) {
      file.close();
      Serial.printf("Upload File End: %s, %u bytes\n", upload.filename.c_str(), upload.totalSize);
    } else {
      Serial.println("Error: File wasn't open");
    }
  }
}
