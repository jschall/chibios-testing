#include <cstdlib>
#include <unistd.h>
#include <uavcan/uavcan.hpp>
#include <uavcan_stm32/uavcan_stm32.hpp>

extern "C" {
#include <uavcan.h>
}
/*
 * We're going to use messages of type uavcan.protocol.debug.KeyValue, so the appropriate header must be included.
 * Given a data type named X, the header file name would be:
 *      X.replace('.', '/') + ".hpp"
 */
#include <uavcan/protocol/debug/KeyValue.hpp> // uavcan.protocol.debug.KeyValue

uavcan_stm32::CanInitHelper<> can;

constexpr unsigned NodeMemoryPoolSize = 16384;
typedef uavcan::Node<NodeMemoryPoolSize> Node;

class Uavcanwrapper {

public:
    Uavcanwrapper(uavcan::ICanDriver& canDriver, uavcan::ISystemClock& sysClock) :
    node(canDriver, sysClock),
    kv_pub(node) { }

    bool uavcanInit(uint8_t self_node_id)
    {
        node.setNodeID(self_node_id);
        node.setName("chtesting");

        /*
         * Dependent objects (e.g. publishers, subscribers, servers, callers, timers, ...) can be initialized only
         * if the node is running. Note that all dependent objects always keep a reference to the node object.
         */
        const int node_start_res = node.start();
        if (node_start_res < 0)
        {
            return false;
        }

        /*
         * Create the publisher object to broadcast standard key-value messages of type uavcan.protocol.debug.KeyValue.
         * Keep in mind that most classes defined in the library are not copyable; attempt to copy objects of
         * such classes will result in compilation failure.
         * A publishing node won't see its own messages.
         */
        const int kv_pub_init_res = kv_pub.init();
        if (kv_pub_init_res < 0)
        {
        	return false;    
        }

        /*
         * This would fail because most of the objects - including publishers - are noncopyable.
         * The error message may look like this:
         *  "error: ‘uavcan::Noncopyable::Noncopyable(const uavcan::Noncopyable&)’ is private"
         */
        // auto pub_copy = kv_pub;  // Don't try this at home.

        /*
         * TX timeout can be overridden if needed.
         * Default value should be OK for most use cases.
         */
        kv_pub.setTxTimeout(uavcan::MonotonicDuration::fromMSec(1000));

        /*
         * Priority of outgoing tranfers can be changed as follows.
         * Default priority is 16 (medium).
         */
        kv_pub.setPriority(uavcan::TransferPriority::MiddleLower);

        /*
         * Running the node.
         */
        node.setModeOperational();
        return true;
    }

    void uavcanUpdate(void)
    {
        auto& node = getNode();
        /*
         * Spinning for 1 second.
         * The method spin() may return earlier if an error occurs (e.g. driver failure).
         * All error codes are listed in the header uavcan/error.hpp.
         */
        const int spin_res = node.spin(uavcan::MonotonicDuration::fromUSec(1000));
        if (spin_res < 0)
        {
            return;
        }

        /*
         * Publishing a random value using the publisher created above.
         * All message types have zero-initializing default constructors.
         * Relevant usage info for every data type is provided in its DSDL definition.
         */
        uavcan::protocol::debug::KeyValue kv_msg;  // Always zero initialized
        kv_msg.value = std::rand() / float(RAND_MAX);

        /*
         * Arrays in DSDL types are quite extensive in the sense that they can be static,
         * or dynamic (no heap needed - all memory is pre-allocated), or they can emulate std::string.
         * The last one is called string-like arrays.
         * ASCII strings can be directly assigned or appended to string-like arrays.
         * For more info, please read the documentation for the class uavcan::Array<>.
         */
        kv_msg.key = "a";   // "a"
        kv_msg.key += "b";  // "ab"
        kv_msg.key += "c";  // "abc"

        /*
         * Publishing the message.
         */
        const int pub_res = kv_pub.broadcast(kv_msg);
        if (pub_res < 0)
        {
            return;
        }

    }

    Node& getNode()
    {
        return node;
    }
private:
    Node node;
    uavcan::Publisher<uavcan::protocol::debug::KeyValue> kv_pub;
};

static Uavcanwrapper *uc_wrapper;


extern "C" __EXPORT bool uavcanInit(uint8_t node_id)
{
    if (uc_wrapper == nullptr) {
        uc_wrapper = new Uavcanwrapper(can.driver, uavcan_stm32::SystemClock::instance());
        if (uc_wrapper == nullptr) {
            return false;
        }
    }
    return uc_wrapper->uavcanInit(node_id);
}

extern "C" __EXPORT void uavcanUpdate(void)
{
    if (uc_wrapper == nullptr) {
        return;
    }
    uc_wrapper->uavcanUpdate();
}