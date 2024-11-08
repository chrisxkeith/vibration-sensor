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
    time_t n = Time.now();
    time_t restarted = n - (millis() / 1000);
    time_t lastSyncTime = restarted + (lastSyncMillis / 1000);

    String json("{");
    JSonizer::addFirstSetting(json, "restarted", timeStr(restarted));
    JSonizer::addSetting(json, "lastSyncMillis", String(lastSyncMillis));
    JSonizer::addSetting(json, "lastSyncTime", timeStr(lastSyncTime));
    JSonizer::addSetting(json, "timeZoneOffset", String(timeZoneOffset));
    JSonizer::addSetting(json, "isDST", JSonizer::toString(isDST()));
    JSonizer::addSetting(json, "internalTime", now());
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
    return Time.format(t, TIME_FORMAT_ISO8601_FULL);
}

String TimeSupport::now() {
    return timeStr(Time.now());
}

void TimeSupport::doHandleTime() {
  Particle.syncTime();
  this->setDST();
  Time.zone(this->timeZoneOffset);
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

#define DELAY_BEFORE_RESET 2000
const unsigned int resetDelayMillis = DELAY_BEFORE_RESET;
unsigned long resetSync = millis();
bool resetFlag = false;

const static String PHOTON_01 = "1c002c001147343438323536";
const static String PHOTON_02 = "300040001347343438323536";
const static String PHOTON_08 = "500041000b51353432383931";
const static String PHOTON_09 = "1f0027001347363336383437";
const static String PHOTON_15 = "270037000a47373336323230";
class Utils {
  public:
    static int setInt(String command, int& i, int lower, int upper) {
      int tempMin = command.toInt();
      if (tempMin >= lower && tempMin <= upper) {
          i = tempMin;
          return 1;
      }
      return -1;
    }
    static void publish(String event, String data) {
      Particle.publish(event, data);
      delay(1000);
    }
    static String elapsedUpTime() {
      unsigned long ms = millis();
      unsigned long seconds = (ms / 1000) % 60;
      unsigned long minutes = (ms / 1000 / 60) % 60;
      unsigned long hours = (ms / 1000 / 60 / 60);
      String elapsed;
      if (hours > 24) {
        elapsed.concat("hours > 24");
      } else {
        char s[32];
        sprintf(s, "%02u:%02u:%02u", hours, minutes, seconds);
        elapsed.concat(s);
      }
      return elapsed;
    }
    static void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "githubRepo", "https://github.com/chrisxkeith/vibration-sensor");
      JSonizer::addSetting(json, "build", "~ Wed Nov  6 01:37:27 PM PST 2024");
      JSonizer::addSetting(json, "timeSinceRestart", elapsedUpTime());
      json.concat("}");
      Particle.publish("Utils json", json);
    }
    static String getDeviceName() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "PHOTON_01"; }
      if (deviceID.equals(PHOTON_08)) { return "PHOTON_08"; }
      if (deviceID.equals(PHOTON_15)) { return "PHOTON_15"; }
      return "Unknown deviceID: " + deviceID;
    }
    static String getDeviceLocation() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "Dryer";  }
      if (deviceID.equals(PHOTON_08)) { return "Washer"; }
      if (deviceID.equals(PHOTON_15)) { return "Test Unit"; }
      return getDeviceName();
    }
    static uint16_t getDeviceBaseline() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return 75; }
      if (deviceID.equals(PHOTON_08)) { return 100; }
      if (deviceID.equals(PHOTON_15)) { return 30; }
      return 0;
    }
    static void checkForRemoteReset() {
      if ((resetFlag) && (millis() - resetSync >=  resetDelayMillis)) {
        Particle.publish("Debug", "System.reset() Initiated", 300, PRIVATE);
        System.reset();
      }
    }
};
int remoteResetFunction(String command) {
  resetFlag = true;
  resetSync = millis();
  return 0;
}

#include <SparkFunMicroOLED.h>

class OLEDWrapper {
  public:
    MicroOLED* oled = new MicroOLED();

