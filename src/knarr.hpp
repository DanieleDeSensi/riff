/*
 * This file is part of knarr
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef KNARR_HPP_
#define KNARR_HPP_

#include "archdata.hpp"
#include "external/cppnanomsg/nn.hpp"
#include "external/nanomsg/src/pair.h"

#include <algorithm>
#include <pthread.h>
#include <memory>
#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <string.h>

#define KNARR_MAX_CUSTOM_FIELDS 4

#ifndef KNARR_DEFAULT_SAMPLING_LENGTH
// Never skips any begin() call.
#define KNARR_DEFAULT_SAMPLING_LENGTH 1
#endif

// This value is used to mark an inconsistent value.
#define KNARR_VALUE_INCONSISTENT -1.0
// This value is used when was not possible to collect it.
#define KNARR_VALUE_NOT_AVAILABLE -2.0

namespace knarr{

// Represents the minimum number of
// threads from which the data must
// be collected before replying
// to the monitor requests.
typedef enum ThreadsNeeded{
    KNARR_THREADS_NEEDED_NONE = 0, // We can reply even if no threads stored the sample
    KNARR_THREADS_NEEDED_ONE, // We can reply if at least one thread stored the sample
    KNARR_THREADS_NEEDED_ALL // We can reply only when all the threads stored the sample
}ThreadsNeeded;

// Configuration parameters for knarr behaviour
// when collecting samples. 
typedef struct ApplicationConfiguration{
    // Represents the minimum length in milliseconds
    // between two successive begin() calls. If begin()
    // is called more frequently than this value,
    // intermediate calls are skipped. If this value
    // is set to zero, no calls will be skipped.
    // [default = 10.0]
    double samplingLengthMs;

    // Represents the minimum number of
    // threads from which the data must
    // be collected before replying
    // to the monitor requests.
    // [default = KNARR_THREADS_NEEDED_ALL]
    ThreadsNeeded threadsNeeded;

    // If true and if threadsNeeded is not KNARR_THREADS_NEEDED_ALL, 
    // for the threads that didn't yet stored
    // their sample, we estimate the bandwidth to be
    // the same of the other threads. This should be set to
    // true if you want to provide a consistent view
    // of the application bandwidth. If false,
    // bandwidth will change accordingly to how many threads
    // already stored their samples and we would have
    // fluctuations caused by the way in which data is collected
    // but not actually present in the application.
    // [default = true]
    bool adjustBandwidth;

    // When sampling is applied, the latency estimation
    // (and the idle time/utilization estimation) could be 
    // wrong. This means that we could pick latency sample which are
    // far from the average (lower/higher), thus since we assume that
    // latency is more or less constant, in very skewed situations
    // our estimation could be completly wrong. This can 
    // be detected by knarr by comparing the actual elapsed time
    // with the time computed as the sum of the latency and idle time.
    // When these values are different, it means that either the 
    // latency or the idle time have been not correctly estimated
    // (due to skewness). This can only happen when sampling is applied.
    // The following macro represents the maximum percentage of difference
    // between the estimated time and the actual time we are willing to tolerate.
    // If the estimated time (computed from latency and idle time)
    // is more than consistencyThreshold% different from the
    // actual time, we will mark latency and utilization as
    // inconsistent. Bandwidth computation and tasks count are never
    // affected by inconsistencies.
    // [default = 5.0]
    double consistencyThreshold;

    ApplicationConfiguration(){
        samplingLengthMs = 1.0;
        threadsNeeded = KNARR_THREADS_NEEDED_ALL;
        adjustBandwidth = true;
        consistencyThreshold = 5.0;
    }
}ApplicationConfiguration;

unsigned long long getCurrentTimeNs();

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
    // If equal to KNARR_VALUE_INCONSISTENT, please set samplingLengthMs to 0 in 
    // knarr configuration.
    double loadPercentage;

    // The bandwidth of the application.
    double bandwidth;

    // The average latency (nanoseconds).
    // If equal to KNARR_VALUE_INCONSISTENT, please set samplingLengthMs to 0 in 
    // knarr configuration.
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
        if(loadPercentage != KNARR_VALUE_INCONSISTENT && 
           rhs.loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage += rhs.loadPercentage;
        }else{
            loadPercentage = KNARR_VALUE_INCONSISTENT;
        }

        if(bandwidth != KNARR_VALUE_INCONSISTENT && 
           rhs.bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth += rhs.bandwidth;
        }else{
            bandwidth = KNARR_VALUE_INCONSISTENT;
        }

        if(latency != KNARR_VALUE_INCONSISTENT && 
           rhs.latency != KNARR_VALUE_INCONSISTENT){
            latency += rhs.latency;
        }else{
            latency = KNARR_VALUE_INCONSISTENT;
        }

        if(numTasks != KNARR_VALUE_INCONSISTENT && 
           rhs.numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks += rhs.numTasks;
        }else{
            numTasks = KNARR_VALUE_INCONSISTENT;        
        }

        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] += rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator-=(const ApplicationSample& rhs){
        if(loadPercentage != KNARR_VALUE_INCONSISTENT && 
           rhs.loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage -= rhs.loadPercentage;
        }else{
            loadPercentage = KNARR_VALUE_INCONSISTENT;
        }

        if(bandwidth != KNARR_VALUE_INCONSISTENT && 
           rhs.bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth -= rhs.bandwidth;
        }else{
            bandwidth = KNARR_VALUE_INCONSISTENT;
        }

        if(latency != KNARR_VALUE_INCONSISTENT && 
           rhs.latency != KNARR_VALUE_INCONSISTENT){
            latency -= rhs.latency;
        }else{
            latency = KNARR_VALUE_INCONSISTENT;
        }

        if(numTasks != KNARR_VALUE_INCONSISTENT && 
           rhs.numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks -= rhs.numTasks;
        }else{
            numTasks = KNARR_VALUE_INCONSISTENT;        
        }

        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] -= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator*=(const ApplicationSample& rhs){
        if(loadPercentage != KNARR_VALUE_INCONSISTENT && 
           rhs.loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage *= rhs.loadPercentage;
        }else{
            loadPercentage = KNARR_VALUE_INCONSISTENT;
        }

        if(bandwidth != KNARR_VALUE_INCONSISTENT && 
           rhs.bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth *= rhs.bandwidth;
        }else{
            bandwidth = KNARR_VALUE_INCONSISTENT;
        }

        if(latency != KNARR_VALUE_INCONSISTENT && 
           rhs.latency != KNARR_VALUE_INCONSISTENT){
            latency *= rhs.latency;
        }else{
            latency = KNARR_VALUE_INCONSISTENT;
        }

        if(numTasks != KNARR_VALUE_INCONSISTENT && 
           rhs.numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks *= rhs.numTasks;
        }else{
            numTasks = KNARR_VALUE_INCONSISTENT;        
        }

        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] *= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample& operator/=(const ApplicationSample& rhs){
        if(loadPercentage != KNARR_VALUE_INCONSISTENT && 
           rhs.loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage /= rhs.loadPercentage;
        }else{
            loadPercentage = KNARR_VALUE_INCONSISTENT;
        }

        if(bandwidth != KNARR_VALUE_INCONSISTENT && 
           rhs.bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth /= rhs.bandwidth;
        }else{
            bandwidth = KNARR_VALUE_INCONSISTENT;
        }

        if(latency != KNARR_VALUE_INCONSISTENT && 
           rhs.latency != KNARR_VALUE_INCONSISTENT){
            latency /= rhs.latency;
        }else{
            latency = KNARR_VALUE_INCONSISTENT;
        }

        if(numTasks != KNARR_VALUE_INCONSISTENT && 
           rhs.numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks /= rhs.numTasks;
        }else{
            numTasks = KNARR_VALUE_INCONSISTENT;        
        }

        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] /= rhs.customFields[i];
        }
        return *this;
    }

    ApplicationSample operator/=(double x){
        if(loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage /= x;
        }
        if(bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth /= x;
        }
        if(latency != KNARR_VALUE_INCONSISTENT){
            latency /= x;
        }
        if(numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks /= x;
        }
        for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
            customFields[i] /= x;
        }
        return *this;
    }

    ApplicationSample operator*=(double x){
        if(loadPercentage != KNARR_VALUE_INCONSISTENT){
            loadPercentage *= x;
        }
        if(bandwidth != KNARR_VALUE_INCONSISTENT){
            bandwidth *= x;
        }
        if(latency != KNARR_VALUE_INCONSISTENT){
            latency *= x;
        }
        if(numTasks != KNARR_VALUE_INCONSISTENT){
            numTasks *= x;
        }
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
    pid_t pid;
    bool fromAll;
    ApplicationSample sample;
    struct{
        ulong time;
        unsigned long long totalTasks;
    }summary;
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
    ApplicationSample sample __attribute__((aligned(LEVEL1_DCACHE_LINESIZE)));
    ApplicationSample consolidatedSample;
    unsigned long long rcvStart;
    unsigned long long computeStart;
    unsigned long long idleTime;
    unsigned long long firstBegin;
    unsigned long long lastEnd;
    unsigned long long sampleStartTime;
    unsigned long long totalTasks;
    bool extraTask;
    bool consolidate;
    ulong samplingLength;
    ulong currentSample;
    char padding[LEVEL1_DCACHE_LINESIZE];

    ThreadData():rcvStart(0), computeStart(0), idleTime(0), firstBegin(0),
                 lastEnd(0), sampleStartTime(0), totalTasks(0),
                 extraTask(false), consolidate(false),
                 samplingLength(KNARR_DEFAULT_SAMPLING_LENGTH),
                 currentSample(0){
        memset(&padding, 0, sizeof(padding));
    }
}ThreadData;

void* applicationSupportThread(void*);

class Application{
    friend void waitSampleStore(Application* application);
    friend bool thisSampleNeeded(Application* application, size_t threadId, size_t updatedSamples, bool fromAll);
    friend bool keepWaitingSample(Application* application, size_t threadId, size_t updatedSamples, bool fromAll);
    friend void* applicationSupportThread(void*);
private:
    ApplicationConfiguration _configuration;
    nn::socket* _channel;
    nn::socket& _channelRef;
    int _chid;
    bool _started;
    Aggregator* _aggregator;
    pthread_mutex_t _mutex;
    pthread_t _supportTid;
    bool _supportStop;
    std::vector<ThreadData> _threadData;
    ulong _executionTime;
    unsigned long long _totalTasks;

    // We are sure it is called by at most one thread.
    void notifyStart();

    void updateSamplingLength(ThreadData& td, unsigned long long sampleTime);

    // We do not use abs because they are both unsigned
    // if we do abs(x - y) and x is smaller than y, the temporary
    // result (before applying abs) cannot be negative so it will wrap
    // and assume a huge value.
    static inline unsigned long long absDiff(unsigned long long x, unsigned long long y){
        if(x > y){
            return x - y;
        }else{
            return y - x;
        }
    }
public:
    /**
     * Constructs this object.
     * @param channelName The name of the channel.
     * @param numThreads The number of threads which will concurrently use
     *        this library.
     * @param aggregator An aggregator object to aggregate custom values
     *        stored by multiple threads.
     */
    Application(const std::string& channelName, 
                size_t numThreads = 1,
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
    Application(nn::socket& socket, uint chid, 
                size_t numThreads = 1,
                Aggregator* aggregator = NULL);

    ~Application();

    Application(const Application& a) = delete;
    Application& operator=(Application const &x) = delete;

    /**
     * Sets the application configuration.
     * MUST be called before calling begin() for the first time.
     * @param configuration The application configuration.
     **/
    void setConfiguration(const ApplicationConfiguration& configuration);

    /**
     * Sets the default application configuration for a streaming application.
     * It will set:
     *     - samplingLengthMs = default
     *     - threadsNeeded = KNARR_THREADS_NEEDED_NONE
     *     - adjustBandwidth = true
     *     - consistencyThreshold = default
     * MUST be called before calling begin() for the first time.
     **/
    void setConfigurationStreaming();

    /**
     * Sets the default application configuration for a batch application.
     * It will set:
     *     - samplingLengthMs = default
     *     - threadsNeeded = as specified
     *     - adjustBandwidth = true
     *     - consistencyThreshold = default
     * MUST be called before calling begin() for the first time.
     * @param threadsNeeded Suggested KNARR_THREADS_NEEDED_ALL when 
     *                      there are many iterations per second 
     *                      (> numThreads * 10), KNARR_THREADS_NEEDED_ONE otherwise.
     **/
    void setConfigurationBatch(ThreadsNeeded threadsNeeded = KNARR_THREADS_NEEDED_ALL);
    
    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop begins.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It must be a number univocally
     *        identifying the thread calling this function and in
     *        the range [0, n[, where n is the number of threads specified
     *        in the constructor.
     */
    inline void begin(uint threadId = 0){
        ThreadData& tData = _threadData[threadId];

        // Equivalent to
        // tData.currentSample = (tData.currentSample + 1) % tData.samplingLength;
        // but faster.
        tData.currentSample = (tData.currentSample + 1) >= tData.samplingLength ? 0 : tData.currentSample + 1;
      
        // Skip
        if(tData.currentSample > 1){
            return;
        }

        /********* Only executed once (at startup). - BEGIN *********/
        if(!_started){
            pthread_mutex_lock(&_mutex);
            // This awful double check is done to avoid locking the flag
            // every time (this code is executed at most once).
            if(!_started){
                notifyStart();
                _started = true;
            }
            pthread_mutex_unlock(&_mutex);

        }
        unsigned long long now = getCurrentTimeNs();
        if(!tData.firstBegin){
            tData.firstBegin = now;
        }
        if(!tData.sampleStartTime){
            tData.sampleStartTime = now;
        }
        /********* Only executed once (at startup). - END *********/

        ulong oldSamplingLength = tData.samplingLength;
        if(tData.computeStart){
            // To collect a sample, we need to execute begin two
            // times in a row, i.e.
            //     ... begin(); end(); begin(); ...
            //
            // Otherwise it would not be possible to collect the
            // idleTime.
            //
            // When tData.currentSample == 0:
            //      - We start the timer for recording latency
            //        (tData.computeStart = now).
            // When tData.currentSample == 1:
            //      - We record idleTime (since the timer has)
            //        been started by end() with currentSample == 0.
            // The only exception is for samplingLength == 1, since
            // in this case currentSample is always 0 and we execute
            // both sections.
            if(tData.currentSample == 1 || tData.samplingLength == 1){
                tData.idleTime += ((now - tData.rcvStart) * tData.samplingLength);
                unsigned long long sampleTime = now - tData.sampleStartTime;
                unsigned long long sampleTimeEstimated = (tData.sample.latency + tData.idleTime);

                tData.sample.bandwidth = tData.sample.numTasks /
                                         (sampleTime / 1000000000.0); // From tasks/ns to tasks/sec
                tData.sample.loadPercentage = (tData.sample.latency / sampleTime) * 100.0;

                if(tData.consolidate){
                    tData.consolidatedSample = tData.sample;
                    // Consistency check
                    // If the gap between real total time and the one estimated with
                    // latency and idle time is greater than a threshold, idleTime and
                    // latency are not reliable.
                    if(((absDiff(sampleTime, sampleTimeEstimated) /
                         (double) sampleTime) * 100.0) > _configuration.consistencyThreshold){
                        if(tData.samplingLength == 1){
                            throw std::runtime_error("FATAL ERROR: it is not possible to have inconsistency if sampling is not applied.");
                        }
                        tData.consolidatedSample.latency = KNARR_VALUE_INCONSISTENT;
                        tData.consolidatedSample.loadPercentage = KNARR_VALUE_INCONSISTENT;
                    }
                    tData.sample = ApplicationSample();
                    tData.idleTime = 0;
                    tData.sampleStartTime = now;
                    tData.consolidate = false;
                }

                // DON'T MOVE EARLIER THE SAMPLING UPDATE
                if(_configuration.samplingLengthMs){
                    updateSamplingLength(tData, sampleTime);
                }

                // We need to manage the corner case where sample was one and
                // now is greater than one. In this case currentSample is 0
                // and end() would be executed on the new sample length.
                // We need than to force currentSample to 1 to let the
                // counting work.
                if(oldSamplingLength == 1 && tData.samplingLength > 1){
                    tData.currentSample = 1;
                }
                // If I reduce the samplingLength to 1, I lose 1 task
                // in the count. Indeed, now currentSample = 1 and the
                // next end will not be executed. At next iteration,
                // currentSample = 0 and end() will be executed, but
                // samplingLength is 1 and only one task will be added
                // to the count (but actually they are 2). We use
                // a boolean flag to signal such situation.
                if(oldSamplingLength > 1 && tData.samplingLength == 1){
                    tData.extraTask = true;
                }
            }
        }
        tData.computeStart = now;
    }

    /**
     * This function stores a custom value in the sample. It should be called
     * after 'end()'.
     * @param index The index of the value [0, KNARR_MAX_CUSTOM_FIELDS[
     * @param value The value.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It must be a number univocally
     *        identifying the thread calling this function and in
     *        the range [0, n[, where n is the number of threads specified
     *        in the constructor.
     */
    void storeCustomValue(size_t index, double value, uint threadId = 0);

    /**
     * This function must be called at each loop iteration when the computation
     * part of the loop ends.
     * @param threadId Must be specified when is called by multiple threads
     *        (e.g. inside a parallel loop). It must be a number univocally
     *        identifying the thread calling this function and in
     *        the range [0, n[, where n is the number of threads specified
     *        in the constructor.
     */
    inline void end(uint threadId = 0){
        ThreadData& tData = _threadData[threadId];
        // Skip
        if(tData.currentSample){
            return;
        }
        // We only store samples if tData.currentSample == 0
        unsigned long long now = getCurrentTimeNs();
        tData.rcvStart = now;    

        // If we perform sampling, we assume that all the other samples
        // different from the one recorded had the same latency.
        double newLatency = (tData.rcvStart - tData.computeStart);
        tData.sample.latency += (newLatency * tData.samplingLength);
        tData.sample.numTasks += tData.samplingLength;
        tData.totalTasks += tData.samplingLength;
        if(tData.extraTask){
            ++tData.sample.numTasks;
            ++tData.totalTasks;
            tData.extraTask = false;
        }
        tData.lastEnd = now;
    }

    /**
     * This function must only be called once, when the parallel part
     * of the application terminates.
     * NOTE: It is not thread safe!
     */
    void terminate();
    
    /**
     * Returns the execution time of the application (milliseconds).
     * MUST be called after terminate().
     * @return The execution time of the application (milliseconds).
     * The time is from the first call of begin() to the last call of end().
     */
    ulong getExecutionTime();

    /**
     * Returns the total number of tasks computed by the application.
     * MUST be called after terminate().
     * @return The total number of tasks computed by the application.
     * Is computed as the sum of tasks executed from the first call 
     * of begin() to the last call of end().
     */
    unsigned long long getTotalTasks();
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

    bool getSample(ApplicationSample& sample, bool fromAll = false);

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

//  Windows
#ifdef _WIN32 
#include <intrin.h>
inline uint64_t rdtsc(){
    return __rdtsc();
}
//  Linux/GCC
#else
inline uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

} // End namespace

#endif
