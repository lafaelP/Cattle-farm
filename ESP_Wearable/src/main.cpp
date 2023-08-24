#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  30

constexpr char WIFI_SSID[] = "WIR-Guest";

uint8_t cowhouse_MAC[] = {0xB0, 0xB2, 0x1C, 0x61, 0x10, 0x40};

typedef struct struct_message_bme680 {
  float randomHeartRate;
  float randomSpO2;
  float location;
} struct_message;

struct_message wearable;
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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

void initESPnow() {
    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    esp_now_register_send_cb(OnDataSent);

    memcpy(peerInfo.peer_addr, cowhouse_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }
}

void dummyData(){
  float randomHeartRate = random(60, 120); 
  float randomSpO2 = random(95, 100) + random(0, 99) / 100.0; 
  float location = random(0, 20);

  wearable.randomHeartRate = randomHeartRate;
  wearable.randomSpO2 = randomSpO2;
  wearable.location = location;
  
  Serial.print("Random Heart Rate: ");
  Serial.print(randomHeartRate);
  Serial.println(" bpm");

  Serial.print("Random SpO2: ");
  Serial.print(randomSpO2, 2);
  Serial.println("%");

  Serial.print("Random location: ");
  Serial.println(location);
}

void setup() {
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
    int32_t channel = getWiFiChannel(WIFI_SSID);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    
    initESPnow();
}

void loop() {
  dummyData();

  esp_err_t result = esp_now_send(cowhouse_MAC, (uint8_t *)&wearable, sizeof(wearable));

    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    delay(1000);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
}
