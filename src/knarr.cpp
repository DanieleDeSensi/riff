/*
 * This file is part of knarr
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "external/cppnanomsg/nn.hpp"

#include "knarr.hpp"

#include <cmath>
#include <errno.h>
#include <limits>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#undef DEBUG
#undef DEBUGB

#ifdef DEBUG_KNARR
#define DEBUG(x) do { std::cerr << "[Knarr] " << x << std::endl; } while (0)
#define DEBUGB(x) do {x;} while (0)
#else
#define DEBUG(x)
#define DEBUGB(x)
#endif

namespace knarr{

unsigned long long getCurrentTimeNs(){
#ifdef KNARR_NS_PER_TICK 
    return rdtsc() / KNARR_NS_PER_TICK;
#else
    struct timespec tp;
    int r = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(!r);
    return tp.tv_sec * 1.0e9 + tp.tv_nsec;
#endif
}

inline void waitSampleStore(Application* application){
    if(application->_configuration.samplingLengthMs > 0){
        usleep((1000 * application->_configuration.samplingLengthMs) / application->_threadData.size());
    }else{
        usleep(1000);
    }
}

inline bool thisSampleNeeded(Application* application, size_t threadId, size_t updatedSamples, bool fromAll){
    // If we need samples from all the threads
    //       or 
    // If this is the last thread and there are no 
    // samples stored, then we need this sample.
    return (fromAll ||
            application->_configuration.threadsNeeded == KNARR_THREADS_NEEDED_ALL || 
            ((threadId == application->_threadData.size() - 1) && 
            !updatedSamples && 
            application->_configuration.threadsNeeded == KNARR_THREADS_NEEDED_ONE));
}

inline bool keepWaitingSample(Application* application, size_t threadId, size_t updatedSamples, bool fromAll){
    const ThreadData& chkToAdd = application->_threadData[threadId];
    const ApplicationSample& chkSample = chkToAdd.sample;

    unsigned long long totalTime = chkToAdd.lastEnd - chkToAdd.sampleStartTime;
    if(totalTime == 0 || chkSample.latency == 0 || chkToAdd.idleTime == 0 || chkToAdd.clean){
        if(thisSampleNeeded(application, threadId, updatedSamples, fromAll) &&
           !application->_supportStop){
            return true;
        }

        // If we don't need to wait for thread sample,
        // stop waiting.
        if(application->_configuration.threadsNeeded == KNARR_THREADS_NEEDED_NONE || 
           (application->_configuration.threadsNeeded == KNARR_THREADS_NEEDED_ONE && updatedSamples >= 1) || 
           application->_supportStop){
            return false;
        }
        return true;
    }
    return false;
}

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

void* applicationSupportThread(void* data){
    Application* application = static_cast<Application*>(data);

    while(!application->_supportStop){
        Message recvdMsg;
        int res = application->_channelRef.recv(&recvdMsg, sizeof(recvdMsg), 0);
        bool fromAll = recvdMsg.payload.fromAll;
        if(res == sizeof(recvdMsg)){
            assert(recvdMsg.type == MESSAGE_TYPE_SAMPLE_REQ);
            // Prepare response message.
            Message msg;
            msg.type = MESSAGE_TYPE_SAMPLE_RES;
            msg.payload.sample = ApplicationSample(); // Set sample to all zeros

            // Add the samples of all the threads.
            size_t updatedSamples = 0, inconsistentSamples = 0;
            std::vector<size_t> toClean; // Identifiers of threads to be cleaned.
            size_t numThreads = application->_threadData.size();
            for(size_t i = 0; i < numThreads; i++){
                ThreadData& toAdd = application->_threadData[i];                                
                
                // If needed, wait for thread to store a sample.                
                while(keepWaitingSample(application, i, updatedSamples, fromAll)){
                    waitSampleStore(application);
                }

                if(application->_configuration.preciseCount){
                    while(toAdd.lock->test_and_set(std::memory_order_acquire));
                }
                ApplicationSample sample = toAdd.sample;
                if(application->_configuration.preciseCount){
                    toAdd.lock->clear(std::memory_order_release);
                }
                // If we required a cleaning and it has not yet 
                // been done (clean = true), we skip this thread.
                unsigned long long totalTime = toAdd.lastEnd - toAdd.sampleStartTime;
                if(totalTime && sample.latency && toAdd.idleTime && !toAdd.clean){                    
                    unsigned long long totalTimeEstimated = (sample.latency + toAdd.idleTime);
                    // If the gap between real total time and the one estimated with
                    // latency and idle time is greater than a threshold, idleTime and
                    // latency are not reliable.
                    if(((absDiff(totalTime, totalTimeEstimated) /
                         (double) totalTime) * 100.0) > application->_configuration.consistencyThreshold){
#if defined(KNARR_DEFAULT_SAMPLING_LENGTH) && KNARR_DEFAULT_SAMPLING_LENGTH == 1
                        if(!application->_configuration.samplingLengthMs){
                            throw std::runtime_error("FATAL ERROR: it is not possible to have inconsistency if sampling is not used.");
                        }
#endif
                        ++inconsistentSamples;
                    }else{
                        sample.loadPercentage = (sample.latency / totalTime) * 100.0;
                        sample.latency /= sample.numTasks;
                        msg.payload.sample.loadPercentage += sample.loadPercentage;
                        msg.payload.sample.latency += sample.latency;
                    }
                    sample.bandwidth = sample.numTasks / (totalTime / 1000000000.0); // From tasks/ns to tasks/sec
                    msg.payload.sample.bandwidth += sample.bandwidth;
                    msg.payload.sample.numTasks += sample.numTasks;

                    ++updatedSamples;
                    toClean.push_back(i);
                }
            }

            // If at least one thread is progressing.
            if(updatedSamples){
                if(application->_configuration.adjustBandwidth && 
                   updatedSamples != application->_threadData.size()){
                    // This can only happen if we didn't need to store
                    // data from all the threads
                    if(!application->_supportStop){
                        assert(application->_configuration.threadsNeeded != KNARR_THREADS_NEEDED_ALL);
                    }
                    msg.payload.sample.bandwidth += (msg.payload.sample.bandwidth / updatedSamples) * (application->_threadData.size() - updatedSamples);
                }

                // If we collected only inconsistent samples, we mark latency and load as inconsistent.
                if(inconsistentSamples == updatedSamples){
                    msg.payload.sample.loadPercentage = KNARR_VALUE_INCONSISTENT;
                    msg.payload.sample.latency = KNARR_VALUE_INCONSISTENT;
                }else{
                    msg.payload.sample.loadPercentage /= updatedSamples;
                    msg.payload.sample.latency /= updatedSamples;
                }
            }else{
                // This can only happens if the threadsNeeded is NONE
                if(!application->_supportStop){
                    assert(application->_configuration.threadsNeeded == KNARR_THREADS_NEEDED_NONE);
                }
                msg.payload.sample.bandwidth = 0;
                msg.payload.sample.latency = KNARR_VALUE_NOT_AVAILABLE;
                msg.payload.sample.loadPercentage = 0;
                msg.payload.sample.numTasks = 0;
            }
            
            // Aggregate custom values.
            if(application->_aggregator){
                std::vector<double> customVec;
                customVec.reserve(application->_threadData.size());
                for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
                    customVec.clear();
                    for(ThreadData& td : application->_threadData){
                        customVec.push_back(td.sample.customFields[i]);
                    }
                    msg.payload.sample.customFields[i] = application->_aggregator->aggregate(i, customVec);
                }
            }
            DEBUG(msg.payload.sample);
            // Send message
            application->_channelRef.send(&msg, sizeof(msg), 0);
            // Tell all threads to clear their sample since current one have
            // been already sent.
            for(size_t i = 0; i < toClean.size(); i++){
                application->_threadData[i].clean = true;
            }
            toClean.clear();
        }else if(res == -1){
            throw std::runtime_error("Received less bytes than expected.");
        }
    }
    return NULL;
}

Application::Application(const std::string& channelName, size_t numThreads,
                         Aggregator* aggregator):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _started(false), _aggregator(aggregator), _executionTime(0), _totalTasks(0){
    _chid = _channelRef.connect(channelName.c_str());
    assert(_chid >= 0);
    pthread_mutex_init(&_mutex, NULL);
    _supportStop = false;
    _threadData.resize(numThreads);
    // Pthread Create must be the last thing we do in constructor
    pthread_create(&_supportTid, NULL, applicationSupportThread, (void*) this);
}

Application::Application(nn::socket& socket, uint chid, size_t numThreads,
                         Aggregator* aggregator):
        _channel(NULL), _channelRef(socket), _chid(chid), _started(false),
        _aggregator(aggregator), _executionTime(0), _totalTasks(0){
    pthread_mutex_init(&_mutex, NULL);
    _supportStop = false;
    _threadData.resize(numThreads);
    // Pthread Create must be the last thing we do in constructor
    pthread_create(&_supportTid, NULL, applicationSupportThread, (void*) this);
}


Application::~Application(){
    if(_channel){
        _channel->shutdown(_chid);
        delete _channel;
    }
}    

void Application::notifyStart(){
    Message msg;
    msg.type = MESSAGE_TYPE_START;
    msg.payload.pid = getpid();
    int r = _channelRef.send(&msg, sizeof(msg), 0);
    assert(r == sizeof(msg));
}

void Application::updateSamplingLength(ThreadData& tData){
    if(tData.sample.numTasks){
        double latencyNs = tData.sample.latency / tData.sample.numTasks;
        double latencyMs = latencyNs / 1000000.0;
        if(latencyMs){
            tData.samplingLength = std::ceil((double) _configuration.samplingLengthMs / latencyMs);
        }else{
            tData.samplingLength = 1;
        }
    }
}

void Application::setConfiguration(const ApplicationConfiguration& configuration){
    _configuration = configuration;
}

void Application::setConfigurationStreaming(){
    ApplicationConfiguration configuration;
    configuration.threadsNeeded = KNARR_THREADS_NEEDED_NONE;
    configuration.adjustBandwidth = true;
    configuration.preciseCount = true;
    _configuration = configuration;
}

void Application::setConfigurationBatch(ThreadsNeeded threadsNeeded){
    ApplicationConfiguration configuration;
    configuration.threadsNeeded = threadsNeeded;
    configuration.adjustBandwidth = true;
    configuration.preciseCount = true;
    _configuration = configuration;
}

void Application::storeCustomValue(size_t index, double value, uint threadId){
    if(threadId > _threadData.size()){
        throw new std::runtime_error("Wrong threadId specified (greater than number of threads).");
    }
    if(index < KNARR_MAX_CUSTOM_FIELDS){
        ThreadData& tData = _threadData[threadId];
        tData.sample.customFields[index] = value;
    }else{
        throw std::runtime_error("Custom value index out of bound. Please "
                                 "increase KNARR_MAX_CUSTOM_FIELDS macro value.");
    }
}

void Application::terminate(){
    unsigned long long lastEnd = 0, firstBegin = std::numeric_limits<unsigned long long>::max(); 
    for(ThreadData& td : _threadData){
        // If I was doing sampling, I could have spurious
        // tasks that I didn't record. For this reason,
        // I record them now.
        td.totalTasks += td.currentSample;    
        
        _totalTasks += td.totalTasks;
        if(td.firstBegin < firstBegin){
            firstBegin = td.firstBegin;
        }
        if(td.lastEnd > lastEnd){
            lastEnd = td.lastEnd;
        }
        // Unlock all the locks (they were locked with end() and so never unlocked)
        if(_configuration.preciseCount){
            td.lock->clear(std::memory_order_release);
        }
    }
    _executionTime = (lastEnd - firstBegin) / 1000000.0; // Must be in ms

    _supportStop = true;
    pthread_join(_supportTid, NULL);
    
    Message msg;
    msg.type = MESSAGE_TYPE_STOP;
    msg.payload.summary.time = _executionTime;
    msg.payload.summary.totalTasks = _totalTasks;
    int r = _channelRef.send(&msg, sizeof(msg), 0);
    assert (r == sizeof(msg));
    // Wait for ack before leaving (otherwise if object is destroyed
    // the monitor could never receive the stop).
    r = _channelRef.recv(&msg, sizeof(msg), 0);
    assert(r == sizeof(msg));
}

ulong Application::getExecutionTime(){
    return _executionTime;
}

unsigned long long Application::getTotalTasks(){
    return _totalTasks;
}

Monitor::Monitor(const std::string& channelName):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _executionTime(0), _totalTasks(0){
    _chid = _channelRef.bind(channelName.c_str());
    assert(_chid >= 0);
}

Monitor::Monitor(nn::socket& socket, uint chid):
        _channel(NULL), _channelRef(socket), _chid(chid), _executionTime(0), _totalTasks(0){
    ;
}

Monitor::~Monitor(){
    if(_channel){
        _channel->shutdown(_chid);
        delete _channel;
    }
}

pid_t Monitor::waitStart(){
    Message m;
    int r = _channelRef.recv(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    assert(m.type == MESSAGE_TYPE_START);
    return m.payload.pid;
}

bool Monitor::getSample(ApplicationSample& sample, bool fromAll){
    Message m;
    m.type = MESSAGE_TYPE_SAMPLE_REQ;
    m.payload.fromAll = fromAll;
    int r = _channelRef.send(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    r = _channelRef.recv(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    if(m.type == MESSAGE_TYPE_SAMPLE_RES){
        sample = m.payload.sample;
        return true;
    }else if(m.type == MESSAGE_TYPE_STOP){
        _executionTime = m.payload.summary.time;
        _totalTasks = m.payload.summary.totalTasks;
        // Send ack.
        m.type = MESSAGE_TYPE_STOPACK;
        r = _channelRef.send(&m, sizeof(m), 0);
        assert(r == sizeof(m));
        return false;
    }else{
        throw runtime_error("Unexpected message type.");
    }
}

ulong Monitor::getExecutionTime(){
    return _executionTime;
}

unsigned long long Monitor::getTotalTasks(){
    return _totalTasks;
}

} // End namespace
