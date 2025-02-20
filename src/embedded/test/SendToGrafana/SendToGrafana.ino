#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>
#include <WiFiManager.h>  // New include for WiFi Manager
#include <HTTPUpdate.h>  // New include for HTTP Update
#include <WiFiClientSecure.h>  // Ensure this is included

const char* url = "http://grafana.altermundi.net:8086/write?db=cto";
const char* INICIALES = "ASC02";
const char* token_grafana = "token:e98697797a6a592e6c886277041e6b95";
const char* FIRMWARE_VERSION = "1.0.0";  // Set this to the current version
const char* UPDATE_URL = "http://192.168.0.106:8080/version.txt";  // URL with the latest version info
const char* FIRMWARE_BIN_URL = "http://192.168.0.106:8080/bins/SendToGrafana.ino.bin";

Adafruit_SCD30 scd30;

WiFiManager wifiManager; // New WiFi Manager instance

void setup() {
  Serial.begin(115200);

  // WiFi Manager configuration
  wifiManager.autoConnect("ESP32-AP"); // Create an AP with this SSID if no configuration
  
  Serial.println("Conectado a WiFi");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (!scd30.begin()) {
    Serial.println("No se pudo inicializar el sensor SCD30!");
    //while (1) { delay(10); }
  }
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}


void checkForUpdates() {
  WiFiClient client;
  HTTPClient http;

  Serial.println("Checking for firmware updates...");
  
  if (http.begin(client, UPDATE_URL)) {  
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) { 
          String latestVersion = http.getString();
          latestVersion.trim();

          Serial.printf("Current version: %s, Available version: %s\n", FIRMWARE_VERSION, latestVersion.c_str());

          if (latestVersion != FIRMWARE_VERSION) {
              Serial.println("New firmware available! Updating...");
              
              httpUpdate.onStart(update_started);
              httpUpdate.onEnd(update_finished);
              httpUpdate.onProgress(update_progress);
              httpUpdate.onError(update_error);

              t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_BIN_URL);

              if (ret == HTTP_UPDATE_OK) {
                  Serial.println("Update successful!");
              } else {
                  Serial.printf("Update failed: %d\n", httpUpdate.getLastError());
              }
          } else {
              Serial.println("Firmware is up to date. Skipping update.");
          }
      } else {
          Serial.printf("Failed to check update: HTTP error %d\n", httpCode);
      }
      http.end();
  }
}


void loop() {
  
  //static unsigned long lastCheck = 0;
  //const unsigned long updateInterval = 24*3600000;  // Check every day

//  if (millis() - lastCheck > updateInterval) {
//      lastCheck = millis();
      checkForUpdates();
//  }
  
  float temperature = 99;
  float humidity = 100;
  float co2 = 999999;

  if (scd30.dataReady()) {
    if (!scd30.read()) {
      Serial.println("Error leyendo el sensor!");
      return;
    }

    float temperature = scd30.temperature;
    float humidity = scd30.relative_humidity;
    float co2 = scd30.CO2;
  }
  Serial.printf("Free heap before send_data_grafana: %d bytes\n", ESP.getFreeHeap());
  send_data_grafana(temperature, humidity, co2);
  Serial.printf("Free heap AFTER  send_data_grafana: %d bytes\n", ESP.getFreeHeap());
  delay(10000); // Esperar 10 segundos antes de la próxima lectura
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
   //client.setInsecure(); // Disable SSL certificate verification (if using HTTPS)

    String data = create_grafana_message(temperature, humidity, co2);

    http.begin(client, url);
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Authorization", "Basic " + String(token_grafana));

    int httpResponseCode = http.POST(data);

    if (httpResponseCode == 204) {
      Serial.println("Datos enviados correctamente");
    } else {
      Serial.print("Error en el envío: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("Error en la conexión WiFi");
  }
}
