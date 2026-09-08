// command_handler.cpp
#include "command_handler.h"
#include "hx711_sensor.h"
#include "dht_sensor.h"

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
    // Unfiltered and signed — a negative here is why the reported weight is 0.
    Serial.print("Current Weight: "); Serial.print(scale.get_units(5), 1); Serial.println(" g");
    Serial.print("Raw Reading: "); Serial.println(scale.read_average(5));
    Serial.print("Tare Offset: "); Serial.println(scale.get_offset());
    Serial.print("Cal Factor: "); Serial.println(scale.get_scale(), 4);
  }
  Serial.print("DHT11: "); Serial.println(dhtOK ? "OK" : "FAIL");
  Serial.println("====================\n");
}

void CommandHandler::printSensorDataOnce() {
  Serial.println("\n=== SINGLE READING ===");
  dht_update();
  float waste = readWeightSafe();

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
      Serial.println("----------------------------");

      lastPrintTime = now;
    }
  }
}
