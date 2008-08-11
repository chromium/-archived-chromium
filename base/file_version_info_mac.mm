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
// Copied from base/basictypes.h with some modifications

#import <Cocoa/Cocoa.h>

#include "base/file_version_info.h"

#include "base/logging.h"
#include "base/string_util.h"

FileVersionInfo::FileVersionInfo(const std::wstring& file_path, NSBundle *bundle)
    : file_path_(file_path), bundle_(bundle) {
  if (!bundle_) {
    NSString* path = [[NSString alloc]
        initWithCString:reinterpret_cast<const char*>(file_path_.c_str())
               encoding:NSUTF32StringEncoding];
    bundle_ = [NSBundle bundleWithPath: path];
  }      
}

FileVersionInfo::~FileVersionInfo() {

}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  NSBundle* bundle = [NSBundle mainBundle];
  return new FileVersionInfo(L"", bundle);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const std::wstring& file_path) {
  return new FileVersionInfo(file_path, nil);
}

std::wstring FileVersionInfo::company_name() {
  return L"";
}

std::wstring FileVersionInfo::company_short_name() {
  return L"";
}

std::wstring FileVersionInfo::internal_name() {
  return L"";
}

std::wstring FileVersionInfo::product_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::product_short_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::comments() {
  return L"";
}

std::wstring FileVersionInfo::legal_copyright() {
  return GetStringValue(L"CFBundleGetInfoString");
}

std::wstring FileVersionInfo::product_version() {
  return GetStringValue(L"CFBundleShortVersionString");
}

std::wstring FileVersionInfo::file_description() {
  return L"";
}

std::wstring FileVersionInfo::legal_trademarks() {
  return L"";
}

std::wstring FileVersionInfo::private_build() {
  return L"";
}

std::wstring FileVersionInfo::file_version() {
  return GetStringValue(L"CFBundleVersion");
}

std::wstring FileVersionInfo::original_filename() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::special_build() {
  return L"";
}

std::wstring FileVersionInfo::last_change() {
  return L"";
}

bool FileVersionInfo::is_official_build() {
  return false;
}

bool FileVersionInfo::GetValue(const wchar_t* name, std::wstring* value_str) {
  std::wstring str;
  if (bundle_) {
    NSString* value = [bundle_ objectForInfoDictionaryKey:
        [NSString stringWithUTF8String:WideToUTF8(name).c_str()]];
    if (value) {
      *value_str = reinterpret_cast<const wchar_t*>(
          [value cStringUsingEncoding:NSUTF32StringEncoding]);
      return true;
    }
  }
  return false;
}

std::wstring FileVersionInfo::GetStringValue(const wchar_t* name) {
  std::wstring str;
  if (GetValue(name, &str))
    return str;
  return L"";
}

