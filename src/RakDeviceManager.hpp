
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

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

class RakDeviceManager {
public:
    static inline constexpr uint32_t TRANSMIT_AWAIT_CONFIRMATION_DELAY = 100;

    struct ConfigLoraOperation {
        Lora::Mode mode = Lora::Mode::MODE_LORAWAN;
        Lora::Band band = Lora::Band::BAND_EU868;
        Lora::Class clazz = Lora::Class::CLASS_A;
        Lora::Join join = Lora::Join::JOIN_OTAA;
    };
    struct ConfigLoraIdentifiers {
        String devEUI, appEUI, appKey;
    };
    struct ConfigLoraParameters {  
        Lora::AutoJoin autoJoin { Lora::AutoJoin::NO_AUTOJOIN };
        bool confirmMode { true };          // Use confirmed messages
        bool dutyCycle { true };            // Enable duty cycle
        bool adaptiveDataRate { true };     // Enable ADR
        bool publicNetworkMode { true };    // Enable public network mode
        Lora::Datarate dataRate { Lora::Datarate::SF12 };
        Lora::TxPower txPower { Lora::TxPower::HIGHEST };
        int rx1Delay { 5 };    // RX1 window delay (seconds)
        int rx2Delay { 6 };    // RX2 window delay (seconds)
        Lora::Datarate rx2DataRate { Lora::Datarate::SF12 };
        int joinAttemptsDelay { 10 };
        int joinAttemptsNumber { 8 };
    };
    struct Config {
        ConfigLoraOperation loraOperation;
        ConfigLoraIdentifiers loraIdentifiers;
        ConfigLoraParameters loraParameters;

        interval_t rejoinInterval { 4 * 60 * 1000 };
        interval_t statusInterval { 1 * 60 * 1000 };
        interval_t linkCheckInterval { 2 * 60 * 1000 };
        interval_t networkTimeInterval { 30 * 60 * 1000 };
    };

    struct Status {
        using Channels = RakDeviceCommand_RSSI_ALL::ChannelsRSSI;
        static String toString (const Channels &status) {
            String result;
            for (const auto &[channel, RSSI] : status)
                result += (result.isEmpty () ? "" : ",") + String (channel) + ":" + String (RSSI);
            return result;
        }

        String version, hardware, serialno, apiversion, hardwareid;

        String devAddr;

        TrackableValue<String> networkTime;
        TrackableValue<bool> transmitConfirmation;
        TrackableValue<Lora::ReceiveStatus> receiveStatus;
        TrackableValue<Lora::LinkStatus> linkStatus;
        TrackableValue<Channels> channelStatus;
    };

private:
    const Config _config;
    RakDeviceTransceiver _transceiver;
    RakDeviceCommander _commander;
    Status _status;

    enum class State {
        UNINITIALISED = 0,
        INITIALISED = 1,
        SUSPENDED = 2,
        JOIN_PENDING = 3,
        JOIN_SUCCESS = 4,
        JOIN_FAILURE = 5
    } _state { State::UNINITIALISED },
        _stateSuspended;

    Intervalable _networkRestriction;

    static String toString (const State state) {
        if (state == State::UNINITIALISED)
            return "UNINITIALISED";
        else if (state == State::INITIALISED)
            return "INITIALISED";
        else if (state == State::SUSPENDED)
            return "SUSPENDED";
        else if (state == State::JOIN_PENDING)
            return "JOIN_PENDING";
        else if (state == State::JOIN_SUCCESS)
            return "JOIN_SUCCESS";
        else if (state == State::JOIN_FAILURE)
            return "JOIN_FAILURE";
        else
            return "UNKNOWN";
    }

    Intervalable _intervalRejoin;
    Intervalable _intervalStatus, _intervalLinkCheck, _intervalNetworkTime;

    ActivationTracker _transmitCounter, _receiveCounter;
    ActivationTracker _transmitSuccesses, _transmitFailures;

public:
    RakDeviceManager (const Config &config, Stream &stream) :
        _config (config),
        _transceiver (stream),
        _commander (_transceiver, [this] (const RakDeviceEvent &event) { events (event); }),
        _intervalRejoin (config.rejoinInterval),
        _intervalStatus (config.statusInterval),
        _intervalLinkCheck (config.linkCheckInterval),
        _intervalNetworkTime (config.networkTimeInterval) { }
    ~RakDeviceManager () {
        teardown ();
    }

    //

