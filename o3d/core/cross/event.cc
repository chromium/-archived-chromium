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


// This file contains the implementation of Event.

#include "core/cross/precompile.h"
#include "core/cross/event.h"

namespace o3d {

bool Event::operator==(const Event& e) const {
  CHECK(valid() && e.valid());
  if (type_ != e.type_) {
    return false;
  }
  if (button_valid_ != e.button_valid_) {
    return false;
  }
  if (button_valid_ && (button_ != e.button_)) {
    return false;
  }
  if (modifier_state_valid_ != e.modifier_state_valid_) {
    return false;
  }
  if (modifier_state_valid_ && (modifier_state_ != e.modifier_state_)) {
    return false;
  }
  if (key_code_valid_ != e.key_code_valid_) {
    return false;
  }
  if (key_code_valid_ && (key_code_ != e.key_code_)) {
    return false;
  }
  if (char_code_valid_ != e.char_code_valid_) {
    return false;
  }
  if (char_code_valid_ && (char_code_ != e.char_code_)) {
    return false;
  }
  if (position_valid_ != e.position_valid_) {
    return false;
  }
  if (position_valid_) {
    if (x_ != e.x_) {
      return false;
    }
    if (y_ != e.y_) {
      return false;
    }
    if (screen_x_ != e.screen_x_) {
      return false;
    }
    if (screen_y_ != e.screen_y_) {
      return false;
    }
    if (in_plugin_ != e.in_plugin_) {
      return false;
    }
  }
  if (delta_valid_ != e.delta_valid_) {
    return false;
  }
  if (delta_valid_) {
    if (delta_x_ != e.delta_x_) {
      return false;
    }
    if (delta_y_ != e.delta_y_) {
      return false;
    }
  }
  return true;
}

}  // namespace o3d
