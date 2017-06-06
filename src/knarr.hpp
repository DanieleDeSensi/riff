#ifndef KNARR_HPP_
#define KNARR_HPP_

#include "external/cppnanomsg/nn.hpp"

#include <pthread.h>
#include <iostream>
#include <string>
#include <vector>

#define KNARR_MAX_CUSTOM_FIELDS 8

namespace knarr{

typedef enum MessageType{
    MESSAGE_TYPE_START = 0,
    MESSAGE_TYPE_SAMPLE_REQ,
    MESSAGE_TYPE_SAMPLE_RES,
    MESSAGE_TYPE_STOP,
    MESSAGE_TYPE_STOPACK
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

    // Custom user fields.
    double customFields[KNARR_MAX_CUSTOM_FIELDS];

    ApplicationSample():loadPercentage(0), tasksCount(0),
                   latency(0), bandwidthTotal(0){
        // We do not use memset due to cppcheck warnings.
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] = 0;
        }
    }
}ApplicationSample;


inline std::ostream& operator<<(std::ostream& os, const ApplicationSample& obj){
    os << "[";
    os << "Load: " << obj.loadPercentage << " ";
    os << "TasksCount: " << obj.tasksCount << " ";
    os << "Latency: " << obj.latency << " ";
    os << "Bandwidth: " << obj.bandwidthTotal << " ";
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        os << "CustomField" << i << ": " << obj.customFields[i] << " ";
    }
    os << "]";
    return os;
}

typedef union Payload{
    ApplicationSample sample;
    ulong time;
    pid_t pid;

    Payload(){;}
}Payload;

typedef struct Message{
    MessageType type;
    Payload payload;
}Message;


class Aggregator{
public:
    virtual ~Aggregator(){;}

    /**
     * When using custom values, if storeCustomValue is called by multiple
     * threads, this function implements the aggregation between values stored
     * by different threads.
     * This function is called by at most one thread.
     * @param index The index of the custom value.
     * @param customValues The values stored by the threads.
     */
    virtual double aggregate(size_t index, const std::vector<double>& customValues) = 0;
};

typedef struct ThreadData{
    ApplicationSample sample;
    ulong rcvStart;
    ulong computeStart;
    ulong idleTime;
    ulong firstBegin;
    ulong lastEnd;
    bool clean;

    ThreadData():rcvStart(0), computeStart(0), idleTime(0), firstBegin(0),
                 lastEnd(0), clean(false){;}

    void reset(){
        sample = ApplicationSample();
        rcvStart = 0;
        idleTime = 0;
    }
}ThreadData;

class Application{
private:
    nn::socket* _channel;
    nn::socket& _channelRef;
    int _chid;
    bool _started;
    Aggregator* _aggregator;
    pthread_mutex_t _mutex;

    std::vector<ThreadData> _threadData;

    // We are sure it is called by at most one thread.
    void notifyStart();

    ulong getCurrentTimeNs();
public:
    /**
     * Constructs this object.
     * @param channelName The name of the channel.
     * @param numThreads The number of threads which will concurrently use
     *        this library.
     * @param aggregator An aggregator object to aggregate custom values
     *        stored by multiple threads.
     */
    Application(const std::string& channelName, size_t numThreads = 1,
                Aggregator* aggregator = NULL);

    /**
     * Constructs this object.
     * @param socket The nanomsg socket.
     * @param chid The channel identifier.
     * @param numThreads The number of threads which will concurrently use
     *        this library.
     * @param aggregator An aggregator object to aggregate custom values
     *        stored by multiple threads.
     */
    Application(nn::socket& socket, uint chid, size_t numThreads = 1,
                Aggregator* aggregator = NULL);
    ~Application();

    Application(const Application& a) = delete;
    void operator=(Application const &x) = delete;
    
    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop begins.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). The value is the thread id, in the
     *        range [0, n[, where n is the number of threads calling this
     *        function. n must also have been specified when constructing this
     *        object.
     */
    void begin(size_t threadId = 0);

    /**
     * This function stores a custom value in the sample. It should be called
     * before 'end()'.
     * @param index The index of the value [0, KNARR_MAX_CUSTOM_FIELDS[
     * @param value The value.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). The value is the thread id, in the
     *        range [0, n[, where n is the number of threads calling this
     *        function. n must also have been specified when constructing this
     *        object.
     */
    void storeCustomValue(size_t index, double value, size_t threadId = 0);

    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop ends.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). The value is the thread id, in the
     *        range [0, n[, where n is the number of threads calling this
     *        function. n must also have been specified when constructing this
     *        object.
     */
    void end(size_t threadId = 0);

    /**
     * This function must only be called once, when the parallel part
     * of the application terminates.
     * NOTE: It is not thread safe!
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
    explicit Monitor(const std::string& channelName);
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
