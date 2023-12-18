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

const int PIEZO_PIN = A0; // Piezo output

// setup() runs once, when the device is first turned on
void setup() {
  Serial.begin(57600);
  pinMode(PIEZO_PIN, INPUT);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // Read Piezo ADC value in, and convert it to a voltage
  const int WAIT_BETWEEN_READS_MS = 25;
  const int NUM_SAMPLES = 1000 / 25;
  float total_piezo = 0.0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int piezoADC = analogRead(PIEZO_PIN);
    float piezoV = piezoADC / 1023.0 * 5.0;
    total_piezo += piezoV;
    delay(WAIT_BETWEEN_READS_MS);
  }
  float average_piezo = total_piezo / NUM_SAMPLES;
  Particle.publish("Voltage", String(average_piezo)); // Print the voltage.
}
