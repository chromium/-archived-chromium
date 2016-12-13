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


// This file contains the declarations of Counter related classes

#ifndef O3D_CORE_CROSS_COUNTER_H_
#define O3D_CORE_CROSS_COUNTER_H_

#include <map>
#include <vector>
#include "core/cross/callback.h"
#include "core/cross/param_object.h"

namespace o3d {

class Counter : public ParamObject {
 public:
  virtual ~Counter();

  // Manages a closure so it can not be called recursively. The manager takes
  // ownership of the closure.
  class CounterCallbackManager : public RefCounted {
   public:
    typedef SmartPointer<CounterCallbackManager> Ref;
    typedef Closure ClosureType;

    CounterCallbackManager(Counter* counter, ClosureType* closure);
    ~CounterCallbackManager();

    // Runs the closure if it is not already inside a previous call.
    void Run() const;

    ClosureType* callback() const {
      return closure_;
    }

   private:
    Counter* counter_;
    ClosureType* closure_;
    mutable bool called_;
  };

  typedef CounterCallbackManager::ClosureType CounterCallback;

  // A class to associate a count with a callback.
  class CounterCallbackInfo {
   public:
    CounterCallbackInfo(float count, CounterCallbackManager* manager)
        : count_(count),
          callback_manager_(CounterCallbackManager::Ref(manager)) {
    }

    inline float count() const {
      return count_;
    }

    inline CounterCallbackManager* callback_manager() const {
      return callback_manager_;
    }

    inline void set_callback_manager(CounterCallbackManager* manager) {
      callback_manager_ = CounterCallbackManager::Ref(manager);
    }

   private:
    float count_;
    CounterCallbackManager::Ref callback_manager_;
  };

  typedef std::vector<CounterCallbackInfo> CounterCallbackInfoArray;

  // A class for queuing counter callbacks and then calling them.
  class CounterCallbackQueue {
   public:
    CounterCallbackQueue() { }

    // Queue a counter callback. This is an internal function only called by
    // class Counter. Because a callback could effect structures in the client
    // we must not call them while walking any of those structures so Counter
    // adds all the callbacks that need calling through this method and the
    // client calls them all at the appropriate time.
    // Parameters:
    //   callback_manager: callback manager to queue.
    void QueueCounterCallback(CounterCallbackManager* callback_manager);

    // Calls all the queue callbacks and clears the queue. This is an internal
    // function.
    void CallCounterCallbacks();

   private:
    // Counter callback managers.
    typedef std::vector<CounterCallbackManager::Ref>
        CounterCallbackManagerArray;
    CounterCallbackManagerArray counter_callbacks_;

    DISALLOW_COPY_AND_ASSIGN(CounterCallbackQueue);
  };

  static const char* kRunningParamName;
  static const char* kForwardParamName;
  static const char* kCountModeParamName;
  static const char* kStartParamName;
  static const char* kEndParamName;
  static const char* kCountParamName;
  static const char* kMultiplierParamName;

  enum CountMode {
    CONTINUOUS,  // Keep running the counter forever.
    ONCE,  // Stop at start or end depending on the time.
    CYCLE,  // When at end, jump back to start.
    OSCILLATE,  // Go from start to end back to start.
  };

  // Updates the counter. This is an internal function called by the client.
  // Parameters:
  //   advance_amount: How much to advance the counter. For a SecondCounter
  //       this will be the number of seconds since the last call to Advance,
  //       For a RenderFrameCounter or TickCounter this will always be 1.0
  //   queue: queue to hold any callbacks that need to be called because of
  //       advance.
  void Advance(float advance_amount, CounterCallbackQueue* queue);

  // Resets the counter to start or end depending on the current direction. Note
  // that resetting a counter does not Stop it. If you want to both reset and
  // stop an counter call Stop() then call Reset().
  void Reset();

  // Gets the running state.
  bool running() {
    return running_param_->value();
  }

  // Sets the running state.
  void set_running(bool running) {
    running_param_->set_value(running);
  }

  // Gets the forward setting. Whether the counter is counting forward or back.
  bool forward() {
    return forward_param_->value();
  }

  // Sets the forward setting.
  void set_forward(bool forward) {
    forward_param_->set_value(forward);
  }

  // Gets the count_mode.
  CountMode count_mode() {
    return static_cast<CountMode>(count_mode_param_->value());
  }

  // Sets the count_mode.
  void set_count_mode(CountMode count_mode) {
    count_mode_param_->set_value(count_mode);
  }

  // Gets the current count.
  float count() const {
    return count_param_->value();
  }

  // Sets the current count value for this counter as well as resetting the
  // state of the callbacks.
  //
  // In other words. Assume start = 1, end = 5, count = 1, and you have a
  // callback at 3.
  //
  // myCounter->set_start(1.0f);
  // myCounter->set_end(5.0f);
  // myCounter->AddCallback(3.0f, myCallback);
  // myCounter->Reset();
  //
  // myCounter->Advance(2.0f, &queue);  // count is now 3, myCallback is queued.
  // myCounter->Advance(2.0f, &queue);  // count is now 5
  //
  // vs.
  //
  // myCounter->set_start(1.f);
  // myCounter->set_end(5.f);
  // myCounter->AddCallback(3.f, myCallback);
  // myCounter->Reset();
  //
  // myCounter->Advance(2.f, &queue);  // count is now 3, myCallback is queued.
  // myCounter->SetCount(3.f); // count is now 3, callback state has been reset.
  // myCounter->advance(2.f, &queue);  // count is now 5, myCallback is queued.
  //
  // In the second case myCallback was queued twice.
  //
  // Parameters:
  //   count: Value to set the count to.
  void SetCount(float value);

