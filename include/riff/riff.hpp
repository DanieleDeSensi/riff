/*
 * This file is part of riff
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#ifndef RIFF_HPP_
#define RIFF_HPP_

#include <riff/archdata.hpp>
#include <riff/external/cppnanomsg/nn.hpp>
#include <riff/external/nanomsg/src/pair.h>

#include <pthread.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#define RIFF_MAX_CUSTOM_FIELDS 4

#ifndef RIFF_DEFAULT_SAMPLING_LENGTH
// Never skips any begin() call.
#define RIFF_DEFAULT_SAMPLING_LENGTH 1
#endif

namespace riff {

// Configuration parameters for riff behaviour
// when collecting samples.
typedef struct ApplicationConfiguration {
  // Represents the minimum length in milliseconds
  // between two successive begin() calls. If begin()
  // is called more frequently than this value,
  // intermediate calls are skipped. If this value
  // is set to zero, no calls will be skipped.
  // [default = 10.0]
  double samplingLengthMs;

  // If true, for the threads that didn't yet stored
  // their sample, we estimate the throughput to be
  // the same of the other threads. This should be set to
  // true if you want to provide a consistent view
  // of the application throughput. If false,
  // throughput will change accordingly to how many threads
  // already stored their samples and we would have
  // fluctuations caused by the way in which data is collected
  // but not actually present in the application.
  // [default = true]
  bool adjustThroughput;

  // When sampling is applied, the latency estimation
  // (and the idle time/utilization estimation) could be
  // wrong. This means that we could pick latency sample which are
  // far from the average (lower/higher), thus since we assume that
  // latency is more or less constant, in very skewed situations
  // our estimation could be completly wrong. This can
  // be detected by riff by comparing the actual elapsed time
  // with the time computed as the sum of the latency and idle time.
  // When these values are different, it means that either the
  // latency or the idle time have been not correctly estimated
  // (due to skewness). This can only happen when sampling is applied.
  // The following macro represents the maximum percentage of difference
  // between the estimated time and the actual time we are willing to tolerate.
  // If the estimated time (computed from latency and idle time)
  // is more than consistencyThreshold% different from the
  // actual time, we will mark latency and utilization as
  // inconsistent. Throughput computation and tasks count are never
  // affected by inconsistencies.
  // [default = 5.0]
  double consistencyThreshold;

  ApplicationConfiguration() {
    samplingLengthMs = 10.0;
    adjustThroughput = true;
    consistencyThreshold = 5.0;
  }
} ApplicationConfiguration;

unsigned long long getCurrentTimeNs();

typedef enum MessageType {
  MESSAGE_TYPE_START = 0,
  MESSAGE_TYPE_SAMPLE_REQ,
  MESSAGE_TYPE_SAMPLE_RES,
  MESSAGE_TYPE_STOP,
  MESSAGE_TYPE_STOPACK
} MessageType;

/*!
 * \struct ApplicationSample
 * \brief Represents a sample of values taken from an application.
 *
 * This struct represents a sample of values taken from an adaptive node.
 */
