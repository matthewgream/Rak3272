
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

static constexpr char CMD_VER [] = "+VER", CMD_HWM [] = "+HWMODEL", CMD_SNO [] = "+SN", CMD_API [] = "+APIVER", CMD_TIM [] = "+LTIME", CMD_HID [] = "+HWID";

static constexpr char CMD_SLEEP [] = "+SLEEP", CMD_RESET [] = "+RESET", CMD_LOWPOW [] = "+LPM", CMD_DEBUG [] = "+DEBUG";

static constexpr char CMD_BAND [] = "+BAND", CMD_CLASS [] = "+CLASS", CMD_DR [] = "+DR", CMD_TXP [] = "+TXP", CMD_ADR [] = "+ADR", CMD_DCS [] = "+DCS", CMD_PNM [] = "+PNM";
static constexpr char CMD_CFM [] = "+CFM", CMD_RETY [] = "+RETY";

static constexpr char CMD_DEV_EUI [] = "+DEVEUI", CMD_APP_EUI [] = "+APPEUI", CMD_APP_KEY [] = "+APPKEY", CMD_DEV_ADDR [] = "+DEVADDR";
static constexpr char CMD_NWM [] = "+NWM", CMD_NJM [] = "+NJM", CMD_NJS [] = "+NJS";
static constexpr char CMD_JOIN [] = "+JOIN";

static constexpr char CMD_RX1_DELAY [] = "+RX1DL", CMD_RX2_DELAY [] = "+RX2DL", CMD_RX2_DR [] = "+RX2DR";
static constexpr char CMD_JN1DL_DELAY [] = "+JN1DL", CMD_JN2DL_DELAY [] = "+JN2DL";
static constexpr char CMD_RX2_FREQ [] = "+RX2FQ";

static constexpr char CMD_RSSI [] = "+RSSI", CMD_SNR [] = "+SNR";
static constexpr char CMD_ARSSI [] = "+ARSSI";
static constexpr char CMD_LINKCHECK [] = "+LINKCHECK";

static constexpr char CMD_SEND_STATUS [] = "+CFS";
static constexpr char CMD_SEND [] = "+SEND", CMD_RECV [] = "+RECV";
static constexpr char CMD_TIMEREQUEST [] = "+TIMEREQ";

// -----------------------------------------------------------------------------------------------

static constexpr char ERR_BAND [] = "0 = EU433, 1 = CN470, 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, 8 = AS923-1, 9 = AS923-2, 10 = AS923-3, 11 = AS923-4, 12 = LA915";
static constexpr char ERR_CLASS [] = "A = Class A, B = Class B, C = Class B";
static constexpr char ERR_DR [] = "EU868: 0 = SF12, 1 = SF11, 2 = SF10, 3 = SF9, 4 = SF8, 5 = SF7";
static constexpr char ERR_TXP [] = "EU868: 0 = Highest, 7 = Lowest";
static constexpr char ERR_RETY [] = "0 to 7 attempts";

static constexpr char ERR_NWM [] = "0 = P2P_LORA, 1 = LoRaWAN, 2 = P2P_FSK";
static constexpr char ERR_NJM [] = "0 = ABP, 1 = OTAA";

static constexpr char ERR_RX1_DELAY [] = "1 to 15 seconds";
static constexpr char ERR_RX2_DELAY [] = "2 to 15 seconds";
static constexpr char ERR_RX2_DR [] = "EU868: 0 = SF12, 1 = SF11, 2 = SF10, 3 = SF9, 4 = SF8, 5 = SF7";
static constexpr char ERR_JN1DL_DELAY [] = "1 to 14 seconds";
static constexpr char ERR_JN2DL_DELAY [] = "2 to 15 seconds";

static constexpr char ERR_LINKCHECK [] = "0 = disabled, 1 = once, 2 = everytime";

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <const char *CMD, bool IS_QUERY>
class RakDeviceCommand_Simple : public RakDeviceCommand {

protected:
    String requestBuild () const override {
        return String (CMD) + (IS_QUERY ? "=?" : "");
    }
    RakDeviceResult responseSet (const String &response) override {
        const String command ("AT" + String (CMD));
        RakDeviceResult result = RakDeviceAttributeValidator::validateIsCommandWithEquals (response, command);
        if (! result.success)
            return result;
        _response = response.substring (command.length () + 1);
        return true;
    }
};

