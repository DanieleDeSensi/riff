/*
 * This file is part of riff
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "external/cppnanomsg/nn.hpp"

#include "riff.hpp"

#include <cmath>
#include <errno.h>
#include <limits>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#undef DEBUG
#undef DEBUGB

#ifdef DEBUG_RIFF
#define DEBUG(x) do { std::cerr << "[Knarr] " << x << std::endl; } while (0)
#define DEBUGB(x) do {x;} while (0)
#else
#define DEBUG(x)
#define DEBUGB(x)
#endif

namespace riff{

unsigned long long getCurrentTimeNs(){
#ifdef RIFF_NS_PER_TICK 
    return rdtsc() / RIFF_NS_PER_TICK;
#else
    struct timespec tp;
    int r = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(!r);
    return tp.tv_sec * 1.0e9 + tp.tv_nsec;
#endif
}

inline bool thisSampleNeeded(Application* application, size_t threadId, size_t updatedSamples, bool fromAll){
    // If we need samples from all the threads
    //       or 
    // If this is the last thread and there are no 
    // samples stored, then we need this sample.
    return (fromAll ||
            application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_ALL || 
            ((threadId == application->_threadData.size() - 1) && 
            !updatedSamples && 
            application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_ONE));
}

inline bool keepWaitingSample(Application* application, size_t threadId, size_t updatedSamples, bool fromAll){
    if(*application->_threadData[threadId].consolidate){
        if(thisSampleNeeded(application, threadId, updatedSamples, fromAll) &&
           !application->_supportStop){
            return true;
        }

        // If we don't need to wait for thread sample,
        // stop waiting.
        if(application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_NONE || 
           (application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_ONE && (threadId != application->_threadData.size() - 1)) || 
           application->_supportStop){
            return false;
        }
        return true;
    }
    return false;
}

void* applicationSupportThread(void* data){
    Application* application = static_cast<Application*>(data);
    
    // TODO: We only support ALL, the other modes will be soon deleted.
    // To manage streaming applications with ebbs we need to manage it client side (i.e. if
    // monitor doesn't receive a response for a while it will say bw = 0).
    assert(application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_ALL);
    
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
            size_t numThreads = application->_threadData.size();
            std::vector<double> customVec[RIFF_MAX_CUSTOM_FIELDS];

            for(size_t i = 0; i < numThreads; i++){
                *application->_threadData[i].consolidate = true;
            }
            unsigned long long consolidationTimestamp = getCurrentTimeNs();

            for(size_t i = 0; i < numThreads; i++){
                // If needed, wait for thread to store a sample.
                while(keepWaitingSample(application, i, updatedSamples, fromAll)){
                    // To wait, the idea is that after the consolidation request has been
                    // sent, samples should be stored at most after samplingLengthMs milliseconds.
                    // If that time is already elapsed, we just wait for one millisecond 
                    // (to avoid too tight spin loop), otherwise, we wait for that time to elapse.
                    // (it is performed at most once independently from the number of needed samples)
                    unsigned long long timeFromConsolidation = (getCurrentTimeNs() - consolidationTimestamp) / 1000000.0;
                    if(timeFromConsolidation > application->_configuration.samplingLengthMs){
                        usleep(1000);
                    }else{
                        usleep((application->_configuration.samplingLengthMs - timeFromConsolidation)*1000);
                    }
                }

                ThreadData& toAdd = application->_threadData[i];
                if(!*toAdd.consolidate){
                    ApplicationSample& sample = toAdd.consolidatedSample;
                    if(sample.latency == RIFF_VALUE_INCONSISTENT){
                        ++inconsistentSamples;
                    }else{
                        sample.latency /= sample.numTasks;
                        msg.payload.sample.loadPercentage += sample.loadPercentage;
                        msg.payload.sample.latency += sample.latency;
                    }
                    msg.payload.sample.bandwidth += sample.bandwidth;
                    msg.payload.sample.numTasks += sample.numTasks;

                    ++updatedSamples;
                    for(size_t j = 0; j < RIFF_MAX_CUSTOM_FIELDS; j++){
                        // TODO How to manage not-yet-stored custom values?
                        customVec[j].push_back(sample.customFields[j]);
                    }
                    /**
                     * We need to reset the consolidated sample. Otherwise,
                     * when stop command is received, we could send
                     * again this sample even if it was not updated.
                     **/
                    toAdd.consolidatedSample = ApplicationSample();
                }
            }

            // If at least one thread is progressing.
            if(updatedSamples){
                if(application->_configuration.adjustBandwidth && 
                   updatedSamples != numThreads){
                    // This can only happen if we didn't need to store
                    // data from all the threads
                    if(!application->_supportStop){
                        assert(application->_configuration.threadsNeeded != RIFF_THREADS_NEEDED_ALL);
                    }
                    msg.payload.sample.bandwidth += (msg.payload.sample.bandwidth / updatedSamples) * (numThreads - updatedSamples);
                }

                // If we collected only inconsistent samples, we mark latency and load as inconsistent.
                if(inconsistentSamples == updatedSamples){
                    msg.payload.sample.loadPercentage = RIFF_VALUE_INCONSISTENT;
                    msg.payload.sample.latency = RIFF_VALUE_INCONSISTENT;
                }else{
                    msg.payload.sample.loadPercentage /= (updatedSamples - inconsistentSamples);
                    msg.payload.sample.latency /= (updatedSamples - inconsistentSamples);
                }
            }else{
                // This can only happens if the threadsNeeded is NONE
                if(!application->_supportStop){
                    assert(application->_configuration.threadsNeeded == RIFF_THREADS_NEEDED_NONE);
                }
                msg.payload.sample.bandwidth = 0;
                msg.payload.sample.latency = RIFF_VALUE_NOT_AVAILABLE;
                msg.payload.sample.loadPercentage = 0;
                msg.payload.sample.numTasks = 0;
            }
            
            // Aggregate custom values.
            if(application->_aggregator){
                for(size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++){
                    msg.payload.sample.customFields[i] = application->_aggregator->aggregate(i, customVec[i]);
                }
            }

            DEBUG(msg.payload.sample);
            // Send message
            if(!application->_supportStop){
                application->_channelRef.send(&msg, sizeof(msg), 0);
            }
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

