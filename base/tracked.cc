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

#include "base/tracked.h"

#include "base/string_util.h"
#include "base/tracked_objects.h"

namespace tracked_objects {

//------------------------------------------------------------------------------
void Location::Write(bool display_filename, bool display_function_name,
                     std::string* output) const {
  StringAppendF(output, "%s[%d] ",
      display_filename ? file_name_ : "line",
      line_number_);

  if (display_function_name) {
    WriteFunctionName(output);
    output->push_back(' ');
  }
}

void Location::WriteFunctionName(std::string* output) const {
  // Translate "<" to "&lt;" for HTML safety.
  // TODO(jar): Support ASCII or html for logging in ASCII.
  for (const char *p = function_name_; *p; p++) {
    switch (*p) {
      case '<':
        output->append("&lt;");
        break;

      case '>':
        output->append("&gt;");
        break;

      default:
        output->push_back(*p);
        break;
    }
  }
}

//------------------------------------------------------------------------------

#ifndef TRACK_ALL_TASK_OBJECTS

Tracked::Tracked() {}
Tracked::~Tracked() {}
void Tracked::SetBirthPlace(const Location& from_here) {}
void Tracked::ResetBirthTime() {}
bool Tracked::MissingBirthplace() const { return false; }
void Tracked::ResetBirthTime() {}

#else

Tracked::Tracked() : tracked_births_(NULL), tracked_birth_time_(Time::Now()) {
  if (!ThreadData::IsActive())
    return;
  SetBirthPlace(Location("NoFunctionName", "NeedToSetBirthPlace", -1));
}

Tracked::~Tracked() {
  if (!ThreadData::IsActive() || !tracked_births_)
    return;
  ThreadData::current()->TallyADeath(*tracked_births_,
                                     Time::Now() - tracked_birth_time_);
}

void Tracked::SetBirthPlace(const Location& from_here) {
  if (!ThreadData::IsActive())
    return;
  if (tracked_births_)
    tracked_births_->ForgetBirth();
  ThreadData* current_thread_data = ThreadData::current();
  if (!current_thread_data)
    return;  // Shutdown started, and this thread wasn't registered.
  tracked_births_ = current_thread_data->FindLifetime(from_here);
  tracked_births_->RecordBirth();
}

void Tracked::ResetBirthTime() {
  tracked_birth_time_ = Time::Now();
}

bool Tracked::MissingBirthplace() const {
  return -1 == tracked_births_->location().line_number();
}

#endif  // NDEBUG

}  // namespace tracked_objects

