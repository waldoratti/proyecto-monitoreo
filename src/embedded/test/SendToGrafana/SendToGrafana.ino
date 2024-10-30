#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_SCD30.h>
#include <time.h>

const char* ssid = "ChinaNet-POLLO2_0";
const char* password = "1234554321";

const char* token_grafana = "token:e98697797a6a592e6c886277041e6b95";
const char* url = "http://grafana.altermundi.net:8086/write?db=cto";
const char* INICIALES = "ASC02";

Adafruit_SCD30 scd30;

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (!scd30.begin()) {
    Serial.println("No se pudo inicializar el sensor SCD30!");
    while (1) { delay(10); }
  }
}

void loop() {
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