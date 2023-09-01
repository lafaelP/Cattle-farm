#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "FS.h"
#include "SD.h"

#define INFLUXDB_URL "http://172.25.20.88:8086/"
#define INFLUXDB_TOKEN "N-LNqiKNsxCWEHY5_mO-xmE3GmnAqkETI5wyNeMFinr2wSAIcbP5ytf9d_Enfc_zw7cug5G3K3bUjwt-T_RzpA=="
#define INFLUXDB_ORG "318d55f5f9e04263"
#define INFLUXDB_BUCKET "IoT3"
#define TZ_INFO "WIB-7"

const char* ssid = "WIR-Guest";
const char* password = "Guest@WIRgroup";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7*60*60;
const int   daylightOffset_sec = 0;

uint8_t wearable_MAC[] = {0xB0, 0xB2, 0x1C, 0x61, 0x14, 0x84};

const long interval = 60000;
long previousMillis = 0;
unsigned long lastInfluxDBCheckMillis = 0;
const unsigned long influxDBCheckInterval = 50000;

float random1, random2, random3, random4;

bool newDataReceived = false;
bool uploadedFromSD = false;

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

    if (client.validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else {
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
            
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }

    esp_now_register_recv_cb(OnDataRecv);
}

void initTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void initSD() {
    if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%d %B %Y %H:%M:%S");
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void dataHouse() {
    random1 = random(0, 10);
    random2 = random(11, 20);
    random3 = random(21, 30);
    random4 = random(31, 40);

    Serial.println("Cowhouse data:");
    Serial.print("Random 1: ");
    Serial.println(random1);
    Serial.print("Random 2: ");
    Serial.println(random2);
    Serial.print("Random 3: ");
    Serial.println(random3);
    Serial.print("Random 4: ");
    Serial.println(random4);
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

void SDwearable() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }

    char timestamp[40];
    strftime(timestamp, sizeof(timestamp), "%d %B %Y %H:%M:%S", &timeinfo);

    String wearableData = String(timestamp); 
    wearableData += ",";
    wearableData += String(wearable.randomHeartRate);
    wearableData += ",";
    wearableData += String(wearable.randomSpO2);
    wearableData += ",";
    wearableData += String(wearable.location);
    wearableData += "\n";

    appendFile(SD, "/wearable_data.txt", wearableData.c_str());
}

void SDcowhouse() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }

    char timestamp[40];
    strftime(timestamp, sizeof(timestamp), "%d %B %Y %H:%M:%S", &timeinfo);

    String cowhouseData = String(timestamp);
    cowhouseData += ",";
    cowhouseData += String(random1);
    cowhouseData += ",";
    cowhouseData += String(random2);
    cowhouseData += ",";
    cowhouseData += String(random3);
    cowhouseData += ",";
    cowhouseData += String(random4);
    cowhouseData += "\n";

    appendFile(SD, "/cowhouse_data.txt", cowhouseData.c_str());
}

void readSDwearable(fs::FS &fs, const char * path, InfluxDBClient &client, Point &WearableReadings) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  while(file.available()){
    String line = file.readStringUntil('\n');
    line.trim();

    char timestamp[40];
    float heartRate, spO2, location;
    sscanf(line.c_str(), "%[^,],%f,%f,%f", timestamp, &heartRate, &spO2, &location);

    WearableReadings.addField("HeartRate", heartRate);
    WearableReadings.addField("SpO2", spO2);
    WearableReadings.addField("Location", location);

    if (client.writePoint(WearableReadings)) {
      Serial.println("Data uploaded to InfluxDB");
    } else {
      Serial.println("Failed to upload data to InfluxDB");
    }
  }
  file.close();
}

void readSDcowhouse(fs::FS &fs, const char * path, InfluxDBClient &client, Point &CowhouseReadings) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  while(file.available()){
    String line = file.readStringUntil('\n');
    line.trim();

    char timestamp[40];
    float random1, random2, random3, random4;
    sscanf(line.c_str(), "%[^,],%f,%f,%f,%f", &random1, &random2, &random3, &random4);

    CowhouseReadings.addField("Env1", random1);
    CowhouseReadings.addField("Env2", random2);
    CowhouseReadings.addField("Env3", random3);
    CowhouseReadings.addField("Env4", random4);

    if (client.writePoint(CowhouseReadings)) {
      Serial.println("Data uploaded to InfluxDB");
    } else {
      Serial.println("Failed to upload data to InfluxDB");
    }
  }
  file.close();
}

void setup() {
    Serial.begin(9600);

    connectWifi();
    connectInflux();
    initESPnow();
    initTime();
    initSD();
    writeFile(SD, "/wearable_data.txt", "Heart Rate, SpO2, Location \r\n");
    writeFile(SD, "/cowhouse_data.txt", "random1,random2,random3,random4 \r\n");
}

void loop() {
    unsigned long currentMillis = millis();

    if (!client.validateConnection()) {
        Serial.println("InfluxDB connection lost. Reconnecting...");
        connectInflux();

        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            dataHouse();
            SDcowhouse();
        }
        if (newDataReceived){
            SDwearable();
            newDataReceived = false;
        }
        
        uploadedFromSD = false;
    }
    else {
        if (!uploadedFromSD) {
            readSDwearable(SD, "/wearable_data.txt", client, WearableReadings);
            readSDcowhouse(SD, "/cowhouse_data.txt", client, CowhouseReadings);
            deleteFile(SD, "/werable_data.txt");
            deleteFile(SD, "/cowhouse_data.txt");
            uploadedFromSD = true;
        }

        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            dataHouse();
            InfluxHouse();
        }
        if (newDataReceived){
          InfluxWearable();
          newDataReceived = false;
        }
    }
}