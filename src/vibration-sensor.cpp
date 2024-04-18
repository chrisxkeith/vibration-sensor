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
const static String PHOTON_15 = "270037000a47373336323230";
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
    static bool hasSerial() {
      String deviceID = System.deviceID();
      return deviceID.equals(PHOTON_02) || deviceID.equals(PHOTON_08);
    }
    static void println(String s) {
      if (hasSerial()) {
        Serial.println(s.c_str());
      }
    }
    static String getDeviceName() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "PHOTON_01"; }
      if (deviceID.equals(PHOTON_02)) { return "PHOTON_02"; }
      if (deviceID.equals(PHOTON_08)) { return "PHOTON_08"; }
      if (deviceID.equals(PHOTON_09)) { return "PHOTON_09"; }
      if (deviceID.equals(PHOTON_15)) { return "PHOTON_15"; }
      return "Unknown deviceID: " + deviceID;
    }
    static String getDeviceLocation() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "Dryer";  }
      if (deviceID.equals(PHOTON_08)) { return "Washer"; }
      return getDeviceName();
    }
    static uint16_t getDeviceCutoff() {
      return 550;
    }
};

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
    unsigned long last_print_time = 0;

    const int PIEZO_PIN_0 = A0;
    const int PIEZO_PIN_1 = A1;
    const int NUM_SAMPLES = 1000;
    const int PUBLISH_RATE_IN_SECONDS = 5;

    void getVoltages() {
      const uint16_t MAX_VIBRATION_VALUE = 600;
      max_A0 = 0;
      max_A1 = 0;
      for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t piezoV = analogRead(PIEZO_PIN_0);
        if (piezoV > max_A0) {
          max_A0 = piezoV;
        }
        piezoV = analogRead(PIEZO_PIN_1);
        if (piezoV > max_A1) {
          max_A1 = piezoV;
        }
      }
      if (max_A0 > MAX_VIBRATION_VALUE) {
        max_A0 = MAX_VIBRATION_VALUE;
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
        Utils::println(ret);
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
        JSonizer::addFirstSetting(json, "max_A0", getJson(Utils::getDeviceLocation() + " A0", max_A0));
        if (! (Utils::getDeviceLocation().startsWith("Washer") || 
              (Utils::getDeviceLocation().startsWith("Dryer")) ||
              (Utils::getDeviceLocation().equals(Utils::getDeviceName())))) {
          JSonizer::addSetting(json, "max_A1", getJson(Utils::getDeviceLocation() + " A1", max_A1));
        }
        json.concat("}");
        Particle.publish("vibration", json);
        int theDelay = PUBLISH_RATE_IN_SECONDS - seconds_for_sample;
        delay(theDelay * 1000);
    }
    bool in_washing_window() {
      int hour = Time.hour();
      return ((hour > 6) && (hour < 22)); // 7 am to 9 pm, one hopes
    }
    String getJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "seconds_for_sample", String(seconds_for_sample));
      JSonizer::addSetting(json, "PIEZO_PIN_0", String(PIEZO_PIN_0));
      JSonizer::addSetting(json, "PIEZO_PIN_1", String(PIEZO_PIN_1));
      JSonizer::addSetting(json, "NUM_SAMPLES", String(NUM_SAMPLES));
      JSonizer::addSetting(json, "max_weighted", String(max_A0));
      JSonizer::addSetting(json, "in_washing_window()", String(JSonizer::toString(in_washing_window())));
      json.concat("}");
      return json;
    }
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_0, INPUT);
      pinMode(PIEZO_PIN_1, INPUT);
    }
    void monitor_sensor() {
      if (in_washing_window()) {
        sample_and_publish();
      }
      printRawValues(false);
      if (millis() - last_print_time > 2000) {
        Utils::println(getJson());
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
    uint16_t getMaxA0() {
        getVoltages();
        return max_A0;
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

int lastDisplay = 0;
const int DISPLAY_RATE_IN_MS = 2000;
void display() {
  int thisMS = millis();
  if (thisMS - lastDisplay > DISPLAY_RATE_IN_MS) {
    oledWrapper.displayNumber(String(sensorhandler.getMaxA0()));
  }
  lastDisplay = thisMS;
}


void setup() {
  Serial.begin(57600);
  oledWrapper.display("Starting setup...", 1);
  Particle.function("GetData", sample_and_publish);
  Particle.function("GetSetting", publish_settings);
  Particle.function("PrintRaw", print_raw);
  Particle.function("Cutoff", determine_cutoff);
  oledWrapper.display("Finished setup.", 1);
}

void loop() {
  timeSupport.handleTime();
  sensorhandler.monitor_sensor();
  display();
}
