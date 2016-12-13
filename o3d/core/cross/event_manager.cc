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


// This file contains the implementation for the EventManager class.

// The precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include "core/cross/event_manager.h"

#include "core/cross/client.h"
#include "core/cross/error.h"

namespace o3d {

void EventManager::ProcessQueue() {
  // Process up to one event per frame.
  // If we've removed all callbacks by clearing event_callbacks_, we're shutting
  // down, so we can't process any late-added events.
  if (valid_ && !event_queue_.empty()) {
#ifndef NDEBUG
    DCHECK(!processing_event_queue_);
    processing_event_queue_ = true;
#endif
    const Event& event = event_queue_.front();
    event_callbacks_[event.type()].Run(event);
    event_queue_.pop_front();
#ifndef NDEBUG
    processing_event_queue_ = false;
#endif
  }
}

void EventManager::SetEventCallback(Event::Type type,
                                    EventCallback* event_callback) {
  DCHECK(Event::ValidType(type));
  if (valid_)
    event_callbacks_[type].Set(event_callback);
}

void EventManager::ClearEventCallback(Event::Type type) {
  DCHECK(Event::ValidType(type));
  if (valid_)
    event_callbacks_[type].Clear();
}

void EventManager::AddEventToQueue(const Event& event) {
  // If we're shutting down or there's no callback registered to handle
  // this event type, we drop it.
  if (valid_ && event_callbacks_[event.type()].IsSet()) {
    if (!event_queue_.empty()) {
      switch (event.type()) {
        case Event::TYPE_MOUSEMOVE:
          if (event_queue_.back().type() == Event::TYPE_MOUSEMOVE) {
            // Just keep the last MOUSEMOVE; there's no need to queue up lots
            // of them.
            event_queue_.back() = event;
            return;
          }
          break;
        case Event::TYPE_KEYPRESS:
          // If we're backed up with keydowns and keypresses [which alternate
          // on key repeat], just throw away the new ones.  Throwing them away
          // one at a time could lead to aliased repeat patterns in which we
          // throw away more keydowns than keypresses, so we have to detect the
          // pair together and throw them both away.  This means that we won't
          // start chucking stuff until there are at least 4 events backed up [3
          // in the queue plus the new one], but that'll keep us from getting
          // more than a few frames behind.
          if (event_queue_.size() >= 3) {
            EventQueue::reverse_iterator iter = event_queue_.rbegin();
            const Event& event3 = *iter;
            if (event3.type() != Event::TYPE_KEYDOWN) {
              break;
            }
            const Event& event2 = *++iter;
            if (event2 != event) {
              break;
            }
            const Event& event1 = *++iter;
            if (event1 != event3) {
              break;
            }
            event_queue_.pop_back();  // Throw away the keydown.
            return;  // Throw away the keypress.
          }
          break;
        default:
          break;
      }
    }
    if (event.type() == Event::TYPE_MOUSEDOWN) {
      if (event.in_plugin()) {
        mousedown_in_plugin_ = true;
      } else {
        mousedown_in_plugin_ = false;
        return;  // Why did we even get this event?
      }
    }

    event_queue_.push_back(event);

    if (event.type() == Event::TYPE_MOUSEUP) {
      if (mousedown_in_plugin_ && event.in_plugin()) {
        Event temp = event;
        temp.set_type(Event::TYPE_CLICK);
        event_queue_.push_back(temp);
        if (temp.button() == Event::BUTTON_RIGHT) {
          temp.set_type(Event::TYPE_CONTEXTMENU);
          temp.clear_modifier_state();
          temp.clear_button();
          event_queue_.push_back(temp);
        }
      }
      mousedown_in_plugin_ = false;
    }
  }
}

void EventManager::ClearAll() {
  valid_ = false;
  for (unsigned int i = 0; i < arraysize(event_callbacks_); ++i) {
    event_callbacks_[i].Clear();
  }
  event_queue_.clear();
}

}  // namespace o3d