    OLEDWrapper() {
        oled->begin();    // Initialize the OLED
        oled->clear(ALL); // Clear the display's internal memory
        oled->display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled->clear(PAGE); // Clear the buffer.
    }

    void display(String title, int font, uint8_t x, uint8_t y) {
        oled->clear(PAGE);
        oled->setFontType(font);
        oled->setCursor(x, y);
        oled->print(title);
        oled->display();
    }

    void display(String title, int font) {
        display(title, font, 0, 0);
    }

    void displayNumber(String s) {
        // To reduce OLED burn-in, shift the digits (if possible) on the odd minutes.
        int x = 0;
        if (Time.minute() % 2) {
            const int MAX_DIGITS = 5;
            if (s.length() < MAX_DIGITS) {
                const int FONT_WIDTH = 12;
                x += FONT_WIDTH * (MAX_DIGITS - s.length());
            }
        }
        display(s, 3, x, 0);
    }

    void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled->getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled->getLCDHeight()));
        json.concat("}");
        Particle.publish("OLED", json);
    }

    void clear() {
      oled->clear(ALL);
    }
};

OLEDWrapper oledWrapper;

class SensorHandler {
  private:
    int seconds_for_sample = 1;
    uint16_t max_A0 = 0;
    uint16_t max_A1 = 0;
    unsigned long last_publish_time = 0;

    const int PIEZO_PIN_0 = A0;
    const int PIEZO_PIN_1 = A1;
    const int NUM_SAMPLES = 1000;
    const int PUBLISH_RATE_IN_SECONDS = 5;
    String rawValues;

