
#ifdef RAKDEVICE_STANDALONE

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

class Intervalable {
    interval_t _interval, _previous;
    counter_t _exceeded = 0;

public:
    explicit Intervalable (const interval_t interval = 0, const interval_t previous = 0) :
        _interval (interval),
        _previous (previous) { }
    operator bool () {
        const interval_t current = millis ();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    bool active () const {
        return _interval > 0;
    }
    interval_t remaining () const {
        const interval_t current = millis ();
        return _interval - (current - _previous);
    }
    bool passed (interval_t *interval = nullptr, const bool atstart = false) {
        const interval_t current = millis ();
        if ((atstart && _previous == 0) || current - _previous > _interval) {
            if (interval != nullptr)
                (*interval) = current - _previous;
            _previous = current;
            return true;
        }
        return false;
    }
    void reset (const interval_t interval = std::numeric_limits<interval_t>::max ()) {
        if (interval != std::numeric_limits<interval_t>::max ())
            _interval = interval;
        _previous = millis ();
    }
    void setat (const interval_t place) {
        _previous = millis () - ((_interval - place) % _interval);
    }
    void wait () {
        const interval_t current = millis ();
        if (current - _previous < _interval)
            delay (_interval - (current - _previous));
        else if (_previous > 0)
            _exceeded++;
        _previous = millis ();
    }
    counter_t exceeded () const {
        return _exceeded;
    }
};

class ActivationTracker {
    counter_t _count = 0;
    interval_t _seconds = 0;

public:
    inline const interval_t &seconds () const {
        return _seconds;
    }
    inline const counter_t &count () const {
        return _count;
    }
    ActivationTracker &operator++ (int) {
        _seconds = millis () / 1000;
        _count++;
        return *this;
    }
    ActivationTracker &operator= (const counter_t count) {
        _seconds = millis () / 1000;
        _count = count;
        return *this;
    }
    inline operator counter_t () const {
        return _count;
    }
};

#include <ctime>

String getTimeString (time_t timet = 0) {
    struct tm timeinfo;
    char timeString [sizeof ("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
    if (timet == 0)
        time (&timet);
    if (gmtime_r (&timet, &timeinfo) != nullptr)
        strftime (timeString, sizeof (timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return timeString;
}

template <typename T>
class TrackableValue {
private:
    T value;
    bool updateResult { false };
    interval_t updateTime { 0 };

public:
    TrackableValue () = default;
    explicit TrackableValue (const T &initial) :
        value (initial) { }

    void update (const T &newValue) {
        value = newValue;
        updateResult = true;
        updateTime = millis ();
    }
    TrackableValue &operator= (const T &newValue) {
        update (newValue);
        return *this;
    }

    void invalidate () {
        updateResult = false;
        updateTime = millis ();
    }

    const T &get () const { return value; }
    // const T *operator->() const { return &value; }
    operator const T & () const { return value; }

    bool lastResult () const { return updateResult; }
    interval_t lastTime () const { return updateTime; }
};

static String bytesToHexString (const uint8_t *data, size_t length) {
    String result;
    result.reserve (length * 2);
    for (size_t i = 0; i < length; i++) {
        const uint8_t byte = data [i];
        result += "0123456789ABCDEF" [byte >> 4];
        result += "0123456789ABCDEF" [byte & 0x0F];
    }
    return result;
}

static std::vector<uint8_t> hexStringToBytes (const String &data) {
    auto hexDigitToInt = [] (char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return 0;
    };
    std::vector<uint8_t> result;
    size_t length = data.length () - (data.length () % 2);
    result.reserve (length / 2);
    for (size_t i = 0; i < length; i += 2)
        result.push_back ((hexDigitToInt (data [i]) << 4) | hexDigitToInt (data [i + 1]));
    return result;
}

template <typename... Args>
static String join (const char delimiter, const Args &...args) {
    const String values [] = { String (args)... };
    String result;
    for (size_t i = 0; i < sizeof...(args); i++) {
        if (i > 0)
            result += delimiter;
        result += values [i];
    }
    return result;
}

static String debugHexString (const String &data) {
    String r;
    const auto x = hexStringToBytes (data);
    if (x.size () > 0) {
        bool printable = true;
        for (int i = 0; i < x.size () && printable; i++)
            if (! isPrintable (x [i]))
                printable = false;
        if (printable) {
            r.reserve (x.size ());
            r += ", printable=<<";
            for (uint8_t byte : x)
                r += (char) byte;
            r += ">>";
        }
    }
    return "size=" + String (data.length ()) + ", data=" + data + r;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <WiFi.h>
#include "Secrets.hpp"

void inline startWiFi () {
    Serial.println ("Wifi ...");
    WiFi.setHostname ("experimental");
    WiFi.setAutoReconnect (true);
    WiFi.mode (WIFI_MODE_STA);
    WiFi.begin (WIFI_SSID, WIFI_PASS);
    WiFi.setTxPower (WIFI_POWER_8_5dBm);
    while (WiFi.status () != WL_CONNECTED)
        delay (500);
    Serial.println ("Wifi OK");
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#define DEBUG_RAKDEVICE
#define DEBUG_RAKDEVICE_TRANSCEIVER

#include "RakDeviceCommon.hpp"
#include "RakDeviceCommands.hpp"
#include "RakDeviceManager.hpp"
#include "RakDeviceMessenger.hpp"

#include "Secrets.hpp"

const int serialId = 1;
HardwareSerial serial (serialId);

static inline constexpr gpio_num_t PIN_RAK3272_TX = GPIO_NUM_2;
static inline constexpr gpio_num_t PIN_RAK3272_RX = GPIO_NUM_1;

RakDeviceManager *rak3272 = nullptr;
const RakDeviceManager::Config rak3272_config = {
    .loraIdentifiers = {
                        .devEUI = LORA_DEVEUI,
                        .appEUI = LORA_APPEUI,
                        .appKey = LORA_APPKEY }
};

// RakDeviceMessenger *rak3272_messenger = nullptr;

void loraEventHandler (const RakDeviceManager::Event event, const RakDeviceManager::EventArgs &args) {
    switch (event) {
    case RakDeviceManager::Event::JOIN_PENDING :
        Serial.println ("LORA EVENT: Join pending");
        break;
    case RakDeviceManager::Event::JOIN_SUCCESS :
        Serial.printf ("LORA EVENT: Join success, addr=%s\n", args [0].c_str ());
        break;
    case RakDeviceManager::Event::JOIN_FAILURE :
        Serial.printf ("LORA EVENT: Join failed, reason=%s\n", args [0].c_str ());
        break;
    case RakDeviceManager::Event::DATA_RECEIVED :
        Serial.printf ("LORA EVENT: Data received: port=%s, data=%s\n", args [0].c_str (), args [1].c_str ());
        break;
    case RakDeviceManager::Event::TRANSMIT_SUCCESS :
        Serial.println ("LORA EVENT: Transmit success");
        break;
    case RakDeviceManager::Event::TRANSMIT_FAILURE :
        Serial.println ("LORA EVENT: Transmit failure");
        break;
    }
}

void setup () {
    Serial.begin (115200);
    delay (2.5 * 1000);
    Serial.println ("UP");

    startWiFi ();

    pinMode (PIN_RAK3272_RX, INPUT);
    pinMode (PIN_RAK3272_TX, OUTPUT);
    serial.setRxBufferSize (1024);
    serial.setTxBufferSize (512);
    serial.begin (115200, SERIAL_8N1, PIN_RAK3272_RX, PIN_RAK3272_TX, false);

    rak3272 = new RakDeviceManager (rak3272_config, serial);
    if (! rak3272->begin ())
        Serial.printf ("RakDeviceManager::setup () failed\n");
    rak3272->addEventListener (loraEventHandler);

    //    rak3272_messenger = new RakDeviceMessenger (*rak3272);
}

// -----------------------------------------------------------------------------------------------

Intervalable second (1 * 1000);
Intervalable ping (30 * 1000);

void loop () {
    second.wait ();

    rak3272->process ();
    // rak3272_messenger->process ();

    // RakDeviceMessenger::Message message;
    // while (rak3272_messenger->receive (message))
    //     Serial.printf ("Received message on port %d: %s\n", message.port, message.data.c_str ());

    // if (rak3272->isAvailable () && rak3272_messenger->transmit_queue_size () < 32 && ping) {
    if (rak3272->isAvailable () && ping) {
        static int counter = 1;
        rak3272->transmit (Lora::Port (1), "{\"ping\": \"" + String (counter++) + "\"}");
        // rak3272_messenger->transmit (RakDeviceMessenger::Message (Lora::Port (1), "{\"ping\": \"" + String (counter++) + "\"}"));
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#endif
