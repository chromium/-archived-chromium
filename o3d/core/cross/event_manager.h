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


// This file contains the declaration of the EventManager class.

#ifndef O3D_CORE_CROSS_EVENT_MANAGER_H_
#define O3D_CORE_CROSS_EVENT_MANAGER_H_

#include <vector>
#include "core/cross/event.h"
#include "core/cross/event_callback.h"

namespace o3d {

class Client;

//  This class manages the queue of events that need to get forwarded through to
//  JavaScript handlers.  It deals with synthesizing any events that need
//  synthesis in the same way across platforms, and does throttling on mousemove
//  events.
class EventManager {
 public:

  explicit EventManager() :
      mousedown_in_plugin_(false),
#ifndef NDEBUG
      processing_event_queue_(false),
#endif
      valid_(true) { }

  // Do per-timer-tick queue processing.
  void ProcessQueue();

  // Sets the callback for a events of a supplied type.
  // NOTE: The client takes ownership of the EventCallback you pass in. It will
  // be deleted if you call SetEventCallback a second time for the same event
  // type or if you call ClearEventCallback for that type.
  //
  // Parameters:
  //   event_callback: EventCallback to call each time an event of the right
  //                   type occurs.
  //   type: Type of event this callback handles.
  void SetEventCallback(Event::Type type, EventCallback* event_callback);

  // Clears the callback for events of a given type.
  void ClearEventCallback(Event::Type type);

  // Automatically drops some mousemove events to throttle event bandwidth.
  void AddEventToQueue(const Event& event);

  // Deletes all callbacks and events.  May optionally be used before deletion,
  // as it accomplishes what the destructor does, but is safe to use twice or
  // not at all.  After this is called, no events will be processed, although
  // all functions are still safe to call.
  void ClearAll();

 private:
  // One EventCallbackManager for each type of event that we might have to pass
  // through to the user.
  EventCallbackManager event_callbacks_[Event::NUM_TYPES];
  // Queue of events that have come in and need to get sent to the user's
  // handler(s).  We throttle the rate at which events get handled by sending
  // out a fixed number [currently one] per timer tick.  The ticks [and thus the
  // events] still happen even if rendering on-demand.
  EventQueue event_queue_;
  bool mousedown_in_plugin_;

#ifndef NDEBUG
  // Test flag used to guarantee nonreentrance.
  bool processing_event_queue_;
#endif
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(EventManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_EVENT_MANAGER_H_
