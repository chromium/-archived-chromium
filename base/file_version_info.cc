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

#include <windows.h>

#include "base/file_version_info.h"

#include "base/logging.h"
#include "base/path_service.h"

// This has to be last.
#include <strsafe.h>

FileVersionInfo::FileVersionInfo(void* data, int language, int code_page)
    : language_(language), code_page_(code_page) {
  data_.reset((char*) data);
  fixed_file_info_ = NULL;
  UINT size;
  ::VerQueryValue(data_.get(), L"\\", (LPVOID*)&fixed_file_info_, &size);
}

FileVersionInfo::~FileVersionInfo() {
  DCHECK(data_.get());
}

typedef struct {
  WORD language;
  WORD code_page;
} LanguageAndCodePage;

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  std::wstring app_path;
  if (!PathService::Get(base::FILE_MODULE, &app_path))
    return NULL;

  return CreateFileVersionInfo(app_path);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const std::wstring& file_path) {
  DWORD dummy;
  const wchar_t* path = file_path.c_str();
  DWORD length = ::GetFileVersionInfoSize(path, &dummy);
  if (length == 0)
    return NULL;

  void* data = calloc(length, 1);
  if (!data)
    return NULL;

  if (!::GetFileVersionInfo(path, dummy, length, data)) {
    free(data);
    return NULL;
  }

  LanguageAndCodePage* translate = NULL;
  uint32 page_count;
  BOOL query_result = VerQueryValue(data, L"\\VarFileInfo\\Translation",
                                   (void**) &translate, &page_count);

  if (query_result && translate) {
    return new FileVersionInfo(data, translate->language,
                               translate->code_page);

  } else {
    free(data);
    return NULL;
  }
}

std::wstring FileVersionInfo::company_name() {
  return GetStringValue(L"CompanyName");
}

std::wstring FileVersionInfo::company_short_name() {
  return GetStringValue(L"CompanyShortName");
}

std::wstring FileVersionInfo::internal_name() {
  return GetStringValue(L"InternalName");
}

std::wstring FileVersionInfo::product_name() {
  return GetStringValue(L"ProductName");
}

std::wstring FileVersionInfo::product_short_name() {
  return GetStringValue(L"ProductShortName");
}

std::wstring FileVersionInfo::comments() {
  return GetStringValue(L"Comments");
}

std::wstring FileVersionInfo::legal_copyright() {
  return GetStringValue(L"LegalCopyright");
}

std::wstring FileVersionInfo::product_version() {
  return GetStringValue(L"ProductVersion");
}

std::wstring FileVersionInfo::file_description() {
  return GetStringValue(L"FileDescription");
}

std::wstring FileVersionInfo::legal_trademarks() {
  return GetStringValue(L"LegalTrademarks");
}

std::wstring FileVersionInfo::private_build() {
  return GetStringValue(L"PrivateBuild");
}

std::wstring FileVersionInfo::file_version() {
  return GetStringValue(L"FileVersion");
}

std::wstring FileVersionInfo::original_filename() {
  return GetStringValue(L"OriginalFilename");
}

std::wstring FileVersionInfo::special_build() {
  return GetStringValue(L"SpecialBuild");
}

std::wstring FileVersionInfo::last_change() {
  return GetStringValue(L"LastChange");
}

bool FileVersionInfo::is_official_build() {
  return (GetStringValue(L"Official Build").compare(L"1") == 0);
}

bool FileVersionInfo::GetValue(const wchar_t* name, std::wstring* value_str) {

  WORD lang_codepage[8];
  int i = 0;
  // Use the language and codepage from the DLL.
  lang_codepage[i++] = language_;
  lang_codepage[i++] = code_page_;
  // Use the default language and codepage from the DLL.
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = code_page_;
  // Use the language from the DLL and Latin codepage (most common).
  lang_codepage[i++] = language_;
  lang_codepage[i++] = 1252;
  // Use the default language and Latin codepage (most common).
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = 1252;

  i = 0;
  while (i < arraysize(lang_codepage)) {
    wchar_t sub_block[MAX_PATH];
    WORD language = lang_codepage[i++];
    WORD code_page = lang_codepage[i++];
    _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
                 L"\\StringFileInfo\\%04x%04x\\%ls", language, code_page, name);
    LPVOID value = NULL;
    uint32 size;
    BOOL r = ::VerQueryValue(data_.get(), sub_block, &value, &size);
    if (r && value) {
      value_str->assign(static_cast<wchar_t*>(value));
      return true;
    }
  }
  return false;
}

std::wstring FileVersionInfo::GetStringValue(const wchar_t* name) {
  std::wstring str;
  if (GetValue(name, &str))
    return str;
  else
    return L"";
}

