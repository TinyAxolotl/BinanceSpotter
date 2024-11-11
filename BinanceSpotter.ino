#include <WiFi.h>
#include <WiFiClient.h>

const char* ssid = "wifiname";
const char* password = "pass";

void setup() {
  Serial.begin(115200);
  Serial.println("Binance spotter started");
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nWifi connected!\n");
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {

}
