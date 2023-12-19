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

const int PIEZO_PIN_0 = A0;
const int PIEZO_PIN_1 = A1;
const int WAIT_BETWEEN_READS_MS = 25;
const int NUM_SAMPLES = 1000 / 25;

void setup() {
  pinMode(PIEZO_PIN_0, INPUT);
  pinMode(PIEZO_PIN_1, INPUT);
}

void loop() {
  float total_piezo_0 = 0.0;
  float total_piezo_1 = 0.0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int piezoADC = analogRead(PIEZO_PIN_0);
    float piezoV = piezoADC / 1023.0 * 3.0;
    total_piezo_0 += piezoV;
    piezoADC = analogRead(PIEZO_PIN_1);
    piezoV = piezoADC / 1023.0 * 3.0;
    total_piezo_1 += piezoV;
    delay(WAIT_BETWEEN_READS_MS);
  }
  String vals(total_piezo_0 / NUM_SAMPLES);
  vals.concat(",");
  vals.concat(total_piezo_1 / NUM_SAMPLES);
  Particle.publish("unweighted,weighted", vals);
}
