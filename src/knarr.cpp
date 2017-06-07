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

Application::Application(const std::string& channelName, size_t numThreads,
                         Aggregator* aggregator):
	    _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
	    _started(false), _aggregator(aggregator){
    _chid = _channelRef.connect(channelName.c_str());
    assert(_chid >= 0);
    _threadData.resize(numThreads);
    pthread_mutex_init(&_mutex, NULL);
}

Application::Application(nn::socket& socket, uint chid, size_t numThreads,
                         Aggregator* aggregator):
        _channel(NULL), _channelRef(socket), _chid(chid), _started(false),
        _aggregator(aggregator){
    _threadData.resize(numThreads);
    pthread_mutex_init(&_mutex, NULL);
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

ulong Application::getCurrentTimeNs(){
    struct timespec tp;
    int r = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(!r);
    return tp.tv_sec * 1.0e9 + tp.tv_nsec;
}

void Application::begin(size_t threadId){
    ThreadData& tData = _threadData[threadId];
    ulong now = getCurrentTimeNs();
    if(!_started){
        pthread_mutex_lock(&_mutex);
        // This awful double check is done to avoid locking the flag
        // every time (this code is executed at most once).
        if(!_started){
            notifyStart();
            _started = true;
            tData.firstBegin = now;
        }
        pthread_mutex_unlock(&_mutex);

    }
    tData.lastEnd = now;
    if(tData.computeStart){
        if(tData.rcvStart){
            tData.idleTime += (now - tData.rcvStart);
        }else{
            tData.sample.latency += (now - tData.computeStart);
        }
        ++tData.sample.tasksCount;
        tData.computeStart = now;

        Message recvdMsg;
        pthread_mutex_lock(&_mutex);
        int res = _channelRef.recv(&recvdMsg, sizeof(recvdMsg), NN_DONTWAIT);
        if(res == sizeof(recvdMsg)){
            assert(recvdMsg.type == MESSAGE_TYPE_SAMPLE_REQ);
            // Prepare response message.
            Message msg;
            msg.type = MESSAGE_TYPE_SAMPLE_RES;
            msg.payload.sample = ApplicationSample(); // Set sample to all zeros

            // Add the samples of the other threads.
            for(size_t i = 0; i < _threadData.size(); i++){
                ApplicationSample& sample = _threadData[i].sample;
                ulong totalTime = (td.sample.latency + _threadData[i].idleTime);
                sample.bandwidthTotal = sample.tasksCount / totalTime;
                sample.loadPercentage = (sample.latency / totalTime) * 100.0;
                sample.latency /= sample.tasksCount;

                msg.payload.sample.bandwidthTotal += sample.bandwidthTotal;
                msg.payload.sample.latency += sample.latency;
                msg.payload.sample.loadPercentage += sample.loadPercentage;
                msg.payload.sample.tasksCount += sample.tasksCount;
            }
            msg.payload.sample.loadPercentage /= _threadData.size();
            msg.payload.sample.latency /= _threadData.size();
            // Aggregate custom values.
            if(_aggregator){
                std::vector<double> customVec;
                customVec.reserve(_threadData.size());
                for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
                    customVec.clear();
                    for(ThreadData td : _threadData){
                        customVec.push_back(td.sample.customFields[i]);
                    }
                    msg.payload.sample.customFields[i] = _aggregator->aggregate(i, customVec);
                }
            }
            DEBUG(msg.payload.sample);
            // Send message
            _channelRef.send(&msg, sizeof(msg), 0);
            // Reset fields
            tData.reset();
            // Tell other threads to clear their sample since current one have
            // been already sent.
            for(size_t i = 0; i < _threadData.size(); i++){
                if(i != threadId){
                    _threadData[i].clean = true;
                }
            }
        }else if(res == -1 && errno != EAGAIN){
            pthread_mutex_unlock(&_mutex);
            throw std::runtime_error("Unexpected error on recv");
        }else if(res != -1){
            pthread_mutex_unlock(&_mutex);
            throw std::runtime_error("Received less bytes than expected.");
        }
        pthread_mutex_unlock(&_mutex);

        // It may seem stupid to clean things after they have just been set,
        // but it is better to do not move earlier the following code.
        if(tData.clean){
            tData.clean = false;
            // Reset fields
            tData.reset();
        }

    }else{
        tData.computeStart = now;
    }
}

void Application::storeCustomValue(size_t index, double value, size_t threadId){
    if(index < KNARR_MAX_CUSTOM_FIELDS){
        _threadData[threadId].sample.customFields[index] = value;
    }else{
        throw std::runtime_error("Custom value index out of bound. Please "
                                 "increase KNARR_MAX_CUSTOM_FIELDS macro value.");
    }
}

void Application::end(size_t threadId){
    if(!_started){
        throw std::runtime_error("end() called without begin().");
    }
    _threadData[threadId].rcvStart = getCurrentTimeNs();
    if(_threadData[threadId].computeStart){
        double newLatency = (_threadData[threadId].rcvStart - 
                             _threadData[threadId].computeStart);
        _threadData[threadId].sample.latency += newLatency;
    }
    _threadData[threadId].lastEnd = _threadData[threadId].rcvStart;
}

void Application::terminate(){
    Message msg;
    msg.type = MESSAGE_TYPE_STOP;
    uint lastEnd = 0, firstBegin = std::numeric_limits<uint>::max();
    for(ThreadData td : _threadData){
        if(td.firstBegin < firstBegin){
            firstBegin = td.firstBegin;
        }
        if(td.lastEnd > lastEnd){
            lastEnd = td.lastEnd;
        }
    }
    msg.payload.time = (lastEnd - firstBegin) / 1000000; // Must be in ms
    int r = _channelRef.send(&msg, sizeof(msg), 0);
    assert (r == sizeof(msg));
    // Wait for ack before leaving (otherwise if object is destroyed
    // the monitor could never receive the stop).
    r = _channelRef.recv(&msg, sizeof(msg), 0);
    assert(r == sizeof(msg));
}

Monitor::Monitor(const std::string& channelName):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _executionTime(0){
    _chid = _channelRef.bind(channelName.c_str());
    assert(_chid >= 0);
}

Monitor::Monitor(nn::socket& socket, uint chid):
        _channel(NULL), _channelRef(socket), _chid(chid), _executionTime(0){
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

bool Monitor::getSample(ApplicationSample& sample){
    Message m;
    m.type = MESSAGE_TYPE_SAMPLE_REQ;
    int r = _channelRef.send(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    r = _channelRef.recv(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    if(m.type == MESSAGE_TYPE_SAMPLE_RES){
        sample = m.payload.sample;
        return true;
    }else if(m.type == MESSAGE_TYPE_STOP){
        _executionTime = m.payload.time;
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

} // End namespace
