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


// This file defines the TickEvent class.

#ifndef O3D_CORE_CROSS_TICK_EVENT_H_
#define O3D_CORE_CROSS_TICK_EVENT_H_

namespace o3d {

// This class is used to pass infomation to a registered ontick callback.
class TickEvent {
 public:
  TickEvent()
      : elapsed_time_(0.0f) {
  }

  // Use this function to get elapsed time since the last tick event in
  // seconds.
  float elapsed_time() const;

  // The ticker will use this function to set the elapsed time. You should
  // never call this function.
  void set_elapsed_time(float time);

 private:

  // This is the elapsed time in seconds since the last tick event.
  float elapsed_time_;
};

inline void TickEvent::set_elapsed_time(float time) {
  elapsed_time_ = time;
}

inline float TickEvent::elapsed_time() const {
  return elapsed_time_;
}

}  // namespace o3d

#endif  // O3D_CORE_CROSS_TICK_EVENT_H_
