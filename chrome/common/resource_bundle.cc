// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/resource_bundle.h"

#include <atlbase.h>

#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "chrome/common/gfx/chrome_font.h"
#include "SkBitmap.h"

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

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
    : locale_resources_data_(NULL),
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
  bool success = LoadResourceBytes(data_handle, resource_id, &raw_data);
  if (!success)
    return NULL;

  // Decode the PNG.
  int image_width;
  int image_height;
  if (!PNGDecoder::Decode(&raw_data.front(), raw_data.size(), PNGDecoder::FORMAT_BGRA,
                          &png_data, &image_width, &image_height)) {
    NOTREACHED() << "Unable to decode image resource " << resource_id;
    return NULL;
  }

  return PNGDecoder::CreateSkBitmapFromBGRAFormat(png_data,
                                                  image_width,
                                                  image_height);
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

  // Try to load the bitmap from the theme data.
  if (theme_data_)
    bitmap.reset(LoadBitmap(theme_data_, resource_id));

#if defined(OS_WIN)
  // If we did not find the bitmap in the theme DLL, try the current one.
  if (!bitmap.get())
    bitmap.reset(LoadBitmap(_AtlBaseModule.GetModuleInstance(), resource_id));
#endif

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
