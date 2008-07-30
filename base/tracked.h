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

//------------------------------------------------------------------------------
// Tracked is the base class for all tracked objects.  During construction, it
// registers the fact that an instance was created, and at destruction time, it
// records that event.  The instance may be tagged with a name, which is refered
// to as its Location.  The Location is a file and line number, most
// typically indicated where the object was constructed.  In some cases, as the
// object's significance is refined (for example, a Task object is augmented to
// do additonal things), its Location may be redefined to that later location.

// Tracking includes (for each instance) recording the birth thread, death
// thread, and duration of life (from construction to destruction).  All this
// data is accumulated and filtered for review at about:objects.

#ifndef BASE_TRACKED_H__
#define BASE_TRACKED_H__

#include <string>

#include "base/time.h"

#ifndef NDEBUG
#define TRACK_ALL_TASK_OBJECTS
#endif

namespace tracked_objects {

//------------------------------------------------------------------------------
// Location provides basic info where of an object was constructed, or was
// significantly brought to life.

class Location {
 public:
  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  Location(const char* function_name, const char* file_name, int line_number)
      : function_name_(function_name),
        file_name_(file_name),
        line_number_(line_number) { }

  // Provide a default constructor for easy of debugging.
  Location()
      : function_name_("Unknown"),
        file_name_("Unknown"),
        line_number_(-1) { }

  // Comparison operator for insertion into a std::map<> hash tables.
  // All we need is *some* (any) hashing distinction.  Strings should already
  // be unique, so we don't bother with strcmp or such.
  // Use line number as the primary key (because it is fast, and usually gets us
  // a difference), and then pointers as secondary keys (just to get some
  // distinctions).
  bool operator < (const Location& other) const {
    if (line_number_ != other.line_number_)
      return line_number_ < other.line_number_;
    if (file_name_ != other.file_name_)
      return file_name_ != other.file_name_;
    return function_name_ < other.function_name_;
  }

  const char* function_name() const { return function_name_; }
  const char* file_name()     const { return file_name_; }
  int line_number()           const { return line_number_; }

  void Write(bool display_filename, bool display_function_name,
           std::string* output) const;

  // Write function_name_ in HTML with '<' and '>' properly encoded.
  void WriteFunctionName(std::string* output) const;

 private:
  const char* const function_name_;
  const char* const file_name_;
  const int line_number_;
};


//------------------------------------------------------------------------------
// Define a macro to record the current source location.

#define FROM_HERE tracked_objects::Location(__FUNCTION__, __FILE__, __LINE__)


//------------------------------------------------------------------------------


class Births;

class Tracked {
 public:
  Tracked();
  virtual ~Tracked();
  void SetBirthPlace(const Location& from_here);

  bool MissingBirthplace() const;

 private:
  Births* tracked_births_;  // At same birthplace, and same thread.
  const Time tracked_birth_time_;

  DISALLOW_EVIL_CONSTRUCTORS(Tracked);
};

}  // namespace tracked_objects

#endif  // BASE_TRACKED_H__
