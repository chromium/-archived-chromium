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


// This file contains the definitions of Counter related classes

#include "core/cross/precompile.h"
#include "core/cross/counter.h"
#include "core/cross/counter_manager.h"

namespace o3d {

O3D_DEFN_CLASS(Counter, ParamObject);
O3D_DEFN_CLASS(SecondCounter, Counter);
O3D_DEFN_CLASS(RenderFrameCounter, Counter);
O3D_DEFN_CLASS(TickCounter, Counter);

const char* Counter::kRunningParamName =
    O3D_STRING_CONSTANT("running");
const char* Counter::kForwardParamName =
    O3D_STRING_CONSTANT("forward");
const char* Counter::kStartParamName =
    O3D_STRING_CONSTANT("start");
const char* Counter::kEndParamName =
    O3D_STRING_CONSTANT("end");
const char* Counter::kCountModeParamName =
    O3D_STRING_CONSTANT("countMode");
const char* Counter::kCountParamName =
    O3D_STRING_CONSTANT("count");
const char* Counter::kMultiplierParamName =
    O3D_STRING_CONSTANT("multiplier");

Counter::Counter(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      next_callback_valid_(false),
      prev_callback_valid_(false),
      last_call_callbacks_end_count_(0.0f) {
  RegisterParamRef(kRunningParamName, &running_param_);
  RegisterParamRef(kForwardParamName, &forward_param_);
  RegisterParamRef(kCountModeParamName, &count_mode_param_);
  RegisterParamRef(kStartParamName, &start_param_);
  RegisterParamRef(kEndParamName, &end_param_);
  RegisterParamRef(kCountParamName, &count_param_);
  RegisterParamRef(kMultiplierParamName, &multiplier_param_);
  set_multiplier(1.0f);
  set_forward(true);
  set_running(true);
  set_count_mode(Counter::CONTINUOUS);
}

Counter::~Counter() {
  // We have to call this manually to make sure all the callback managers
  // unregister themselves before callback_managers_ is destoryed.
  callbacks_.clear();
}

void Counter::Reset() {
  SetCount(forward() ? start() : end());
}

void Counter::SetCount(float value) {
  set_count(value);
  next_callback_valid_ = false;
  prev_callback_valid_ = false;
}

void Counter::Advance(float advance_amount, CounterCallbackQueue* queue) {
  DCHECK(queue != NULL);

  float old_count = count_param_->value();

  // Update the count.
  if (count_param_->input_connection() != NULL) {
    float new_count = count_param_->value();
    CallCallbacks(old_count, new_count, queue);
  } else {
    bool direction = forward();
    float start_count = start();
    float end_count = end();
    float delta = (direction ? advance_amount : -advance_amount) * multiplier();
    float period = end_count - start_count;

    CountMode mode = count_mode();
    if (period >= 0.0f) {
      // end > start
      float new_count = old_count + delta;
      if (delta >= 0.0f) {
        switch (mode) {
          case Counter::ONCE: {
            if (new_count >= end_count) {
              new_count = end_count;
              set_running(false);
            }
            break;
          }
          case Counter::CYCLE: {
            while (new_count >= end_count) {
              CallCallbacks(old_count, end_count, queue);
              if (period == 0.0f) {
                break;
              }
              old_count = start_count;
              new_count -= period;
            }
            break;
          }
          case Counter::OSCILLATE: {
            while (delta > 0.0f) {
              new_count = old_count + delta;
              if (new_count < end_count) {
                break;
              }
              CallCallbacks(old_count, end_count, queue);
              direction = !direction;
              float amount = end_count - old_count;
              delta -= amount;
              old_count = end_count;
              new_count = end_count;
              if (delta <= 0.0f || period == 0.0f) {
                break;
              }
              new_count -= delta;
              if (new_count > start_count) {
                break;
              }
              CallCallbacks(old_count, start_count, queue);
              direction = !direction;
              amount = old_count - start_count;
              delta -= amount;
              old_count = start_count;
              new_count = start_count;
            }
            set_forward(direction);
            break;
          }
          case Counter::CONTINUOUS:
          default:
            break;
        }
        CallCallbacks(old_count, new_count, queue);
        set_count(new_count);
      } else if (delta < 0.0f) {
        switch (mode) {
          case Counter::ONCE: {
            if (new_count <= start_count) {
              new_count = start_count;
              set_running(false);
            }
            break;
          }
          case Counter::CYCLE: {
            while (new_count <= start_count) {
              CallCallbacks(old_count, start_count, queue);
              if (period == 0.0f) {
                break;
              }
              old_count = end_count;
              new_count += period;
            }
            break;
          }
          case Counter::OSCILLATE: {
            while (delta < 0.0f) {
              new_count = old_count + delta;
              if (new_count > start_count) {
                break;
              }
              CallCallbacks(old_count, start_count, queue);
              direction = !direction;
              float amount = old_count - start_count;
              delta += amount;
              old_count = start_count;
              new_count = start_count;
              if (delta >= 0.0f || period == 0.0f) {
                break;
              }
              new_count -= delta;
              if (new_count < end_count) {
                break;
              }
              CallCallbacks(old_count, end_count, queue);
              direction = !direction;
              amount = end_count - old_count;
              delta += amount;
              old_count = end_count;
              new_count = end_count;
            }
            set_forward(direction);
            break;
          }
          case Counter::CONTINUOUS:
          default:
            break;
        }
        CallCallbacks(old_count, new_count, queue);
        set_count(new_count);
      }
    } else if (period < 0.0f) {
      // start > end
      period = -period;
      float new_count = old_count - delta;
      if (delta > 0.0f) {
        switch (mode) {
          case Counter::ONCE: {
            if (new_count <= end_count) {
              new_count = end_count;
              set_running(false);
            }
            break;
          }
          case Counter::CYCLE: {
            while (new_count <= end_count) {
              CallCallbacks(old_count, end_count, queue);
              old_count = start_count;
              new_count += period;
            }
            break;
          }
          case Counter::OSCILLATE: {
            while (delta > 0.0f) {
              new_count = old_count - delta;
              if (new_count > end_count) {
                break;
              }
              CallCallbacks(old_count, end_count, queue);
              direction = !direction;
              float amount = old_count - end_count;
              delta -= amount;
              old_count = end_count;
              new_count = end_count;
              if (delta <= 0.0f) {
                break;
              }
              new_count += delta;
              if (new_count < start_count) {
                break;
              }
              CallCallbacks(old_count, start_count, queue);
              direction = !direction;
              amount = start_count - old_count;
              delta -= amount;
              old_count = start_count;
              new_count = start_count;
            }
            set_forward(direction);
            break;
          }
          case Counter::CONTINUOUS:
          default:
            break;
        }
        CallCallbacks(old_count, new_count, queue);
        set_count(new_count);
      } else if (delta < 0.0f) {
        switch (mode) {
          case Counter::ONCE: {
            if (new_count >= start_count) {
              new_count = start_count;
              set_running(false);
            }
            break;
          }
          case Counter::CYCLE: {
            while (new_count >= start_count) {
              CallCallbacks(old_count, start_count, queue);
              old_count = end_count;
              new_count -= period;
            }
            break;
          }
          case Counter::OSCILLATE: {
            while (delta < 0.0f) {
              new_count = old_count - delta;
              if (new_count < start_count) {
                break;
              }
              CallCallbacks(old_count, start_count, queue);
              direction = !direction;
              float amount = start_count - old_count;
              delta += amount;
              old_count = start_count;
              new_count = start_count;
              if (delta >= 0.0f) {
                break;
              }
              new_count += delta;
              if (new_count > end_count) {
                break;
              }
              CallCallbacks(old_count, end_count, queue);
              direction = !direction;
              amount = old_count - end_count;
              delta += amount;
              old_count = end_count;
              new_count = end_count;
            }
            set_forward(direction);
            break;
          }
          case Counter::CONTINUOUS:
          default:
            break;
        }
        CallCallbacks(old_count, new_count, queue);
        set_count(new_count);
      }
    }
  }
}

void Counter::CallCallbacks(float start_count,
                            float end_count,
                            CounterCallbackQueue* queue) {
  DCHECK(queue != NULL);

  if (end_count > start_count) {
    // Going forward.
    // If next_callback is not valid, find the first possible callback.
    if (!next_callback_valid_ ||
        start_count != last_call_callbacks_end_count_) {
      next_callback_ = callbacks_.begin();
      while (next_callback_ != callbacks_.end() &&
             next_callback_->count() < start_count) {
        ++next_callback_;
      }
    }

    // add callbacks until we get to some callback past end_count.
    while (next_callback_ != callbacks_.end()) {
      if (next_callback_->count() > end_count) {
        break;
      }
      queue->QueueCounterCallback(next_callback_->callback_manager());
      ++next_callback_;
    }
    next_callback_valid_ = true;
    prev_callback_valid_ = false;
    last_call_callbacks_end_count_ = end_count;
  } else if (end_count < start_count) {
    // Going backward.
    // If prev_callback is not valid, find the first possible callback.
    if (!prev_callback_valid_ ||
        start_count != last_call_callbacks_end_count_) {
      prev_callback_ = callbacks_.rbegin();
      while (prev_callback_ != callbacks_.rend() &&
             prev_callback_->count() > start_count) {
        ++prev_callback_;
      }
    }

    // add callbacks until we get to some callback past end_count.
    while (prev_callback_ != callbacks_.rend()) {
      if (prev_callback_->count() < end_count) {
        break;
      }
      queue->QueueCounterCallback(prev_callback_->callback_manager());
      ++prev_callback_;
    }

    prev_callback_valid_ = true;
    next_callback_valid_ = false;
    last_call_callbacks_end_count_ = end_count;
  }
}

void Counter::AddCallback(float count, CounterCallback* callback) {
  next_callback_valid_ = false;
  prev_callback_valid_ = false;
  CounterCallbackManager* manager;

  {
    CallbackManagerMap::iterator iter = callback_managers_.find(callback);
    if (iter != callback_managers_.end()) {
      manager = iter->second;
    } else {
      manager = new CounterCallbackManager(this, callback);
    }
  }

  {
    CounterCallbackInfoArray::iterator end(callbacks_.end());
    CounterCallbackInfoArray::iterator iter(callbacks_.begin());
    while (iter != end) {
      if (iter->count() == count) {
        iter->set_callback_manager(manager);
        return;
      } else if (iter->count() > count) {
        break;
      }
      ++iter;
    }
    callbacks_.insert(iter, CounterCallbackInfo(count, manager));
  }
}

bool Counter::RemoveCallback(float count) {
  CounterCallbackInfoArray::iterator end(callbacks_.end());
  for (CounterCallbackInfoArray::iterator iter(callbacks_.begin());
       iter != end;
       ++iter) {
    if (iter->count() == count) {
      next_callback_valid_ = false;
      prev_callback_valid_ = false;
      callbacks_.erase(iter);
      return true;
    }
  }
  return false;
}

void Counter::RemoveAllCallbacks() {
  callbacks_.clear();
  next_callback_valid_ = false;
  prev_callback_valid_ = false;
}

void Counter::RegisterCallbackManager(
    Counter::CounterCallbackManager* manager) {
  DCHECK(callback_managers_.find(manager->callback()) ==
         callback_managers_.end());
  callback_managers_.insert(std::make_pair(manager->callback(), manager));
}

void Counter::UnregisterCallbackManager(
    Counter::CounterCallbackManager* manager) {
  CallbackManagerMap::iterator iter = callback_managers_.find(
      manager->callback());
  DCHECK(iter != callback_managers_.end());
  callback_managers_.erase(iter);
}

ObjectBase::Ref Counter::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Counter(service_locator));
}

