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
    json.concat("\":");
    if (val.charAt(0) != '{') {
      json.concat("\"");
    }
    json.concat(val);
    if (val.charAt(0) != '{') {
      json.concat("\"");
    }
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
    long unsigned int ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
    if (millis() - lastSyncMillis > ONE_DAY_IN_MILLISECONDS) {    // If it's been a day since last sync...
                                                            // Request time synchronization from the Particle Cloud
      this->doHandleTime();
    }
}

void TimeSupport::publishJson() {
    Particle.publish("TimeSupport", getSettings());
}
TimeSupport    timeSupport(-8);

const static String PHOTON_01 = "1c002c001147343438323536";
const static String PHOTON_02 = "300040001347343438323536";
const static String PHOTON_08 = "500041000b51353432383931";
const static String PHOTON_09 = "1f0027001347363336383437";
class Utils {
  private:
  public:
    static int setInt(String command, int& i, int lower, int upper) {
      int tempMin = command.toInt();
      if (tempMin >= lower && tempMin <= upper) {
          i = tempMin;
          return 1;
      }
      return -1;
    }
    static void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "githubRepo", "https://github.com/chrisxkeith/vibration-sensor");
      JSonizer::addSetting(json, "getDeviceLocation()", getDeviceLocation());
      JSonizer::addSetting(json, "getDeviceCutoff()", String(getDeviceCutoff()));
      json.concat("}");
      Particle.publish("Utils json", json);
    }
    static String getDeviceLocation() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) {
        return "Washer";
      }
      if (deviceID.equals(PHOTON_02)) {
        return "Test_02";
      }
      if (deviceID.equals(PHOTON_08)) {
        return "Test_08";
      }
      if (deviceID.equals(PHOTON_09)) {
        return "Dryer";
      }
      return "DeviceID: " + deviceID;
    }
    static uint16_t getDeviceCutoff() {
      const uint16_t VERTICAL_SMALL_WEIGHTED_SENSOR_CUTOFF = 400;
      const uint16_t VERTICAL_LARGE_WEIGHTED_SENSOR_CUTOFF = 550;
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) {
        return VERTICAL_LARGE_WEIGHTED_SENSOR_CUTOFF;
      }
      if (deviceID.equals(PHOTON_02)) {
        return VERTICAL_SMALL_WEIGHTED_SENSOR_CUTOFF;
      }
      if (deviceID.equals(PHOTON_08)) {
        return VERTICAL_SMALL_WEIGHTED_SENSOR_CUTOFF;
      }
      if (deviceID.equals(PHOTON_09)) {
        return VERTICAL_LARGE_WEIGHTED_SENSOR_CUTOFF;
      }
      Particle.publish("Error", "Unknown device: " + deviceID + " in getDeviceCutoff()");
      return 0;
    }
};

class SensorHandler {
  private:
    int seconds_for_sample = 1;
    uint16_t max_weighted = 0;
    unsigned long last_print_time = 0;

    const int PIEZO_PIN_0 = A0;
    const int NUM_SAMPLES = 1000;
    const int PUBLISH_RATE_IN_SECONDS = 5;

    void getVoltages() {
      max_weighted = 0;
      for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t piezoV = analogRead(PIEZO_PIN_0);
        if (piezoV > max_weighted) {
          max_weighted = piezoV;
        }
      }
    }
    String getJson(String name, uint16_t value) {
      String json("{");
      JSonizer::addFirstSetting(json, "eventName", name);
      JSonizer::addSetting(json, "value", String(value));
      json.concat("}");
      return json;
    }
    void printRawValues(bool force) {
      uint16_t A0_val = 0;
      uint16_t num_reads = 0;
      unsigned long then = millis();
      while (millis() - then < 1000) {
        uint16_t raw = analogRead(A0);
        if (raw > A0_val) {
          A0_val = raw;
        }
        num_reads++;
      }
      if (force || (num_reads > 0 && (A0_val > Utils::getDeviceCutoff()))) {
        String ret;
        ret.concat(timeSupport.now());
        ret.concat(",");
        ret.concat(A0_val);
        ret.concat(",");
        ret.concat(num_reads);
        Serial.println(ret);
      }
    }
    uint16_t getMaxForPin(int pin) {
      uint16_t pinVal = 0;
      unsigned long then = millis();
      while (millis() - then < 1000) {
        uint16_t raw = analogRead(pin);
        if (raw > pinVal) {
          pinVal = raw;
        }
      }
      return pinVal;
    }
    void sample_and_publish() {
        getVoltages();
        String json("{");
        JSonizer::addFirstSetting(json, "max_weighted", getJson(Utils::getDeviceLocation() + " weighted", max_weighted));
        json.concat("}");
        Particle.publish("vibration", json);
        int theDelay = PUBLISH_RATE_IN_SECONDS - seconds_for_sample;
        delay(theDelay * 1000);
    }
    bool in_washing_window() {
      int hour = Time.hour();
      return ((hour > 7) && (hour < 18)); // 8 am to 5 pm, one hopes
    }
    String getJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "seconds_for_sample", String(seconds_for_sample));
      JSonizer::addSetting(json, "PIEZO_PIN_0", String(PIEZO_PIN_0));
      JSonizer::addSetting(json, "NUM_SAMPLES", String(NUM_SAMPLES));
      JSonizer::addSetting(json, "max_weighted", String(max_weighted));
      JSonizer::addSetting(json, "in_washing_window()", String(JSonizer::toString(in_washing_window())));
      json.concat("}");
      return json;
    }
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_0, INPUT);
    }
    void monitor_sensor() {
      if (in_washing_window()) {
        sample_and_publish();
      }
      printRawValues(false);
      if (millis() - last_print_time > 2000) {
        Serial.println(getJson());
        last_print_time = millis();
      }
    }
    void sample_and_publish_() {
      sample_and_publish();
    }
    void print_raw_values() {
      printRawValues(true);
    }
    void determine_cutoff() {
      Particle.publish("A0 cutoff", String(getMaxForPin(A0)));
    }
    void publishJson() {
      Particle.publish("SensorHandler json", getJson());
    }
};
SensorHandler sensorhandler;

int sample_and_publish(String cmd) {
  sensorhandler.sample_and_publish_();
  return 1;
}

int print_raw(String cmd) {
  sensorhandler.print_raw_values();
  return 1;
}

// Only call when device is not vibrating.
int determine_cutoff(String cmd) {
  sensorhandler.determine_cutoff();
  return 1;
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
        String msg(command);
        msg.concat(" : expected [empty], \"time\", \"sensor\" or \"rate\"");
        Particle.publish("publish_settings bad input", msg);
        return -1;
    }
    return 1;
}

void setup() {
  Serial.begin(57600);
  Particle.function("GetData", sample_and_publish);
  Particle.function("GetSetting", publish_settings);
  Particle.function("PrintRaw", print_raw);
  Particle.function("Cutoff", determine_cutoff);
}

void loop() {
  timeSupport.handleTime();
  sensorhandler.monitor_sensor();
}
