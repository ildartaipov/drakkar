#include <Arduino.h>
#include <Wire.h>
#include "CG_RadSens.h"   //ClimateGuard Radsense https://github.com/climateguard/RadSens
#include "ArduinoJson.h"  //https://arduinojson.org/?utm_source=meta&utm_medium=library.properties

#define VERSION "0.1"

#define DEFAULT_TIMEOUT 300
#define DEFAULT_HOST_TRANSMIT_INTERVAL 1000


int hostTimeout = DEFAULT_TIMEOUT;
int hostTransmitInterval = DEFAULT_HOST_TRANSMIT_INTERVAL;


unsigned long timeNow;

unsigned long hostTimeRecivie = 0;
unsigned long hostTimeSend = 0;


//SDA pin4 on rp2040 zero
//SDl pin5 on rp2040 zero

CG_RadSens radSens(RS_DEFAULT_I2C_ADDRESS); /*Constructor of the class ClimateGuard_RadSens1v2,
                                                           sets the address parameter of I2C sensor.
                                                           Default address: 0x66.*/

int getHostTimeout(void) {
  return hostTimeout;
}

int setHostTimeout(int _timeout) {
  hostTimeout = _timeout;
  return hostTimeout;
}

int getHostReceive(void) {
  return hostTimeout;
}

int setHostTransmitInterval(int _interval) {
  hostTransmitInterval = _interval;
  return hostTransmitInterval;
}

int getHostTransmitInterval(void) {
  return hostTransmitInterval;
}

// json буфер для пакетов от orange pi
StaticJsonDocument<1024> jsondoc;
DynamicJsonDocument answer(1024);

void setup() {
  Serial.begin(115200);
  Wire.begin();  // This function initializes the Wire library
  delay(1000);
  while (!radSens.init()) /*Initializates function and sensor connection. Returns false if the sensor is not connected to the I2C bus.*/
  {
    Serial.println("Sensor wiring error!");
    delay(1000);
  }


  answer["name"] = "Rover radsense shield";
  answer["version"] = VERSION;
  serializeJson(answer, Serial);
}

void sendStatus() {

  answer["status"]["radsens"]["intensyStatic"] = radSens.getRadIntensyStatic();
  answer["status"]["radsens"]["intensyDynamic"] = radSens.getRadIntensyDynamic();
  answer["status"]["radsens"]["sensorChipId"] = "0x" + String(radSens.getChipId(), HEX);
  answer["status"]["radsens"]["firwareVersion"] = radSens.getFirmwareVersion();
  answer["status"]["radsens"]["pulseCount"] = radSens.getNumberOfPulses();
  // answer["status"]["radsens"]["adressI2C"] = "0x"+String(radSens.getSensorAddress(), HEX);
  answer["status"]["radsens"]["HVGeneratorState"] = radSens.getHVGeneratorState();
  answer["status"]["radsens"]["sensivity"] = radSens.getSensitivity();
  answer["status"]["radsens"]["ledState"] = radSens.getLedState();
  answer["status"]["radsens"]["time"] = millis();
}

void recieveDataFromHost(void) {
  if (Serial.available()) {
    DeserializationError err = deserializeJson(jsondoc, Serial);  // получаем сообщение от orange pi через uart
    if (err == DeserializationError::Ok)                          // если cообщение принято
    {

      hostTimeRecivie = timeNow + getHostTimeout();
      // JsonObject root = jsondoc.to<JsonObject>();
      if (jsondoc["configure"]) {
#if (SERIAL_DEBUG == 1)
        Serial.println("Configure received!");
#endif
        String setLedState, setHVGeneratorState, setSensitivity;
        setLedState = jsondoc["configure"]["setLed"].as<String>();
        setHVGeneratorState = jsondoc["configure"]["setHVGeneratorState"].as<String>();
        setSensitivity = jsondoc["configure"]["setSensitivity"].as<String>();
        if (!setLedState.equalsIgnoreCase("null")) {
          if (setLedState.equalsIgnoreCase("true")) {
            radSens.setLedState(true);
#if (SERIAL_DEBUG == 1)
            Serial.println("Led ON");
#endif
          } else if (setLedState.equalsIgnoreCase("false")) {
            radSens.setLedState(false);
#if (SERIAL_DEBUG == 1)
            Serial.println("Led OFF");
#endif
          }
        }
        if (!setHVGeneratorState.equalsIgnoreCase("null")) {
          if (setHVGeneratorState.equalsIgnoreCase("true")) {
            radSens.setHVGeneratorState(true);
#if (SERIAL_DEBUG == 1)
            Serial.println("HVGen ON");
#endif
          } else if (setHVGeneratorState.equalsIgnoreCase("false")) {
            radSens.setHVGeneratorState(false);
#if (SERIAL_DEBUG == 1)
            Serial.println("HVGen OFF");
#endif
          }
        }
        if (!setSensitivity.equalsIgnoreCase("null")) {
          uint16_t sensitivity = jsondoc["configure"]["setSensitivity"].as<int>();
          radSens.setSensitivity(sensitivity);
#if (SERIAL_DEBUG == 1)
          Serial.print("Set sensivity to ");
          Serial.println(sensitivity);
#endif
        }
      }
    } else {
      while (Serial.available() > 0)
        Serial.read();  // чистим буфер
    }
  }
}

void sendDataToHost(void) {
  if (hostTimeSend > timeNow)
    return;
  hostTimeSend = timeNow + getHostTransmitInterval();
  sendStatus();
  serializeJson(answer, Serial);
  Serial.println("");
}
void loop() {

  timeNow = millis();
  recieveDataFromHost();  // получаем данные от хоста
  sendDataToHost();       // отправляем данные хосту c интервалом hostTransmitInterval
  // showInfoOnDisplay();

  delay(2000);
}