void Counter::CounterCallbackQueue::CallCounterCallbacks() {
  // call all the queued callbacks.
  for (unsigned ii = 0; ii < counter_callbacks_.size(); ++ii) {
    counter_callbacks_[ii]->Run();
  }
  counter_callbacks_.clear();
}

void Counter::CounterCallbackQueue::QueueCounterCallback(
    CounterCallbackManager* callback_manager) {
  counter_callbacks_.push_back(CounterCallbackManager::Ref(callback_manager));
}

Counter::CounterCallbackManager::CounterCallbackManager(Counter* counter,
                                                        ClosureType* closure)
    : counter_(counter),
      closure_(closure),
      called_(false) {
  DCHECK(counter != NULL);
  DCHECK(closure != NULL);
  counter_->RegisterCallbackManager(this);
}

Counter::CounterCallbackManager::~CounterCallbackManager() {
  counter_->UnregisterCallbackManager(this);
  delete closure_;
}

// Runs the closure if one is currently set and if it is not already inside a
// previous call.
void Counter::CounterCallbackManager::Run() const {
  if (!called_) {
    called_ = true;
    closure_->Run();
    called_ = false;
  }
}

// Second Counter ------------------------

SecondCounter::SecondCounter(ServiceLocator* service_locator)
    : Counter(service_locator) {
  CounterManager* counter_manager =
      service_locator->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->RegisterSecondCounter(this);
}

