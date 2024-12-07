
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <map>

#if defined(DEBUG_RAKDEVICE) && ! defined(DEBUG_PRINTF)
#ifndef DEBUG_RAKDEVICE_SERIAL
#define DEBUG_RAKDEVICE_SERIAL Serial
#endif
#ifndef DEBUG_RAKDEVICE_SERIAL_BAUD
#define DEBUG_RAKDEVICE_SERIAL_BAUD 115200
#endif
#define DEBUG_START(...) DEBUG_RAKDEVICE_SERIAL.begin (DEBUG_RAKDEVICE_SERIAL_BAUD);
#define DEBUG_END(...)               \
    DEBUG_RAKDEVICE_SERIAL.flush (); \
    DEBUG_RAKDEVICE_SERIAL.end ();
#define DEBUG_PRINTF    DEBUG_RAKDEVICE_SERIAL.printf
#define DEBUG_ONLY(...) __VA_ARGS__
#else
#define DEBUG_START(...)
#define DEBUG_END(...)
#define DEBUG_PRINTF(...) \
    do {                  \
    } while (0)
#define DEBUG_ONLY(...)
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct Lora {

    enum class Class : char {
        CLASS_A = 'A',
        CLASS_B = 'B',
        CLASS_C = 'C'
    };

    enum class ClassB_Status : int {
        DEVICETIME_REQ = 0,
        BEACON_SEARCHING = 1,
        BEACON_LOCKED = 2,
        BEACON_FAILED = 3
    };

    enum class Mode : int {
        MODE_UNDEFINED = -1,
        MODE_P2PLORA = 0,
        MODE_LORAWAN = 1,
        MODE_P2PFSK = 2
    };

    enum class Band : int {
        BAND_EU868 = 4
    };
    static constexpr int MINIMUM_BAND = 0, MAXIMUM_BAND = 12;
    static constexpr Band DEFAULT_BAND = Band::BAND_EU868;

    enum class Datarate : int {
        SF12 = 0,
        SF11 = 1,
        SF10 = 2,
        SF9 = 3,
        SF8 = 4,
        SF7 = 5
    };
    static constexpr int MINIMUM_DATARATE = 0, MAXIMUM_DATARATE = 5;

    enum class Join : int {
        JOIN_ABP = 0,
        JOIN_OTAA = 1
    };

    enum class TxPower : int {
        HIGHEST = 0,
        LOWEST = 7
    };
    static constexpr int MINIMUM_TXPOWER = 0, MAXIMUM_TXPOWER = 7;

    enum class AutoJoin : int {
        AUTOJOIN = 1,
        NO_AUTOJOIN = 0
    };
    static constexpr AutoJoin DEFAULT_AUTOJOIN = AutoJoin::NO_AUTOJOIN;

    enum class LinkCheck : int {
        LINKCHECK_DISABLED = 0,
        LINKCHECK_ONCE = 1,
        LINKCHECK_EVERYTIME = 2
    };
    static constexpr int MINIMUM_LINKCHECK = 0, MAXIMUM_LINKCHECK = 2;

    typedef int Port;
    typedef int RSSI;
    typedef int SNR;
    typedef long Frequency;

    struct LinkStatus {
        int DemodMargin = 0, NbGateways = 0;
        Lora::RSSI RSSI = 0;
        Lora::SNR SNR = 0;
    };
    struct ReceiveStatus {
        Lora::RSSI RSSI = 0;
        Lora::SNR SNR = 0;
    };

    static constexpr int MINIMUM_SEND_SIZE = 1, MAXIMUM_SEND_SIZE = 2500;    // 1256 hexadecimal numbers
    static constexpr int MINIMIM_SEND_PORT = 1, MAXIMUM_SEND_PORT = 233;
    static constexpr int MINIMUM_JOIN_ATTEMPTS_DELAY = 7, MAXIMUM_JOIN_ATTEMPTS_DELAY = 255, DEFAULT_JOIN_ATTEMPTS_DELAY = 8;
    static constexpr int MINIMUM_JOIN_ATTEMPTS = 0, MAXIMUM_JOIN_ATTEMPTS = 255, DEFAULT_JOIN_ATTEMPTS = 0;
    static constexpr int MAXIMUM_CHANNELS = 16;
    static constexpr int MINIMUM_RX1_DELAY = 1, MAXIMUM_RX1_DELAY = 15;
    static constexpr int MINIMUM_RX2_DELAY = 2, MAXIMUM_RX2_DELAY = 15;
    static constexpr int MINIMUM_JN1_DELAY = 1, MAXIMUM_JN1_DELAY = 14;
    static constexpr int MINIMUM_JN2_DELAY = 2, MAXIMUM_JN2_DELAY = 15;
    static constexpr int MINIMUM_CONFIRMRETRY = 0, MAXIMUM_CONFIRMRETRY = 7;
    static constexpr int MINIMUM_NJM = 0, MAXIMUM_NJM = 2;
    static constexpr int MINIMUM_NWM = 0, MAXIMUM_NWM = 2;

    static String toString (const Class c) {
        switch (c) {
        case Class::CLASS_A :
            return "CLASS_A";
        case Class::CLASS_B :
            return "CLASS_B";
        case Class::CLASS_C :
            return "CLASS_C";
        default :
            return "UNDEFINED";
        }
    }
    static String toString (const Mode m) {
        switch (m) {
        case Mode::MODE_P2PLORA :
            return "LoRa P2P";
        case Mode::MODE_LORAWAN :
            return "LoRaWAN";
        case Mode::MODE_P2PFSK :
            return "FSK";
        default :
            return "UNDEFINED";
        }
    }
    static String toString (const Band b) {
        switch (b) {
        case Band::BAND_EU868 :
            return "4 (EU868)";
        default :
            return "UNDEFINED";
        }
    }
    static String toString (const Join j) {
        switch (j) {
        case Join::JOIN_ABP :
            return "ABP";
        case Join::JOIN_OTAA :
            return "OTAA";
        default :
            return "UNDEFINED";
        }
    }
    static String toString (const LinkStatus &s) {
        return "RSSI=" + String (s.RSSI) + ", SNR=" + String (s.SNR) + ", Margin=" + String (s.DemodMargin) + ", Gateways=" + String (s.NbGateways);
    }
    static String toString (const ReceiveStatus &s) {
        return "RSSI=" + String (s.RSSI) + ", SNR=" + String (s.SNR);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct RakDeviceResult {
    bool success;
    String details;
    RakDeviceResult (const bool s = false, const String &d = String ()) :
        success (s),
        details (d) { }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct RakDeviceEvent {
    using Handler = std::function<void (const RakDeviceEvent &)>;
    String type, args;
    RakDeviceEvent (const String &t, const String &a = String ()) :
        type (t),
        args (a) { }
    static RakDeviceEvent extract (const String &evt, const String &prefix, const char separator) {
        const int argsOffset = evt.indexOf (separator, prefix.length ());
        return RakDeviceEvent (
            evt.substring (prefix.length (), argsOffset > 0 ? argsOffset : evt.length ()),
            (argsOffset > 0) ? evt.substring (argsOffset + 1) : "");
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct RakDeviceAttributeValidator {
    static bool isHexadecimalString (const String &str) {
        for (const char c : str)
            if (! isHexadecimalDigit (c))
                return false;
        return true;
    }
    static RakDeviceResult validateIsCommandWithEquals (const String &candidate, const String &command) {
        if (! candidate.startsWith (command + "="))
            return RakDeviceResult (false, command.substring (1) + " response '" + candidate + "' does not start with '" + command + "='");
        return true;
    }
    static RakDeviceResult validateIsValueWithinMinMax (const int value, const String &command, const String &type, const int value_min, const int value_max, const String &errorString) {
        if (value < value_min || value > value_max)
            return RakDeviceResult (false, command.substring (1) + " " + type + " has value '" + String (value) + "' but must be between " + String (value_min) + "and" + String (value_max) + (! errorString.isEmpty () ? String (" [" + errorString + "]") : String ("")));
        return true;
    }
    static RakDeviceResult validateStringIsHexadecimal (const String &candidate, const String &command, const String &errorString) {
        if (candidate.length () % 2 != 0)
            return RakDeviceResult (false, command.substring (1) + " data has length '" + String (candidate.length ()) + "' that is not even (for hexadecimal pairs)" + (! errorString.isEmpty () ? String (" [" + errorString + "]") : String ("")));
        if (! isHexadecimalString (candidate))
            return RakDeviceResult (false, command.substring (1) + " data has non hexadecimal characters" + (! errorString.isEmpty () ? String (" [" + errorString + "]") : String ("")));
        return true;
    }
    static RakDeviceResult validateIsValueIsZeroOrOne (const int value, const String &command, const String &type, const String &errorString) {
        if (value != 0 && value != 1)
            return RakDeviceResult (false, command.substring (1) + " " + type + " has value '" + String (value) + "' but must be value 0 or 1" + (! errorString.isEmpty () ? String (" [" + errorString + "]") : String ("")));
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceTransceiver {
    static inline constexpr unsigned int BUFFER_MINIMUM_SIZE = 128;    // most responses are less than this
    static inline constexpr uint32_t BLOCKING_WAIT_DELAY = 5;
    Stream &_stream;

public:
    RakDeviceTransceiver (Stream &stream) :
        _stream (stream) {
        _stream.setTimeout (2000);
    }
    void poke () {
        _stream.print ("\n");
    }
    bool send (const String &cmd) {
#ifdef DEBUG_RAKDEVICE_TRANSCEIVER
        DEBUG_PRINTF ("-TX-> <<%s>>\n", cmd.c_str ());
#endif
        _stream.print (cmd + "\n");
        return true;
    }
    bool available () const {
        return _stream.available () > 0;
    }
    String readLine (const bool blocking = false) {
        String buffer;
        if (blocking || _stream.available ()) {
            buffer.reserve (BUFFER_MINIMUM_SIZE);
            int r;
            do {
                while (blocking && ! _stream.available ())
                    delay (BLOCKING_WAIT_DELAY);
                if (_stream.available () && (r = _stream.read ()) >= 0)
                    buffer += (char) r;
            } while (! (r == '\r' || r == '\n'));
            while ((r = _stream.peek ()) >= 0 && (r == '\r' || r == '\n'))
                (void) _stream.read ();
            buffer.trim ();
#ifdef DEBUG_RAKDEVICE_TRANSCEIVER
            if (! buffer.isEmpty ())
                DEBUG_PRINTF ("<-RX- <<%s>>\n", buffer.c_str ());
#endif
        }
        return buffer;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommander;
class RakDeviceCommand {
protected:
    String _response;
    virtual String requestBuild () const = 0;
    virtual RakDeviceResult requestValidate () const { return true; }
    friend RakDeviceCommander;

public:
    virtual ~RakDeviceCommand () { }
    const String &responseGet () const { return _response; }
    virtual RakDeviceResult responseSet (const String &response) {
        _response = response;
        return RakDeviceResult (_response == "OK", "response == 'OK'");
    }
    virtual bool isAsync () const { return false; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommander {
    RakDeviceTransceiver &_transceiver;
    RakDeviceEvent::Handler _eventHandler;

public:
    static inline constexpr int AT_BUSY_DELAY = 10, AT_BUSY_TRIES = 3;

    RakDeviceCommander (RakDeviceTransceiver &transceiver, const RakDeviceEvent::Handler &eventHandler = nullptr) :
        _transceiver (transceiver),
        _eventHandler (eventHandler) { }

    void process () {
        do {
            const String communique = _transceiver.readLine (false);
            if (communique.isEmpty ())
                return;
            if (communique != "OK" && communique != "AT_BUSY_ERROR")
                processUnsolicited (communique);
        } while (true);
    }

    void processEvent (const RakDeviceEvent &event) {
        if (_eventHandler)
            _eventHandler (event);
    }
    bool processUnsolicited (const String &communique) {
        // https://github.com/RAKWireless/RAK-STM32-RUI/
        // +EVT:LINKCHECK:Y0,Y1,Y2,Y3,Y4
        // +BC: ... LOCKED/DONE/FAILED
        // +PS: ... DONE
        // +EVT:RX_1:-70:8:UNICAST:1:1234
        // +EVT:RX_B:-47:3:UNICAST:2:4321
        // +EVT:JOINED
        // +EVT:JOIN_FAILED_TX_TIMEOUT
        // +EVT:JOIN_FAILED_RX_TIMEOUT
        // +EVT:JOIN_FAILED_errorcode
        // +EVT:TX_DONE
        // +EVT:SEND_CONFIRMED_OK
        // +EVT:SEND_CONFIRMED_FAILED
        // +EVT:TXP2P DONE
        // +EVT:RXP2P RECEIVE TIMEOUT
        // +EVT:RXP2P
        // +EVT:TIMEREQ_FAILED // not in the RU13 specification
        // +EVT:TIMEREQ_OK // not in the RU13 specification
        // +EVT:SWITCH_FAILED // not in the RU13 specification
        // +BC: ...ONGOING/LOST/FAILED_errorcode // not in the RU13 specification
        // Restricted_Wait_3343902_ms
        // AT_BUSY_ERROR
        // RAKwireless RAK3272-SiP Example
        // ------------------------------------------------------
        // Current Work Mode: LoRaWAN.
        if (communique.startsWith ("+EVT:"))
            processEvent (RakDeviceEvent::extract (communique, "+EVT:", ':'));
        else if (communique.startsWith ("+BC:"))
            processEvent (RakDeviceEvent ("BC", communique.substring (sizeof ("+BC:") - 1)));
        else if (communique.startsWith ("+PS:"))
            processEvent (RakDeviceEvent ("PS", communique.substring (sizeof ("+PS:") - 1)));
        else if (communique.startsWith ("Restricted_Wait_"))
            processEvent (RakDeviceEvent ("RestrictedWait", communique.substring (sizeof ("Restricted_Wait_") - 1)));
        else if (communique.startsWith ("Current Work Mode:"))
            processEvent (RakDeviceEvent ("CurrentWorkMode", communique.substring (sizeof ("Current Work Mode: ") - 1, communique.length () - 1)));
        else if (communique == String ("------------------------------------------------------") || communique == String ("RAKwireless RAK3272-SiP Example"))
            ;
        else {
            DEBUG_PRINTF ("RakDeviceCommander::processUnsolicited: unprocessable = <<%s>>\n", communique.c_str ());
            return false;
        }
        return true;
    }

    RakDeviceResult issue (RakDeviceCommand &cmd) {

        process ();

        const RakDeviceResult validateResult = cmd.requestValidate ();
        if (! validateResult.success)
            return validateResult;
        const String request = cmd.requestBuild ();
        _transceiver.send ("AT" + request);
        const String response = _transceiver.readLine (true);
        int tries = 0;
        while (true) {
            const RakDeviceResult responseResult = cmd.responseSet (response);
            if (! responseResult.success) {
                if (response == "AT_BUSY_ERROR") {
                    if (tries ++ >= AT_BUSY_TRIES)
                        return false;
                    DEBUG_PRINTF ("RakDeviceCommander::issue: AT_BUSY, retry #%d\n", tries);
                    delay (AT_BUSY_DELAY);
                } else {                                        
                    if (!processUnsolicited (response)) // typically restricted wait
                        DEBUG_PRINTF ("RakDeviceCommander::issue: invalid-response = <<%s>>\n", response.c_str ());
                    return responseResult;
                }
            } else {
                return true;
            }
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