    uint16_t applyBaseline(uint16_t v) {
      if (v < Utils::getDeviceBaseline()) {
        return 0;
      }
      return v - Utils::getDeviceBaseline();
    }
    void do_publish() {
        String json("{");
        JSonizer::addFirstSetting(json, "max_A0", getJson(Utils::getDeviceLocation() + " A0", max_in_publish_interval));
        json.concat("}");
        Particle.publish("vibration", json);
    }
    long unsigned int last_millis_of_max = 0;
    String last_time_of_max;
    void getVoltages() {
      max_A0 = 0;
      max_A1 = 0;
      rawValues.remove(0);
      for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t piezoV = analogRead(PIEZO_PIN_0);
        if (i < 20) {
          rawValues.concat(piezoV);
          rawValues.concat(" ");
        }
        if (piezoV > max_A0) {
          max_A0 = piezoV;
        }
        piezoV = analogRead(PIEZO_PIN_1);
        if (piezoV > max_A1) {
          max_A1 = piezoV;
        }
      }
      max_A0 = applyBaseline(max_A0);
      if (max_A0 > MAX_VIBRATION_VALUE) {
        max_A0 = MAX_VIBRATION_VALUE;
        if (! in_publishing_window()) {
          last_millis_of_max = millis();
          last_time_of_max = timeSupport.now();
        }
      }
      if (max_A0 > max_in_publish_interval) {
        max_in_publish_interval = max_A0;
      }
      if (max_A1 > MAX_VIBRATION_VALUE) {
        max_A1 = MAX_VIBRATION_VALUE;
      }
    }
    String getJson(String name, uint16_t value) {
      String json("{");
      JSonizer::addFirstSetting(json, "eventName", name);
      JSonizer::addSetting(json, "value", String(value));
      json.concat("}");
      return json;
    }
    void publish_max() {
      unsigned long now = millis();
      if (now - last_publish_time > PUBLISH_RATE_IN_SECONDS * 1000) {
        do_publish();
        last_publish_time = millis();
        max_in_publish_interval = 0;
      }
    }
    uint16_t max_in_publish_interval = 0;
    const uint16_t BASE_LINE = 425;
    const uint16_t MAX_VIBRATION_VALUE = 150 + BASE_LINE; // Keep max low enough to show 'usual' vibration in graph.
    bool in_publishing_window() {
      const unsigned long TWO_HOURS_IN_MS = 1000 * 60 * 60 * 2;
      return ((last_millis_of_max > 0) && (millis() - last_millis_of_max < TWO_HOURS_IN_MS));
    }
    String getJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "last_time_of_max", last_time_of_max);
      JSonizer::addSetting(json, "seconds_for_sample", String(seconds_for_sample));
      JSonizer::addSetting(json, "PIEZO_PIN_0", String(PIEZO_PIN_0));
      JSonizer::addSetting(json, "PIEZO_PIN_1", String(PIEZO_PIN_1));
      JSonizer::addSetting(json, "NUM_SAMPLES", String(NUM_SAMPLES));
      JSonizer::addSetting(json, "max_weighted", String(max_A0));
      JSonizer::addSetting(json, "in_publishing_window()", String(JSonizer::toString(in_publishing_window())));
      JSonizer::addSetting(json, "MAX_VIBRATION_VALUE", String(MAX_VIBRATION_VALUE));
      JSonizer::addSetting(json, "getDeviceName()", Utils::getDeviceName());
      JSonizer::addSetting(json, "getDeviceLocation()", Utils::getDeviceLocation());
      JSonizer::addSetting(json, "getDeviceBaseline()", String(Utils::getDeviceBaseline()));
      JSonizer::addSetting(json, "last_millis_of_max", String(last_millis_of_max));
      JSonizer::addSetting(json, "rawValues", rawValues);
      json.concat("}");
      return json;
    }
    int       lastDisplay = 0;
    const int DISPLAY_RATE_IN_MS = 1000;
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_0, INPUT);
      pinMode(PIEZO_PIN_1, INPUT);
    }
    void monitor_sensor() {
      getVoltages();
      if (in_publishing_window()) {
        publish_max();
        if (Utils::getDeviceLocation().equals("Test Unit")) {
          display();
        }
      }
    }
    void display() {
      int thisMS = millis();
      if (thisMS - lastDisplay > DISPLAY_RATE_IN_MS) {
        if (max_A0 >= BASE_LINE) {
          oledWrapper.displayNumber(String(max_A0));
        } else {
          oledWrapper.clear();
        }
        lastDisplay = millis();
      }
    }
    void sample_and_publish_() {
      getVoltages();
      do_publish();
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

// getSettings() is already defined somewhere.
int publish_settings(String command) {
    if (command.compareTo("") == 0) {
        Utils::publishJson();
    } else if (command.compareTo("time") == 0) {
        timeSupport.publishJson();
    } else if (command.compareTo("sensor") == 0) {
        sensorhandler.publishJson();
    } else if (command.compareTo("oled") == 0) {
        oledWrapper.publishJson();
    } else {
        String msg(command);
        msg.concat(" : expected one of [empty], \"time\", \"sensor\", \"oled\"");
        Particle.publish("publish_settings bad input", msg);
        return -1;
    }
    return 1;
}

unsigned long lastUpTimeDisplay = 0;
unsigned int lastY = 0;
void displayUpTime() {
  if (millis() - lastUpTimeDisplay > 1000) {
    lastY += 1;
    if (lastY > 32 - 12) {
      lastY = 0;
    }
    unsigned long ms = millis();
    unsigned long seconds = (ms / 1000) % 60;
    unsigned long minutes = (ms / 1000 / 60) % 60;
    char s[32];
    sprintf(s, "%02u:%02u", minutes, seconds);
    oledWrapper.display(s, 2, 0, lastY);
    lastUpTimeDisplay = millis();
  }
}

void setup() {
  oledWrapper.display("Starting setup...", 1);
  Particle.function("GetData", sample_and_publish);
  Particle.function("GetSetting", publish_settings);
  Particle.function("reset", remoteResetFunction);
  delay(1000);
  sensorhandler.sample_and_publish_();
  oledWrapper.display("Setup finished", 1);
  delay(2000);
  Utils::publish("setup()", "Finished");
  oledWrapper.clear();
}

void loop() {
  timeSupport.handleTime();
  if (!Utils::getDeviceLocation().equals("Test Unit")) {
    displayUpTime();
  }
  sensorhandler.monitor_sensor();
  Utils::checkForRemoteReset();
}
