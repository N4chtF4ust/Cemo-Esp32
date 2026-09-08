#include "ble_sensor.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <Arduino.h>

#include "hx711_sensor.h"
#include "dht_sensor.h"

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "abcd1234-ab12-ab12-ab12-abcdef123456"
#define CMD_CHAR_UUID       "abcd1234-ab12-ab12-ab12-abcdef123457"
#define DEVICE_NAME         "CEMO-IOT"
#define CLIENT_TIMEOUT_MS   120000  // allow long sensor read jitter before any forced recovery
#define BLE_NOTIFY_INTERVAL_MS 1000

static BLECharacteristic* pSensorChar     = nullptr;
static BLECharacteristic* pCommandChar    = nullptr;
static bool               deviceConnected = false;
static unsigned long      lastNotifyMs    = 0;
static unsigned long      lastActivityMs  = 0;
static String             pendingCommand  = "";
static String             pendingEvent    = "";

// ───────────────────────── SECURITY ─────────────────────────

class CemoSecurityCallbacks : public BLESecurityCallbacks {
    bool onConfirmPIN(uint32_t pin) override {
        return true;
    }

    uint32_t onPassKeyRequest() override {
        return 0;
    }

    void onPassKeyNotify(uint32_t pass_key) override {}

    bool onSecurityRequest() override {
        return true;
    }
};

// ───────────────────────── SERVER CALLBACKS ─────────────────────────

class CemoServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        lastActivityMs  = millis();  // reset watchdog on connect
        BLEDevice::setMTU(185);
        Serial.println("[BLE] Connected");
    }

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        lastActivityMs  = 0;         // clear watchdog on disconnect
        Serial.println("[BLE] Disconnected → restarting advertising");
        BLEDevice::startAdvertising();
    }
};

// ───────────────────────── COMMAND CALLBACKS ─────────────────────────

class CemoCommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        String cmd = characteristic->getValue();
        cmd.trim();
        if (cmd.length() == 0) return;

        pendingCommand = cmd;
        lastActivityMs = millis();
        Serial.print("[BLE] Command received: ");
        Serial.println(cmd);
    }
};

// ───────────────────────── INIT ─────────────────────────

void ble_init() {
    BLEDevice::init(DEVICE_NAME);
    BLEDevice::setMTU(185);

    // Security (triggers Android pairing popup — Android drives bond)
    BLEDevice::setSecurityCallbacks(new CemoSecurityCallbacks());

    BLESecurity* security = new BLESecurity();

    security->setAuthenticationMode(
        ESP_LE_AUTH_REQ_SC_BOND
    );

    security->setCapability(ESP_IO_CAP_NONE);

    security->setInitEncryptionKey(
        ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK
    );

    security->setRespEncryptionKey(
        ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK
    );

    // Server
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new CemoServerCallbacks());

    BLEService* service = server->createService(SERVICE_UUID);

    pSensorChar = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCommandChar = service->createCharacteristic(
        CMD_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCommandChar->setCallbacks(new CemoCommandCallbacks());

    pSensorChar->addDescriptor(new BLE2902());

    service->start();

    // Advertising
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);

    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising started");
}

// ───────────────────────── UPDATE LOOP ─────────────────────────

void ble_update() {
    // Safety watchdog for extreme stalls.
    if (deviceConnected && lastActivityMs > 0 &&
        (millis() - lastActivityMs > CLIENT_TIMEOUT_MS)) {
        Serial.println("[BLE] Watchdog timeout — restarting advertising");
        deviceConnected = false;
        lastActivityMs  = 0;
        BLEDevice::startAdvertising();
        return;
    }

    if (!deviceConnected) return;

    if (pendingCommand.length() > 0) {
        String cmd = pendingCommand;
        pendingCommand = "";
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "TARE") {
            // Only fails if the HX711 is missing entirely.
            pendingEvent = hx711_tare() ? "TARE_DONE" : "TARE_FAILED";
        } else if (cmd == "RESETCAL") {
            hx711_resetToDefault();
            pendingEvent = "RESETCAL_DONE";
        } else {
            Serial.print("[BLE] Unknown command: ");
            Serial.println(cmd);
        }
    }

    // Push pending events (e.g. TARE_DONE) immediately so the app's loading
    // state clears the moment the tare finishes; otherwise throttle notifies.
    if (pendingEvent.length() == 0 && (millis() - lastNotifyMs < BLE_NOTIFY_INTERVAL_MS)) return;
    lastNotifyMs = millis();

    // Refresh sensors on the BLE path so payloads are independent from serial print mode.
    dht_update();

    float weight = scaleConnected ? (readWeightSafe(READINGS_AVERAGE_FAST) / 1000.0f) : 0.0f;
    float temp   = dhtOK ? sensorData.temperature : 0.0f;
    float hum    = dhtOK ? sensorData.humidity : 0.0f;

    char payload[170];
    if (pendingEvent.length() > 0) {
        snprintf(payload, sizeof(payload),
            "WEIGHT:%.2f,TEMP:%.1f,HUM:%.1f,EVENT:%s",
            weight, temp, hum, pendingEvent.c_str()
        );
        pendingEvent = "";
    } else {
        snprintf(payload, sizeof(payload),
            "WEIGHT:%.2f,TEMP:%.1f,HUM:%.1f",
            weight, temp, hum
        );
    }

    pSensorChar->setValue((uint8_t*)payload, strlen(payload));
    pSensorChar->notify();

    // Stamp activity after each successful notify
    lastActivityMs = millis();

    Serial.println(payload);
}

// ───────────────────────── STATUS ─────────────────────────

bool ble_isConnected() {
    return deviceConnected;
}
