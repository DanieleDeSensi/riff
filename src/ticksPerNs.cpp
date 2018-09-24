/*
 * This file is part of riff
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */
#include <assert.h>
#include <riff/getticks.h>
#include <sched.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <cmath>
#include <iostream>
#include <sstream>

unsigned long getNanoSeconds() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return spec.tv_sec * 1000000000 + spec.tv_nsec;
}

double getTicksPerNanosec() {
  double x = 0.691812048120;

  unsigned long start = getNanoSeconds();
  uint64_t t1 = getticks();
  while (getticks() - t1 < 1000000) {
    x = std::sin(x);
  }
  uint64_t t2 = getticks();
  unsigned long end = getNanoSeconds();

  std::ostringstream sstream;
  sstream << x;
  FILE* p =
      popen(std::string("echo " + sstream.str() + " >/dev/null").c_str(), "r");
  pclose(p);

  return ((double)(t2 - t1) / (double)(end - start));
}

int main(int argc, char** argv) {
#ifndef __linux__
#error "This only works on Linux"
#endif
  // Pin to a core because TSC may not be coherent between different cores.
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  assert(!sched_setaffinity(0, sizeof(mask), &mask));
  setpriority(PRIO_PROCESS, 0, -20);

  uint numIterations = 1000;
  double x = 0;
  for (size_t i = 0; i < numIterations; i++) {
    x += getTicksPerNanosec();
  }
  std::cout << x / (double)numIterations << std::endl;
}
