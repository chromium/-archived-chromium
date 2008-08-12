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

#ifndef BASE_FILE_VERSION_INFO_H__
#define BASE_FILE_VERSION_INFO_H__

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#ifdef OS_MACOSX
#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif
#endif

// Provides a way to access the version information for a file.
// This is the information you access when you select a file in the Windows
// explorer, right-click select Properties, then click the Version tab.

class FileVersionInfo {
 public:
  // Creates a FileVersionInfo for the specified path. Returns NULL if something
  // goes wrong (typically the file does not exit or cannot be opened). The
  // returned object should be deleted when you are done with it.
  static FileVersionInfo* CreateFileVersionInfo(const std::wstring& file_path);

  // Creates a FileVersionInfo for the current module. Returns NULL in case
  // of error. The returned object should be deleted when you are done with it.
  static FileVersionInfo*
      FileVersionInfo::CreateFileVersionInfoForCurrentModule();

  ~FileVersionInfo();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  std::wstring company_name();
  std::wstring company_short_name();
  std::wstring product_name();
  std::wstring product_short_name();
  std::wstring internal_name();
  std::wstring product_version();
  std::wstring private_build();
  std::wstring special_build();
  std::wstring comments();
  std::wstring original_filename();
  std::wstring file_description();
  std::wstring file_version();
  std::wstring legal_copyright();
  std::wstring legal_trademarks();
  std::wstring last_change();
  bool is_official_build();

  // Lets you access other properties not covered above.
  bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  std::wstring GetStringValue(const wchar_t* name);

#ifdef OS_WIN
  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }
#endif

 private:
#if defined(OS_WIN)
  FileVersionInfo(void* data, int language, int code_page);

  scoped_ptr_malloc<char> data_;
  int language_;
  int code_page_;
  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;
#elif defined(OS_MACOSX)
  explicit FileVersionInfo(NSBundle *bundle);
  explicit FileVersionInfo(const std::wstring& file_path);
  
  NSBundle *bundle_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(FileVersionInfo);
};

#endif  // BASE_FILE_VERSION_INFO_H__
