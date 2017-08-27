#ifndef KNARR_HPP_
#define KNARR_HPP_

#include "external/cppnanomsg/nn.hpp"
#include "external/nanomsg/src/pair.h"

#include <algorithm>
#include <pthread.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <limits>

#define KNARR_MAX_CUSTOM_FIELDS 4

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

    // The bandwidth of the application.
    double bandwidth;

    // The average latency (nanoseconds).
    double latency;

    // The number of computed tasks.
    double numTasks;

    // Custom user fields.
    double customFields[KNARR_MAX_CUSTOM_FIELDS];

    ApplicationSample():loadPercentage(0), bandwidth(0),
                        latency(0), numTasks(0){
        // We do not use memset due to cppcheck warnings.
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] = 0;
        }
    }

    ApplicationSample(ApplicationSample const& sample):
        loadPercentage(sample.loadPercentage), bandwidth(sample.bandwidth),
        latency(sample.latency), numTasks(sample.numTasks){
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] = sample.customFields[i];
        }
    }

    void swap(ApplicationSample& x){
        using std::swap;

        swap(loadPercentage, x.loadPercentage);
        swap(bandwidth, x.bandwidth);
        swap(latency, x.latency);
        swap(numTasks, x.numTasks);
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            swap(customFields[i], x.customFields[i]);
        }
    }

    ApplicationSample& operator=(ApplicationSample rhs){
        swap(rhs);
        return *this;
    }

    ApplicationSample& operator+=(const ApplicationSample& rhs){
        loadPercentage += rhs.loadPercentage;
        bandwidth += rhs.bandwidth;
        latency += rhs.latency;
        numTasks += rhs.numTasks;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] += rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator-=(const ApplicationSample& rhs){
        loadPercentage -= rhs.loadPercentage;
        bandwidth -= rhs.bandwidth;
        latency -= rhs.latency;
        numTasks -= rhs.numTasks;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] -= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator*=(const ApplicationSample& rhs){
        loadPercentage *= rhs.loadPercentage;
        bandwidth *= rhs.bandwidth;
        latency *= rhs.latency;
        numTasks *= rhs.numTasks;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] *= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator/=(const ApplicationSample& rhs){
        loadPercentage /= rhs.loadPercentage;
        bandwidth /= rhs.bandwidth;
        latency /= rhs.latency;
        numTasks /= rhs.numTasks;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] /= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample operator/=(double x){
        loadPercentage /= x;
        bandwidth /= x;
        latency /= x;
        numTasks /= x;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] /= x;
        }
        return *this;
    }

    ApplicationSample operator*=(double x){
        loadPercentage *= x;
        bandwidth *= x;
        latency *= x;
        numTasks *= x;
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] *= x;
        }
        return *this;
    }
}ApplicationSample;


inline ApplicationSample operator+(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs){
    ApplicationSample r = lhs;
    r += rhs;
    return r;
}

inline ApplicationSample operator-(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs){
    ApplicationSample r = lhs;
    r -= rhs;
    return r;
}

inline ApplicationSample operator*(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs){
    ApplicationSample r = lhs;
    r *= rhs;
    return r;
}

inline ApplicationSample operator*(const ApplicationSample& lhs, double x){
    ApplicationSample r = lhs;
    r *= x;
    return r;
}

inline ApplicationSample operator/(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs){
    ApplicationSample r = lhs;
    r /= rhs;
    return r;
}

inline ApplicationSample operator/(const ApplicationSample& lhs, double x){
    ApplicationSample r = lhs;
    r /= x;
    return r;
}

inline std::ostream& operator<<(std::ostream& os, const ApplicationSample& obj){
    os << "[";
    os << "Load: " << obj.loadPercentage << " ";
    os << "Bandwidth: " << obj.bandwidth << " ";
    os << "Latency: " << obj.latency << " ";
    os << "NumTasks: " << obj.numTasks << " ";
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        os << "CustomField" << i << ": " << obj.customFields[i] << " ";
    }
    os << "]";
    return os;
}

inline std::istream& operator>>(std::istream& is, ApplicationSample& sample){
    is.ignore(std::numeric_limits<std::streamsize>::max(), '[');
    is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    is >> sample.loadPercentage;
    is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    is >> sample.bandwidth;
    is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    is >> sample.latency;
    is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    is >> sample.numTasks;
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
        is >> sample.customFields[i];
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), ']');
    return is;
}

typedef union Payload{
    ApplicationSample sample;
    ulong time;
    unsigned long long totalTasks;
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
    unsigned long long totalTasks;
    bool clean;

    ThreadData():rcvStart(0), computeStart(0), idleTime(0), firstBegin(0),
                 lastEnd(0), clean(false){;}

    void reset(){
        sample = ApplicationSample();
        rcvStart = 0;
        idleTime = 0;
    }
}ThreadData;

void* applicationSupportThread(void*);

class Application{
    friend void* applicationSupportThread(void*);
private:
    nn::socket* _channel;
    nn::socket& _channelRef;
    int _chid;
    bool _started;
    Aggregator* _aggregator;
    pthread_mutex_t _mutex;
    pthread_t _supportTid;
    bool _supportStop;
    std::map<uint, ThreadData> _threadData;

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
    Application& operator=(Application const &x) = delete;
    
    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop begins.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It can be any number univocally
     *        identifying the thread calling this function.
     */
    void begin(uint threadId = 0);

    /**
     * This function stores a custom value in the sample. It should be called
     * before 'end()'.
     * @param index The index of the value [0, KNARR_MAX_CUSTOM_FIELDS[
     * @param value The value.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It can be any number univocally
     *        identifying the thread calling this function.
     */
    void storeCustomValue(size_t index, double value, uint threadId = 0);

    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop ends.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It can be any number univocally
     *        identifying the thread calling this function.
     */
    void end(uint threadId = 0);

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
    unsigned long long _totalTasks;
public:
    explicit Monitor(const std::string& channelName);
    Monitor(nn::socket& socket, uint chid);
    ~Monitor();

    Monitor(const Monitor& m) = delete;
    Monitor& operator=(Monitor const &x) = delete;
    
    pid_t waitStart();

    bool getSample(ApplicationSample& sample);

    /**
     * Returns the execution time of the application (milliseconds).
     * @return The execution time of the application (milliseconds).
     * The time is from the first call of begin() to the last call of end().
     */
    ulong getExecutionTime();

    /**
     * Returns the total number of tasks computed by the application.
     * @return The total number of tasks computed by the application.
     * Is computed as the sum of tasks executed from the first call 
     * of begin() to the last call of end().
     */
    unsigned long long getTotalTasks();
};

} // End namespace

#endif
