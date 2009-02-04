// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RESOURCE_BUNDLE_H__
#define CHROME_COMMON_RESOURCE_BUNDLE_H__

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"

#if defined(OS_LINUX)
namespace base {
  class DataPack;
};
#endif
class ChromeFont;
class SkBitmap;
class StringPiece;

// ResourceBundle is a central facility to load images and other resources,
// such as theme graphics.
// Every resource is loaded only once.
class ResourceBundle {
 public:
  // An enumeration of the various font styles used throughout Chrome.
  // The following holds true for the font sizes:
  // Small <= Base <= Medium <= MediumBold <= Large.
  enum FontStyle {
    SmallFont,
    BaseFont,
    MediumFont,
    // NOTE: depending upon the locale, this may *not* result in a bold font.
    MediumBoldFont,
    LargeFont,
    WebFont
  };

  // Initialize the ResourceBundle for this process.
  static void InitSharedInstance(const std::wstring& pref_locale);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Return the global resource loader instance.
  static ResourceBundle& GetSharedInstance();

  // Load the data file that contains theme resources if present.
  void LoadThemeResources();

  // Gets the bitmap with the specified resource_id, first by looking into the
  // theme data, than in the current module data if applicable.
  // Returns a pointer to a shared instance of the SkBitmap in the given out
  // parameter. This shared bitmap is owned by the resource bundle and should
  // not be freed.
  //
  // The bitmap is assumed to exist. This function will log in release, and
  // assert in debug mode if it does not. On failure, this will return a
  // pointer to a shared empty placeholder bitmap so it will be visible what
  // is missing.
  SkBitmap* GetBitmapNamed(int resource_id);

  // Loads the raw bytes of an image resource into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  bool LoadImageResourceBytes(int resource_id,
                              std::vector<unsigned char>* bytes);

  // Loads the raw bytes of a data resource into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  bool LoadDataResourceBytes(int resource_id,
                             std::vector<unsigned char>* bytes);

  // Return the contents of a file in a string given the resource id.
  // This will copy the data from the resource and return it as a string.
  // TODO(port): deprecate this and replace with GetRawDataResource to avoid
  // needless copying.
  std::string GetDataResource(int resource_id);

  // Like GetDataResource(), but avoids copying the resource.  Instead, it
  // returns a StringPiece which points into the actual resource in the image.
  StringPiece GetRawDataResource(int resource_id);

  // Get a localized string given a message id.  Returns an empty
  // string if the message_id is not found.
  std::wstring GetLocalizedString(int message_id);

  // Returns the font for the specified style.
  ChromeFont GetFont(FontStyle style);

#if defined(OS_WIN)
  // Loads and returns an icon from the theme dll.
  HICON LoadThemeIcon(int icon_id);

  // Loads and returns the global accelerators.
  HACCEL GetGlobalAccelerators();

  // Loads and returns a cursor from the app module.
  HCURSOR LoadCursor(int cursor_id);
#endif  // OS_WIN

 private:
  // We define a DataHandle typedef to abstract across how data is stored
  // across platforms.
#if defined(OS_WIN)
  // Windows stores resources in DLLs, which are managed by HINSTANCE.
  typedef HINSTANCE DataHandle;
#elif defined(OS_LINUX)
  // Linux uses base::DataPack.
  typedef base::DataPack* DataHandle;
#elif defined(OS_MACOSX)
  // TODO(port): Implement resource loading on OS X.
  typedef void* DataHandle;
#endif

  // Ctor/dtor are private, since we're a singleton.
  ResourceBundle();
  ~ResourceBundle();

  // Free skia_images_.
  void FreeImages();

  // Try to load the main resources and the locale specific strings from an
  // external data module.
  void LoadResources(const std::wstring& pref_locale);

  // Initialize all the ChromeFont members if they haven't yet been initialized.
  void LoadFontsIfNecessary();

  // Returns the full pathname of the locale file to load.  May return an empty
  // string if no locale data files are found.
  FilePath GetLocaleFilePath(const std::wstring& pref_locale);

  // Loads the raw bytes of a resource from |module| into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  static bool LoadResourceBytes(DataHandle module,
                                int resource_id,
                                std::vector<unsigned char>* bytes);

  // Creates and returns a new SkBitmap given the data file to look in and the
  // resource id.  It's up to the caller to free the returned bitmap when
  // done.
  static SkBitmap* LoadBitmap(DataHandle dll_inst, int resource_id);

  // Class level lock.  Used to protect internal data structures that may be
  // accessed from other threads (e.g., skia_images_).
  Lock lock_;

  // Handles for data sources.
  DataHandle resources_data_;
  DataHandle locale_resources_data_;
  DataHandle theme_data_;

  // Cached images. The ResourceBundle caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  SkImageMap skia_images_;

  // The various fonts used. Cached to avoid repeated GDI creation/destruction.
  scoped_ptr<ChromeFont> base_font_;
  scoped_ptr<ChromeFont> small_font_;
  scoped_ptr<ChromeFont> medium_font_;
  scoped_ptr<ChromeFont> medium_bold_font_;
  scoped_ptr<ChromeFont> large_font_;
  scoped_ptr<ChromeFont> web_font_;

  static ResourceBundle* g_shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceBundle);
};

#endif // CHROME_COMMON_RESOURCE_BUNDLE_H__

