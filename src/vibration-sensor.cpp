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
    String getUpTime();
    String getMinSecString(unsigned long ms);
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

String TimeSupport::getMinSecString(unsigned long ms) {
  unsigned long seconds = (ms / 1000) % 60;
  unsigned long minutes = (ms / 1000 / 60) % 60;
  char s[32];
  sprintf(s, "%02u:%02u", minutes, seconds);
  String elapsed(s);
  return elapsed;
}

String TimeSupport::getUpTime() {
  return getMinSecString(millis());
}

TimeSupport    timeSupport(-8);

#define DELAY_BEFORE_RESET 2000
const unsigned int resetDelayMillis = DELAY_BEFORE_RESET;
unsigned long resetSync = millis();
bool resetFlag = false;

const static String PHOTON_01 = "1c002c001147343438323536";
const static String PHOTON_02 = "300040001347343438323536";
const static String PHOTON_05 = "19002a001347363336383438";
const static String PHOTON_07 = "32002e000e47363433353735";
const static String PHOTON_08 = "500041000b51353432383931";
const static String PHOTON_09 = "1f0027001347363336383437";
const static String PHOTON_15 = "270037000a47373336323230";
const static String PHOTON2_16= "0a10aced202194944a045288";
class Utils {
  public:
    static unsigned long startPublishDataMillis;
    static bool          alwaysPublishData;
    const static unsigned long ALWAYS_PUBLISH_DATA_MILLIS = 1000 * 60 * 60 * 3; // 3 hours

    static void setAlwaysPublishData() {
      alwaysPublishData = true;
      startPublishDataMillis = millis();
    }
    static bool publishDataDone() {
      if (millis() - startPublishDataMillis > ALWAYS_PUBLISH_DATA_MILLIS) {
        alwaysPublishData = false;
        startPublishDataMillis = 0;
        return true;
      }
      return false;
    }
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
    static String elapsedTime(unsigned long ms) {
      unsigned long seconds = (ms / 1000) % 60;
      unsigned long minutes = (ms / 1000 / 60) % 60;
      unsigned long hours = (ms / 1000 / 60 / 60);
      String elapsed;
      if (hours > 24) {
        elapsed.concat("hours > 24");
      } else {
        char s[32];
        if (hours > 9) {
          sprintf(s, "!:%02u:%02u", minutes, seconds);
        } else {
          sprintf(s, "%01u:%02u:%02u", hours, minutes, seconds);
        }
        elapsed.concat(s);
      }
      return elapsed;
    }
    static String elapsedUpTime() {
      return elapsedTime(millis());
    }
    static void publishJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "githubRepo", "https://github.com/chrisxkeith/vibration-sensor");
      JSonizer::addSetting(json, "build", "~ Thu, Sep 18, 2025  5:55:01 PM");
      JSonizer::addSetting(json, "timeSinceRestart", elapsedUpTime());
      JSonizer::addSetting(json, "getDeviceID", getDeviceID());
      JSonizer::addSetting(json, "getDeviceLocation", getDeviceLocation());
      JSonizer::addSetting(json, "getDeviceBaseline", String(getDeviceBaseline()));
      JSonizer::addSetting(json, "getDeviceZeroCorrection", String(getDeviceZeroCorrection()));
      JSonizer::addSetting(json, "startPublishDataMillis", String(startPublishDataMillis));
      JSonizer::addSetting(json, "alwaysPublishData", JSonizer::toString(alwaysPublishData));
      JSonizer::addSetting(json, "ALWAYS_PUBLISH_DATA_MILLIS", String(ALWAYS_PUBLISH_DATA_MILLIS));
      json.concat("}");
      Particle.publish("Utils json", json);
    }
    static String getDeviceID() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "PHOTON_01"; }
      if (deviceID.equals(PHOTON_05)) { return "PHOTON_05"; }
      if (deviceID.equals(PHOTON_07)) { return "PHOTON_07"; }
      if (deviceID.equals(PHOTON_08)) { return "PHOTON_08"; }
      if (deviceID.equals(PHOTON_15)) { return "PHOTON_15"; }
      return "Unknown deviceID: " + deviceID;
    }
    static String getDeviceLocation() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return "Dryer";  }
      if (deviceID.equals(PHOTON_05)) { return "Test Unit 05"; }
      if (deviceID.equals(PHOTON_07)) { return "Test Unit 07"; }
      if (deviceID.equals(PHOTON_08)) { return "Washer"; }
      if (deviceID.equals(PHOTON_15)) { return "Test Unit 15"; }
      return getDeviceID();
    }
    static uint16_t getDeviceBaseline() {
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return 75; }
      if (deviceID.equals(PHOTON_05)) { return 100; }
      if (deviceID.equals(PHOTON_07)) { return 100; }
      if (deviceID.equals(PHOTON_08)) { return 75; }
      if (deviceID.equals(PHOTON_15)) { return 30; }
      return 0;
    }
    static uint16_t getDeviceZeroCorrection() {
      const uint16_t NO_VIBRATION_SENSOR_ATTACHED = 575;
      String deviceID = System.deviceID();
      if (deviceID.equals(PHOTON_01)) { return 515; }
      if (deviceID.equals(PHOTON_05)) { return NO_VIBRATION_SENSOR_ATTACHED; }
      if (deviceID.equals(PHOTON_07)) { return 415; }
      if (deviceID.equals(PHOTON_08)) { return 440; }
      if (deviceID.equals(PHOTON_15)) { return 490; }
      return 0;
    }
    static void checkForRemoteReset() {
      if ((resetFlag) && (millis() - resetSync >=  resetDelayMillis)) {
        Particle.publish("Debug", "System.reset() Initiated", 300, PRIVATE);
        System.reset();
      }
    }
};

