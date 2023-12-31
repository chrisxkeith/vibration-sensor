/* 
 * Project myProject
 * Author: Your Name
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHandler(LOG_LEVEL_INFO);

const int PIEZO_PIN_UNWEIGHTED = A0;
const int PIEZO_PIN_WEIGHTED = A1;
const int WAIT_BETWEEN_READS_MS = 25;
const int NUM_SAMPLES = 1000 / 25; // Collect samples for 1000 ms. Also controls publish frequency.

void setup() {
  pinMode(PIEZO_PIN_UNWEIGHTED, INPUT);
  pinMode(PIEZO_PIN_WEIGHTED, INPUT);
}

float getVoltage(int pin) {
  float total_piezo_0 = 0.0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int piezoADC = analogRead(pin);
    float piezoV = piezoADC / 1023.0 * 3.0;
    total_piezo_0 += piezoV;
    delay(WAIT_BETWEEN_READS_MS);
  }
  return total_piezo_0 / NUM_SAMPLES;
}

void loop() {
  String val1(getVoltage(PIEZO_PIN_WEIGHTED));
  Particle.publish("Voltage weighted sensor", val1);
  String val2(getVoltage(PIEZO_PIN_UNWEIGHTED));
  Particle.publish("Voltage unweighted sensor", val2);
}
