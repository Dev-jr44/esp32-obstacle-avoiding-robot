#include <ESP32Servo.h>

// ---------- HC-SR04 ----------
const int trigPin = 5;
const int echoPin = 18;  // Keep existing voltage divider
const float alertDistance = 20.0;

// ---------- Servo ----------
const int servoPin = 13;
Servo scanner;

// ---------- L298N ----------
const int ENA = 25;
const int IN1 = 27;
const int IN2 = 14;

const int ENB = 26;
const int IN3 = 23;
const int IN4 = 19;

// ---------- Settings ----------
const int MOTOR_SPEED = 100;
const int TURN_TIME = 400;
const int REVERSE_TIME = 500;

// ---------- Distance measurement ----------
float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration =
      pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.034 / 2.0;
}

// ---------- Your working scan logic ----------
float scanAt(int angle, const char* direction) {
  scanner.write(angle);
  delay(400);

  float distance = measureDistance();

  Serial.print(direction);
  Serial.print(" distance: ");

  if (distance < 0) {
    Serial.println("No echo");
  } else {
    Serial.print(distance);
    Serial.println(" cm");
  }

  return distance;
}

// ---------- Motor control ----------
void stopMotors() {
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  ledcWrite(ENA, MOTOR_SPEED);
  ledcWrite(ENB, MOTOR_SPEED);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  ledcWrite(ENA, MOTOR_SPEED);
  ledcWrite(ENB, MOTOR_SPEED);
}

void turnLeft() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  ledcWrite(ENA, MOTOR_SPEED);
  ledcWrite(ENB, MOTOR_SPEED);
}

void turnRight() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  ledcWrite(ENA, MOTOR_SPEED);
  ledcWrite(ENB, MOTOR_SPEED);
}

// ---------- Obstacle avoidance ----------
void avoidObstacle() {
  stopMotors();
  delay(300);

  // Limit retries so it cannot reverse forever.
  const int MAX_RESCANS = 3;

  for (int attempt = 0; attempt < MAX_RESCANS; attempt++) {
    Serial.println("Scanning sides...");

    float rightDistance = scanAt(30, "RIGHT");
    float leftDistance = scanAt(160, "LEFT");

    scanner.write(90);
    delay(500);

    // Invalid readings are not treated as safe paths.
    bool rightBlocked =
        rightDistance < 0 ||
        rightDistance <= alertDistance;

    bool leftBlocked =
        leftDistance < 0 ||
        leftDistance <= alertDistance;

    if (rightBlocked && leftBlocked) {
      Serial.println("Both sides blocked. Reversing...");

      moveBackward();
      delay(REVERSE_TIME);
      stopMotors();
      delay(300);

      continue;
    }

    if (!rightBlocked &&
        (leftBlocked || rightDistance > leftDistance)) {
      Serial.println("Turning RIGHT");
      turnRight();
      delay(TURN_TIME);
    } else {
      Serial.println("Turning LEFT");
      turnLeft();
      delay(TURN_TIME);
    }

    stopMotors();
    delay(200);
    return;
  }

  Serial.println("No clear path after 3 scans. Staying stopped.");
  stopMotors();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // 1. Servo first, with timers reserved so ESP32Servo owns its own
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  scanner.setPeriodHertz(50);
  scanner.attach(servoPin, 500, 2500);
  scanner.write(90);
  delay(1000);

  // 2. Motors afterward, on explicit channels away from the servo's
  ledcAttachChannel(ENA, 1000, 8, 4);
  ledcAttachChannel(ENB, 1000, 8, 5);

  stopMotors();

  Serial.println("Obstacle avoidance robot ready.");
}

// Fast front check: servo is already facing forward, so no move and no delay
float checkFront() {
  float distance = measureDistance();

  Serial.print("FRONT distance: ");
  if (distance < 0) {
    Serial.println("No echo");
  } else {
    Serial.print(distance);
    Serial.println(" cm");
  }

  return distance;
}

// ---------- Main loop ----------
void loop() {
  float frontDistance = checkFront();   // was: scanAt(90, "FRONT");

  if (frontDistance < 0) {
    Serial.println("Front reading unavailable. Stopping.");
    stopMotors();
  }
  else if (frontDistance > alertDistance) {
    moveForward();
  }
  else {
    Serial.println("OBSTACLE DETECTED!");
    avoidObstacle();
  }

  Serial.println("-------------------------");

  delay(50);
}