    bool setup () {
        if (_state != State::UNINITIALISED)
            return false;

        RakDeviceCommand_VERSION commandVersion;
        RakDeviceCommand_HWMODEL commandHardware;
        RakDeviceCommand_HWID commandHardwareId;
        RakDeviceCommand_SERIALNO commandSerialNo;
        RakDeviceCommand_APIVERSION commandApiVersion;
        if (! _commander.issue (commandVersion).success || ! _commander.issue (commandHardware).success || ! _commander.issue (commandHardwareId).success || ! _commander.issue (commandSerialNo).success || ! _commander.issue (commandApiVersion).success)
            return false;
        _status.version = commandVersion.responseGet ();
        _status.hardware = commandHardware.responseGet ();
        _status.hardwareid = commandHardwareId.responseGet ();
        _status.serialno = commandSerialNo.responseGet ();
        _status.apiversion = commandApiVersion.responseGet ();
        DEBUG_PRINTF ("RakDeviceManager::setup: Version=%s, Hardware=%s, HardwareId=%s, Serialno=%s, APIversion=%s\n", _status.version.c_str (), _status.hardware.c_str (), _status.hardwareid.c_str (), _status.serialno.c_str (), _status.apiversion.c_str ());

        RakDeviceCommand_NWM commandModeNetworkSet (static_cast<int> (_config.loraOperation.mode));
        if (! _commander.issue (commandModeNetworkSet).success)
            return false;
        RakDeviceCommand_NJM commandModeJoinSet (static_cast<int> (_config.loraOperation.join));
        (void) _commander.issue (commandModeJoinSet);
        delay (250);                 // wait 250ms for mode switch
        _transceiver.send ("\n");    // soak up banner "RAKwireless RAK3272-SiP Example------------------------------------------------------"
        if (! _commander.issue (commandModeJoinSet).success)
            return false;
        RakDeviceCommand_CLASS commandClassSet (String ((char) _config.loraOperation.clazz));
        if (! _commander.issue (commandClassSet).success)
            return false;
        RakDeviceCommand_BAND commandBandSet (static_cast<int> (_config.loraOperation.band));
        if (! _commander.issue (commandBandSet).success)
            return false;
        DEBUG_PRINTF ("RakDeviceManager::setup: Mode=%s, Join=%s, Class=%s, Band=%s\n", Lora::toString (_config.loraOperation.mode).c_str (), Lora::toString (_config.loraOperation.join).c_str (), Lora::toString (_config.loraOperation.clazz).c_str (), Lora::toString (_config.loraOperation.band).c_str ());

        RakDeviceCommand_DEVEUI commandDevEuiSet (_config.loraIdentifiers.devEUI);
        RakDeviceCommand_APPEUI commandAppEuiSet (_config.loraIdentifiers.appEUI);
        RakDeviceCommand_APPKEY commandAppKeySet (_config.loraIdentifiers.appKey);
        if (! _commander.issue (commandDevEuiSet).success || ! _commander.issue (commandAppEuiSet).success || ! _commander.issue (commandAppKeySet).success)
            return false;
        DEBUG_PRINTF ("RakDeviceManager::setup: DevEUI=%s, AppEUI=%s, AppKey=%s\n", _config.loraIdentifiers.devEUI.c_str (), _config.loraIdentifiers.appEUI.c_str (), _config.loraIdentifiers.appKey.c_str ());

        RakDeviceCommand_CONFIRM_MODE commandConfirmModeSet (_config.loraParameters.confirmMode);
        RakDeviceCommand_DUTY_CYCLE commandDutyCycleSet (_config.loraParameters.dutyCycle);
        RakDeviceCommand_DATARATE commandDataRateSet (static_cast<int> (_config.loraParameters.dataRate));
        RakDeviceCommand_TX_POWER commandTxPowerSet (static_cast<int> (_config.loraParameters.txPower));
        if (! _commander.issue (commandConfirmModeSet).success || ! _commander.issue (commandDutyCycleSet).success || ! _commander.issue (commandDataRateSet).success || ! _commander.issue (commandTxPowerSet).success)
            return false;

        RakDeviceCommand_ADR commandAdrSet (_config.loraParameters.adaptiveDataRate);
        RakDeviceCommand_PNM commandPnmSet (_config.loraParameters.publicNetworkMode);
        RakDeviceCommand_RX1_DELAY commandRx1DelaySet (_config.loraParameters.rx1Delay);
        RakDeviceCommand_RX2_DELAY commandRx2DelaySet (_config.loraParameters.rx2Delay);
        RakDeviceCommand_RX2_DATARATE commandRx2DataRateSet (static_cast<int> (_config.loraParameters.rx2DataRate));
        if (! _commander.issue (commandAdrSet).success || ! _commander.issue (commandPnmSet).success || ! _commander.issue (commandRx1DelaySet).success || ! _commander.issue (commandRx2DelaySet).success || ! _commander.issue (commandRx2DataRateSet).success)
            return false;

        _state = State::INITIALISED;
        joinCommence ();

        return true;
    }

