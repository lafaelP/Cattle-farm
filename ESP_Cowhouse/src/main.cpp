#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define INFLUXDB_URL "http://172.25.20.88:8086/"
#define INFLUXDB_TOKEN "N-LNqiKNsxCWEHY5_mO-xmE3GmnAqkETI5wyNeMFinr2wSAIcbP5ytf9d_Enfc_zw7cug5G3K3bUjwt-T_RzpA=="
#define INFLUXDB_ORG "318d55f5f9e04263"
#define INFLUXDB_BUCKET "IoT3"
#define TZ_INFO "WIB-7"

const char* ssid = "WIR-Guest";
const char* password = "Guest@WIRgroup";

uint8_t wearable_MAC[] = {0xB0, 0xB2, 0x1C, 0x61, 0x14, 0x84};

const long interval = 60000;
long previousMillis = 0;
unsigned long lastInfluxDBCheckMillis = 0;
const unsigned long influxDBCheckInterval = 10000;

bool newDataReceived = false;

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point WearableReadings("Wearable");
Point CowhouseReadings("Cowhouse");

typedef struct struct_message_bme680 {
  float randomHeartRate;
  float randomSpO2;
  float location;
} struct_message;

struct_message wearable;
esp_now_peer_info_t peerInfo;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&wearable, incomingData, sizeof(wearable));
    float randomHeartRate = wearable.randomHeartRate;
    float randomSpO2 = wearable.randomSpO2;
    float location = wearable.location;

    Serial.print("Random Heart Rate: ");
    Serial.print(randomHeartRate);
    Serial.println(" bpm");

    Serial.print("Random SpO2: ");
    Serial.print(randomSpO2, 2);
    Serial.println("%");

    Serial.print("Random location: ");
    Serial.println(location);

    newDataReceived = true;
}

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

void connectWifi() {
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Setting as a Wi-Fi Station..");
    }
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());

    int32_t channel = getWiFiChannel(ssid);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void connectInflux() {
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    if (client.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }
}

void initESPnow() {
    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    memcpy(peerInfo.peer_addr, wearable_MAC, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
            
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }

    esp_now_register_recv_cb(OnDataRecv);
}

void InfluxWearable() {
    WearableReadings.addField("Heartrate", wearable.randomHeartRate);
    WearableReadings.addField("SpO2", wearable.randomSpO2);
    WearableReadings.addField("Location", wearable.location);

    Serial.print("Writing: ");
    Serial.println(client.pointToLineProtocol(WearableReadings));
    Serial.println();

    client.writePoint(WearableReadings);
    WearableReadings.clearFields();
}

void InfluxHouse() {
    float random1 = random(0, 10);
    float random2 = random(11, 20);
    float random3 = random(21, 30);
    float random4 = random(31, 40);

    Serial.println("Cowhouse data:");
    Serial.print("Random 1: ");
    Serial.println(random1);
    Serial.print("Random 2: ");
    Serial.println(random2);
    Serial.print("Random 3: ");
    Serial.println(random3);
    Serial.print("Random 4: ");
    Serial.println(random4);

    CowhouseReadings.addField("Env1", random1);
    CowhouseReadings.addField("Env2", random2);
    CowhouseReadings.addField("Env3", random3);
    CowhouseReadings.addField("Env4", random4);

    Serial.print("Writing: ");
    Serial.println(client.pointToLineProtocol(CowhouseReadings));
    Serial.println();

    client.writePoint(CowhouseReadings);
    CowhouseReadings.clearFields();
}


void setup() {
    Serial.begin(9600);

    connectWifi();
    connectInflux();
    initESPnow();
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastInfluxDBCheckMillis >= influxDBCheckInterval) {
        lastInfluxDBCheckMillis = currentMillis;

        // Check if the connection is still valid
        if (!client.validateConnection()) {
            Serial.println("InfluxDB connection lost. Reconnecting...");
            connectInflux();
        }
    }  
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

    InfluxHouse();
    }
    if (newDataReceived){
      InfluxWearable();
      newDataReceived = false;
    }
}