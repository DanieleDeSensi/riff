#ifndef KNARR_HPP_
#define KNARR_HPP_

#include "external/cppnanomsg/nn.hpp"

#include <iostream>
#include <string>

namespace knarr{

typedef enum MessageType{
    MESSAGE_TYPE_START = 0,
    MESSAGE_TYPE_SAMPLE_REQ,
    MESSAGE_TYPE_SAMPLE_RES,
    MESSAGE_TYPE_STOP
}MessageType;

/*!
 * \struct ApplicationSample
 * \brief Represents a sample of values taken from an application.
 *
 * This struct represents a sample of values taken from an adaptive node.
 */
typedef struct ApplicationSample{
    // The percentage ([0, 100]) of time that the node spent in the computation.
    double loadPercentage;

    // The number of computed tasks.
    double tasksCount;

    // The average latency (nanoseconds).
    double latency;

    // The bandwidth of the application.
    double bandwidthTotal;

    ApplicationSample():loadPercentage(0), tasksCount(0),
                   latency(0), bandwidthTotal(0){;}

    ApplicationSample& operator+=(const ApplicationSample& rhs){
        loadPercentage += rhs.loadPercentage;
        tasksCount += rhs.tasksCount;
        latency += rhs.latency;
        bandwidthTotal += rhs.bandwidthTotal;
        return *this;
    }
}ApplicationSample;

inline ApplicationSample operator+(ApplicationSample lhs, const ApplicationSample& rhs){
    lhs += rhs;
    return lhs;
}

inline std::ostream& operator<<(std::ostream& os, const ApplicationSample& obj){
    os << "[";
    os << "Load: " << obj.loadPercentage << " ";
    os << "TasksCount: " << obj.tasksCount << " ";
    os << "Latency: " << obj.latency << " ";
    os << "Bandwidth: " << obj.bandwidthTotal << " ";
    os << "]";
    return os;
}

typedef union Payload{
    ApplicationSample sample;
    ulong time;
    pid_t pid;

    Payload(){memset(this, 0, sizeof(Payload));}
}Payload;

typedef struct Message{
    MessageType type;
    Payload payload;
}Message;

class Application{
private:
    nn::socket* _channel;
    nn::socket& _channelRef;
    int _chid;
    bool _started;
    ulong _rcvStart;
    ulong _computeStart;
    ulong _idleTime;
    ulong _firstBegin;
    ulong _lastEnd;

    Message _currentMsg;
    Message _sentMsg;
    
    void notifyStart();

    ulong getCurrentTimeNs();
public:
    Application(const std::string& channelName);
    Application(nn::socket& socket, uint chid);
    ~Application();

    Application(const Application& a) = delete;
    void operator=(Application const &x) = delete;
    
    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop begins.
     */
    void begin();

    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop ends.
     */
    void end();

    /**
     * This function must only be called once, when the parallel part
     * of the application terminates.
     */
    void terminate();
};

class Monitor{
private:
    nn::socket* _channel;
    nn::socket& _channelRef;
    int _chid;
    ulong _executionTime;
public:
    Monitor(const std::string& channelName);
    Monitor(nn::socket& socket, uint chid);
    ~Monitor();

    Monitor(const Monitor& m) = delete;
    void operator=(Monitor const &x) = delete;
    
    pid_t waitStart();

    bool getSample(ApplicationSample& sample);

    /**
     * Returns the execution time of the application (milliseconds).
     * @return The execution time of the application (milliseconds).
     * The time is from the first call of begin() to the last call of end().
     */
    ulong getExecutionTime();
};

} // End namespace

#endif
