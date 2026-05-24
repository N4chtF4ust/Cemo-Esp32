// command_handler.cpp
#include "command_handler.h"
#include "hx711_sensor.h"
#include "dht_sensor.h"
#include "ina226_sensor.h"
#include "max17043_sensor.h"

CommandHandler cmdHandler;

void CommandHandler::begin() {
  inputString = "";
  stringComplete = false;
  printEnabled = true;
  lastPrintTime = 0;
  
  Serial.println("\n\n=== WASTE BIN MONITOR ===");
  Serial.println("Empty baseline is saved in flash");
  Serial.println("Scale auto-tares on startup");
  Serial.println();
  printHelp();
  Serial.println();
}

void CommandHandler::printHelp() {
  Serial.println("=== AVAILABLE COMMANDS ===");
  Serial.println("TARE      - Re-calibrate empty baseline (NO weight on scale)");
  Serial.println("CALIBRATE [g] - Auto-calibrate with known weight in grams (default: 1000)");
  Serial.println("RESETCAL  - Reset calibration (restart required)");
  Serial.println("START     - Start continuous printing");
  Serial.println("STOP      - Stop continuous printing");
  Serial.println("ONCE      - Print single reading");
  Serial.println("STATUS    - Show system status");
  Serial.println("HELP      - Show this help");
  Serial.println("==========================");
}

void CommandHandler::printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("Printing: "); Serial.println(printEnabled ? "ENABLED" : "DISABLED");
  Serial.print("Scale Connected: "); Serial.println(scaleConnected ? "YES" : "NO");
  if (scaleConnected) {
    Serial.print("Current Weight: "); Serial.print(scale.get_units(5), 1); Serial.println(" g");
    Serial.print("Raw Reading: "); Serial.println(scale.read_average(5));
  }
  Serial.print("DHT11: "); Serial.println(dhtOK ? "OK" : "FAIL");
  Serial.print("INA226: "); Serial.println(inaConnected ? "OK" : "FAIL");
  Serial.print("MAX17043: "); Serial.println(max17043Connected ? "OK" : "FAIL");
  Serial.println("====================\n");
}

void CommandHandler::printSensorDataOnce() {
  Serial.println("\n=== SINGLE READING ===");
  dht_update();
  float waste = readWeightSafe();
  ina226_read();
  max17043_read();
  
  Serial.println("===== SENSOR DATA =====");
  if (scaleConnected) {
    Serial.print("Waste    : "); Serial.print(waste, 2); Serial.println(" g");
  } else {
    Serial.println("Waste    : HX711 not connected");
  }
  if (dhtOK) {
    Serial.print("Temp     : "); Serial.print(sensorData.temperature, 2); Serial.println(" C");
    Serial.print("Humidity : "); Serial.print(sensorData.humidity, 1);    Serial.println(" %");
  } else {
    Serial.println("Temp     : DHT11 no reading");
    Serial.println("Humidity : DHT11 no reading");
  }
  if (inaConnected) {
    float displayCurrent = ina226_getDisplayCurrent();
    String sign = (displayCurrent > 0) ? "+" : (displayCurrent < 0) ? "-" : "";
    Serial.print("Voltage  : "); Serial.print(loadVoltage_V, 3); Serial.println(" V");
    Serial.print("Current  : "); Serial.print(sign); Serial.print(abs(displayCurrent), 2); Serial.println(" mA");
    Serial.print("Status   : "); Serial.println(ina226_getStatus());
  } else {
    Serial.println("INA226   : not connected");
  }
  if (max17043Connected) {
    Serial.print("Batt SOC : "); Serial.print(max17043_getSoc(), 1); Serial.println(" %");
    Serial.print("Batt V   : "); Serial.print(batteryVoltage_V, 3); Serial.println(" V");
  } else {
    Serial.println("MAX17043 : not connected");
  }
  Serial.println("----------------------------");
  Serial.println("=====================\n");
}

