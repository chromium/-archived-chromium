// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_EVENT_RECORDER_H_
#define BASE_EVENT_RECORDER_H_

#include <string>
#include <windows.h>
#include "base/basictypes.h"

namespace base {

// A class for recording and playing back keyboard and mouse input events.
//
// Note - if you record events, and the playback with the windows in
//        different sizes or positions, the playback will fail.  When
//        recording and playing, you should move the relevant windows
//        to constant sizes and locations.
// TODO(mbelshe) For now this is a singleton.  I believe that this class
//        could be easily modified to:
//             support two simultaneous recorders
//             be playing back events while already recording events.
//        Why?  Imagine if the product had a "record a macro" feature.
//        You might be recording globally, while recording or playing back
//        a macro.  I don't think two playbacks make sense.
class EventRecorder {
 public:
  // Get the singleton EventRecorder.
  // We can only handle one recorder/player at a time.
  static EventRecorder* current() {
    if (!current_)
      current_ = new EventRecorder();
    return current_;
  }

  // Starts recording events.
  // Will clobber the file if it already exists.
  // Returns true on success, or false if an error occurred.
  bool StartRecording(std::wstring &filename);

  // Stops recording.
  void StopRecording();

  // Is the EventRecorder currently recording.
  bool is_recording() const { return is_recording_; }

  // Plays events previously recorded.
  // Returns true on success, or false if an error occurred.
  bool StartPlayback(std::wstring &filename);

  // Stops playback.
  void StopPlayback();

  // Is the EventRecorder currently playing.
  bool is_playing() const { return is_playing_; }

  // C-style callbacks for the EventRecorder.
  // Used for internal purposes only.
  LRESULT RecordWndProc(int nCode, WPARAM wParam, LPARAM lParam);
  LRESULT PlaybackWndProc(int nCode, WPARAM wParam, LPARAM lParam);

 private:
  // Create a new EventRecorder.  Events are saved to the file filename.
  // If the file already exists, it will be deleted before recording
  // starts.
  explicit EventRecorder()
      : is_recording_(false),
        is_playing_(false),
        journal_hook_(NULL),
        file_(NULL) {
  }
  ~EventRecorder();

  static EventRecorder* current_;  // Our singleton.

  bool is_recording_;
  bool is_playing_;
  HHOOK journal_hook_;
  FILE* file_;
  EVENTMSG playback_msg_;
  int playback_first_msg_time_;
  int playback_start_time_;

  DISALLOW_EVIL_CONSTRUCTORS(EventRecorder);
};

}  // namespace base

#endif // BASE_EVENT_RECORDER_H_