SecondCounter::~SecondCounter() {
  CounterManager* counter_manager =
      service_locator()->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->UnregisterSecondCounter(this);
}

ObjectBase::Ref SecondCounter::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new SecondCounter(service_locator));
}

// Render Frame Counter ------------------------

RenderFrameCounter::RenderFrameCounter(ServiceLocator* service_locator)
    : Counter(service_locator) {
  CounterManager* counter_manager =
      service_locator->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->RegisterRenderFrameCounter(this);
}

RenderFrameCounter::~RenderFrameCounter() {
  CounterManager* counter_manager =
      service_locator()->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->UnregisterRenderFrameCounter(this);
}

ObjectBase::Ref RenderFrameCounter::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new RenderFrameCounter(service_locator));
}

// Tick Counter ------------------------

TickCounter::TickCounter(ServiceLocator* service_locator)
    : Counter(service_locator) {
  CounterManager* counter_manager =
      service_locator->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->RegisterTickCounter(this);
}

TickCounter::~TickCounter() {
  CounterManager* counter_manager =
      service_locator()->GetService<CounterManager>();
  DCHECK(counter_manager);
  counter_manager->UnregisterTickCounter(this);
}

ObjectBase::Ref TickCounter::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new TickCounter(service_locator));
}
}  // namespace o3d
