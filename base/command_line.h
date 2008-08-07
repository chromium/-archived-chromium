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
//
// This file contains a class that can be used to extract the salient
// elements of a command line in a relatively lightweight manner.
// Switches can optionally have a value attached using an equals sign,
// as in "-switch=value".  Arguments that aren't prefixed with a
// switch prefix are considered "loose parameters".  Switch names
// are case-insensitive.

#ifndef BASE_COMMAND_LINE_H__
#define BASE_COMMAND_LINE_H__

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class CommandLine {
 public:
  // Creates a parsed version of the command line used to launch
  // the current process.
  CommandLine();

#if defined(OS_WIN)
  // Creates a parsed version of the given command-line string.
  // The program name is assumed to be the first item in the string.  
  CommandLine(const std::wstring& command_line);
#elif defined(OS_POSIX)
  CommandLine(int argc, const char** argv);
#endif

  ~CommandLine();

  // Returns true if this command line contains the given switch.
  // (Switch names are case-insensitive.)
  bool HasSwitch(const std::wstring& switch_string) const;

  // Returns the value associated with the given switch.  If the
  // switch has no value or isn't present, this method returns
  // the empty string.
  std::wstring GetSwitchValue(const std::wstring& switch_string) const;

  // Returns the number of "loose values" found in the command line.
  // Loose values are arguments that aren't switches.
  // (The program name is also excluded from the set of loose values.)
  size_t GetLooseValueCount() const;

  typedef std::vector<std::wstring>::const_iterator LooseValueIterator;

  // Returns a const_iterator to the list of loose values.
  LooseValueIterator GetLooseValuesBegin() const;

  // Returns the end const_iterator for the list of loose values.
  LooseValueIterator GetLooseValuesEnd() const;

  // Simply returns the original command line string.
  std::wstring command_line_string() const;

  // Returns the program part of the command line string (the first item).
  std::wstring program() const;

  // An array containing the prefixes that identify an argument as
  // a switch.
  static const wchar_t* const kSwitchPrefixes[];

  // The string that's used to separate switches from their values.
  static const wchar_t kSwitchValueSeparator[];

  // Appends the given switch string (preceded by a space and a switch
  // prefix) to the given string.
  static void AppendSwitch(std::wstring* command_line_string,
                           const std::wstring& switch_string);

  // Appends the given switch string (preceded by a space and a switch
  // prefix) to the given string, with the given value attached.
  static void AppendSwitchWithValue(std::wstring* command_line_string,
                                    const std::wstring& switch_string,
                                    const std::wstring& value_string);

 private:
  class Data;

  // True if we are responsible for deleting our |data_| pointer.  In some cases
  // we cache the result of parsing the command line and |data_|'s lifetime is
  // managed by someone else (e.g., the |Singleton| class).
  bool we_own_data_;

  // A pointer to the parsed version of the command line.
  Data* data_;
  
  DISALLOW_EVIL_CONSTRUCTORS(CommandLine);
};

#endif  // BASE_COMMAND_LINE_H__
