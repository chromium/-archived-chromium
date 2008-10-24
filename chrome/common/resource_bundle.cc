// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/resource_bundle.h"

#include <atlbase.h>

#include "base/file_util.h"
#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/win_util.h"
#include "SkBitmap.h"

using namespace std;

ResourceBundle *ResourceBundle::g_shared_instance_ = NULL;

// Returns the flags that should be passed to LoadLibraryEx.
DWORD GetDataDllLoadFlags() {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
    return LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;

  return DONT_RESOLVE_DLL_REFERENCES;
}

/* static */
void ResourceBundle::InitSharedInstance(const std::wstring& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();

  g_shared_instance_->LoadLocaleResources(pref_locale);
}

/* static */
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

/* static */
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

ResourceBundle::ResourceBundle()
    : locale_resources_dll_(NULL),
      theme_dll_(NULL) {
}

ResourceBundle::~ResourceBundle() {
  for (SkImageMap::iterator i = skia_images_.begin();
	   i != skia_images_.end(); i++) {
    delete i->second;
  }
  skia_images_.clear();

  if (locale_resources_dll_) {
    BOOL rv = FreeLibrary(locale_resources_dll_);
    DCHECK(rv);
  }
  if (theme_dll_) {
    BOOL rv = FreeLibrary(theme_dll_);
    DCHECK(rv);
  }
}

void ResourceBundle::LoadLocaleResources(const std::wstring& pref_locale) {
  DCHECK(NULL == locale_resources_dll_) << "locale dll already loaded";
  const std::wstring& locale_path = GetLocaleDllPath(pref_locale);
  if (locale_path.empty()) {
    // It's possible that there are no locale dlls found, in which case we just
    // return.
    NOTREACHED();
    return;
  }

  // The dll should only have resources, not executable code.
  locale_resources_dll_ = LoadLibraryEx(locale_path.c_str(), NULL,
                                        GetDataDllLoadFlags());
  DCHECK(locale_resources_dll_ != NULL) << "unable to load generated resources";
}

std::wstring ResourceBundle::GetLocaleDllPath(const std::wstring& pref_locale) {
  std::wstring locale_path;
  PathService::Get(chrome::DIR_LOCALES, &locale_path);

  const std::wstring app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (app_locale.empty())
    return app_locale;

  file_util::AppendToPath(&locale_path, app_locale + L".dll");
  return locale_path;
}

void ResourceBundle::LoadThemeResources() {
  DCHECK(NULL == theme_dll_) << "theme dll already loaded";
  std::wstring theme_dll_path;
  PathService::Get(chrome::DIR_THEMES, &theme_dll_path);
  file_util::AppendToPath(&theme_dll_path, L"default.dll");

  // The dll should only have resources, not executable code.
  theme_dll_ = LoadLibraryEx(theme_dll_path.c_str(), NULL,
                             GetDataDllLoadFlags());
  DCHECK(theme_dll_ != NULL) << "unable to load " << theme_dll_path;
}

/* static */
SkBitmap* ResourceBundle::LoadBitmap(HINSTANCE dll_inst, int resource_id) {
  void* data_ptr = NULL;
  size_t data_size;
  bool success = base::GetDataResourceFromModule(dll_inst, resource_id,
                                                 &data_ptr, &data_size);
  if (!success)
    return NULL;

  unsigned char* data = static_cast<unsigned char*>(data_ptr);

  // Decode the PNG.
  vector<unsigned char> png_data;
  int image_width;
  int image_height;
  if (!PNGDecoder::Decode(data, data_size, PNGDecoder::FORMAT_BGRA,
                          &png_data, &image_width, &image_height)) {
    NOTREACHED() << "Unable to decode theme resource " << resource_id;
    return NULL;
  }

  return PNGDecoder::CreateSkBitmapFromBGRAFormat(png_data,
                                                  image_width,
                                                  image_height);
}