    void teardown () {
        if (_state == State::UNINITIALISED)
            return;

        suspend ();
        _state = State::UNINITIALISED;
    }

    void process () {
        DEBUG_PRINTF ("RakDeviceManager::STATE: %s\n", toString (_state).c_str ());

        if (! (_state == State::JOIN_PENDING || _state == State::JOIN_FAILURE || _state == State::JOIN_SUCCESS))
            return;

        _commander.process ();

        if (_networkRestriction.active ())
            updateNetworkRestriction ();

        if (_state == State::JOIN_PENDING)
            updateJoinStatus ();
        else if (_state == State::JOIN_SUCCESS) {
            if (_intervalStatus)
                updateStatus (), updateNetworkTime ();
            if (_intervalLinkCheck)
                updateLinkStatus ();
        }
    }

    bool suspend () {
        if (_state == State::SUSPENDED)
            return false;

        RakDeviceCommand_SLEEP commandSleep;
        if (! _commander.issue (commandSleep).success)
            return false;
        _stateSuspended = _state;
        _state = State::SUSPENDED;
        return true;
    }

    bool resume () {
        if (_state != State::SUSPENDED)
            return false;

        _transceiver.poke ();
        delay (100);
        _state = _stateSuspended;
        return updateStatus ();
    }

    //

    inline bool transmit (Lora::Port port, const String &data, const bool awaitConfirmation = false) {
        if (_state != State::JOIN_SUCCESS)
            return false;
        return processTransmit (port, bytesToHexString (reinterpret_cast<const uint8_t *> (data.c_str ()), data.length ()), awaitConfirmation);
    }
    inline bool transmit (Lora::Port port, const uint8_t *data, const size_t length, const bool awaitConfirmation = false) {
        if (_state != State::JOIN_SUCCESS)
            return false;
        return processTransmit (port, bytesToHexString (data, length), awaitConfirmation);
    }

    const Status &status () const { return _status; }
    bool isAvailable () const { return _state == State::JOIN_SUCCESS; }

private:
    //

    void joinCommence () {
        DEBUG_PRINTF ("RakDeviceManager::JOIN-COMMENCE\n");
        RakDeviceCommand_JOIN commandJoin (RakDeviceCommand_JOIN::Command::JOIN, _config.loraParameters.autoJoin, _config.loraParameters.joinAttemptsDelay, _config.loraParameters.joinAttemptsNumber);
        if (_commander.issue (commandJoin).success)
            joinPending ();
        else
            joinFailure ("unable to issue JOIN request");
    }
    void joinPending () {
        DEBUG_PRINTF ("RakDeviceManager::JOIN-PENDING\n");
        _intervalRejoin.reset ();
        _state = State::JOIN_PENDING;
    }
    void joinSuccess () {
        RakDeviceCommand_DEVADDR commandDevAddr;
        if (_commander.issue (commandDevAddr).success)
            _status.devAddr = commandDevAddr.getValue ();
        DEBUG_PRINTF ("RakDeviceManager::JOIN-SUCCESS: DevAddr=%s\n", _status.devAddr.c_str ());
        updateStatus ();
        _state = State::JOIN_SUCCESS;
    }
    void joinFailure (const String &reason = String ()) {
        DEBUG_PRINTF ("RakDeviceManager::JOIN-FAILURE%s%s\n", (reason.isEmpty () ? "" : ": "), reason.c_str ());
        _state = State::JOIN_FAILURE;
    }
    void updateJoinStatus () {
        if (_intervalRejoin) {
            RakDeviceCommand_JOIN_STATUS commandJoinStatus;
            if (_commander.issue (commandJoinStatus).success) {
                if (commandJoinStatus.isJoined ())
                    joinSuccess ();
                else
                    joinCommence ();
            }
        }
    }
    void updateJoinStatus (const RakDeviceCommand_JOIN &commandJoin) {
        if (commandJoin.isJoined ())
            joinSuccess ();
        else
            joinFailure (commandJoin.failureString ());
    }

    //

