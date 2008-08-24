// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#include <algorithm>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"

using namespace std;

// Since we use a lazy match, make sure that longer versions (like L"--")
// are listed before shorter versions (like L"-") of similar prefixes.
#if defined(OS_WIN)
const wchar_t* const CommandLine::kSwitchPrefixes[] = {L"--", L"-", L"/"};
#elif defined(OS_POSIX)
// Unixes don't use slash as a switch.
const wchar_t* const CommandLine::kSwitchPrefixes[] = {L"--", L"-"};
#endif

const wchar_t CommandLine::kSwitchValueSeparator[] = L"=";

// Needed to avoid a typecast on the tolower() function pointer in Lowercase().
// MSVC accepts it as-is but GCC requires the typecast.
static int ToLower(int c) {
  return tolower(c);
}

static void Lowercase(wstring* parameter) {
  transform(parameter->begin(), parameter->end(), parameter->begin(), 
            ToLower);
}

// CommandLine::Data
//
// This object holds the parsed data for a command line.  We hold this in a
// separate object from |CommandLine| so that we can share the parsed data
// across multiple |CommandLine| objects.  When we share |Data|, we might be
// accessing this object on multiple threads.  To ensure thread safety, the
// public interface of this object is const only.
//
// Do NOT add any non-const methods to this object.  You have been warned.
class CommandLine::Data {
 public:
#if defined(OS_WIN)
  Data() { 
    Init(GetCommandLineW());
  }
#elif defined(OS_POSIX)
  Data() {
    // Owner must call Init().
  }
#endif

#if defined(OS_WIN)
  Data(const wstring& command_line) {
    Init(command_line);
  }
#elif defined(OS_POSIX)
  Data(int argc, const char* const* argv) {
    Init(argc, argv);
  }
#endif

#if defined(OS_WIN)
  // Does the actual parsing of the command line.
  void Init(const std::wstring& command_line) {
    TrimWhitespace(command_line, TRIM_ALL, &command_line_string_);

    if (command_line_string_.empty())
      return;

    int num_args = 0;
    wchar_t** args = NULL;

    args = CommandLineToArgvW(command_line_string_.c_str(), &num_args);

    // Populate program_ with the trimmed version of the first arg.
    TrimWhitespace(args[0], TRIM_ALL, &program_);

    for (int i = 1; i < num_args; ++i) {
      wstring arg;
      TrimWhitespace(args[i], TRIM_ALL, &arg);

      wstring switch_string;
      wstring switch_value;
      if (IsSwitch(arg, &switch_string, &switch_value)) {
        switches_[switch_string] = switch_value;
      } else {
        loose_values_.push_back(arg);
      }
    }

    if (args)
      LocalFree(args);
  }

#elif defined(OS_POSIX)
  // Does the actual parsing of the command line.
  void Init(int argc, const char* const* argv) {
    if (argc < 1)
      return;
    program_ = base::SysNativeMBToWide(argv[0]);
    command_line_string_ = program_;

    for (int i = 1; i < argc; ++i) {
      std::wstring arg = base::SysNativeMBToWide(argv[i]);
      command_line_string_.append(L" ");
      command_line_string_.append(arg);

      wstring switch_string;
      wstring switch_value;
      if (IsSwitch(arg, &switch_string, &switch_value)) {
        switches_[switch_string] = switch_value;
      } else {
        loose_values_.push_back(arg);
      }
    }
  }
#endif

  const std::wstring& command_line_string() const {
    return command_line_string_;
  }

  const std::wstring& program() const {
    return program_;
  }

  const std::map<std::wstring, std::wstring>& switches() const {
    return switches_;
  }

  const std::vector<std::wstring>& loose_values() const {
    return loose_values_;
  }

