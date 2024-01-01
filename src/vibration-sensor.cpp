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

int seconds_for_sample = 1;

const int PIEZO_PIN_UNWEIGHTED = A0;
const int PIEZO_PIN_WEIGHTED = A1;
const int WAIT_BETWEEN_READS_MS = 25;
const int NUM_SAMPLES = (seconds_for_sample * 1000) / 25;

class Utils {
  public:
    static int publishRateInSeconds;

    static int setInt(String command, int& i, int lower, int upper) {
      int tempMin = command.toInt();
      if (tempMin > lower && tempMin < upper) {
          i = tempMin;
          return 1;
      }
      return -1;
    }
    static int setPublishRate(String cmd) {
      return setInt(cmd, publishRateInSeconds, 1, 60);
    }
};
int Utils::publishRateInSeconds = 10;

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

int sample_and_publish(String cmd) {
  String val1(getVoltage(PIEZO_PIN_WEIGHTED));
  Particle.publish("Voltage weighted sensor", val1);
  return 1;
}

int set_publish_rate(String cmd) {
  return Utils::setPublishRate(cmd);
}

void setup() {
  pinMode(PIEZO_PIN_UNWEIGHTED, INPUT);
  pinMode(PIEZO_PIN_WEIGHTED, INPUT);
  Particle.function("GetData", sample_and_publish);
  Particle.function("SetPubRate", set_publish_rate);
}

void loop() {
  sample_and_publish("");
  delay((Utils::publishRateInSeconds - seconds_for_sample) * 1000);
}
