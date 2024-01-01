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

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val);
    static void addSetting(String& json, String key, String val);
    static String toString(bool b);
};

void JSonizer::addFirstSetting(String& json, String key, String val) {
    json.concat("\"");
    json.concat(key);
    json.concat("\":\"");
    json.concat(val);
    json.concat("\"");
}

void JSonizer::addSetting(String& json, String key, String val) {
    json.concat(",");
    addFirstSetting(json, key, val);
}

String JSonizer::toString(bool b) {
    if (b) {
        return "true";
    }
    return "false";
}

class TimeSupport {
  private:
    unsigned long ONE_DAY_IN_MILLISECONDS;
    unsigned long lastSyncMillis;
    int timeZoneOffset;
    String getSettings();
    bool isDST();
    void setDST();
    void doHandleTime();
  public:
    TimeSupport(int timeZoneOffset);
    String timeStr(time_t t);
    String now();
    void handleTime();
    int setTimeZoneOffset(String command);
    void publishJson();
};

String TimeSupport::getSettings() {
    String json("{");
    JSonizer::addFirstSetting(json, "lastSyncMillis", String(lastSyncMillis));
    JSonizer::addSetting(json, "internalTime", now());
    JSonizer::addSetting(json, "isDST", JSonizer::toString(isDST()));
    JSonizer::addSetting(json, "timeZoneOffset", String(timeZoneOffset));
    JSonizer::addSetting(json, "Time.day()", String(Time.day()));
    JSonizer::addSetting(json, "Time.month()", String(Time.month()));
    JSonizer::addSetting(json, "Time.weekday()", String(Time.weekday()));
    json.concat("}");
    return json;
}

TimeSupport::TimeSupport(int timeZoneOffset) {
    this->timeZoneOffset = timeZoneOffset;
    this->doHandleTime();
}

void TimeSupport::setDST() {
    if (isDST()) {
      Time.beginDST();
    } else {
      Time.endDST();
    }
}

bool TimeSupport::isDST() {
  int dayOfMonth = Time.day();
  int month = Time.month();
  int dayOfWeek = Time.weekday();
	if (month < 3 || month > 11) {
		return false;
	}
	if (month > 3 && month < 11) {
		return true;
	}
	int previousSunday = dayOfMonth - (dayOfWeek - 1);
	if (month == 3) {
		return previousSunday >= 8;
	}
	return previousSunday <= 0;
}

String TimeSupport::timeStr(time_t t) {
    String fmt("%Y-%m-%dT%H:%M:%S");
    return Time.format(t, fmt);
}

String TimeSupport::now() {
    return timeStr(Time.now());
}

void TimeSupport::doHandleTime() {
  Time.zone(this->timeZoneOffset);
  Particle.syncTime();
  this->setDST();
  Particle.syncTime(); // Now that we know (mostly) if we're in daylight savings time.
  this->lastSyncMillis = millis();
}

void TimeSupport::handleTime() {
    int ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
    if (millis() - lastSyncMillis > ONE_DAY_IN_MILLISECONDS) {    // If it's been a day since last sync...
                                                            // Request time synchronization from the Particle Cloud
      this->doHandleTime();
    }
}

void TimeSupport::publishJson() {
    Particle.publish("TimeSupport", getSettings());
}
TimeSupport    timeSupport(-8);

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
    static void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "githubRepo", "https://github.com/chrisxkeith/vibration-sensor");
      JSonizer::addSetting(json, "publishRateInSeconds", String(publishRateInSeconds));
      json.concat("}");
      Particle.publish("Utils json", json);
    }
};
int Utils::publishRateInSeconds = 10;

class SensorHandler {
  private:
    int seconds_for_sample = 1;

    const int PIEZO_PIN_WEIGHTED = A1;
    const int WAIT_BETWEEN_READS_MS = 25;
    const int NUM_SAMPLES = (seconds_for_sample * 1000) / 25;

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
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_WEIGHTED, INPUT);
      pinMode(PIEZO_PIN_WEIGHTED, INPUT);
    }
    int sample_and_publish() {
      String val1(getVoltage(PIEZO_PIN_WEIGHTED));
      Particle.publish("Voltage weighted sensor", val1);
      delay((Utils::publishRateInSeconds - seconds_for_sample) * 1000);
      return 1;
    }
    void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "seconds_for_sample", String(seconds_for_sample));
      JSonizer::addSetting(json, "PIEZO_PIN_WEIGHTED", String(PIEZO_PIN_WEIGHTED));
      JSonizer::addSetting(json, "WAIT_BETWEEN_READS_MS", String(WAIT_BETWEEN_READS_MS));
      JSonizer::addSetting(json, "NUM_SAMPLES", String(NUM_SAMPLES));
      json.concat("}");
      Particle.publish("SensorHandler json", json);
    }
};
SensorHandler sensorhandler;

int set_publish_rate(String cmd) {
  return Utils::setPublishRate(cmd);
}

int sample_and_publish(String cmd) {
  return sensorhandler.sample_and_publish();
}

// getSettings() is already defined somewhere.
int publish_settings(String command) {
    if (command.compareTo("") == 0) {
        Utils::publishJson();
    } else if (command.compareTo("time") == 0) {
        timeSupport.publishJson();
    } else if (command.compareTo("sensor") == 0) {
        sensorhandler.publishJson();
    } else {
        Particle.publish("publish_settings bad input", command);
        return -1;
    }
    return 1;
}

void setup() {
  Particle.function("GetData", sample_and_publish);
  Particle.function("SetPubRate", set_publish_rate);
  Particle.function("GetSetting", publish_settings);
}

void loop() {
  timeSupport.handleTime();
  sample_and_publish("");
}
