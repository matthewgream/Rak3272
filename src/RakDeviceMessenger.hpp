
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <mutex>
#include <queue>

class RakDeviceMessenger {
public:
    static constexpr interval_t RETRY_DELAY = 30 * 1000;    // 10 seconds in milliseconds

    struct Message {
        Lora::Port port;
        String data;
        bool confirmed;
        interval_t timestamp;
        Message (Lora::Port p = Lora::Port (0), const String &d = String (), bool c = false, interval_t t = 0) :
            port (p),
            data (d),
            confirmed (c),
            timestamp (t) { }
    };

private:
    RakDeviceManager &_device;
    RakDeviceManager::EventHandlerId _handlerId;
    mutable std::mutex _receiveMutex;
    std::queue<Message> _receiveQueue;
    mutable std::mutex _transmitMutex;
    std::queue<Message> _transmitQueue;
    bool _transmitPending = false;

    struct Stats {
        size_t transmitsAttempted = 0;
        size_t transmitsSucceeded = 0;
        size_t transmitsFailed = 0;
        size_t retransmitsAttempted = 0;
    } _stats;

    void onDeviceEvent (const RakDeviceManager::Event event, const RakDeviceManager::EventArgs &args) {

        if (event == RakDeviceManager::Event::DATA_RECEIVED) {
            std::lock_guard<std::mutex> guard (_receiveMutex);
            _receiveQueue.push (Message (args [0].toInt (), args [1], false, millis ()));

        } else if (event == RakDeviceManager::Event::TRANSMIT_SUCCESS) {
            std::lock_guard<std::mutex> guard (_transmitMutex);
            _transmitPending = false;
            _stats.transmitsSucceeded++;
            _transmitQueue.pop ();
            RAKDEVICE_DEBUG_PRINTF ("Messenger: Transmit success (successes=%u, failures=%u, retries=%u)\n", _stats.transmitsSucceeded, _stats.transmitsFailed, _stats.retransmitsAttempted);

        } else if (event == RakDeviceManager::Event::TRANSMIT_FAILURE) {
            std::lock_guard<std::mutex> guard (_transmitMutex);
            _transmitPending = false;
            _stats.transmitsFailed++;
            if (! _transmitQueue.empty ()) {
                Message &message = _transmitQueue.front ();
                message.timestamp = millis () + RETRY_DELAY;
                RAKDEVICE_DEBUG_PRINTF ("Messenger: Transmit failure, retry in %u ms (successes=%u, failures=%u, retries=%u)\n", RETRY_DELAY, _stats.transmitsSucceeded, _stats.transmitsFailed, _stats.retransmitsAttempted);
            }
        }
    }

    void doProcess () {
        if (! _transmitPending && ! _transmitQueue.empty ()) {
            const Message &message = _transmitQueue.front ();
            if (millis () >= message.timestamp) {
                RAKDEVICE_DEBUG_PRINTF ("Messenger: Transmit actuate (attempt=%u) -- port=%d, data=%s\n", _stats.transmitsAttempted, message.port, message.data.c_str ());
                if (_device.transmit (message.port, message.data)) {
                    _transmitPending = true;
                    _stats.retransmitsAttempted++;
                    _stats.transmitsAttempted++;
                }
            }
        }
    }

public:
    explicit RakDeviceMessenger (RakDeviceManager &device) :
        _device (device) {
        _handlerId = _device.addEventListener ([this] (const RakDeviceManager::Event event, const RakDeviceManager::EventArgs &args) {
            this->onDeviceEvent (event, args);
        });
    }

    ~RakDeviceMessenger () {
        _device.removeEventListener (_handlerId);
    }

    bool transmit (const Message &message) {
        std::lock_guard<std::mutex> guard (_transmitMutex);
        RAKDEVICE_DEBUG_PRINTF ("Messenger: Transmit enqueue (queue_size=%d) -- port=%d, data=%s\n", _transmitQueue.size () + 1, message.port, message.data.c_str ());
        _transmitQueue.push (message);
        doProcess ();
        return true;
    }

    bool receive (Message &message) {
        std::lock_guard<std::mutex> guard (_receiveMutex);
        if (_receiveQueue.empty ())
            return false;
        message = _receiveQueue.front ();
        _receiveQueue.pop ();
        return true;
    }

    size_t transmit_queue_size () const {
        std::lock_guard<std::mutex> guard (_transmitMutex);
        return _transmitQueue.size ();
    }
    size_t receive_queue_size () const {
        std::lock_guard<std::mutex> guard (_receiveMutex);
        return _receiveQueue.size ();
    }
    void process () {
        RAKDEVICE_DEBUG_PRINTF ("RakDeviceMessenger: tx_queue=%d, rx_queue=%d\n", transmit_queue_size (), receive_queue_size ());
        std::lock_guard<std::mutex> guard (_transmitMutex);
        doProcess ();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
