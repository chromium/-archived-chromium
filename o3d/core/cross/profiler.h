/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_CORE_CROSS_PROFILER_H_
#define O3D_CORE_CROSS_PROFILER_H_

#include <string>

#include "core/cross/service_implementation.h"
#include "core/cross/timingtable.h"

namespace o3d {

class StructuredWriter;

// Provides support for profiling sections of code.
class Profiler {
 public:
  static const InterfaceId kInterfaceId;

  explicit Profiler(ServiceLocator* service_locator);

#ifdef PROFILE_CLIENT
  // Starts the timer ticking for the code range identified by key.
  inline void ProfileStart(const std::string& key) {
    timing_table_.Start(key);
  }

  // Stops the timer for the code range identified by key.
  inline void ProfileStop(const std::string& key) {
    timing_table_.Stop(key);
  }

  // Resets the profiler, clearing out all data.
  inline void ProfileReset() {
    timing_table_.Reset();
  }

  // Dumps all profiler state to a string.
  void Write(StructuredWriter* writer);

#else  // PROFILE_CLIENT
  inline void ProfileStart(const std::string& key) { }

  inline void ProfileStop(const std::string& key) { }

  inline void ProfileReset() { }

  inline void Write(StructuredWriter* writer) {
  }
#endif  // PROFILE_CLIENT

 private:
  ServiceImplementation<Profiler> service_;

#ifdef PROFILE_CLIENT
  TimingTable timing_table_;
#endif  // PROFILE_CLIENT

  DISALLOW_COPY_AND_ASSIGN(Profiler);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_PROFILER_H_
