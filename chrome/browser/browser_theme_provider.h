// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
#define CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "skia/ext/skia_utils.h"

class Extension;
class Profile;
class DictionaryValue;

class BrowserThemeProvider : public base::RefCounted<BrowserThemeProvider>,
                             public NonThreadSafe,
                             public ThemeProvider {
 public:
  BrowserThemeProvider();
  virtual ~BrowserThemeProvider();

  enum {
    COLOR_FRAME,
    COLOR_FRAME_INACTIVE,
    COLOR_FRAME_INCOGNITO,
    COLOR_FRAME_INCOGNITO_INACTIVE,
    COLOR_TOOLBAR,
    COLOR_TAB_TEXT,
    COLOR_BACKGROUND_TAB_TEXT,
    COLOR_BOOKMARK_TEXT,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_TEXT,
    COLOR_NTP_LINK,
    COLOR_NTP_SECTION,
    COLOR_NTP_SECTION_TEXT,
    COLOR_NTP_SECTION_LINK,
    COLOR_CONTROL_BACKGROUND,
    COLOR_BUTTON_BACKGROUND,
    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,
    NTP_BACKGROUND_ALIGNMENT
  };

  // A bitfield mask for alignments.
  typedef enum {
    ALIGN_CENTER = 0x0,
    ALIGN_LEFT = 0x1,
    ALIGN_TOP = 0x2,
    ALIGN_RIGHT = 0x4,
    ALIGN_BOTTOM = 0x8,
  } AlignmentMasks;

  void Init(Profile* profile);

  // ThemeProvider implementation.
  virtual SkBitmap* GetBitmapNamed(int id);
  virtual SkColor GetColor(int id);
  virtual bool GetDisplayProperty(int id, int* result);
  virtual bool ShouldUseNativeFrame();
  virtual bool HasCustomImage(int id);
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  virtual GdkPixbuf* GetPixbufNamed(int id);
#elif defined(OS_MACOSX)
  virtual NSImage* GetNSImageNamed(int id);
  virtual NSColor* GetNSColorTint(int id);
#endif

  // Set the current theme to the theme defined in |extension|.
  void SetTheme(Extension* extension);

  // Reset the theme to default.
  void UseDefaultTheme();

  // Convert a bitfield alignment into a string like "top left". Public so that
  // it can be used to generate CSS values. Takes a bitfield of AlignmentMasks.
  static std::string AlignmentToString(int alignment);

  // Parse alignments from something like "top left" into a bitfield of
  // AlignmentMasks
  static int StringToAlignment(const std::string &alignment);

 private:
  typedef std::map<const int, std::string> ImageMap;
  typedef std::map<const std::string, SkColor> ColorMap;
  typedef std::map<const std::string, skia::HSL> TintMap;
  typedef std::map<const std::string, int> DisplayPropertyMap;

  // Reads the image data from the theme file into the specified vector. Returns
  // true on success.
  bool ReadThemeFileData(int id, std::vector<unsigned char>* raw_data);

  // Loads a bitmap from the theme, which may be tinted or
  // otherwise modified, or an application default.
  SkBitmap* LoadThemeBitmap(int id);

  // Returns the string key for the given tint |id| TINT_* enum value.
  const std::string GetTintKey(int id);

  // Returns the default tint for the given tint |id| TINT_* enum value.
  skia::HSL GetDefaultTint(int id);

  // Get the specified tint - |id| is one of the TINT_* enum values.
  skia::HSL GetTint(int id);

  // Tint |bitmap| with the tint specified by |hsl_id|
  SkBitmap TintBitmap(const SkBitmap& bitmap, int hsl_id);

  // The following load data from specified dictionaries (either from
  // preferences or from an extension manifest) and update our theme
  // data appropriately.
  // Allow any ResourceBundle image to be overridden. |images| should
  // contain keys defined in ThemeResourceMap, and values as paths to
  // the images on-disk.
  void SetImageData(DictionaryValue* images,
                    FilePath images_path);
  // Set our theme colors. The keys of |colors| are any of the kColor*
  // constants, and the values are a three-item list containing 8-bit
  // RGB values.
  void SetColorData(DictionaryValue* colors);

  // Set tint data for our images and colors. The keys of |tints| are
  // any of the kTint* contstants, and the values are a three-item list
  // containing real numbers in the range 0-1 (and -1 for 'null').
  void SetTintData(DictionaryValue* tints);

  // Set miscellaneous display properties. While these can be defined as
  // strings, they are currently stored as integers.
  void SetDisplayPropertyData(DictionaryValue* display_properties);

  // Generate any frame colors that weren't specified.
  void GenerateFrameColors();

  // Generate any frame images that weren't specified. The resulting images
  // will be stored in our cache.
  void GenerateFrameImages();

  // Create any images that aren't pregenerated (e.g. background tab images).
  SkBitmap* GenerateBitmap(int id);

  // Save our data - when saving images we need the original dictionary
  // from the extension because it contains the text ids that we want to save.
  void SaveImageData(DictionaryValue* images);
  void SaveColorData();
  void SaveTintData();
  void SaveDisplayPropertyData();

  // Let all the browser views know that themes have changed.
  void NotifyThemeChanged();

  // Load theme data from preferences.
  void LoadThemePrefs();

  SkColor FindColor(const char* id, SkColor default_color);

  // Frees generated images and clears the image cache.
  void ClearCaches();

  // Clears the platform-specific caches. Do not call directly; it's called
  // from ClearCaches().
  void FreePlatformCaches();

  // Cached images. We cache all retrieved and generated bitmaps and keep
  // track of the pointers. We own these and will delete them when we're done
  // using them.
  typedef std::map<int, SkBitmap*> ImageCache;
  ImageCache image_cache_;
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  typedef std::map<int, GdkPixbuf*> GdkPixbufMap;
  GdkPixbufMap gdk_pixbufs_;
#elif defined(OS_MACOSX)
  typedef std::map<int, NSImage*> NSImageMap;
  NSImageMap nsimage_cache_;
  typedef std::map<int, NSColor*> NSColorMap;
  NSColorMap nscolor_cache_;
#endif

  ResourceBundle& rb_;
  Profile* profile_;

  ImageMap images_;
  ColorMap colors_;
  TintMap tints_;
  DisplayPropertyMap display_properties_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThemeProvider);
};

#endif  // CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