ulong Application::updateSamplingLength(unsigned long long numTasks, unsigned long long sampleTime){
    if(numTasks){
        double latencyNs = sampleTime / numTasks;
        double latencyMs = latencyNs / 1000000.0;
        // If samplingLength == 1 we would have one begin()-end() pair every latencyMs milliseconds
        if(latencyMs){
            return std::ceil((double) _configuration.samplingLengthMs / latencyMs);
        }else{
            return RIFF_DEFAULT_SAMPLING_LENGTH;
        }
    }else{
        throw std::runtime_error("updateSamplingLength called with no tasks stored. You probably called begin() twice in a row without calling end() after begin().");
    }
}

void Application::setConfiguration(const ApplicationConfiguration& configuration){
    _configuration = configuration;
}

void Application::setConfigurationStreaming(){
    ApplicationConfiguration configuration;
    configuration.threadsNeeded = RIFF_THREADS_NEEDED_NONE;
    configuration.adjustBandwidth = true;
    _configuration = configuration;
}

void Application::setConfigurationBatch(ThreadsNeeded threadsNeeded){
    ApplicationConfiguration configuration;
    configuration.threadsNeeded = threadsNeeded;
    configuration.adjustBandwidth = true;
    _configuration = configuration;
}

void Application::storeCustomValue(size_t index, double value, uint threadId){
    if(threadId > _threadData.size()){
        throw new std::runtime_error("Wrong threadId specified (greater than number of threads).");
    }
    if(index < RIFF_MAX_CUSTOM_FIELDS){
        ThreadData& tData = _threadData[threadId];
        tData.sample.customFields[index] = value;
    }else{
        throw std::runtime_error("Custom value index out of bound. Please "
                                 "increase RIFF_MAX_CUSTOM_FIELDS macro value.");
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
    int linger = 5000;
    _channel->setsockopt(NN_SOL_SOCKET, NN_LINGER, &linger, sizeof (linger));
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
