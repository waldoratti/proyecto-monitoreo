#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>
#include <WiFiManager.h> 
#include <HTTPUpdate.h>  
#include <WiFiClientSecure.h>  
#include <esp_ota_ops.h>
#include "version.h"
#include <ArduinoJson.h>

const char* url = "http://grafana.altermundi.net:8086/write?db=cto";
const char* INICIALES = "ASC02";
const char* token_grafana = "token:e98697797a6a592e6c886277041e6b95";
const char* UPDATE_URL = "http://192.168.0.106:8080/version.txt";  
const char* FIRMWARE_BIN_URL = "http://192.168.0.106:8080/bins/SendToGrafana.ino.bin";
const char* YOUR_GITHUB_USERNAME = "AlterMundi-MonitoreoyControl";
const char* YOUR_REPO_NAME = "proyecto-monitoreo";

Adafruit_SCD30 scd30;
WiFiManager wifiManager; 
WiFiClientSecure clientSecure;  
void setup() {
  Serial.begin(115200);
  wifiManager.autoConnect("ESP32-AP"); 

  Serial.println("Conectado a WiFi");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (!scd30.begin()) {
    Serial.println("No se pudo inicializar el sensor SCD30!");
  }

  clientSecure.setInsecure(); 
}

String getLatestReleaseTag(const char* repoOwner, const char* repoName) {
  HTTPClient http;
  String apiUrl = "https://api.github.com/repos/" + String(repoOwner) + "/" + String(repoName) + "/releases/latest";
  Serial.println("API URL: " + apiUrl);

  if (http.begin(clientSecure, apiUrl)) {
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();  

      // Manually find "tag_name": "..."
      int tagPos = payload.indexOf("\"tag_name\":");
      if (tagPos != -1) {
        int startQuote = payload.indexOf("\"", tagPos + 11);  // Skip `"tag_name":`
        int endQuote = payload.indexOf("\"", startQuote + 1);
        if (startQuote != -1 && endQuote != -1) {
          return payload.substring(startQuote + 1, endQuote);
        }
      }
      
      Serial.println("Tag not found in JSON!");
    } else {
      Serial.printf("HTTP GET failed, error: %d\n", httpCode);
    }

    http.end();  
  } else {
    Serial.println("Unable to connect to GitHub API.");
  }
  return "";
}


void checkForUpdates() {
  HTTPClient http;
  String latestTag = getLatestReleaseTag(YOUR_GITHUB_USERNAME, YOUR_REPO_NAME);
  Serial.printf("Current version: %s, Available version: %s\n", FIRMWARE_VERSION, latestTag.c_str());

  if (http.begin(clientSecure, UPDATE_URL)) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) { 
      String latestVersion = http.getString();
      latestVersion.trim();
      http.end(); 

      if (latestTag != FIRMWARE_VERSION) {
        const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
        HTTPUpdate httpUpdate;
        
        httpUpdate.onStart([](){ Serial.println("Update started..."); });
        httpUpdate.onEnd([](){ Serial.println("Update finished!"); });
        httpUpdate.onProgress([](int cur, int total) { Serial.printf("Update progress: %d/%d\n", cur, total); });
        httpUpdate.onError([](int err) { Serial.printf("Update error: %d\n", err); });

        String firmwareURL = "https://github.com/" + String(YOUR_GITHUB_USERNAME) + "/" + String(YOUR_REPO_NAME) + "/releases/download/" + latestTag + "/firmware.bin";
        t_httpUpdate_return ret = httpUpdate.update(clientSecure, firmwareURL);

        if (ret == HTTP_UPDATE_OK) {
          Serial.println("Update successful!");
          esp_ota_set_boot_partition(update_partition);
        } else {
          Serial.printf("Update failed: %d\n", httpUpdate.getLastError());
        }
      } else {
        Serial.println("Firmware is up to date.");
      }
    } else {
      Serial.printf("Update check failed: HTTP error %d\n", httpCode);
    }
  }
  http.end();
}

void loop() {
  checkForUpdates();

  float temperature = 99, humidity = 100, co2 = 999999;

  if (scd30.dataReady()) {
    if (!scd30.read()) {
      Serial.println("Error leyendo el sensor!");
      return;
    }
    temperature = scd30.temperature;
    humidity = scd30.relative_humidity;
    co2 = scd30.CO2;
  }

  Serial.printf("Free heap before sending: %d bytes\n", ESP.getFreeHeap());
  send_data_grafana(temperature, humidity, co2);
  Serial.printf("Free heap after sending: %d bytes\n", ESP.getFreeHeap());

  delay(10000);
}

String create_grafana_message(float temperature, float humidity, float co2) {
  char buffer[150];
  unsigned long long timestamp = time(nullptr) * 1000000000ULL;
  snprintf(buffer, sizeof(buffer), "mediciones,device=%s temp=%.2f,hum=%.2f,co2=%.2f %llu", INICIALES, temperature, humidity, co2, timestamp);
  return String(buffer);
}

void send_data_grafana(float temperature, float humidity, float co2) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
    String data = create_grafana_message(temperature, humidity, co2);

    http.begin(client, url);
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Authorization", "Basic " + String(token_grafana));

    int httpResponseCode = http.POST(data);

    if (httpResponseCode == 204) {
      Serial.println("Datos enviados correctamente");
    } else {
      Serial.printf("Error en el envío: %d\n", httpResponseCode);
    }

    http.end();  
  } else {
    Serial.println("Error en la conexión WiFi");
  }
}
