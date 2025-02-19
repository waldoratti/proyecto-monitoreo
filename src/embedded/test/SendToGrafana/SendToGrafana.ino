#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>
#include <WiFiManager.h>  // New include for WiFi Manager
#include <ESP32HTTPUpdate.h>  // New include for HTTP Update

const char* url = "http://grafana.altermundi.net:8086/write?db=cto";
const char* INICIALES = "ASC02";

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
    while (1) { delay(10); }
  }
}

void loop() {
  // Check for OTA updates
  t_httpUpdate_return ret = httpUpdate.update("https://github.com/AlterMundi-MonitoreoyControl/proyecto-monitoreo/releases/download/release-11/SendToGrafana.ino.bin");
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }

  if (scd30.dataReady()) {
    if (!scd30.read()) {
      Serial.println("Error leyendo el sensor!");
      return;
    }

    float temperature = scd30.temperature;
    float humidity = scd30.relative_humidity;
    float co2 = scd30.CO2;

    send_data_grafana(temperature, humidity, co2);
  }

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
