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


#include "core/cross/precompile.h"
#include "core/cross/counter_manager.h"
#include "core/cross/counter.h"

namespace o3d {

const InterfaceId CounterManager::kInterfaceId =
    InterfaceTraits<CounterManager>::kInterfaceId;

CounterManager::CounterManager(ServiceLocator* service_locator)
    : service_(service_locator, this),
      profiler_(service_locator) {
}

void CounterManager::RegisterSecondCounter(SecondCounter* counter) {
  DCHECK(std::find(second_counters_.begin(),
                   second_counters_.end(),
                   counter) == second_counters_.end());
  second_counters_.push_back(counter);
}

void CounterManager::UnregisterSecondCounter(SecondCounter* counter) {
  SecondCounterArray::iterator last = std::remove(second_counters_.begin(),
                                                  second_counters_.end(),
                                                  counter);
  DCHECK(last != second_counters_.end());
  second_counters_.erase(last, second_counters_.end());
}

void CounterManager::RegisterTickCounter(TickCounter* counter) {
  DCHECK(std::find(tick_counters_.begin(),
                   tick_counters_.end(),
                   counter) == tick_counters_.end());
  tick_counters_.push_back(counter);
}

void CounterManager::UnregisterTickCounter(TickCounter* counter) {
  TickCounterArray::iterator last = std::remove(tick_counters_.begin(),
                                                tick_counters_.end(),
                                                counter);
  DCHECK(last != tick_counters_.end());
  tick_counters_.erase(last, tick_counters_.end());
}

void CounterManager::RegisterRenderFrameCounter(RenderFrameCounter* counter) {
  DCHECK(std::find(render_frame_counters_.begin(),
                   render_frame_counters_.end(),
                   counter) == render_frame_counters_.end());
  render_frame_counters_.push_back(counter);
}

void CounterManager::UnregisterRenderFrameCounter(RenderFrameCounter* counter) {
  RenderFrameCounterArray::iterator last =
      std::remove(render_frame_counters_.begin(),
                  render_frame_counters_.end(),
                  counter);
  DCHECK(last != render_frame_counters_.end());
  render_frame_counters_.erase(last, render_frame_counters_.end());
}

void CounterManager::AdvanceCounters(float advance_amount,
                                     float seconds_elapsed) {
  Counter::CounterCallbackQueue queue;

  // Update any tick counters.
  for (unsigned ii = 0; ii < tick_counters_.size(); ++ii) {
    if (tick_counters_[ii]->running()) {
      tick_counters_[ii]->Advance(advance_amount, &queue);
    }
  }

  // Update any second counters.
  for (unsigned ii = 0; ii < second_counters_.size(); ++ii) {
    if (second_counters_[ii]->running()) {
      second_counters_[ii]->Advance(seconds_elapsed, &queue);
    }
  }

  profiler_->ProfileStart("Tick Counter callbacks");
  queue.CallCounterCallbacks();
  profiler_->ProfileStop("Tick Counter callbacks");
}

void CounterManager::AdvanceRenderFrameCounters(float advance_amnount) {
  Counter::CounterCallbackQueue queue;

  // Update any render frame counters.
  for (unsigned ii = 0; ii < render_frame_counters_.size(); ++ii) {
    if (render_frame_counters_[ii]->running()) {
      render_frame_counters_[ii]->Advance(advance_amnount, &queue);
    }
  }

  profiler_->ProfileStart("PrepareForFrame Counter callbacks");
  queue.CallCounterCallbacks();
  profiler_->ProfileStop("PrepareForFrame Counter callbacks");
}

void CounterManager::ClearAllCallbacks() {
  for (unsigned ii = 0; ii < tick_counters_.size(); ++ii) {
    tick_counters_[ii]->RemoveAllCallbacks();
  }
  for (unsigned ii = 0; ii < render_frame_counters_.size(); ++ii) {
    render_frame_counters_[ii]->RemoveAllCallbacks();
  }
  for (unsigned ii = 0; ii < second_counters_.size(); ++ii) {
    second_counters_[ii]->RemoveAllCallbacks();
  }
}

}  // namespace o3d
