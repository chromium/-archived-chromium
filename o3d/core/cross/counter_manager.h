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


#ifndef O3D_CORE_CROSS_COUNTER_MANAGER_H_
#define O3D_CORE_CROSS_COUNTER_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "core/cross/profiler.h"
#include "core/cross/service_dependency.h"
#include "core/cross/service_implementation.h"

namespace o3d {

class SecondCounter;
class TickCounter;
class RenderFrameCounter;

// Registry for the various kinds of Counter. Can register and unregister
// counters and advance all registered counters.
class CounterManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit CounterManager(ServiceLocator* service_locator);

  // Registers a second counter with the client. This is an internal function
  // only called by SecondCounter's constructor.
  // Parameters:
  //   counter: The SecondCounter to register.
  void RegisterSecondCounter(SecondCounter* counter);

  // Unregisters a second counter with the client. This is an internal function
  // only called by SecondCounter's destructor.
  // Parameters:
  //   counter: The SecondCounter to unregister.
  void UnregisterSecondCounter(SecondCounter* counter);

  // Registers a tick counter with the client. This is an internal function
  // only called by TickCounter's constructor.
  // Parameters:
  //   counter: The TickCounter to register.
  void RegisterTickCounter(TickCounter* counter);

  // Unregisters a tick counter with the client. This is an internal function
  // only called by TickCounter's destructor.
  // Parameters:
  //   counter: The TickCounter to unregister.
  void UnregisterTickCounter(TickCounter* counter);

  // Registers a render frame counter with the client. This is an internal
  // function only called by RenderFrameCounter's constructor.
  // Parameters:
  //   counter: The RenderFrameCounter to register.
  void RegisterRenderFrameCounter(RenderFrameCounter* counter);

  // Unregisters a render frame counter with the client. This is an internal
  // function only called by RenderFrameCounter's destructor.
  // Parameters:
  //   counter: The RenderFrameCounter to unregister.
  void UnregisterRenderFrameCounter(RenderFrameCounter* counter);

  // Advance the registered TickCounters by advance_amount and the
  // SecondCounters by seconds_elapsed, invoking their Advance functions
  // and any callbacks they enqueue as they run.
  void AdvanceCounters(float advance_amount, float seconds_elapsed);

  // Advance the RenderFrameCounters by the advance_amount, invoking their
  // Advance functions and any callbacks they enqueue.
  void AdvanceRenderFrameCounters(float advance_amount);

  // Clears the callbacks on all counters registered in the
  // CallbackManager. This needs to be called by Client::Cleanup
  // so that these callbacks do not get called after the page
  // has unloaded.
  void ClearAllCallbacks();

 private:
  ServiceImplementation<CounterManager> service_;
  ServiceDependency<Profiler> profiler_;

  // Render frame counters registed with this client.
  typedef std::vector<RenderFrameCounter*> RenderFrameCounterArray;
  RenderFrameCounterArray render_frame_counters_;

  // Second counters registed with this client.
  typedef std::vector<SecondCounter*> SecondCounterArray;
  SecondCounterArray second_counters_;

  // Tick counters registed with this client.
  typedef std::vector<TickCounter*> TickCounterArray;
  TickCounterArray tick_counters_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_COUNTER_MANAGER_H_
