// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include <atlbase.h>

#include "app/app_paths.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/string_piece.h"
#include "base/win_util.h"

namespace {

// Returns the flags that should be passed to LoadLibraryEx.
DWORD GetDataDllLoadFlags() {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
    return LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;

  return DONT_RESOLVE_DLL_REFERENCES;
}

}  // end anonymous namespace

ResourceBundle::~ResourceBundle() {
  FreeImages();

  if (locale_resources_data_) {
    BOOL rv = FreeLibrary(locale_resources_data_);
    DCHECK(rv);
  }
  if (theme_data_) {
    BOOL rv = FreeLibrary(theme_data_);
    DCHECK(rv);
  }
}

void ResourceBundle::LoadResources(const std::wstring& pref_locale) {
  // As a convenience, set resources_data_ to the current module.
  resources_data_ = _AtlBaseModule.GetModuleInstance();

  DCHECK(NULL == locale_resources_data_) << "locale dll already loaded";
  const FilePath& locale_path = GetLocaleFilePath(pref_locale);
  if (locale_path.value().empty()) {
    // It's possible that there are no locale dlls found, in which case we just
    // return.
    NOTREACHED();
    return;
  }

  // The dll should only have resources, not executable code.
  locale_resources_data_ = LoadLibraryEx(locale_path.value().c_str(), NULL,
                                         GetDataDllLoadFlags());
  DCHECK(locale_resources_data_ != NULL) <<
      "unable to load generated resources";
}

FilePath ResourceBundle::GetLocaleFilePath(const std::wstring& pref_locale) {
  FilePath locale_path;
  PathService::Get(app::DIR_LOCALES, &locale_path);

  const std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (app_locale.empty())
    return FilePath();

  return locale_path.AppendASCII(app_locale + ".dll");
}

void ResourceBundle::LoadThemeResources() {
  DCHECK(NULL == theme_data_) << "theme dll already loaded";
  std::wstring theme_data_path;
  PathService::Get(app::DIR_THEMES, &theme_data_path);
  file_util::AppendToPath(&theme_data_path, L"default.dll");

  // The dll should only have resources, not executable code.
  theme_data_ = LoadLibraryEx(theme_data_path.c_str(), NULL,
                             GetDataDllLoadFlags());
  DCHECK(theme_data_ != NULL) << "unable to load " << theme_data_path;
}

/* static */
bool ResourceBundle::LoadResourceBytes(
    DataHandle module,
    int resource_id,
    std::vector<unsigned char>* bytes) {
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                      &data_size)) {
    bytes->resize(data_size);
    memcpy(&(bytes->front()), data_ptr, data_size);
    return true;
  } else {
    return false;
  }
}

HICON ResourceBundle::LoadThemeIcon(int icon_id) {
  return ::LoadIcon(theme_data_, MAKEINTRESOURCE(icon_id));
}

StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(_AtlBaseModule.GetModuleInstance(),
                                      resource_id,
                                      &data_ptr,
                                      &data_size)) {
    return StringPiece(static_cast<const char*>(data_ptr), data_size);
  } else if (locale_resources_data_ &&
             base::GetDataResourceFromModule(locale_resources_data_,
                                             resource_id,
                                             &data_ptr,
                                             &data_size)) {
    return StringPiece(static_cast<const char*>(data_ptr), data_size);
  }
  return StringPiece();
}

// Loads and returns a cursor from the current module.
HCURSOR ResourceBundle::LoadCursor(int cursor_id) {
  return ::LoadCursor(_AtlBaseModule.GetModuleInstance(),
                      MAKEINTRESOURCE(cursor_id));
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  DCHECK(IS_INTRESOURCE(message_id));

  // Get a reference directly to the string resource.
  HINSTANCE hinstance = locale_resources_data_;
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(hinstance,
                                                                  message_id);
  if (!image) {
    // Fall back on the current module (shouldn't be any strings here except
    // in unittests).
    image = AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                      message_id);
    if (!image) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return std::wstring();
    }
  }
  // Copy into a string16 and return.
  return string16(image->achString, image->nLength);
}
