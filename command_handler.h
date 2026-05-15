// command_handler.h
#pragma once
#include <Arduino.h>

class CommandHandler {
private:
  String inputString;
  bool stringComplete;
  bool printEnabled;
  unsigned long lastPrintTime;
  const unsigned long PRINT_INTERVAL;
  
public:
  CommandHandler() : stringComplete(false), printEnabled(false), 
                     lastPrintTime(0), PRINT_INTERVAL(2000) {}
  
  void begin();
  void update();
  void printHelp();
  void printStatus();
  void printSensorDataOnce();
  bool isPrintEnabled() { return printEnabled; }
  void processCommand(String cmd);
};

extern CommandHandler cmdHandler;