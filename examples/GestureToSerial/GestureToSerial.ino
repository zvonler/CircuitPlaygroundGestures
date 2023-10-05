/*
  GestureToSerial

  Sketch for the Adafruit Circuit Playground that watches for user input
  gestures and prints their descriptions to the Serial Monitor.

  This example code is in the public domain.
*/

#include <CircuitPlaygroundGestures.h>
#include <Adafruit_CircuitPlayground.h>

using CPG = CircuitPlaygroundGestures;

void setup()
{
    CircuitPlayground.begin();

    CPG::instance().begin();

    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
}

void loop()
{
    auto now = millis();

    digitalWrite(LED_BUILTIN, (now % 1024) < 512);

    auto& cpg = CPG::instance();

    auto gesture = cpg.update(now);
    if (gesture != CPG::NONE) {
        Serial.print("Received ");
        Serial.print(cpg.to_string(gesture));
        if (gesture == CPG::ORIENTATION_CHANGED) {
            Serial.print(" to ");
            Serial.print(cpg.to_string(cpg.orientation()));
        }
        Serial.println();
    }

    delay(1);
}
