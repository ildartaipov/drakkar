#include <math.h>
#include <Arduino.h>
#include <Servo.h>
#include "ArduinoJson.h"


StaticJsonDocument<1024> jsonIn;
DynamicJsonDocument answer(1024);

int16_t speed_l = 0, speed_r = 0;

unsigned long timeNow, hostTimeRecivie = 0, hostTimeSend = 0;


Servo escL;
Servo escR;

const int max_pwm = 255;

const int minWidth = 900;
const int maxWidth = 1900;

const int transmitTimeout = 1000;
const int hostTimeOut = 300;

// пины 0, 1 на выход (драйверы)
const int M1_PWM_OUT_PIN = A0;  //left мотор
const int M2_PWM_OUT_PIN = A1;  //right мотор


void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);  // serial begin at 115200 bauds
  Serial.println("start");
  Serial.println("calibrating start...");
  delay(1000);

  escL.attach(M1_PWM_OUT_PIN);
  escR.attach(M2_PWM_OUT_PIN);

  // для калибровки драйвера сначала отправляем минимальную длительность,
  // затем максимальную, затем среднее значение

  escL.writeMicroseconds(minWidth);
  escR.writeMicroseconds(minWidth);
  delay(100);
  escL.writeMicroseconds(maxWidth);
  escR.writeMicroseconds(maxWidth);
  delay(100);

  // остановки двум моторам
  escL.writeMicroseconds((minWidth + maxWidth) / 2);
  escR.writeMicroseconds((minWidth + maxWidth) / 2);
  delay(100);

  Serial.println("calibrating finish.");

  answer["name"] = "Drakkar";
  answer["type"] = "chasis";

  serializeJson(answer, Serial);
}

int moveR(int duty) {
  //duty может быть в пределах [-255, 255], поэтому берем модуль числа
  int width = (int)map(duty, -255, 255, minWidth, maxWidth);
  escR.writeMicroseconds(width);
  return width;
}

int moveL(int duty) {
  int width = (int)map(duty, -255, 255, minWidth, maxWidth);
  escL.writeMicroseconds(width);
  return width;
}

// {"speed_l":100, "speed_r":100}
void recieveDataFromHost(void) {
  if (Serial.available()) {
    DeserializationError err = deserializeJson(jsonIn, Serial);  // получаем сообщение от orange pi через uart
    if (err == DeserializationError::Ok)                         // если cообщение принято
    {
      speed_l = constrain(jsonIn["speed_l"].as<int>(), -255, 255);
      speed_r = constrain(jsonIn["speed_r"].as<int>(), -255, 255);

      hostTimeRecivie = timeNow + hostTimeOut;
      // JsonObject root = jsondoc.to<JsonObject>();

    } else {
      while (Serial.available() > 0)
        Serial.read();  // чистим буфер
    }
  }
}
void sendDataToHost(int pwmL, int pwmR) {
  if (timeNow >= hostTimeSend) {
    answer["status"]["pwmL"] = pwmL;
    answer["status"]["pwmR"] = pwmR;
    answer["status"]["time"] = timeNow;
    serializeJson(answer, Serial);
    Serial.println();
    hostTimeSend = timeNow + transmitTimeout;
  }
}
void checkHostTimeout(void)  // если не получали данные от хоста в течение таймаута, то останавливаемся
{
  if (hostTimeRecivie < timeNow) {
    speed_r = 0;
    speed_l = 0;

    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, (timeNow % 2000) < 1000);
  }
}
void loop() {
  timeNow = millis();
  recieveDataFromHost();
  checkHostTimeout();
  int pwmL = moveL(speed_l);
  int pwmR = moveR(speed_r);
  sendDataToHost(pwmL, pwmR);
}