SkBitmap* ResourceBundle::GetBitmapNamed(int resource_id) {
  AutoLock lock_scope(lock_);

  SkImageMap::const_iterator found = skia_images_.find(resource_id);
  SkBitmap* bitmap = NULL;

  // If not found load and store the image
  if (found == skia_images_.end()) {
    // Try to load the bitmap from the theme dll.
    if (theme_dll_)
      bitmap = LoadBitmap(theme_dll_, resource_id);
    // We did not find the bitmap in the theme DLL, try the current one.
    if (!bitmap)
      bitmap = LoadBitmap(_AtlBaseModule.GetModuleInstance(), resource_id);
    skia_images_[resource_id] = bitmap;
  } else {
    bitmap = found->second;
  }

  // This bitmap will be returned when a bitmap is requested that can not be
  // found. The data inside is lazily initialized, so users must lock and
  static SkBitmap* empty_bitmap = NULL;

  // Handle the case where loading the bitmap failed.
  if (!bitmap) {
    LOG(WARNING) << "Unable to load bitmap with id " << resource_id;
    NOTREACHED(); // Want to assert in debug mode.
    if (!empty_bitmap) {
      // The placeholder bitmap is bright red so people notice the problem.	
      empty_bitmap = new SkBitmap();	
      empty_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);	
      empty_bitmap->allocPixels();	
      empty_bitmap->eraseARGB(255, 255, 0, 0);
    }
    return empty_bitmap;
  }
  return bitmap;
}

bool ResourceBundle::LoadImageResourceBytes(int resource_id,
                                            vector<unsigned char>* bytes) {
  return LoadModuleResourceBytes(theme_dll_, resource_id, bytes);
}

bool ResourceBundle::LoadDataResourceBytes(int resource_id,
                                           vector<unsigned char>* bytes) {
  return LoadModuleResourceBytes(_AtlBaseModule.GetModuleInstance(),
                                 resource_id, bytes);
}

bool ResourceBundle::LoadModuleResourceBytes(
    HINSTANCE module,
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
  return ::LoadIcon(theme_dll_, MAKEINTRESOURCE(icon_id));
}

std::string ResourceBundle::GetDataResource(int resource_id) {
  return GetRawDataResource(resource_id).as_string();
}

StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(_AtlBaseModule.GetModuleInstance(), 
                                      resource_id, 
                                      &data_ptr, 
                                      &data_size)) {
    return StringPiece(static_cast<const char*>(data_ptr), data_size);
  } else if (locale_resources_dll_ && 
             base::GetDataResourceFromModule(locale_resources_dll_, 
                                             resource_id, 
                                             &data_ptr, 
                                             &data_size)) {
    return StringPiece(static_cast<const char*>(data_ptr), data_size);
  }
  return StringPiece();
}
// Loads and returns the global accelerators from the current module.
HACCEL ResourceBundle::GetGlobalAccelerators() {
  return ::LoadAccelerators(_AtlBaseModule.GetModuleInstance(),
                            MAKEINTRESOURCE(IDR_MAINFRAME));
}

// Loads and returns a cursor from the current module.
HCURSOR ResourceBundle::LoadCursor(int cursor_id) {
  return ::LoadCursor(_AtlBaseModule.GetModuleInstance(),
                      MAKEINTRESOURCE(cursor_id));
}

std::wstring ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_dll_)
    return std::wstring();

  DCHECK(IS_INTRESOURCE(message_id));

  // Get a reference directly to the string resource.
  HINSTANCE hinstance = locale_resources_dll_;
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
  // Copy into a wstring and return.
  return std::wstring(image->achString, image->nLength);
}

void ResourceBundle::LoadFontsIfNecessary() {
  AutoLock lock_scope(lock_);
  if (!base_font_.get()) {
    base_font_.reset(new ChromeFont());

    small_font_.reset(new ChromeFont());
    *small_font_ = base_font_->DeriveFont(-2);

    medium_font_.reset(new ChromeFont());
    *medium_font_ = base_font_->DeriveFont(3);

    medium_bold_font_.reset(new ChromeFont());
    *medium_bold_font_ =
        base_font_->DeriveFont(3, base_font_->style() | ChromeFont::BOLD);

    large_font_.reset(new ChromeFont());
    *large_font_ = base_font_->DeriveFont(8);

    web_font_.reset(new ChromeFont());
    *web_font_ = base_font_->DeriveFont(1,
        base_font_->style() | ChromeFont::WEB);
  }
}

ChromeFont ResourceBundle::GetFont(FontStyle style) {
  LoadFontsIfNecessary();
  switch(style) {
    case SmallFont:
      return *small_font_;
    case MediumFont:
      return *medium_font_;
    case MediumBoldFont:
      return *medium_bold_font_;
    case LargeFont:
      return *large_font_;
    case WebFont:
      return *web_font_;
    default:
      return *base_font_;
  }
}

