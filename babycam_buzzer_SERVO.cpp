#include <Arduino.h>
#include <ESP32Servo.h>
//ESP32Servo.h 請用0.4.2版本，不然會報錯

#define buzzer_pin 33
#define servo_vertical_pin 1
#define servo_horizontal_pin 3

#define Servo_DelayTime 20
#define Servo_Up_DelayTime 50

Servo vertical;
Servo horizontal;

//蜂鳴器警告(代表錯誤出現, 或處理SD卡程序)
void BUZZER(int times) {
  pinMode(buzzer_pin, OUTPUT);
  for (int i = 1; i <= times; i++) {
    digitalWrite(buzzer_pin, LOW);
    delay(100);
    digitalWrite(buzzer_pin, HIGH);
    delay(100);
  }
  digitalWrite(buzzer_pin, HIGH);
}

//初始化伺服電機
void initServos() {
  vertical.setPeriodHertz(50);    // standard 50 hz servo
  horizontal.setPeriodHertz(50);  // standard 50 hz servo
  vertical.attach(servo_vertical_pin, 500, 2500);
  horizontal.attach(servo_horizontal_pin, 500, 2500);
  vertical.write(90);
  horizontal.write(90);
}

//伺服電機運作
void SERVO(String direction) {
  if (direction == "UP") {
    vertical.write(0);  // 上
    delay(Servo_Up_DelayTime);
    vertical.write(90);
  } else if (direction == "DOWN") {
    vertical.write(180);  // 下
    delay(Servo_DelayTime);
    vertical.write(90);
  } else if (direction == "LEFT") {
    horizontal.write(180);  // 左
    delay(Servo_DelayTime);
    horizontal.write(90);
  } else if (direction == "RIGHT") {
    horizontal.write(0);  // 右
    delay(Servo_DelayTime);
    horizontal.write(90);
  }
}