void CommandHandler::processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  
  if (cmd == "TARE") {
    Serial.println("\n=== RECALIBRATE EMPTY BASELINE ===");
    Serial.println("Make sure NO weight is on the scale!");
    hx711_tare();
    Serial.println("==================================\n");
  }
  else if (cmd == "CALIBRATE" || cmd.startsWith("CALIBRATE ")) {
    float knownWeightG = 1000.0f;
    if (cmd.length() > 9) {
      String arg = cmd.substring(9);
      arg.trim();
      if (arg.length() == 0) {
        Serial.println("Usage: CALIBRATE <known_weight_in_grams>");
        return;
      }
      knownWeightG = arg.toFloat();
      if (knownWeightG <= 0.0f) {
        Serial.println("Invalid weight. Example: CALIBRATE 500");
        return;
      }
    }

    Serial.println("\n=== CALIBRATE SCALE ===");
    Serial.print("Make sure you have exactly ");
    Serial.print(knownWeightG, 0);
    Serial.println("g ready!");
    hx711_calibrate(knownWeightG);
    Serial.println("=======================\n");
  }
  else if (cmd == "RESETCAL") {
    Serial.println("\n=== RESET CALIBRATION ===");
    hx711_resetToDefault();
    Serial.println("Restart the ESP32 to re-calibrate");
    Serial.println("==========================\n");
  }
  else if (cmd == "START" || cmd == "ON") {
    printEnabled = true;
    lastPrintTime = millis();
    Serial.println("\n=== PRINTING ENABLED ===");
    Serial.println("Sensors will print every 2 seconds");
    Serial.println("Type STOP to pause\n");
    printSensorDataOnce();
  }
  else if (cmd == "STOP" || cmd == "OFF") {
    printEnabled = false;
    Serial.println("\n=== PRINTING DISABLED ===");
    Serial.println("Type START to resume printing\n");
  }
  else if (cmd == "ONCE") {
    printSensorDataOnce();
  }
  else if (cmd == "STATUS") {
    printStatus();
  }
  else if (cmd == "HELP") {
    printHelp();
  }
  else if (cmd.length() > 0) {
    Serial.println("Unknown command: " + cmd);
    Serial.println("Type HELP for available commands");
  }
}

void CommandHandler::update() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      if (inputString.length() > 0) {
        processCommand(inputString);
        inputString = "";
      }
      stringComplete = true;
    } else if (inChar != '\r') {
      inputString += inChar;
    }
  }
  
  if (printEnabled) {
    unsigned long now = millis();
    if (now - lastPrintTime >= PRINT_INTERVAL) {
      dht_update();
      float waste = readWeightSafe();
      ina226_read();
      max17043_read();
      
      Serial.println("===== SENSOR DATA =====");
      if (scaleConnected) {
        Serial.print("Waste    : "); Serial.print(waste, 2); Serial.println(" g");
      } else {
        Serial.println("Waste    : HX711 not connected");
      }
      if (dhtOK) {
        Serial.print("Temp     : "); Serial.print(sensorData.temperature, 2); Serial.println(" C");
        Serial.print("Humidity : "); Serial.print(sensorData.humidity, 1);    Serial.println(" %");
      } else {
        Serial.println("Temp     : DHT11 no reading");
        Serial.println("Humidity : DHT11 no reading");
      }
      if (inaConnected) {
        float displayCurrent = ina226_getDisplayCurrent();
        String sign = (displayCurrent > 0) ? "+" : (displayCurrent < 0) ? "-" : "";
        Serial.print("Voltage  : "); Serial.print(loadVoltage_V, 3); Serial.println(" V");
        Serial.print("Current  : "); Serial.print(sign); Serial.print(abs(displayCurrent), 2); Serial.println(" mA");
        Serial.print("Status   : "); Serial.println(ina226_getStatus());
      } else {
        Serial.println("INA226   : not connected");
      }
      if (max17043Connected) {
        Serial.print("Batt SOC : "); Serial.print(max17043_getSoc(), 1); Serial.println(" %");
        Serial.print("Batt V   : "); Serial.print(batteryVoltage_V, 3); Serial.println(" V");
      } else {
        Serial.println("MAX17043 : not connected");
      }
      Serial.println("----------------------------");
      
      lastPrintTime = now;
    }
  }
}
