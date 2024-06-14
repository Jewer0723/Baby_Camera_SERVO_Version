#include <Arduino.h>
#include <ESP32Servo.h>
//ESP32Servo.h 請用0.4.2版本，不然會報錯

void BUZZER(int times);
void initServos();
void SERVO(String direction);