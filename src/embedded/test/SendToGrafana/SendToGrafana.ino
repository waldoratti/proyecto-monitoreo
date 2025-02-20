#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>
#include <WiFiManager.h>  // New include for WiFi Manager
#include <HTTPUpdate.h>  // New include for HTTP Update

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

void loop() {
  // Check for OTA updates
  NetworkClient client;
  
  // The line below is optional. It can be used to blink the LED on the board during flashing
  // The LED will be on during download of one buffer of data from the network. The LED will
  // be off during writing that buffer to flash
  // On a good connection the LED should flash regularly. On a bad connection the LED will be
  // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
  // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
  // httpUpdate.setLedPin(LED_BUILTIN, LOW);
  
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  
  t_httpUpdate_return ret = httpUpdate.update(client, "https://github.com/AlterMundi-MonitoreoyControl/proyecto-monitoreo/releases/download/release-11/SendToGrafana.ino.bin");
  // Or:
  //t_httpUpdate_return ret = httpUpdate.update(client, "server", 80, "/file.bin");
  
  switch (ret) {
    case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str()); break;
  
    case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); break;
  
    case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK"); break;
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