unsigned long Utils::startPublishDataMillis = 0;
bool          Utils::alwaysPublishData = true;

int setAlwaysPublishData(String command) {
  Utils::setAlwaysPublishData();
  return 1;
}

int remoteResetFunction(String command) {
  resetFlag = true;
  resetSync = millis();
  return 0;
}

#include <SparkFunMicroOLED.h>
class OLEDWrapper {
  private:
      void display_no_clear(String title, int font, uint8_t x, uint8_t y) {
        oled->setFontType(font);
        oled->setCursor(x, y);
        oled->print(title);
        oled->display();
    }
  public:
    MicroOLED* oled = new MicroOLED();

    int       lastDisplay = 0;
    const int DISPLAY_RATE_IN_MS = 1000;
    int       baseline = 0;
    const int MAX_BASELINE = 16;

    virtual void startup() {
        oled->begin();    // Initialize the OLED
        oled->clear(ALL); // Clear the display's internal memory
        oled->display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled->clear(PAGE); // Clear the buffer.
    }

    virtual void display(String title, int font, uint8_t x, uint8_t y) {
        oled->clear(PAGE);
        display_no_clear(title, font, x, y);
    }

    virtual void display(String title, int font) {
        display(title, font, 0, 0);
    }

    virtual void displayNumber(String s) {
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

    virtual void displayValueAndTime(int value, String timeStr) {
      int thisMS = millis();
      if (thisMS - lastDisplay > DISPLAY_RATE_IN_MS) {
        clear();
        display(String(value), 1, 0, baseline);
        display_no_clear(timeStr, 1, 0, baseline + 16);
        baseline++;
        if (baseline > MAX_BASELINE) {
          baseline = 0;
        }
        lastDisplay = millis();
      }
    }

    virtual void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled->getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled->getLCDHeight()));
        JSonizer::addSetting(json, "lastDisplay", String(lastDisplay));
        JSonizer::addSetting(json, "DISPLAY_RATE_IN_MS", String(DISPLAY_RATE_IN_MS));
        JSonizer::addSetting(json, "baseline", String(baseline));
        JSonizer::addSetting(json, "MAX_BASELINE", String(MAX_BASELINE));
        json.concat("}");
        Particle.publish("OLED", json);
    }

    virtual void clear() {
      oled->clear(ALL);
    }
};