typedef struct ApplicationSample {
  // If true, the latency and loadPercentage computations are not reliable.
  // If you need reliable values for latency and loadPercentage, please set
  // samplingLengthMs to 0 in riff configuration.
  bool inconsistent;

  // The percentage ([0, 100]) of time that the node spent in the computation.
  double loadPercentage;

  // The throughput of the application.
  double throughput;

  // The average latency (nanoseconds).
  double latency;

  // The number of computed tasks.
  double numTasks;

  // Custom user fields.
  double customFields[RIFF_MAX_CUSTOM_FIELDS];

  ApplicationSample()
      : inconsistent(false),
        loadPercentage(0),
        throughput(0),
        latency(0),
        numTasks(0) {
    // We do not use memset due to cppcheck warnings.
    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] = 0;
    }
  }

  ApplicationSample(ApplicationSample const& sample)
      : inconsistent(sample.inconsistent),
        loadPercentage(sample.loadPercentage),
        throughput(sample.throughput),
        latency(sample.latency),
        numTasks(sample.numTasks) {
    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] = sample.customFields[i];
    }
  }

  void swap(ApplicationSample& x) {
    using std::swap;

    swap(inconsistent, x.inconsistent);
    swap(loadPercentage, x.loadPercentage);
    swap(throughput, x.throughput);
    swap(latency, x.latency);
    swap(numTasks, x.numTasks);
    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      swap(customFields[i], x.customFields[i]);
    }
  }

  ApplicationSample& operator=(ApplicationSample rhs) {
    swap(rhs);
    return *this;
  }

  ApplicationSample& operator+=(const ApplicationSample& rhs) {
    if (rhs.inconsistent) {
      inconsistent = rhs.inconsistent;
    }

    loadPercentage += rhs.loadPercentage;
    throughput += rhs.throughput;
    latency += rhs.latency;
    numTasks += rhs.numTasks;

    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] += rhs.customFields[i];
    }
    return *this;
  }

  ApplicationSample& operator-=(const ApplicationSample& rhs) {
    if (rhs.inconsistent) {
      inconsistent = rhs.inconsistent;
    }

    loadPercentage -= rhs.loadPercentage;
    throughput -= rhs.throughput;
    latency -= rhs.latency;
    numTasks -= rhs.numTasks;

    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] -= rhs.customFields[i];
    }
    return *this;
  }

  ApplicationSample& operator*=(const ApplicationSample& rhs) {
    if (rhs.inconsistent) {
      inconsistent = rhs.inconsistent;
    }

    loadPercentage *= rhs.loadPercentage;
    throughput *= rhs.throughput;
    latency *= rhs.latency;
    numTasks *= rhs.numTasks;

    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] *= rhs.customFields[i];
    }
    return *this;
  }

  ApplicationSample& operator/=(const ApplicationSample& rhs) {
    if (rhs.inconsistent) {
      inconsistent = rhs.inconsistent;
    }

    loadPercentage /= rhs.loadPercentage;
    throughput /= rhs.throughput;
    latency /= rhs.latency;
    numTasks /= rhs.numTasks;

    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] /= rhs.customFields[i];
    }
    return *this;
  }

  ApplicationSample operator/=(double x) {
    loadPercentage /= x;
    throughput /= x;
    latency /= x;
    numTasks /= x;
    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] /= x;
    }
    return *this;
  }

  ApplicationSample operator*=(double x) {
    loadPercentage *= x;
    throughput *= x;
    latency *= x;
    numTasks *= x;
    for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
      customFields[i] *= x;
    }
    return *this;
  }
} ApplicationSample;

inline ApplicationSample operator+(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs) {
  ApplicationSample r = lhs;
  r += rhs;
  return r;
}

inline ApplicationSample operator-(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs) {
  ApplicationSample r = lhs;
  r -= rhs;
  return r;
}

inline ApplicationSample operator*(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs) {
  ApplicationSample r = lhs;
  r *= rhs;
  return r;
}

inline ApplicationSample operator*(const ApplicationSample& lhs, double x) {
  ApplicationSample r = lhs;
  r *= x;
  return r;
}

inline ApplicationSample operator/(const ApplicationSample& lhs,
                                   const ApplicationSample& rhs) {
  ApplicationSample r = lhs;
  r /= rhs;
  return r;
}

inline ApplicationSample operator/(const ApplicationSample& lhs, double x) {
  ApplicationSample r = lhs;
  r /= x;
  return r;
}

inline std::ostream& operator<<(std::ostream& os,
                                const ApplicationSample& obj) {
  os << "[";
  os << "Inconsistent: " << obj.inconsistent << " ";
  os << "Load: " << obj.loadPercentage << " ";
  os << "Throughput: " << obj.throughput << " ";
  os << "Latency: " << obj.latency << " ";
  os << "NumTasks: " << obj.numTasks << " ";
  for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
    os << "CustomField" << i << ": " << obj.customFields[i] << " ";
  }
  os << "]";
  return os;
}