  // Gets the start count.
  float start() const {
    return start_param_->value();
  }

  // Sets the start count.
  void set_start(float start) {
    start_param_->set_value(start);
  }

  // Gets the end count.
  float end() const {
    return end_param_->value();
  }

  // Sets the end count.
  void set_end(float end) {
    end_param_->set_value(end);
  }

  // Gets the multiplier
  float multiplier() {
    return multiplier_param_->value();
  }

  // Sets the multiplier.
  void set_multiplier(float multiplier) {
    multiplier_param_->set_value(multiplier);
  }

  // Adds a callback that will be called when the counter
  // reaches a certain count. If a callback is already registered for that
  // particular count it will be removed.
  //
  // NOTE: A callback at start will only get called when counting backward, a
  // callback at end will only get called counting forward.
  //
  // NOTE: The client takes ownership of the Callback you pass in. It will be
  // deleted if you call AddCallback for the same count or RemoveCallback for
  // the same count.
  //
  // Parameters:
  //   count: count at which callback will be called.
  //   callback: callback to call.
  void AddCallback(float count, CounterCallback* callback);

  // Removes a callback at a particular count.
  // Parameters:
  //   count: count to remove callback at.
  // Returns:
  //   true of there was a callback at that count to remove.
  bool RemoveCallback(float count);

  // Removes all the callbacks on this counter.
  void RemoveAllCallbacks();

  // Returns the callbacks.
  const CounterCallbackInfoArray& GetCallbacks() const {
    return callbacks_;
  }

 protected:
  explicit Counter(ServiceLocator* service_locator);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Calls the callbacks in the range start_count to end_count. start_count is
  // NOT included in callback check, end_count is.
  // Parameters:
  //   start_count: non-inclusive start count.
  //   end_count: inclusive end count.
  //   queue: queue to hold any callbacks that need to be called.
  void CallCallbacks(float start_count,
                     float end_count,
                     CounterCallbackQueue* queue);

  // Finds a callback by count.
  // Parameters:
  //   count: count to find.
  // Returns:
  //   Iterator to callbackInfo
  CounterCallbackInfoArray::iterator FindCallback(float count);

  // Registers a callback manager. This is an internal function called only
  // by CounterCallbackManager's constructor.
  // Parameters:
  //   manager: manager to register.
  void RegisterCallbackManager(CounterCallbackManager* manager);

  // Unregisters a callback manager. This is an internal function called only by
  // CounterCallbackManager's constructor.
  // Parameters:
  //   manager: manager to unregister.
  void UnregisterCallbackManager(CounterCallbackManager* manager);

  // Sets the current count directly, not changing any thing but the count.
  void set_count(float count) {
    count_param_->set_value(count);
  }

  ParamBoolean::Ref running_param_;
  ParamBoolean::Ref forward_param_;
  ParamInteger::Ref count_mode_param_;
  ParamFloat::Ref start_param_;
  ParamFloat::Ref end_param_;
  ParamFloat::Ref count_param_;
  ParamFloat::Ref multiplier_param_;

  // Points to the next callback in the current direction for this counter.
  // In other words if there are callbacks at times 10, 27 and 34 and the count
  // is at 21 and this counter is going forward then this would be pointing to
  // the callback at 27.
  CounterCallbackInfoArray::iterator next_callback_;
  CounterCallbackInfoArray::reverse_iterator prev_callback_;

  // false if the next_callback_ is valid (points to something real)
  bool next_callback_valid_;

  // false if the prev_callback_ is valid (points to something real)
  bool prev_callback_valid_;

  // last end count passed to CallCallbacks.
  float last_call_callbacks_end_count_;

  // Array of counts to callbacks.
  CounterCallbackInfoArray callbacks_;

  // Map fo callbacks to callback managers.
  typedef std::map<CounterCallback*,
                   CounterCallbackManager*> CallbackManagerMap;
  CallbackManagerMap callback_managers_;

  O3D_DECL_CLASS(Counter, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Counter);
};

// A Second counter counts by elasped time.
class SecondCounter : public Counter {
 public:
  explicit SecondCounter(ServiceLocator* service_locator);
  virtual ~SecondCounter();

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(SecondCounter, Counter);
  DISALLOW_COPY_AND_ASSIGN(SecondCounter);
};

// A Render Frame counter counts by render frames.
class RenderFrameCounter : public Counter {
 public:
  explicit RenderFrameCounter(ServiceLocator* service_locator);
  virtual ~RenderFrameCounter();

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(RenderFrameCounter, Counter);
  DISALLOW_COPY_AND_ASSIGN(RenderFrameCounter);
};

// A tick counter counts by ticks.
class TickCounter : public Counter {
 public:
  explicit TickCounter(ServiceLocator* service_locator);
  virtual ~TickCounter();

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(TickCounter, Counter);
  DISALLOW_COPY_AND_ASSIGN(TickCounter);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_COUNTER_H_
