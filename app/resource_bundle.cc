// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include "app/gfx/font.h"
#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "third_party/skia/include/core/SkBitmap.h"

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

/* static */
// TODO(glen): Finish moving these into theme provider (dialogs still
//    depend on these colors).
const SkColor ResourceBundle::frame_color =
     SkColorSetRGB(77, 139, 217);
const SkColor ResourceBundle::frame_color_inactive =
     SkColorSetRGB(184, 209, 240);
const SkColor ResourceBundle::frame_color_incognito =
     SkColorSetRGB(83, 106, 139);
const SkColor ResourceBundle::frame_color_incognito_inactive =
     SkColorSetRGB(126, 139, 156);
const SkColor ResourceBundle::toolbar_color =
     SkColorSetRGB(210, 225, 246);
const SkColor ResourceBundle::toolbar_separator_color =
     SkColorSetRGB(182, 186, 192);

/* static */
void ResourceBundle::InitSharedInstance(const std::wstring& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();

  g_shared_instance_->LoadResources(pref_locale);
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
    : resources_data_(NULL),
      locale_resources_data_(NULL),
      theme_data_(NULL) {
}

void ResourceBundle::FreeImages() {
  for (SkImageMap::iterator i = skia_images_.begin();
       i != skia_images_.end(); i++) {
    delete i->second;
  }
  skia_images_.clear();
}

/* static */
SkBitmap* ResourceBundle::LoadBitmap(DataHandle data_handle, int resource_id) {
  std::vector<unsigned char> raw_data, png_data;
  bool success = false;

  if (!success)
    success = LoadResourceBytes(data_handle, resource_id, &raw_data);
  if (!success)
    return NULL;

  // Decode the PNG.
  int image_width;
  int image_height;
  if (!PNGDecoder::Decode(&raw_data.front(), raw_data.size(),
                          PNGDecoder::FORMAT_BGRA,
                          &png_data, &image_width, &image_height)) {
    NOTREACHED() << "Unable to decode image resource " << resource_id;
    return NULL;
  }

  return PNGDecoder::CreateSkBitmapFromBGRAFormat(png_data,
                                                  image_width,
                                                  image_height);
}

std::string ResourceBundle::GetDataResource(int resource_id) {
  return GetRawDataResource(resource_id).as_string();
}

bool ResourceBundle::LoadImageResourceBytes(int resource_id,
                                            std::vector<unsigned char>* bytes) {
  return LoadResourceBytes(theme_data_, resource_id, bytes);
}

bool ResourceBundle::LoadDataResourceBytes(int resource_id,
                                           std::vector<unsigned char>* bytes) {
  return LoadResourceBytes(resources_data_, resource_id, bytes);
}

SkBitmap* ResourceBundle::GetBitmapNamed(int resource_id) {
  // Check to see if we already have the Skia image in the cache.
  {
    AutoLock lock_scope(lock_);
    SkImageMap::const_iterator found = skia_images_.find(resource_id);
    if (found != skia_images_.end())
      return found->second;
  }

  scoped_ptr<SkBitmap> bitmap;

  if (theme_data_)
    bitmap.reset(LoadBitmap(theme_data_, resource_id));

  // If we did not find the bitmap in the theme DLL, try the current one.
  if (!bitmap.get())
    bitmap.reset(LoadBitmap(resources_data_, resource_id));

  // We loaded successfully.  Cache the Skia version of the bitmap.
  if (bitmap.get()) {
    AutoLock lock_scope(lock_);

    // Another thread raced us, and has already cached the skia image.
    if (skia_images_.count(resource_id))
      return skia_images_[resource_id];

    skia_images_[resource_id] = bitmap.get();
    return bitmap.release();
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  {
    LOG(WARNING) << "Unable to load bitmap with id " << resource_id;
    NOTREACHED();  // Want to assert in debug mode.

    AutoLock lock_scope(lock_);  // Guard empty_bitmap initialization.

    static SkBitmap* empty_bitmap = NULL;
    if (!empty_bitmap) {
      // The placeholder bitmap is bright red so people notice the problem.
      // This bitmap will be leaked, but this code should never be hit.
      empty_bitmap = new SkBitmap();
      empty_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
      empty_bitmap->allocPixels();
      empty_bitmap->eraseARGB(255, 255, 0, 0);
    }
    return empty_bitmap;
  }
}

void ResourceBundle::LoadFontsIfNecessary() {
  AutoLock lock_scope(lock_);
  if (!base_font_.get()) {
    base_font_.reset(new gfx::Font());
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
    // Toolkit views needs a less gigantor base font to more correctly match
    // metrics for the bitmap-based UI.
    *base_font_ = base_font_->DeriveFont(-1);
#endif

    small_font_.reset(new gfx::Font());
    *small_font_ = base_font_->DeriveFont(-2);

    medium_font_.reset(new gfx::Font());
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
    *medium_font_ = base_font_->DeriveFont(2);
#else
    *medium_font_ = base_font_->DeriveFont(3);
#endif

    medium_bold_font_.reset(new gfx::Font());
    *medium_bold_font_ =
        base_font_->DeriveFont(3, base_font_->style() | gfx::Font::BOLD);

    large_font_.reset(new gfx::Font());
    *large_font_ = base_font_->DeriveFont(8);
  }
}

gfx::Font ResourceBundle::GetFont(FontStyle style) {
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
    default:
      return *base_font_;
  }
}