#include <U8g2lib.h>
U8G2_SSD1327_EA_W128128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
// Won't compile inside class, so use global variable instead of member variable.

class OLEDWrapperU8g2 : public OLEDWrapper {
  private:
    void display_(String s, int x, int y) {
      // u8g2.drawUTF8(x, y, s.c_str());
      Utils::publish("Debug", "Displaying: " + s);
    }
    void u8g2_prepare(void) {
      u8g2.setFont(u8g2_font_fur49_tn);
      u8g2.setFontRefHeightExtendedText();
      u8g2.setDrawColor(1);
      u8g2.setFontDirection(0);
    }
    void startDisplay(const uint8_t *font) {
      u8g2_prepare();
      u8g2.clearBuffer();
      u8g2.setFont(font);
    }
    void endDisplay() {
      u8g2.sendBuffer();
    }
  public:
    void startup() override {
      pinMode(10, OUTPUT);
      pinMode(9, OUTPUT);
      digitalWrite(10, 0);
      digitalWrite(9, 0);
      u8g2.begin();
      u8g2.setBusClock(400000);
    }
    void clear() override {
      u8g2.clearBuffer();
      u8g2.sendBuffer();
    }
    void display(String title, int font, uint8_t x, uint8_t y) override {
      startDisplay(u8g2_font_fur49_tn);
      display_(title, x, y);
      endDisplay();
    }
    void display(String title, int font) override {
        display(title, font, 0, 0);
    }
    void displayNumber(String s) override {
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
    void publishJson() override {
        String json("{");
        JSonizer::addFirstSetting(json, "OLEDWrapperU8g2", "active");
        json.concat("}");
        Particle.publish("OLED", json);
    }
  };

OLEDWrapper* oledWrapper = nullptr;

class SensorHandler {
  private:
    uint16_t applyBaseline(uint16_t v) {
      if (v < Utils::getDeviceBaseline()) {
        return 0;
      }
      return v - Utils::getDeviceBaseline();
    }

    uint16_t          max_in_publish_interval = 0;
    long unsigned int last_millis_of_max = 0;

    int getZeroCorrected() {
        if (max_in_publish_interval > Utils::getDeviceZeroCorrection()) {
          return max_in_publish_interval - Utils::getDeviceZeroCorrection();
        }
        return 0;
    }
    void do_publish(unsigned long elapsedMillis) {
        String json("{");
        JSonizer::addFirstSetting(json, "max_in_publish_interval", String(getZeroCorrected()));
        JSonizer::addSetting(json, "elapsedSeconds", String(elapsedMillis / 1000));
        json.concat("}");
        Particle.publish("vibration", json);
    }

    uint16_t      max_A0 = 0;
    const int     NUM_SAMPLES = 1000;
    const int     PIEZO_PIN_0 = A0;
    String        last_time_of_max;

