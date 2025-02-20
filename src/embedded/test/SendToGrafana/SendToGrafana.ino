#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>
#include <WiFiManager.h>  // New include for WiFi Manager
#include <HTTPUpdate.h>  // New include for HTTP Update
#include <WiFiClientSecure.h>  // Ensure this is included
#include <esp_ota_ops.h>
#include <ArduinoJson.h> // Make sure you install the ArduinoJson library

const char* url = "http://grafana.altermundi.net:8086/write?db=cto";
const char* INICIALES = "ASC02";
const char* token_grafana = "token:e98697797a6a592e6c886277041e6b95";
const char* FIRMWARE_VERSION = "1.0.0";  // Set this to the current version
const char* UPDATE_URL = "http://192.168.0.106:8080/version.txt";  // URL with the latest version info
const char* FIRMWARE_BIN_URL = "http://192.168.0.106:8080/bins/SendToGrafana.ino.bin";
const char* YOUR_GITHUB_USERNAME = "AlterMundi-MonitoreoyControl";
const char* YOUR_REPO_NAME = "proyecto-monitoreo";

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

void printPartitionDetails(const esp_partition_t* partition) {
  if (partition == nullptr) {
      Serial.println("Partition is null.");
      return;
  }

  Serial.printf("Partition Details:\n");
  Serial.printf("  Type: %s\n", partition->type == ESP_PARTITION_TYPE_APP ? "App" : "Data");
  Serial.printf("  SubType: %d\n", partition->subtype);
  Serial.printf("  Address: 0x%08X\n", partition->address);
  Serial.printf("  Size: %d bytes\n", partition->size);
  Serial.printf("  Label: %s\n", partition->label);
}


String getLatestReleaseTag(const char* repoOwner, const char* repoName) {
  WiFiClientSecure client;
  //client.setCACert(github_cert); // Or 
  client.setInsecure();// for testing ONLY (unsafe!)
  
  HTTPClient http;

  String apiUrl = "https://api.github.com/repos/" + String(repoOwner) + "/" + String(repoName) + "/releases/latest";
  Serial.println("API URL: " + apiUrl);

  if (http.begin(client, apiUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();

      // Find the "tag_name" within the JSON string
      int startIndex = payload.indexOf("\"tag_name\": \"");
      if (startIndex != -1) {
        startIndex += 12; // Move past the "tag_name": " characters
        int endIndex = payload.indexOf("\"", startIndex);
        if (endIndex != -1) {
          return payload.substring(startIndex, endIndex);
        }
      }
      return ""; // Return empty string if "tag_name" not found
    } else {
      Serial.printf("HTTP GET failed, error: %d\n", httpCode);
      http.end();
      return "";
    }
  } else {
    Serial.println("Unable to connect to GitHub API.");
    return "";
  }
}

void checkForUpdates() {
  WiFiClient client;
  HTTPClient http;
  HTTPUpdate httpUpdate;  // Declare the HTTPUpdate instance
  String latestTag = getLatestReleaseTag("AlterMundi-MonitoreoyControl","proyecto-monitoreo"); // Replace with your repo details  Serial.println("Checking for firmware updates...");
  


  if (http.begin(client, UPDATE_URL)) {  
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) { 
          String latestVersion = http.getString();
          latestVersion.trim();

          Serial.printf("Current version: %s, Available version: %s\n", FIRMWARE_VERSION, latestVersion.c_str());

          if (latestVersion != FIRMWARE_VERSION) {
              // Get the currently running partition
              const esp_partition_t* running_partition = esp_ota_get_running_partition();
              Serial.printf("Running partition:\n");
              printPartitionDetails(running_partition);

              esp_ota_handle_t update_handle = 0;
              const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
              Serial.printf("UPDATE partition:\n");
              printPartitionDetails(update_partition);

              Serial.println("New firmware available! Updating...");
              
              httpUpdate.onStart(update_started);
              httpUpdate.onEnd(update_finished);
              httpUpdate.onProgress(update_progress);
              httpUpdate.onError(update_error);
              httpUpdate.setLedPin(2, LOW);
              //automatically uses for the next partition to boot...
              //t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_BIN_URL);
              String firmwareURL = "https://github.com/" + String(YOUR_GITHUB_USERNAME) + "/" + String(YOUR_REPO_NAME) + "/releases/download/" + latestTag + "/firmware.bin";
              t_httpUpdate_return ret = httpUpdate.update(client, firmwareURL);

              if (ret == HTTP_UPDATE_OK) {
                  Serial.println("Update successful!");
                  esp_ota_set_boot_partition(update_partition);
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
  } else {
      Serial.println("Unable to connect to update URL.");
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