inline std::istream& operator>>(std::istream& is, ApplicationSample& sample) {
  is.ignore(std::numeric_limits<std::streamsize>::max(), '[');
  is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
  is >> sample.inconsistent;
  is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
  is >> sample.loadPercentage;
  is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
  is >> sample.throughput;
  is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
  is >> sample.latency;
  is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
  is >> sample.numTasks;
  for (size_t i = 0; i < RIFF_MAX_CUSTOM_FIELDS; i++) {
    is.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    is >> sample.customFields[i];
  }
  is.ignore(std::numeric_limits<std::streamsize>::max(), ']');
  return is;
}

typedef union Payload {
  pid_t pid;
  ApplicationSample sample;
  struct {
    ulong time;
    unsigned long long totalTasks;
  } summary;
  Payload() { ; }
} Payload;

typedef struct Message {
  MessageType type;
  Payload payload;
  unsigned int phaseId;
  unsigned int totalThreads;
} Message;

class Aggregator {
 public:
  virtual ~Aggregator() { ; }

  /**
   * When using custom values, if storeCustomValue is called by multiple
   * threads, this function implements the aggregation between values stored
   * by different threads.
   * This function is called by at most one thread.
   * @param index The index of the custom value.
   * @param customValues The values stored by the threads.
   */
  virtual double aggregate(size_t index,
                           const std::vector<double>& customValues) = 0;
};

typedef struct ThreadData {
  ApplicationSample sample __attribute__((aligned(LEVEL1_DCACHE_LINESIZE)));
  ApplicationSample consolidatedSample;
  unsigned long long rcvStart;
  unsigned long long computeStart;
  unsigned long long idleTime;
  unsigned long long firstBegin;
  unsigned long long lastEnd;
  unsigned long long sampleStartTime;
  unsigned long long totalTasks;
  std::atomic<bool>* consolidate;
  ulong samplingLength;
  ulong currentSample;
  char padding[LEVEL1_DCACHE_LINESIZE];

  ThreadData()
      : rcvStart(0),
        computeStart(0),
        idleTime(0),
        firstBegin(0),
        lastEnd(0),
        sampleStartTime(0),
        totalTasks(0),
        samplingLength(RIFF_DEFAULT_SAMPLING_LENGTH),
        currentSample(0) {
    memset(&padding, 0, sizeof(padding));
    consolidate = new std::atomic<bool>(false);
  }

  ThreadData(ThreadData const&) = delete;
  ThreadData& operator=(ThreadData const&) = delete;
} ThreadData;

void* applicationSupportThread(void*);

class Application {
  friend void waitSampleStore(Application* application);
  friend bool keepWaitingSample(Application* application, size_t threadId,
                                size_t updatedSamples);
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
  std::vector<ThreadData>* _threadData;
  ulong _executionTime;
  unsigned long long _totalTasks;
  unsigned int _phaseId;
  unsigned int _totalThreads;
  bool _inconsistentSample;

  // We are sure it is called by at most one thread.
  void notifyStart();

  ulong updateSamplingLength(unsigned long long numTasks,
                             unsigned long long sampleTime);