typedef RakDeviceCommand_Simple<CMD_VER, true> RakDeviceCommand_VERSION;
typedef RakDeviceCommand_Simple<CMD_HWM, true> RakDeviceCommand_HWMODEL;
typedef RakDeviceCommand_Simple<CMD_HID, true> RakDeviceCommand_HWID;
typedef RakDeviceCommand_Simple<CMD_SNO, true> RakDeviceCommand_SERIALNO;
class RakDeviceCommand_APIVERSION : public RakDeviceCommand_Simple<CMD_API, true> {
    // get string x.y.z
};
class RakDeviceCommand_LTIME : public RakDeviceCommand_Simple<CMD_TIM, true> {
public:
    String getTimeStringISO () const {    // LTIME: 04h36m00s on 11/27/2023
        const int onPos = _response.indexOf (" on ");
        const String timeStr = _response.substring (0, onPos), dateStr = _response.substring (onPos + 4);
        const int hours = timeStr.substring (0, 2).toInt (), minutes = timeStr.substring (3, 5).toInt (), seconds = timeStr.substring (6, 8).toInt ();
        const int month = dateStr.substring (0, 2).toInt (), day = dateStr.substring (3, 5).toInt (), year = dateStr.substring (6).toInt ();
        return String (year) + "-" + (month < 10 ? "0" : "") + String (month) + "-" + (day < 10 ? "0" : "") + String (day) + "T" +
               (hours < 10 ? "0" : "") + String (hours) + ":" + (minutes < 10 ? "0" : "") + String (minutes) + ":" + (seconds < 10 ? "0" : "") + String (seconds) + "Z";
    }
};
typedef RakDeviceCommand_Simple<CMD_SLEEP, false> RakDeviceCommand_SLEEP;
typedef RakDeviceCommand_Simple<CMD_RESET, false> RakDeviceCommand_RESET;
class RakDeviceCommand_RSSI_ALL : public RakDeviceCommand_Simple<CMD_ARSSI, true> {
public:
    using ChannelsRSSI = std::vector<std::pair<int, Lora::RSSI>>;

protected:
    ChannelsRSSI channelsRSSI {};
    RakDeviceResult responseSet (const String &response) override {
        for (int offset = 0, next = 0; offset < response.length (); offset = next) {
            const int colon = response.indexOf (':', offset), comma = response.indexOf (',', colon);
            next = comma >= 0 ? comma + 1 : response.length ();
            const int channel = response.substring (offset, colon).toInt (), value = response.substring (colon + 1, next).toInt ();
            if (channel >= 0 && channel <= Lora::MAXIMUM_CHANNELS)
                channelsRSSI.push_back (std::pair (channel, value));
        }
        return true;
    }

public:
    const ChannelsRSSI &getChannelsRSSI () const { return channelsRSSI; }
};
class RakDeviceCommand_JOIN_STATUS : public RakDeviceCommand_Simple<CMD_NJS, true> {
public:
    bool isJoined () const { return responseGet () == "1"; }
};
class RakDeviceCommand_RSSI_LAST : public RakDeviceCommand_Simple<CMD_RSSI, true> {
public:
    Lora::RSSI RSSI () const { return responseGet ().toInt (); }
};
class RakDeviceCommand_SNR_LAST : public RakDeviceCommand_Simple<CMD_SNR, true> {
public:
    Lora::SNR SNR () const { return responseGet ().toInt (); }
};
class RakDeviceCommand_SEND_STATUS : public RakDeviceCommand_Simple<CMD_SEND_STATUS, true> {
public:
    bool wasSuccessful () const { return responseGet () == "1"; }
};
class RakDeviceCommand_RX2_FREQ : public RakDeviceCommand_Simple<CMD_RX2_FREQ, true> {
public:
    Lora::Frequency frequency () const { return responseGet ().toInt (); }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <typename T, const char *CMD>
class RakDeviceCommand_Queryable : public RakDeviceCommand {
protected:
    T _value;
    bool _isQuery;
    String requestBuild () const override {
        return String (CMD) + (_isQuery ? "=?" : requestValueBuild ());
    }
    RakDeviceResult responseSet (const String &response) override {
        const String command ("AT" + String (CMD));
        if (_isQuery) {
            RakDeviceResult result = RakDeviceAttributeValidator::validateIsCommandWithEquals (response, command);
            if (! result.success)
                return result;
            return responseProcess (_response = response.substring (command.length () + 1));
        }
        return RakDeviceCommand::responseSet (response);
    }