    bool processTransmit (Lora::Port port, const String &data, const bool awaitConfirmation = false) {
        DEBUG_PRINTF ("RakDeviceManager::TRANSMIT-DATA: port=%d, %s\n", port, debugHexString (data).c_str ());
        RakDeviceCommand_SEND commandSend (port, data);
        if (! _commander.issue (commandSend).success)
            return false;
        _transmitCounter++;
        if (awaitConfirmation) {
            delay (TRANSMIT_AWAIT_CONFIRMATION_DELAY);
            RakDeviceCommand_SEND_STATUS commandSendStatus;
            if (_commander.issue (commandSendStatus).success) {
                const bool wasSuccessful = commandSendStatus.wasSuccessful ();
                _status.transmitConfirmation = wasSuccessful;
                if (wasSuccessful)
                    _transmitSuccesses++;
                else
                    _transmitFailures++;
                return true;
            } else {
                _status.transmitConfirmation.invalidate ();
                return false;
            }
        }
        return true;
    }
    void updateTransmitStatus (const RakDeviceCommand_SEND &commandSend) {
        const bool wasConfirmed = commandSend.wasConfirmed ();
        DEBUG_PRINTF ("RakDeviceManager::TRANSMIT-CONF: %s\n", wasConfirmed ? "true" : "false");
        _status.transmitConfirmation = wasConfirmed;
        if (wasConfirmed)
            _transmitSuccesses++;
        else
            _transmitFailures++;
    }

    //

    void processReceive (const Lora::Port port, const String &data) {
        DEBUG_PRINTF ("RakDeviceManager::RECEIVE-DATA: port=%d, %s\n", port, debugHexString (data).c_str ());
        _receiveCounter++;
        // XXX callback
    }
    void updateReceive (const Lora::Class class_, const String &details) {
        // <-RX- <<+EVT:RX_1:-107:-7:UNICAST:15:beef>>details
        int offset = 0, colon = details.indexOf (':');
        const Lora::RSSI rssi = details.substring (offset, colon).toInt ();
        colon = details.indexOf (':', offset = colon + 1);
        const Lora::SNR snr = details.substring (offset, colon).toInt ();
        colon = details.indexOf (':', offset = colon + 1);
        const String mode = details.substring (offset, colon);
        colon = details.indexOf (':', offset = colon + 1);
        const Lora::Port port = details.substring (offset, colon).toInt ();
        offset = colon + 1;
        const String data = details.substring (offset);
        updateStatusReceive ({ .RSSI = rssi, .SNR = snr });
        processReceive (port, data);
    }
    // void updateReceive () {
    //     String data;
    //     Lora::Port port;
    //     while (receiveRequest (port, data))
    //         processRecv (port, data);
    // }
    // bool receiveRequest (Lora::Port &port, String &data) {
    //     if (_state != State::JOIN_SUCCESS)
    //         return false;

    //     RakDeviceCommand_RECV commandRecv;
    //     if (! _commander.issue (commandRecv).success || commandRecv.isEmpty ())
    //         return false;
    //     port = commandRecv.getPort ();
    //     data = commandRecv.getDataHexString ();
    //     return true;
    // }

    //

    void updateNetworkRestriction () {
        if (_networkRestriction) {
            DEBUG_PRINTF ("RakDeviceManager::RESTRICTION: completed\n");
            _networkRestriction.reset (0);
        } else {
            DEBUG_PRINTF ("RakDeviceManager::RESTRICTION: continues, for another %f mins\n", ((float) (_networkRestriction.remaining ())) / 1000.0f / 60.0f);
        }
    }
    void updateNetworkRestriction (const interval_t milliseconds) {
        DEBUG_PRINTF ("RakDeviceManager::RESTRICTION: notified, for another %f mins\n", ((float) milliseconds) / 1000.0f / 60.0f);
        _networkRestriction.reset (milliseconds);
    }

    //

    void updateWorkMode (const Lora::Mode mode) {
        DEBUG_PRINTF ("RakDeviceManager::WORK-MODE: %s\n", Lora::toString (mode));
    }

    //

