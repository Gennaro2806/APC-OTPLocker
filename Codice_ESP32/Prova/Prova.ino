#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include "esp_wifi.h"

const char* ssid = "Genny";
const char* password = "x095ac6po89";

const char* botToken = "8636959141:AAHTu06-GSurq364-HspS5j8s8yYP64Rphw";
const char* chatId   = "8263269862";

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectIntervalMs = 10000;

bool wifiConnected = false;

String urlEncode(const String& text) {
  String encoded;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else if (c == '\n') {
      encoded += "%0A";
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WIFI] Associata all'AP");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiConnected = true;
      Serial.println("[WIFI] IP ottenuto");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifiConnected = false;
      Serial.println("[WIFI] Disconnessa");
      break;

    default:
      break;
  }
}

void connectWiFi() {
  Serial.println("[WIFI] Avvio connessione...");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);               // importante con hotspot
  esp_wifi_set_ps(WIFI_PS_NONE);      // ancora più esplicito

  WiFi.begin(ssid, password);
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    return true;
  }

  unsigned long now = millis();
  if (now - lastReconnectAttempt < reconnectIntervalMs) {
    return false;
  }
  lastReconnectAttempt = now;

  Serial.println("[WIFI] Tentativo di reconnessione...");
  WiFi.reconnect();

  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("[WIFI] Riconnessa");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("[WIFI] Timeout reconnessione");
  return false;
}

bool sendTelegramMessage(const String& text) {
  if (!ensureWiFi()) {
    Serial.println("[TELEGRAM] WiFi non disponibile");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[TELEGRAM] HTTPS connect fallita");
    return false;
  }

  String encoded = urlEncode(text);
  String url = "/bot" + String(botToken) +
               "/sendMessage?chat_id=" + String(chatId) +
               "&text=" + encoded;

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.telegram.org\r\n" +
               "User-Agent: ESP32-C3\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long start = millis();
  while (client.connected() && !client.available()) {
    if (millis() - start > 8000) {
      Serial.println("[TELEGRAM] Timeout risposta");
      client.stop();
      return false;
    }
    delay(10);
  }

  bool ok200 = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line.indexOf("200 OK") >= 0) {
      ok200 = true;
    }
  }

  client.stop();
  return ok200;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== AVVIO ESP32-C3 ===");
  WiFi.onEvent(onWiFiEvent);

  connectWiFi();

  unsigned long start = millis();
  while (millis() - start < 20000) {
    if (WiFi.status() == WL_CONNECTED) {
      sendTelegramMessage("ESP32-C3 online.");
      break;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.println();
}

void loop() {
  static unsigned long lastTelegramSend = 0;
  const unsigned long telegramIntervalMs = 30000;

  ensureWiFi();

  if (millis() - lastTelegramSend > telegramIntervalMs) {
    lastTelegramSend = millis();

    if (WiFi.status() == WL_CONNECTED) {
      sendTelegramMessage("Ping periodico ESP32-C3.");
    } else {
      Serial.println("[LOOP] WiFi non connesso");
    }
  }

  delay(1000);
}