    void getVoltages() {
      max_A0 = 0;
      for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t piezoV = analogRead(PIEZO_PIN_0);
        if (piezoV > max_A0) {
          max_A0 = piezoV;
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
    }

    unsigned long last_publish_time = 0;
    const int     PUBLISH_RATE_IN_SECONDS = 5;

    void publish_max(unsigned long elapsedMillis) {
      unsigned long now = millis();
      if (now - last_publish_time > PUBLISH_RATE_IN_SECONDS * 1000) {
        do_publish(elapsedMillis);
        last_publish_time = millis();
        max_in_publish_interval = 0;
      }
    }

    const uint16_t  BASE_LINE = 425;
    const uint16_t  MAX_VIBRATION_VALUE = 150 + BASE_LINE; // Keep max low enough to show 'usual' vibration in graph.

    bool in_publishing_window() {
      const unsigned long TWO_HOURS_IN_MS = 1000 * 60 * 60 * 2;
      return ((last_millis_of_max > 0) && (millis() - last_millis_of_max < TWO_HOURS_IN_MS));
    }

    String getJson() {
      String json("{");
      JSonizer::addFirstSetting(json, "last_time_of_max", last_time_of_max);
      JSonizer::addSetting(json, "PIEZO_PIN_0", String(PIEZO_PIN_0));
      JSonizer::addSetting(json, "NUM_SAMPLES", String(NUM_SAMPLES));
      JSonizer::addSetting(json, "max_A0", String(max_A0));
      JSonizer::addSetting(json, "in_publishing_window()", String(JSonizer::toString(in_publishing_window())));
      JSonizer::addSetting(json, "MAX_VIBRATION_VALUE", String(MAX_VIBRATION_VALUE));
      JSonizer::addSetting(json, "last_millis_of_max", String(last_millis_of_max));
      json.concat("}");
      return json;
    }
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_0, INPUT);
    }

    void monitor_sensor() {
      getVoltages();
      if (Utils::alwaysPublishData) {
        publish_max(millis() - Utils::startPublishDataMillis);
        oledWrapper->displayValueAndTime(getZeroCorrected(),
                              Utils::elapsedTime(millis() - Utils::startPublishDataMillis));
        if (Utils::publishDataDone()) {
          oledWrapper->clear();
        }
      } else if (in_publishing_window()) {
        // publish_max(millis() - last_millis_of_max);
        // display();
      }
    }

    void display(String timeStr) {
      oledWrapper->displayValueAndTime(max_A0, timeStr);
    }

    void display() {
      display(timeSupport.getUpTime());
    }

    void sample_and_publish_() {
      getVoltages();
      do_publish(millis() - last_millis_of_max);
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

int publish_settings(String cmd);
int switch_to_u8g2(String cmd);

class App {
  public:
      // getSettings() is already defined somewhere.
    int publish_settings_(String command) {
        if (command.compareTo("") == 0) {
            Utils::publishJson();
        } else if (command.compareTo("time") == 0) {
            timeSupport.publishJson();
        } else if (command.compareTo("sensor") == 0) {
            sensorhandler.publishJson();
        } else if (command.compareTo("oled") == 0) {
            oledWrapper->publishJson();
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
        oledWrapper->display(timeSupport.getUpTime(), 2, 0, lastY);
        lastUpTimeDisplay = millis();
      }
    }
    int switch_to_u8g2_(String cmd) {
      Utils::publish("Debug", "Switching to U8g2");
      if (oledWrapper != nullptr) {
        delete oledWrapper;
      }
      oledWrapper = new OLEDWrapperU8g2();
      oledWrapper->startup();
/*      oledWrapper->display("Using U8g2", 1);
      delay(2000);
      oledWrapper->clear();
*/      return 1;
    } 
    void setup() {
      oledWrapper = new OLEDWrapper();
      oledWrapper->startup();
      oledWrapper->display("Starting setup...", 1);
      Particle.function("GetData", sample_and_publish);
      Particle.function("GetSetting", publish_settings);
      Particle.function("reset", remoteResetFunction);
      Particle.function("alwaysPub", setAlwaysPublishData);
      Particle.function("switchOled", switch_to_u8g2);
      delay(1000);
      Utils::publishJson();
      sensorhandler.sample_and_publish_();
      oledWrapper->display("Setup finished", 1);
      delay(2000);
      oledWrapper->clear();
      Utils::publish("setup()", "Finished");
    }
    void loop() {
      timeSupport.handleTime();
      sensorhandler.monitor_sensor();
      Utils::checkForRemoteReset();
    }
};
App app;

int publish_settings(String cmd) {
  return app.publish_settings_(cmd);
}

int switch_to_u8g2(String cmd) {
  return app.switch_to_u8g2_(cmd);
}

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
