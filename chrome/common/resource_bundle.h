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

#ifndef CHROME_COMMON_RESOURCE_BUNDLE_H__
#define CHROME_COMMON_RESOURCE_BUNDLE_H__

#include <windows.h>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ref_counted_util.h"

class ChromeFont;
class SkBitmap;
class StringPiece;

////////////////////////////////////////////////////////////////////////////////
//
// ResourceBundle class
//
// ResourceBundle is a central facility to load images and other resources.
//
// Every resource is loaded only once
//
////////////////////////////////////////////////////////////////////////////////
class ResourceBundle {
 public:
  // An enumeration of the various font styles used throughout Chrome.
  // The following holds true for the font sizes:
  // Small <= Base <= Medium <= MediumBold <= Large.
  enum FontStyle {
    SmallFont,
    BaseFont,
    MediumFont,
    // NOTE: depending upon the locale, this may *not* result in a bold
    // font.
    MediumBoldFont,
    LargeFont,
    WebFont
  };

  // Initialize the ResrouceBundle for this process.
  static void InitSharedInstance(const std::wstring& pref_locale);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Return the global resource loader instance;
  static ResourceBundle& GetSharedInstance();

  // Load the dll that contains theme resources if present.
  void LoadThemeResources();

  // Gets the bitmap with the specified resource_id, first by looking into the
  // theme dll, than in the current dll.  Returns a pointer to a shared instance
  // of the SkBitmap in the given out parameter. This shared bitmap is owned by
  // the resource bundle and should not be freed.
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

  // Loads and returns an icon from the theme dll.
  HICON LoadThemeIcon(int icon_id);

  // Return the contents of a file in a string given the resource id.
  // This will copy the data from the resource and return it as a string.
  std::string GetDataResource(int resource_id);

  // Like GetDataResource(), but avoids copying the resource.  Instead, it
  // returns a StringPiece which points into the actual resource in the image.
  StringPiece GetRawDataResource(int resource_id);

  // Loads and returns the global accelerators.
  HACCEL GetGlobalAccelerators();

  // Loads and returns a cursor from the app module.
  HCURSOR LoadCursor(int cursor_id);

  // Get a localized string given a message id.  Returns an empty
  // string if the message_id is not found.
  std::wstring GetLocalizedString(int message_id);

  // Returns the font for the specified style.
  ChromeFont GetFont(FontStyle style);

  // Creates and returns a new SkBitmap given the dll to look in and the
  // resource id.  It's up to the caller to free the returned bitmap when
  // done.
  static SkBitmap* LoadBitmap(HINSTANCE dll_inst, int resource_id);

 private:
  ResourceBundle();
  ~ResourceBundle();

  // Try to load the locale specific strings from an external DLL.
  void LoadLocaleResources(const std::wstring& pref_locale);

  void LoadFontsIfNecessary();

  // Returns the full pathname of the locale dll to load.  May return an empty
  // string if no locale dlls are found.
  std::wstring GetLocaleDllPath(const std::wstring& pref_locale);

  // Loads the raw bytes of a resource from |module| into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  bool LoadModuleResourceBytes(HINSTANCE module,
                               int resource_id,
                               std::vector<unsigned char>* bytes);

  // Class level lock.  Used to protect internal data structures that may be
  // accessed from other threads (e.g., skia_images_).
  Lock lock_;

  std::wstring theme_path_;

  // Handle to dlls
  HINSTANCE locale_resources_dll_;
  HINSTANCE theme_dll_;

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

  static ResourceBundle *g_shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceBundle);
};

#endif // CHROME_COMMON_RESOURCE_BUNDLE_H__
