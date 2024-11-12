#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#define DEBUG_MODE        true
#define CONFIG_FILE_NAME  "/config.json"

WebServer server(80);

const char* ssid = "wifiname";
const char* password = "pass";
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

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.printf("Failed to parse JSON: %f\n", error.c_str());
  }

  configFile.close();

  if (DEBUG_MODE) {
    Serial.printf("Content of %s:\n", CONFIG_FILE_NAME);
    serializeJsonPretty(doc, Serial);
  }
 
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();
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