    virtual String requestValueBuild () const = 0;
    virtual RakDeviceResult responseProcess (const String &value) = 0;

public:
    explicit RakDeviceCommand_Queryable (const T &value) :
        _value (value),
        _isQuery (false) { }
    RakDeviceCommand_Queryable () :
        _value (T ()),
        _isQuery (true) { }
    const T &getValue () const { return _value; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <const char *CMD>
class RakDeviceCommand_Boolean : public RakDeviceCommand_Queryable<bool, CMD> {
    using Base = RakDeviceCommand_Queryable<bool, CMD>;

protected:
    String requestValueBuild () const override {
        return "=" + String (Base::_value ? "1" : "0");
    }
    RakDeviceResult requestValidate () const override {
        return true;
    }
    RakDeviceResult responseProcess (const String &value) override {
        Base::_value = (value.toInt () == 1);
        return RakDeviceAttributeValidator::validateIsValueIsZeroOrOne (value.toInt (), CMD, "value", String ());
    }

public:
    using Base::Base;
};

typedef RakDeviceCommand_Boolean<CMD_CFM> RakDeviceCommand_CONFIRM_MODE;
typedef RakDeviceCommand_Boolean<CMD_DCS> RakDeviceCommand_DUTY_CYCLE;
typedef RakDeviceCommand_Boolean<CMD_ADR> RakDeviceCommand_ADR;
typedef RakDeviceCommand_Boolean<CMD_PNM> RakDeviceCommand_PNM;
typedef RakDeviceCommand_Boolean<CMD_LOWPOW> RakDeviceCommand_LPM;
typedef RakDeviceCommand_Boolean<CMD_DEBUG> RakDeviceCommand_DEBUG;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <const char *CMD, int MIN_VALUE = 0, int MAX_VALUE = 0, const char *ERR_MSG = nullptr>
class RakDeviceCommand_Integer : public RakDeviceCommand_Queryable<int, CMD> {
    using Base = RakDeviceCommand_Queryable<int, CMD>;

protected:
    String requestValueBuild () const override {
        return "=" + String (Base::_value);
    }
    RakDeviceResult requestValidate () const override {
        return ! Base::_isQuery ? RakDeviceAttributeValidator::validateIsValueWithinMinMax (Base::_value, CMD, "value", MIN_VALUE, MAX_VALUE, String (ERR_MSG ? ERR_MSG : "")) : true;
    }
    RakDeviceResult responseProcess (const String &value) override {
        Base::_value = value.toInt ();
        return RakDeviceAttributeValidator::validateIsValueWithinMinMax (Base::_value, CMD, "value", MIN_VALUE, MAX_VALUE, String (ERR_MSG ? ERR_MSG : ""));
    }

public:
    using Base::Base;
};

typedef RakDeviceCommand_Integer<CMD_NJM, Lora::MINIMUM_NJM, Lora::MAXIMUM_NJM, ERR_NJM> RakDeviceCommand_NJM;
typedef RakDeviceCommand_Integer<CMD_NWM, Lora::MINIMUM_NWM, Lora::MAXIMUM_NWM, ERR_NWM> RakDeviceCommand_NWM;
typedef RakDeviceCommand_Integer<CMD_BAND, Lora::MINIMUM_BAND, Lora::MAXIMUM_BAND, ERR_BAND> RakDeviceCommand_BAND;
typedef RakDeviceCommand_Integer<CMD_DR, Lora::MINIMUM_DATARATE, Lora::MAXIMUM_DATARATE, ERR_DR> RakDeviceCommand_DATARATE;
typedef RakDeviceCommand_Integer<CMD_TXP, Lora::MINIMUM_TXPOWER, Lora::MAXIMUM_TXPOWER, ERR_TXP> RakDeviceCommand_TX_POWER;
typedef RakDeviceCommand_Integer<CMD_RX1_DELAY, Lora::MINIMUM_RX1_DELAY, Lora::MAXIMUM_RX1_DELAY, ERR_RX1_DELAY> RakDeviceCommand_RX1_DELAY;
typedef RakDeviceCommand_Integer<CMD_RX2_DELAY, Lora::MINIMUM_RX2_DELAY, Lora::MAXIMUM_RX2_DELAY, ERR_RX2_DELAY> RakDeviceCommand_RX2_DELAY;
typedef RakDeviceCommand_Integer<CMD_RX2_DR, Lora::MINIMUM_DATARATE, Lora::MAXIMUM_DATARATE, ERR_RX2_DR> RakDeviceCommand_RX2_DATARATE;
typedef RakDeviceCommand_Integer<CMD_JN1DL_DELAY, Lora::MINIMUM_JN1_DELAY, Lora::MAXIMUM_JN1_DELAY, ERR_JN1DL_DELAY> RakDeviceCommand_JN1DL_DELAY;
typedef RakDeviceCommand_Integer<CMD_JN2DL_DELAY, Lora::MINIMUM_JN2_DELAY, Lora::MAXIMUM_JN2_DELAY, ERR_JN2DL_DELAY> RakDeviceCommand_JN2DL_DELAY;
typedef RakDeviceCommand_Integer<CMD_RETY, Lora::MINIMUM_CONFIRMRETRY, Lora::MAXIMUM_CONFIRMRETRY, ERR_RETY> RakDeviceCOmmand_CONFIRM_RETRY;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <const char *CMD, int MIN_LENGTH, int MAX_LENGTH, const char *ERR_MSG = nullptr>
class RakDeviceCommand_String : public RakDeviceCommand_Queryable<String, CMD> {
    using Base = RakDeviceCommand_Queryable<String, CMD>;

protected:
    String requestValueBuild () const override {
        return "=" + Base::_value;
    }
    RakDeviceResult requestValidate () const override {
        return ! Base::_isQuery ? RakDeviceAttributeValidator::validateIsValueWithinMinMax (Base::_value.length (), CMD, "length", MIN_LENGTH, MAX_LENGTH, String (ERR_MSG ? ERR_MSG : "")) : true;
    }
    RakDeviceResult responseProcess (const String &value) override {
        Base::_value = value;
        return RakDeviceAttributeValidator::validateIsValueWithinMinMax (Base::_value.length (), CMD, "length", MIN_LENGTH, MAX_LENGTH, String (ERR_MSG ? ERR_MSG : ""));
    }

public:
    using Base::Base;
};

class RakDeviceCommand_CLASS : public RakDeviceCommand_String<CMD_CLASS, 1, 1, ERR_CLASS> {
    using Base = RakDeviceCommand_String<CMD_CLASS, 1, 1, ERR_CLASS>;

protected:
    Lora::Class classX;
    Lora::ClassB_Status classBStatus;
    RakDeviceResult responseSet (const String &response) override {
        const String command ("AT" + String (CMD_CLASS));
        if (_isQuery) {
            RakDeviceResult result = RakDeviceAttributeValidator::validateIsCommandWithEquals (response, command);
            if (! result.success)
                return result;
            _response = response.substring (command.length () + 1);
            if (_response.length () <= 0 || ! (_response [0] == static_cast<char> (Lora::Class::CLASS_A) || _response [0] == static_cast<char> (Lora::Class::CLASS_B) || _response [0] == static_cast<char> (Lora::Class::CLASS_C)))
                return RakDeviceResult (false, "unexpected class");
            classX = static_cast<Lora::Class> (_response [0]);
            if (classX == Lora::Class::CLASS_B && _response.length () == 4 && _response [1] == ':' && _response [2] == 'S')
                classBStatus = static_cast<Lora::ClassB_Status> (_response [3]);
            return true;
        }
        return RakDeviceCommand::responseSet (response);
    }

public:
    using Base::Base;
    Lora::Class getClass () const { return classX; }
    Lora::ClassB_Status getClassBStatus () const { return classBStatus; }
};

template <const char *CMD, size_t MIN_LENGTH, size_t MAX_LENGTH, const char *ERR_MSG = nullptr>
class RakDeviceCommand_HexString : public RakDeviceCommand_String<CMD, MIN_LENGTH, MAX_LENGTH, ERR_MSG> {
    using Base = RakDeviceCommand_String<CMD, MIN_LENGTH, MAX_LENGTH, ERR_MSG>;

protected:
    RakDeviceResult requestValidate () const override {
        if (! Base::_isQuery) {
            RakDeviceResult result = RakDeviceAttributeValidator::validateStringIsHexadecimal (Base::_value, CMD, String (ERR_MSG ? ERR_MSG : ""));
            if (! result.success)
                return result;
        }
        return Base::requestValidate ();
    }

public:
    using Base::Base;
};

typedef RakDeviceCommand_HexString<CMD_DEV_EUI, 16, 16> RakDeviceCommand_DEVEUI;
typedef RakDeviceCommand_HexString<CMD_APP_EUI, 16, 16> RakDeviceCommand_APPEUI;
typedef RakDeviceCommand_HexString<CMD_APP_KEY, 32, 32> RakDeviceCommand_APPKEY;
typedef RakDeviceCommand_HexString<CMD_DEV_ADDR, 8, 8> RakDeviceCommand_DEVADDR;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommand_TIMEREQUEST : public RakDeviceCommand_Boolean<CMD_TIMEREQUEST> {
    using Base = RakDeviceCommand_Boolean<CMD_TIMEREQUEST>;

protected:
    bool _succeeded = false;

public:
    using Base::Base;
    explicit RakDeviceCommand_TIMEREQUEST (const RakDeviceEvent &event) {
        if (event.type == "TIMEREQ_FAILED")
            _succeeded = false;
        else if (event.type == "TIMEREQ_OK")
            _succeeded = true;
    }
    bool succeeded () const { return _succeeded; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommand_LINKCHECK : public RakDeviceCommand_Integer<CMD_LINKCHECK, Lora::MINIMUM_LINKCHECK, Lora::MAXIMUM_LINKCHECK, ERR_LINKCHECK> {
    using Base = RakDeviceCommand_Integer<CMD_LINKCHECK, Lora::MINIMUM_LINKCHECK, Lora::MAXIMUM_LINKCHECK, ERR_LINKCHECK>;

public:
    struct Result {
        bool success;
        Lora::LinkStatus status;
    };

protected:
    Result _result;

public:
    bool isAsync () const override { return true; }
    explicit RakDeviceCommand_LINKCHECK (const Lora::LinkCheck value = Lora::LinkCheck::LINKCHECK_ONCE) :
        Base (static_cast<int> (value)) { }
    explicit RakDeviceCommand_LINKCHECK (const RakDeviceEvent &event) {
        static constexpr int valueCount = 5;
        int valueArray [valueCount] = { 0 }, valueIndex = 0;
        for (int offset = 0, next = 0; offset < event.args.length (); offset = next) {
            const int comma = event.args.indexOf (':', offset);
            next = comma >= 0 ? comma + 1 : event.args.length ();
            const int value = event.args.substring (offset, next).toInt ();
            if (valueIndex < valueCount)
                valueArray [valueIndex++] = value;
        }
        // Y0 represents the result of Link Check
        // 0 – represents the Link Check execute success (+EVT:LINKCHECK:0,0,1,-107,4)
        // Non-0 – represents the Link Check execute fail (+EVT:LINKCHECK:1,0,0,0,0)
        if (valueIndex == valueCount && valueArray [0] == 0) {
            _result.success = true;
            _result.status.DemodMargin = valueArray [1];
            _result.status.NbGateways = valueArray [2];
            _result.status.RSSI = valueArray [3];
            _result.status.SNR = valueArray [4];
        } else
            _result.success = false;
    }
    const Result &getResult () const { return _result; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommand_JOIN : public RakDeviceCommand {

public:
    enum class Command { JOIN,
                         LEAVE };

protected:
    Command _command;
    Lora::AutoJoin _autoJoin;
    int _reattemptDelay, _attempts;
    bool _joined = false;
    String _failure;
    String requestBuild () const override {
        return String (CMD_JOIN) + "=" + join (':', _command == Command::JOIN ? "1" : "0", _autoJoin == Lora::AutoJoin::AUTOJOIN ? "1" : "0", String (_reattemptDelay), String (_attempts));
    }
    RakDeviceResult requestValidate () const override {
        RakDeviceResult result;
        result = RakDeviceAttributeValidator::validateIsValueWithinMinMax (_reattemptDelay, CMD_JOIN, "reattempt-delay", Lora::MINIMUM_JOIN_ATTEMPTS_DELAY, Lora::MAXIMUM_JOIN_ATTEMPTS_DELAY, String ());
        if (! result.success)
            return result;
        result = RakDeviceAttributeValidator::validateIsValueWithinMinMax (_attempts, CMD_JOIN, "attempts", Lora::MINIMUM_JOIN_ATTEMPTS, Lora::MAXIMUM_JOIN_ATTEMPTS, String ());
        if (! result.success)
            return result;
        return true;
    }

public:
    bool isAsync () const override { return true; }
    explicit RakDeviceCommand_JOIN (const Command command = Command::JOIN, const Lora::AutoJoin autoJoin = Lora::DEFAULT_AUTOJOIN, const int reattemptDelay = Lora::DEFAULT_JOIN_ATTEMPTS_DELAY, const int attempts = Lora::DEFAULT_JOIN_ATTEMPTS) :
        _command (command),
        _autoJoin (autoJoin),
        _reattemptDelay (reattemptDelay),
        _attempts (attempts) { }
    explicit RakDeviceCommand_JOIN (const RakDeviceEvent &event) {
        if (event.type == "JOINED") {
            _joined = true;
        } else if (event.type.startsWith ("JOIN_FAILED_")) {
            _joined = false;
            _failure = event.type.substring (sizeof ("JOIN_FAILED_") - 1);
        }
    }
    bool isJoined () const { return _joined; }
    const String &failureString () const { return _failure; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommand_RECV : public RakDeviceCommand_Simple<CMD_RECV, true> {
    using Base = RakDeviceCommand_Simple<CMD_RECV, true>;

protected:
    Lora::Port _port;
    RakDeviceResult responseSet (const String &response) override {
        if (response.length () > 0 && response [0] == '0')    // no data
            return false;
        const int colon = response.indexOf (':');
        if (colon < 0)
            return RakDeviceResult (false, "could not find separator");
        _port = response.substring (0, colon).toInt ();
        _response = response.substring (colon + 1);
        return true;
    }

public:
    using Base::Base;
    bool isEmpty () const { return _port == 0; }
    Lora::Port getPort () const { return _port; }
    const String &getDataHexString () const { return _response; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RakDeviceCommand_SEND : public RakDeviceCommand_HexString<CMD_SEND, Lora::MINIMUM_SEND_SIZE, Lora::MAXIMUM_SEND_SIZE> {
    using Base = RakDeviceCommand_HexString<CMD_SEND, Lora::MINIMUM_SEND_SIZE, Lora::MAXIMUM_SEND_SIZE>;

protected:
    Lora::Port _port;
    bool _confirmed = false;
    String requestBuild () const override {
        return String (CMD_SEND) + "=" + join (':', String (_port), Base::_value);
    }
    RakDeviceResult requestValidate () const override {
        RakDeviceResult result = RakDeviceAttributeValidator::validateIsValueWithinMinMax (_port, CMD_SEND, "port", Lora::MINIMIM_SEND_PORT, Lora::MAXIMUM_SEND_PORT, String ());
        if (! result.success)
            return result;
        return Base::requestValidate ();
    }

public:
    bool isAsync () const override { return true; }
    explicit RakDeviceCommand_SEND (const Lora::Port port, const String &dataHexString) :
        Base (dataHexString),
        _port (port) { }
    explicit RakDeviceCommand_SEND (const RakDeviceEvent &event) {
        if (event.type == "SEND_CONFIRMED_OK")
            _confirmed = true;
        else if (event.type == "SEND_CONFIRMED_FAILED")
            _confirmed = false;
    }
    bool wasConfirmed () const { return _confirmed; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