    bool updateNetworkTime () {
        if (! _status.networkTime.lastResult () || _intervalNetworkTime) {
            RakDeviceCommand_TIMEREQUEST commandTimeRequest (true);
            if (! _commander.issue (commandTimeRequest).success)
                return false;
        }
        return true;
    }
    void updateNetworkTime (const RakDeviceCommand_TIMEREQUEST &commandTimeRequest) {
        if (commandTimeRequest.succeeded ()) {
            RakDeviceCommand_LTIME commandTimeRetrieve;
            if (_commander.issue (commandTimeRetrieve).success) {
                _status.networkTime = commandTimeRetrieve.responseGet ();
                _intervalNetworkTime.reset ();
                DEBUG_PRINTF ("RakDeviceManager::NETWORK-TIME: %s\n", _status.networkTime.get ().c_str ());
            } else
                _status.networkTime.invalidate ();
        }
    }
    bool updateLinkStatus () {
        RakDeviceCommand_LINKCHECK commandLinkCheck (Lora::LinkCheck::LINKCHECK_ONCE);
        if (! _commander.issue (commandLinkCheck).success)
            return false;
        return commandLinkCheck.isAsync ();
    }
    void updateLinkStatus (const RakDeviceCommand_LINKCHECK &command) {
        const auto &result = command.getResult ();
        if (result.success)
            updateStatusLink (result.status);
    }
    void updateSignalQuality () {
        RakDeviceCommand_RSSI_LAST commandRSSI;
        RakDeviceCommand_SNR_LAST commandSNR;
        if (_commander.issue (commandRSSI).success && _commander.issue (commandSNR).success)
            updateStatusReceive ({ .RSSI = commandRSSI.RSSI (), .SNR = commandSNR.SNR () });
    }
    void updateChannelHealth () {
        RakDeviceCommand_RSSI_ALL commandRSSI;
        if (_commander.issue (commandRSSI).success)
            updateStatusChannel (commandRSSI.getChannelsRSSI ());
    }

    //

    bool updateStatus () {
        updateSignalQuality ();
        updateChannelHealth ();
        return true;
    }

    void updateStatusReceive (const Lora::ReceiveStatus &status) {
        _status.receiveStatus = status;
        DEBUG_PRINTF ("RakDeviceManager::STATUS-RECEIVE: %s\n", Lora::toString (status).c_str ());
    }
    void updateStatusLink (const Lora::LinkStatus &status) {
        _status.linkStatus = status;
        DEBUG_PRINTF ("RakDeviceManager::STATUS-LINK: %s\n", Lora::toString (status).c_str ());
    }
    void updateStatusChannel (const Status::Channels &status) {
        _status.channelStatus = status;
        DEBUG_PRINTF ("RakDeviceManager::STATUS-CHANNEL: %s\n", Status::toString (status).c_str ());
    }

    //

    void events (const RakDeviceEvent &event) {
        if (event.type.startsWith ("JOIN"))
            updateJoinStatus (RakDeviceCommand_JOIN (event));    // +EVT:JOINED, +EVT:JOIN_FAILED_TX_TIMEOUT, +EVT:JOIN_FAILED_RX_TIMEOUT, +EVT:JOIN_FAILED_errorcode
        else if (event.type.startsWith ("SEND"))
            updateTransmitStatus (RakDeviceCommand_SEND (event));    // +EVT:SEND_CONFIRMED_OK, +EVT:SEND_CONFIRMED_FAILED
        else if (event.type.startsWith ("LINKCHECK"))
            updateLinkStatus (RakDeviceCommand_LINKCHECK (event));    // +EVT:LINKCHECK:Y0,Y1,Y2,Y3,Y4
        else if (event.type.startsWith ("TIMEREQ"))
            updateNetworkTime (RakDeviceCommand_TIMEREQUEST (event));    // +EVT:TIMEREQ_FAILED, +EVT:TIMEREQ_OK
        else if (event.type == "BC")
            ;    // updateBeaconStatus (event.args);    // +BC: ... LOCKED/DONE/FAILED//ONGOING/LOST/FAILED_errorcode
        else if (event.type == "PS")
            ;    // updatePingSlotStatus (event.args);    // +PS: ... DONE
        else if (event.type == "RX_1" || event.type == "RX_2")
            updateReceive (Lora::Class::CLASS_A, event.args);    // +EVT:RX_1:-70:8:UNICAST:1:1234
        else if (event.type == "RX_B")
            updateReceive (event.args.indexOf ("UNICAST") > 0 ? Lora::Class::CLASS_B : Lora::Class::CLASS_C, event.args);    // +EVT:RX_B:-47:3:UNICAST:2:4321
        else if (event.type == "RestrictedWait")
            updateNetworkRestriction (std::atol (event.args.c_str ()));    // Restricted_Wait_3343902_ms
        else if (event.type == "CurrentWorkMode")
            updateWorkMode ((event.args == "LoRaWAN" ? Lora::Mode::MODE_LORAWAN : (event.args == "P2PLoRa" ? Lora::Mode::MODE_P2PLORA : (event.args == "P2PFSK" ? Lora::Mode::MODE_P2PFSK : Lora::Mode::MODE_UNDEFINED))));    // Current Work Mode: LoRaWAN.
        else
            DEBUG_PRINTF ("RakDeviceManager::EVENT: UNHANDLED, type=(%s), args=(%s)\n", event.type.c_str (), event.args.c_str ());
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