 private:
  // Returns true if parameter_string represents a switch.  If true,
  // switch_string and switch_value are set.  (If false, both are
  // set to the empty string.)
  static bool IsSwitch(const wstring& parameter_string,
                       wstring* switch_string,
                       wstring* switch_value) {

    *switch_string = L"";
    *switch_value = L"";

    for (size_t i = 0; i < arraysize(kSwitchPrefixes); ++i) {
      std::wstring prefix(kSwitchPrefixes[i]);
      if (parameter_string.find(prefix) != 0)  // check prefix
        continue;

      const size_t switch_start = prefix.length();
      const size_t equals_position = parameter_string.find(
          kSwitchValueSeparator, switch_start);
      if (equals_position == wstring::npos) {
        *switch_string = parameter_string.substr(switch_start);
      } else {
        *switch_string = parameter_string.substr(
            switch_start, equals_position - switch_start);
        *switch_value = parameter_string.substr(equals_position + 1);
      }
      Lowercase(switch_string);

      return true;
    }

    return false;
  }

  std::wstring command_line_string_;
  std::wstring program_;
  std::map<std::wstring, std::wstring> switches_;
  std::vector<std::wstring> loose_values_;

  DISALLOW_EVIL_CONSTRUCTORS(Data);
};

CommandLine::CommandLine()
    : we_own_data_(false),  // The Singleton class will manage it for us.
      data_(Singleton<Data>::get()) {
  DCHECK(!data_->command_line_string().empty()) <<
    "You must call CommandLine::SetArgcArgv before making any CommandLine "
    "calls.";
}

#if defined(OS_WIN)
CommandLine::CommandLine(const wstring& command_line)
    : we_own_data_(true),
      data_(new Data(command_line)) {
}
#elif defined(OS_POSIX)
CommandLine::CommandLine(const int argc, const char* const* argv)
    : we_own_data_(true),
      data_(new Data(argc, argv)) {
}
#endif

CommandLine::~CommandLine() {
  if (we_own_data_)
    delete data_;
}

// static
void CommandLine::SetArgcArgv(int argc, const char* const* argv) {
#if !defined(OS_WIN)
  Singleton<Data>::get()->Init(argc, argv);
#endif
}

bool CommandLine::HasSwitch(const wstring& switch_string) const {
  wstring lowercased_switch(switch_string);
  Lowercase(&lowercased_switch);
  return data_->switches().find(lowercased_switch) != data_->switches().end();
}

wstring CommandLine::GetSwitchValue(const wstring& switch_string) const {
  wstring lowercased_switch(switch_string);
  Lowercase(&lowercased_switch);

  const map<wstring, wstring>::const_iterator result =
    data_->switches().find(lowercased_switch);

  if (result == data_->switches().end()) {
    return L"";
  } else {
    return result->second;
  }
}

size_t CommandLine::GetLooseValueCount() const {
  return data_->loose_values().size();
}

CommandLine::LooseValueIterator CommandLine::GetLooseValuesBegin() const {
  return data_->loose_values().begin();
}

CommandLine::LooseValueIterator CommandLine::GetLooseValuesEnd() const {
  return data_->loose_values().end();
}

std::wstring CommandLine::command_line_string() const {
  return data_->command_line_string();
}

std::wstring CommandLine::program() const {
  return data_->program();
}

// static
void CommandLine::AppendSwitch(wstring* command_line_string,
                               const wstring& switch_string) {
  DCHECK(command_line_string);
  command_line_string->append(L" ");
  command_line_string->append(kSwitchPrefixes[0]);
  command_line_string->append(switch_string);
}

// static
void CommandLine::AppendSwitchWithValue(wstring* command_line_string,
                                        const wstring& switch_string,
                                        const wstring& value_string) {
  AppendSwitch(command_line_string, switch_string);

  if (value_string.empty())
    return;

  command_line_string->append(kSwitchValueSeparator);
  // NOTE(jhughes): If the value contains a quotation mark at one
  //                end but not both, you may get unusable output.
  if ((value_string.find(L" ") != std::wstring::npos) &&
      (value_string[0] != L'"') &&
      (value_string[value_string.length() - 1] != L'"')) {
    // need to provide quotes
    StringAppendF(command_line_string, L"\"%ls\"", value_string.c_str());
  } else {
    command_line_string->append(value_string);
  }
}

