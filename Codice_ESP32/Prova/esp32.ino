#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =========================
// UART STM32 <-> ESP32
// =========================
static const int STM32_RX_PIN = 4;   // ESP32 riceve dalla STM32 TX
static const int STM32_TX_PIN = 21;   // ESP32 trasmette verso STM32 RX

String uartBuffer = "";

// =========================
// BLE UUID
// =========================
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define MESSAGE_CHAR_UUID   "12345678-1234-1234-1234-1234567890ac"
#define COMMAND_CHAR_UUID   "12345678-1234-1234-1234-1234567890ad"

// =========================
// CONFIG DEBUG
// =========================
static const bool ENABLE_BLE_PERIODIC_TEST = false;  // metti true per test BLE puro

// =========================
// Oggetti BLE globali
// =========================
BLEServer* pServer = nullptr;
BLEService* pService = nullptr;
BLECharacteristic* messageCharacteristic = nullptr;
BLECharacteristic* commandCharacteristic = nullptr;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// =========================
// BLE Callbacks
// =========================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    Serial.println("[BLE] Smartphone connesso");
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    Serial.println("[BLE] Smartphone disconnesso");
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String value = pCharacteristic->getValue().c_str();

    if (value.length() > 0) {
      Serial.print("[BLE CMD] ");
      Serial.println(value);

      // opzionale: inoltro del comando ricevuto dal telefono verso STM32
      Serial1.print(value + "\n");
    }
  }
};

// =========================
// Helper UART -> STM32
// =========================
void sendToSTM32(const String& msg) {
  Serial1.print(msg + "\n");
  Serial1.flush();  // assicura che il messaggio sia inviato subito
  Serial.print("[UART->STM32] ");
  Serial.println(msg);
}

// =========================
// Helper BLE -> Smartphone
// =========================
void sendBleMessage(const String& msg) {
  Serial.print("[BLE MSG] ");
  Serial.println(msg);

  if (messageCharacteristic == nullptr) {
    Serial.println("[BLE] messageCharacteristic NULL");
    return;
  }

  messageCharacteristic->setValue(msg.c_str());
  Serial.println("[BLE] setValue() OK");

  if (deviceConnected) {
    Serial.println("[BLE] deviceConnected = true, invio notify()");
    messageCharacteristic->notify();
    Serial.println("[BLE] notify() chiamata");
  } else {
    Serial.println("[BLE] Nessun client connesso");
  }
}

// =========================
// Servizi di alto livello
// =========================
void notifyLockdown() {
  sendBleMessage("[ADMIN] Cassaforte in LOCKDOWN. Serve intervento amministratore.");
  sendToSTM32("ACK_LOCKDOWN_SENT");
}

void sendOtpToUser(const String& otp) {
  sendBleMessage("[OTP] Il tuo codice OTP e': " + otp);
  sendToSTM32("ACK_OTP_SENT");
  sendToSTM32("OTP:" + otp);
}

// =========================
// Parser comandi da STM32
// =========================
void handleSTM32Command(String cmd) {
  cmd.trim();

  // Rimuove eventuali caratteri non stampabili all'inizio
  while (cmd.length() > 0 && (uint8_t)cmd[0] < 32) {
    cmd.remove(0, 1);
  }

  Serial.print("[STM32 CMD CLEAN] ");
  Serial.println(cmd);

  int lockdownPos = cmd.indexOf("LOCKDOWN_ALERT");
  int otpPos      = cmd.indexOf("OTP_SEND:");
  int pingPos     = cmd.indexOf("PING");

  if (lockdownPos >= 0) {
    Serial.println("[CMD] LOCKDOWN riconosciuto");
    notifyLockdown();
  }
  else if (otpPos >= 0) {
    String otp = cmd.substring(otpPos + strlen("OTP_SEND:"));
    otp.trim();

    Serial.print("[CMD] OTP estratto: ");
    Serial.println(otp);

    if (otp.length() > 0) {
      sendOtpToUser(otp);
    } else {
      sendToSTM32("ERR_OTP_EMPTY");
    }
  }
  else if (pingPos >= 0) {
    Serial.println("[CMD] PING riconosciuto");
    sendToSTM32("PONG");
  }
  else {
    Serial.println("[CMD] comando sconosciuto");
    sendToSTM32("ERR_UNKNOWN_CMD");
  }
}

// =========================
// Lettura UART da STM32
// =========================
void readSTM32UART() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();

    Serial.print("[RAW HEX] ");
    Serial.println((uint8_t)c, HEX);

    if (c == '\n' || c == '\r') {
      if (uartBuffer.length() > 0) {
        Serial.print("[RAW CMD] ");
        Serial.println(uartBuffer);

        handleSTM32Command(uartBuffer);
        uartBuffer = "";
      }
    } else {
      uartBuffer += c;
    }
  }
}

// =========================
// Setup BLE
// =========================
void setupBLE() {
  BLEDevice::init("SAFEBOX-C3");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);

  // Characteristic messaggi verso smartphone
  messageCharacteristic = pService->createCharacteristic(
    MESSAGE_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE
  );

  BLE2902 *messageDescriptor = new BLE2902();
  messageDescriptor->setNotifications(true);
  messageDescriptor->setIndications(true);
  messageCharacteristic->addDescriptor(messageDescriptor);

  messageCharacteristic->setValue("SAFEBOX READY");

  // Characteristic comandi da smartphone
  commandCharacteristic = pService->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  commandCharacteristic->setCallbacks(new CommandCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  // fix utili per compatibilità
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising attivo: SAFEBOX-C3");
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  delay(1000);
  Serial.println("=== ESP32-C3 BLE GATEWAY START ===");

  setupBLE();
  delay(500);

  // Test diretto una sola volta all'avvio
  // utile per verificare che il telefono riceva
  // collegati prima col telefono e abilita Notify
  // poi premi reset sull'ESP32
  delay(2000);
  sendBleMessage("[TEST] ESP32 BLE READY");
}

// =========================
// Loop
// =========================
void loop() {
  readSTM32UART();

  // gestione reconnessione advertising
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("[BLE] Restart advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  // test BLE periodico opzionale
  if (ENABLE_BLE_PERIODIC_TEST && deviceConnected) {
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 10000) {
      sendBleMessage("[TEST] Notifica BLE periodica OK");
      lastSend = millis();
    }
  }

  delay(10);
}