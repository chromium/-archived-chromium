// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include <gtk/gtk.h>

#include "app/app_paths.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/base_paths.h"
#include "base/data_pack.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Convert the raw image data into a GdkPixbuf.  The GdkPixbuf that is returned
// has a ref count of 1 so the caller must call g_object_unref to free the
// memory.
GdkPixbuf* LoadPixbuf(std::vector<unsigned char>& data, bool rtl_enabled) {
  ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());
  bool ok = gdk_pixbuf_loader_write(loader.get(),
      static_cast<guint8*>(data.data()), data.size(), NULL);
  if (!ok)
    return NULL;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader.get(), NULL);
  if (!ok)
    return NULL;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());
  if (!pixbuf)
    return NULL;

  if ((l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) &&
      rtl_enabled) {
    // |pixbuf| will get unreffed and destroyed (see below). The returned value
    // has ref count 1.
    return gdk_pixbuf_flip(pixbuf, TRUE);
  } else {
    // The pixbuf is owned by the loader, so add a ref so when we delete the
    // loader (when the ScopedGObject goes out of scope), the pixbuf still
    // exists.
    g_object_ref(pixbuf);
    return pixbuf;
  }
}

}  // namespace

ResourceBundle::~ResourceBundle() {
  FreeImages();
  // Free GdkPixbufs.
  for (GdkPixbufMap::iterator i = gdk_pixbufs_.begin();
       i != gdk_pixbufs_.end(); i++) {
    g_object_unref(i->second);
  }
  gdk_pixbufs_.clear();

  delete locale_resources_data_;
  locale_resources_data_ = NULL;
  delete theme_data_;
  theme_data_ = NULL;
  delete resources_data_;
  resources_data_ = NULL;
}

void ResourceBundle::LoadResources(const std::wstring& pref_locale) {
  FilePath resources_data_path;
  PathService::Get(base::DIR_EXE, &resources_data_path);
  resources_data_path = resources_data_path.Append(
      FILE_PATH_LITERAL("chrome.pak"));
  DCHECK(resources_data_ == NULL) << "resource data already loaded!";
  resources_data_ = new base::DataPack;
  bool success = resources_data_->Load(resources_data_path);
  DCHECK(success) << "failed to load chrome.pak";

  DCHECK(locale_resources_data_ == NULL) << "locale data already loaded!";
  const FilePath& locale_path = GetLocaleFilePath(pref_locale);
  if (locale_path.value().empty()) {
    // It's possible that there are no locale dlls found, in which case we just
    // return.
    NOTREACHED();
    return;
  }

  locale_resources_data_ = new base::DataPack;
  success = locale_resources_data_->Load(locale_path);
  DCHECK(success) << "failed to load locale pak file";
}

FilePath ResourceBundle::GetLocaleFilePath(const std::wstring& pref_locale) {
  FilePath locale_path;
  PathService::Get(app::DIR_LOCALES, &locale_path);

  const std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (app_locale.empty())
    return FilePath();

  return locale_path.AppendASCII(app_locale + ".pak");
}

void ResourceBundle::LoadThemeResources() {
  FilePath theme_data_path;
  PathService::Get(app::DIR_THEMES, &theme_data_path);
  theme_data_path = theme_data_path.Append(FILE_PATH_LITERAL("default.pak"));
  theme_data_ = new base::DataPack;
  bool success = theme_data_->Load(theme_data_path);
  DCHECK(success) << "failed to load theme data";
}

/* static */
bool ResourceBundle::LoadResourceBytes(DataHandle module, int resource_id,
                                       std::vector<unsigned char>* bytes) {
  DCHECK(module);
  StringPiece data;
  if (!module->Get(resource_id, &data))
    return false;

  bytes->resize(data.length());
  memcpy(&(bytes->front()), data.data(), data.length());

  return true;
}

StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  DCHECK(resources_data_);
  StringPiece data;
  if (!resources_data_->Get(resource_id, &data))
    return StringPiece();
  return data;
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  StringPiece data;
  if (!locale_resources_data_->Get(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return string16();
    }
  }

  // Data pack encodes strings as UTF16.
  string16 msg(reinterpret_cast<const char16*>(data.data()),
               data.length() / 2);
  return msg;
}

GdkPixbuf* ResourceBundle::GetPixbufImpl(int resource_id, bool rtl_enabled) {
  // Use the negative |resource_id| for the key for BIDI-aware images.
  int key = rtl_enabled ? -resource_id : resource_id;

  // Check to see if we already have the pixbuf in the cache.
  {
    AutoLock lock_scope(lock_);
    GdkPixbufMap::const_iterator found = gdk_pixbufs_.find(key);
    if (found != gdk_pixbufs_.end())
      return found->second;
  }


  std::vector<unsigned char> data;
  LoadImageResourceBytes(resource_id, &data);
  GdkPixbuf* pixbuf = LoadPixbuf(data, rtl_enabled);

  // We loaded successfully.  Cache the pixbuf.
  if (pixbuf) {
    AutoLock lock_scope(lock_);

    // Another thread raced us, and has already cached the pixbuf.
    if (gdk_pixbufs_.count(key)) {
      g_object_unref(pixbuf);
      return gdk_pixbufs_[key];
    }

    gdk_pixbufs_[key] = pixbuf;
    return pixbuf;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  {
    LOG(WARNING) << "Unable to load GdkPixbuf with id " << resource_id;
    NOTREACHED();  // Want to assert in debug mode.

    AutoLock lock_scope(lock_);  // Guard empty_bitmap initialization.

    static GdkPixbuf* empty_bitmap = NULL;
    if (!empty_bitmap) {
      // The placeholder bitmap is bright red so people notice the problem.
      // This bitmap will be leaked, but this code should never be hit.
      scoped_ptr<SkBitmap> skia_bitmap(new SkBitmap());
      skia_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
      skia_bitmap->allocPixels();
      skia_bitmap->eraseARGB(255, 255, 0, 0);
      empty_bitmap = gfx::GdkPixbufFromSkBitmap(skia_bitmap.get());
    }
    return empty_bitmap;
  }
}

GdkPixbuf* ResourceBundle::GetPixbufNamed(int resource_id) {
  return GetPixbufImpl(resource_id, false);
}

GdkPixbuf* ResourceBundle::GetRTLEnabledPixbufNamed(int resource_id) {
  return GetPixbufImpl(resource_id, true);
}