  // We do not use abs because they are both unsigned
  // if we do abs(x - y) and x is smaller than y, the temporary
  // result (before applying abs) cannot be negative so it will wrap
  // and assume a huge value.
  static inline unsigned long long absDiff(unsigned long long x,
                                           unsigned long long y) {
    if (x > y) {
      return x - y;
    } else {
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
  Application(nn::socket& socket, unsigned int chid, size_t numThreads = 1,
              Aggregator* aggregator = NULL);

  ~Application();

  Application(const Application& a) = delete;
  Application& operator=(Application const& x) = delete;

  /**
   * Sets the application configuration.
   * MUST be called before calling begin() for the first time.
   * @param configuration The application configuration.
   **/
  void setConfiguration(const ApplicationConfiguration& configuration);

  /**
   * This function must be called at each loop iteration when the computation
   * part of the loop begins.
   * @param threadId Must be specified when is called by multiple threads
   *        (e.g. inside a parallel loop). It must be a number univocally
   *        identifying the thread calling this function and in
   *        the range [0, n[, where n is the number of threads specified
   *        in the constructor.
   */
  inline void begin(unsigned int threadId = 0) {
    ThreadData& tData = _threadData->at(threadId);

    // Equivalent to
    // tData.currentSample = (tData.currentSample + 1) % tData.samplingLength;
    // but faster.
    tData.currentSample = (tData.currentSample + 1) >= tData.samplingLength
                              ? 0
                              : tData.currentSample + 1;

    // Skip
    if (tData.currentSample > 1) {
      return;
    }

    /********* Only executed once (at startup). - BEGIN *********/
    if (!_started) {
      pthread_mutex_lock(&_mutex);
      // This awful double check is done to avoid locking the flag
      // every time (this code is executed at most once).
      if (!_started) {
        notifyStart();
        _started = true;
      }
      pthread_mutex_unlock(&_mutex);
    }
    unsigned long long now = getCurrentTimeNs();
    if (!tData.firstBegin) {
      tData.firstBegin = now;
    }
    if (!tData.sampleStartTime) {
      tData.sampleStartTime = now;
    }
    /********* Only executed once (at startup). - END *********/

    if (tData.computeStart) {
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
      if (tData.currentSample == 1 || tData.samplingLength == 1) {
        tData.idleTime += ((now - tData.rcvStart) * tData.samplingLength);
        unsigned long long sampleTime = now - tData.sampleStartTime;
        unsigned long long sampleTimeEstimated =
            (tData.sample.latency + tData.idleTime);
        ulong oldSamplingLength = tData.samplingLength,
              newSamplingLength = tData.samplingLength;

        tData.sample.throughput =
            tData.sample.numTasks /
            (sampleTime / 1000000000.0);  // From tasks/ns to tasks/sec
        tData.sample.loadPercentage =
            (tData.sample.latency / sampleTime) * 100.0;

        if (_configuration.samplingLengthMs) {
          newSamplingLength =
              updateSamplingLength(tData.sample.numTasks, sampleTime);
          /*
          We commented this since it could impair too much the reactiveness of
          the adaptive sampling.
          if(newSamplingLength > 10*oldSamplingLength){
              // To avoid setting too quickly a too long sampling length.
              newSamplingLength = 10*oldSamplingLength;
          }
          */
        }

        if (*tData.consolidate) {
          tData.consolidatedSample = tData.sample;
          // Consistency check
          // If the gap between real total time and the one estimated with
          // latency and idle time is greater than a threshold, idleTime and
          // latency are not reliable.
          if (((absDiff(sampleTime, sampleTimeEstimated) / (double)sampleTime) *
               100.0) > _configuration.consistencyThreshold) {
            if (!_configuration.samplingLengthMs) {
#if defined RIFF_DEFAULT_SAMPLING_LENGTH && RIFF_DEFAULT_SAMPLING_LENGTH == 1
              // If not adaptive sampling and if sampling length == 1
              throw std::runtime_error(
                  "FATAL ERROR: it is not possible to have inconsistency if "
                  "sampling is not applied.");
#endif
            }
            tData.consolidatedSample.inconsistent = true;
          }
          tData.sample = ApplicationSample();
          tData.idleTime = 0;
          tData.sampleStartTime = now;
          *(tData.consolidate) = false;
        }

        tData.samplingLength = newSamplingLength;

        // We need to manage the corner case where sample was one and
        // now is greater than one. In this case currentSample is 0
        // and end() would be executed on the new sample length.
        // We need than to force currentSample to 1 to let the
        // counting work.
        if (oldSamplingLength == 1 && tData.samplingLength > 1) {
          tData.currentSample = 1;
        }
        // If I reduce the samplingLength to 1, the only
        // possible value for currentSample would be 0,
        // so we set it to 0.
        if (oldSamplingLength > 1 && tData.samplingLength == 1) {
          tData.currentSample = 0;
        }
      }
    }
    tData.computeStart = now;
  }

  /**
   * This function stores a custom value in the sample. It should be called
   * after 'end()'.
   * @param index The index of the value [0, RIFF_MAX_CUSTOM_FIELDS[
   * @param value The value.
   * @param threadId Must be specified when is called by multiple threads
   *        (e.g. inside a parallel loop). It must be a number univocally
   *        identifying the thread calling this function and in
   *        the range [0, n[, where n is the number of threads specified
   *        in the constructor.
   */
  void storeCustomValue(size_t index, double value, unsigned int threadId = 0);

  /**
   * This function must be called at each loop iteration when the computation
   * part of the loop ends.
   * @param threadId Must be specified when is called by multiple threads
   *        (e.g. inside a parallel loop). It must be a number univocally
   *        identifying the thread calling this function and in
   *        the range [0, n[, where n is the number of threads specified
   *        in the constructor.
   */
  inline void end(unsigned int threadId = 0) {
    ThreadData& tData = _threadData->at(threadId);
    // Skip
    if (tData.currentSample) {
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
    tData.lastEnd = now;
  }

  /**
   * Sets the number of threads contributing to this phase
   * @param totalThreads The number of threads contributing to this phase.
   * ATTENTION: This may be different from the number of threads you specified
   * in the constructor. Indeed, you may have one thread calling the
   * begin()/end() calls but more threads contributing to the computation.
   * Consider for example this case:
   *
   * |---------------------------------------------|
   * | for(uint i = 0; i < 100; i++){              |
   * |     instr.begin();                          |
   * |     #pragma omp parallel for num_threads(4) |
   * |     for(uint j = 0; j < 100; j++){          |
   * |         // ...compute...                    |
   * |     }                                       |
   * |     instr.end();                            |
   * | }                                           |
   * |---------------------------------------------|
   *
   * In this case, begin() and end() are called by one thread only (so you
   * specify 1 in the constructor). However, the computation is executed
   * by 4 threads, so you should specify 4 as second argument of the
   * setPhaseId(...) call.
   */
  void setTotalThreads(unsigned int totalThreads);

  /**
   * Notify the start of a new phase.
   * @param phaseId A unique identifier for the phase.
   * @param totalThreads The number of threads contributing to this phase
   * (see setTotalThreads documentation).
   */
  void setPhaseId(unsigned int phaseId, unsigned int totalThreads = 0);

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

  /**
   * Sets all the subsequent samples as inconsistent (i.e. latency and
   * loadPercentage may be erroneous.
   * For example, consider an application composed by two pipelined
   * threads, i.e. a sender (S) and a receiver (R).
   * If we instrument only the receiver R, we would have a correct
   * throughput measurement but an inconsistency latency and loadPercentage
   * measurement. Indeed, to correctly measure latency we would need
   * to instrument both S and R, but this would require storing individual
   * latencies for each message sent from S to R. This is not possible at
   * the moment. So we provide the possibility to notify this situation
   * by explicitly marking the latency and loadPercentage as inconsistent.
   **/
  void markInconsistentSamples();
};

class Monitor {
 private:
  nn::socket* _channel;
  nn::socket& _channelRef;
  int _chid;
  ulong _executionTime;
  unsigned long long _totalTasks;
  unsigned int _lastPhaseId;
  unsigned int _lastTotalThreads;

 public:
  /**
   * Creates a monitor.
   * @param channelName The name of the channel used
   * to communicate with the application.
   **/
  explicit Monitor(const std::string& channelName);

  /**
   * Creates a monitor starting from an already existing
   * nanomsg socket.
   * bind() must already have bee called on the socket.
   * @param socket The nanomsg socket. bind() must already have
   * been called on it.
   * @param chid The channel identifier.
   **/
  Monitor(nn::socket& socket, unsigned int chid);

  ~Monitor();

  Monitor(const Monitor& m) = delete;
  Monitor& operator=(Monitor const& x) = delete;

  /**
   * Waits for an application to start.
   * @return The pid (process identifier) of the monitored application.
   **/
  pid_t waitStart();

  /**
   * Returns the current sample.
   * @param sample The returned sample.
   * @return True if the sample has been succesfully stored,
   * False if the application terminated and thus there are
   * no samples to be stored.
   **/
  bool getSample(ApplicationSample& sample);

  /**
   * Gets the identifier of the last recorded phase.
   * @return The identifier of the last recorded phase.
   */
  unsigned int getPhaseId() const;

  /**
   * Gets the number of total threads executing a parallel phase.
   * @return The number of total threads executing a parallel phase.
   * If 0, this number is not known.
   */
  unsigned int getTotalThreads() const;

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

}  // namespace riff

#endif
