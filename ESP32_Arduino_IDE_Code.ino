/*
==============================================================
FireGuard IoT
Cloud-Based Fire Detection and Alert System
Platform : ESP32
Framework: Arduino
MQTT Broker : EMQX Cloud (TLS)

Sensors:
MQ-2 Smoke Sensor
DHT22 Temperature & Humidity Sensor

Outputs:
Red LED
Active Buzzer

Cloud Services:
EMQX Cloud MQTT Broker
Node-RED
MongoDB Atlas

MQTT Publish Topic: iot/sensor/data

MQTT Subscribe Topic: iot/sensor/config
==============================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ==========================================================
// WiFi Configuration
// ==========================================================
const char* WIFI_SSID = "yeah_esp32";
const char* WIFI_PASSWORD = "JINITAIMEI";

// ==========================================================
// MQTT Configuration
// ==========================================================
const char* MQTT_SERVER = "v6612156.ala.asia-southeast1.emqxsl.com";
const int MQTT_PORT = 8883;
const char* MQTT_USERNAME = "utem-iot-esp32";
const char* MQTT_PASSWORD = "esp32";
const char* MQTT_PUBLISH_TOPIC = "iot/sensor/data";
const char* MQTT_SUBSCRIBE_TOPIC = "iot/sensor/config";
const char* DEVICE_ID = "ESP32-01";

// ==========================================================
// GPIO Configuration
// ==========================================================
#define DHTPIN 4
#define DHTTYPE DHT22
#define MQ2_PIN 34
#define LED_PIN 18
#define BUZZER_PIN 19

// ==========================================================
// Default Configuration
// ==========================================================
int smokeThreshold = 1500;
unsigned long detectInterval = 10000;

// ==========================================================
// Fire Detection Parameters
// ==========================================================
const float FIRE_TEMPERATURE = 35.0;

// ==========================================================
// Sensor Variables
// ==========================================================
float temperature = 0.0;
float humidity = 0.0;
int smokeRaw = 0;
bool fireDetected = false;

// ==========================================================
// Timing
// ==========================================================
unsigned long previousMillis = 0;

// ==========================================================
// WiFi / MQTT Objects
// ==========================================================
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
DHT dht(DHTPIN, DHTTYPE);

// ==========================================================
// Function Prototypes
// ==========================================================
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void readSensors();
void evaluateFire();
void updateAlarm();
void publishSensorData();
void printSensorReport(bool publishStatus);
String getRiskLevel();
void blinkLED();

// ==========================================================
// WiFi Connection
// ==========================================================
void connectWiFi() {
  Serial.println();
  Serial.println("==========================================");
  Serial.println("ESP32 Fire Monitoring System");
  Serial.println("==========================================");

  Serial.print("[WiFi] Connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WiFi] Connected successfully!");
  Serial.print("[WiFi] IP Address : ");
  Serial.println(WiFi.localIP());

  Serial.println();
}

// ==========================================================
// MQTT Callback
// ==========================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.println("--- Configuration Message Received ---");

  String message;

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.println("Invalid JSON received.");
    return;
  }

  if (doc.containsKey("smokeThreshold")) {
    smokeThreshold = doc["smokeThreshold"];
  }

  if (doc.containsKey("detectInterval")) {
    detectInterval = doc["detectInterval"];
  }

  Serial.print("Updated smokeThreshold: ");
  Serial.println(smokeThreshold);

  Serial.print("Updated detectInterval: ");
  Serial.println(detectInterval);

  Serial.println("[Bi-Directional] New configuration will be applied in the next monitoring cycle.");
  Serial.println();
}

// ==========================================================
// MQTT Connection
// ==========================================================
void connectMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  mqttClient.setCallback(mqttCallback);

  secureClient.setInsecure();

  while (!mqttClient.connected()) {
    Serial.println("[MQTT] Attempting connection...");

    if (mqttClient.connect(
          DEVICE_ID,
          MQTT_USERNAME,
          MQTT_PASSWORD)) {
      Serial.println("[MQTT] Connected successfully!");

      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC);

      Serial.print("[MQTT] Subscribed : ");
      Serial.println(MQTT_SUBSCRIBE_TOPIC);

      Serial.println();
    } else {
      Serial.print("[MQTT] Failed. State = ");
      Serial.println(mqttClient.state());

      Serial.println("[MQTT] Retry in 5 seconds...\n");

      delay(5000);
    }
  }
}

// ==========================================================
// Maintain Connections
// ==========================================================
void maintainConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();
}

// ==========================================================
// Read Sensors
// ==========================================================
void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  smokeRaw = analogRead(MQ2_PIN);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[ERROR] Failed to read DHT22!");

    temperature = 0.0;
    humidity = 0.0;
  }
}

// ==========================================================
// Fire Detection
// ==========================================================
void evaluateFire() {
  fireDetected =
    (temperature >= FIRE_TEMPERATURE) && (smokeRaw >= smokeThreshold);
}

// ==========================================================
// Risk Level
// ==========================================================
String getRiskLevel() {
  if (fireDetected)
    return "HIGH";

  if (smokeRaw >= smokeThreshold * 0.75)
    return "MEDIUM";

  return "LOW";
}

// ==========================================================
// LED Blink
// ==========================================================
void blinkLED() {
  digitalWrite(LED_PIN, HIGH);

  delay(100);

  digitalWrite(LED_PIN, LOW);
}

// ==========================================================
// Alarm Control
// ==========================================================
void updateAlarm() {
  if (fireDetected) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ==========================================================
// Publish Sensor Data
// ==========================================================
void publishSensorData() {
  StaticJsonDocument<256> doc;

  doc["device_id"] = DEVICE_ID;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["smoke"] = fireDetected;
  doc["smoke_raw"] = smokeRaw;

  char payload[256];

  serializeJson(doc, payload);

  bool status = mqttClient.publish(
    MQTT_PUBLISH_TOPIC,
    payload);

  printSensorReport(status);

  if (!fireDetected) {
    if (status) {
      blinkLED();
    }
  }
}

// ==========================================================
// Pretty Serial Report
// ==========================================================
void printSensorReport(bool publishStatus) {
  Serial.println("========== Sensor Report ==========");

  Serial.print("Temperature   : ");
  Serial.print(temperature, 2);
  Serial.println(" \xC2\xB0"
                 "C");

  Serial.print("Humidity      : ");
  Serial.print(humidity, 2);
  Serial.println(" %");

  Serial.print("Smoke Raw     : ");
  Serial.print(smokeRaw);
  Serial.print(" (Threshold: ");
  Serial.print(smokeThreshold);
  Serial.println(")");

  Serial.print("Fire Detected : ");
  Serial.println(fireDetected ? "YES" : "NO");

  Serial.print("Risk Level    : ");
  Serial.println(getRiskLevel());

  Serial.print("Alarm Active  : ");

  if (fireDetected)
    Serial.println("🔴 FIRE");
  else
    Serial.println("🟢 SAFE");

  Serial.print("MQTT Publish  : ");

  if (publishStatus)
    Serial.println("SUCCESS");
  else
    Serial.println("FAILED");

  StaticJsonDocument<256> doc;

  doc["device_id"] = DEVICE_ID;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["smoke"] = fireDetected;
  doc["smoke_raw"] = smokeRaw;

  Serial.print("Payload Size  : ");
  Serial.print(measureJson(doc));
  Serial.println(" bytes");

  serializeJson(doc, Serial);

  Serial.println();

  Serial.println("=========================================");
  Serial.println();
}

// ==========================================================
// setup()
// ==========================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  connectWiFi();

  connectMQTT();
}

// ==========================================================
// loop()
// ==========================================================
void loop() {
  maintainConnection();

  if (millis() - previousMillis >= detectInterval) {
    previousMillis = millis();

    readSensors();

    evaluateFire();

    updateAlarm();

    publishSensorData();
  }
}