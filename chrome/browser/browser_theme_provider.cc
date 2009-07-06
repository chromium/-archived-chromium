// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#include "base/gfx/png_decoder.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/theme_resources_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/skia_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif


namespace themes {

// Strings used by themes to identify colors for different parts of our UI.
const char* kColorFrame = "frame";
const char* kColorFrameInactive = "frame_inactive";
const char* kColorFrameIncognito = "frame_incognito";
const char* kColorFrameIncognitoInactive = "frame_incognito_inactive";
const char* kColorToolbar = "toolbar";
const char* kColorTabText = "tab_text";
const char* kColorBackgroundTabText = "background_tab_text";
const char* kColorBookmarkText = "bookmark_text";
const char* kColorNTPBackground = "ntp_background";
const char* kColorNTPText = "ntp_text";
const char* kColorNTPLink = "ntp_link";
const char* kColorNTPSection = "ntp_section";
const char* kColorNTPSectionText = "ntp_section_text";
const char* kColorNTPSectionLink = "ntp_section_link";
const char* kColorControlBackground = "control_background";
const char* kColorButtonBackground = "button_background";

// Strings used by themes to identify tints to apply to different parts of
// our UI. The frame tints apply to the frame color and produce the
// COLOR_FRAME* colors.
const char* kTintButtons = "buttons";
const char* kTintFrame = "frame";
const char* kTintFrameInactive = "frame_inactive";
const char* kTintFrameIncognito = "frame_incognito";
const char* kTintFrameIncognitoInactive = "frame_incognito_inactive";
const char* kTintBackgroundTab = "background_tab";

// Strings used by themes to identify miscellaneous numerical properties.
const char* kDisplayPropertyNTPAlignment = "ntp_background_alignment";

// Strings used in alignment properties.
const char* kAlignmentTop = "top";
const char* kAlignmentBottom = "bottom";
const char* kAlignmentLeft = "left";
const char* kAlignmentRight = "right";

// Default colors.
const SkColor kDefaultColorFrame = SkColorSetRGB(77, 139, 217);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(152, 188, 233);
const SkColor kDefaultColorFrameIncognito = SkColorSetRGB(83, 106, 139);
const SkColor kDefaultColorFrameIncognitoInactive =
    SkColorSetRGB(126, 139, 156);
const SkColor kDefaultColorToolbar = SkColorSetRGB(210, 225, 246);
const SkColor kDefaultColorTabText = SkColorSetRGB(0, 0, 0);
const SkColor kDefaultColorBackgroundTabText = SkColorSetRGB(64, 64, 64);
const SkColor kDefaultColorBookmarkText = SkColorSetRGB(64, 64, 64);
const SkColor kDefaultColorNTPBackground = SkColorSetRGB(255, 255, 255);
const SkColor kDefaultColorNTPText = SkColorSetRGB(0, 0, 0);
const SkColor kDefaultColorNTPLink = SkColorSetRGB(0, 0, 204);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(225, 236, 254);
const SkColor kDefaultColorNTPSectionText = SkColorSetRGB(0, 0, 0);
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(0, 0, 204);
const SkColor kDefaultColorControlBackground = NULL;
const SkColor kDefaultColorButtonBackground = NULL;

// Default tints.
const skia::HSL kDefaultTintButtons = { -1, -1, -1 };
const skia::HSL kDefaultTintFrame = { -1, -1, -1 };
const skia::HSL kDefaultTintFrameInactive = { -1, 0.5f, 0.72f };
const skia::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const skia::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const skia::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };
}  // namespace themes

// We really want every member of the previous namespace to be exposed
// here. The alternative is to list every member of namespace themes in a using
// directive.
using namespace themes;

// Default display properties.
static const int kDefaultDisplayPropertyNTPAlignment =
    BrowserThemeProvider::ALIGN_BOTTOM;

// The image resources that will be tinted by the 'button' tint value.
static const int kToolbarButtonIDs[] = {
  IDR_BACK, IDR_BACK_D, IDR_BACK_H, IDR_BACK_P,
  IDR_FORWARD, IDR_FORWARD_D, IDR_FORWARD_H, IDR_FORWARD_P,
  IDR_RELOAD, IDR_RELOAD_H, IDR_RELOAD_P,
  IDR_HOME, IDR_HOME_H, IDR_HOME_P,
  IDR_STAR, IDR_STAR_D, IDR_STAR_H, IDR_STAR_P,
  IDR_STARRED, IDR_STARRED_H, IDR_STARRED_P,
  IDR_GO, IDR_GO_H, IDR_GO_P,
  IDR_STOP, IDR_STOP_H, IDR_STOP_P,
  IDR_MENU_PAGE, IDR_MENU_PAGE_RTL,
  IDR_MENU_CHROME, IDR_MENU_CHROME_RTL,
  IDR_MENU_DROPARROW,
  IDR_THROBBER,
};

// A map for kToolbarButtonIDs.
static std::map<const int, bool> button_images_;

// A map of frame image IDs to the tints for those ids.
static std::map<const int, int> frame_tints_;

BrowserThemeProvider::BrowserThemeProvider()
    : rb_(ResourceBundle::GetSharedInstance()) {
  static bool initialized = false;
  if (!initialized) {
    for (size_t i = 0; i < arraysize(kToolbarButtonIDs); ++i) {
      button_images_[kToolbarButtonIDs[i]] = true;
    }
    frame_tints_[IDR_THEME_FRAME] = TINT_FRAME;
    frame_tints_[IDR_THEME_FRAME_INACTIVE] = TINT_FRAME_INACTIVE;
    frame_tints_[IDR_THEME_FRAME_INCOGNITO] = TINT_FRAME_INCOGNITO;
    frame_tints_[IDR_THEME_FRAME_INCOGNITO_INACTIVE] =
        TINT_FRAME_INCOGNITO_INACTIVE;
  }
}

BrowserThemeProvider::~BrowserThemeProvider() {
  ClearCaches();
}

void BrowserThemeProvider::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;
  LoadThemePrefs();
}

SkBitmap* BrowserThemeProvider::GetBitmapNamed(int id) {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the Skia image in the cache.
  ImageCache::const_iterator found = image_cache_.find(id);
  if (found != image_cache_.end())
    return found->second;

  scoped_ptr<SkBitmap> result;

  // Try to load the image from the extension.
  result.reset(LoadThemeBitmap(id));

  // If the extension doesn't provide the requested image, but has provided
  // a custom frame, then we may be able to generate the image required.
  if (!result.get())
    result.reset(GenerateBitmap(id));

  // If we still don't have an image, load it from resourcebundle.
  if (!result.get())
    result.reset(new SkBitmap(*rb_.GetBitmapNamed(id)));

  if (result.get()) {
    // If the requested image is part of the toolbar button set, and we have
    // a provided tint for that set, tint it appropriately.
    if (button_images_.count(id) && tints_.count(kTintButtons)) {
      SkBitmap* tinted =
          new SkBitmap(TintBitmap(*result.release(), TINT_BUTTONS));
      result.reset(tinted);
    }

    // We loaded successfully. Cache the bitmap.
    image_cache_[id] = result.get();
    return result.release();
  } else {
    NOTREACHED() << "Failed to load a requested image";
    return NULL;
  }
}

SkColor BrowserThemeProvider::GetColor(int id) {
  DCHECK(CalledOnValidThread());

  // TODO(glen): Figure out if we need to tint these. http://crbug.com/11578
  switch (id) {
    case COLOR_FRAME:
      return FindColor(kColorFrame, kDefaultColorFrame);
    case COLOR_FRAME_INACTIVE:
      return FindColor(kColorFrameInactive, kDefaultColorFrameInactive);
    case COLOR_FRAME_INCOGNITO:
      return FindColor(kColorFrameIncognito, kDefaultColorFrameIncognito);
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      return FindColor(kColorFrameIncognitoInactive,
                       kDefaultColorFrameIncognitoInactive);
    case COLOR_TOOLBAR:
      return FindColor(kColorToolbar, kDefaultColorToolbar);
    case COLOR_TAB_TEXT:
      return FindColor(kColorTabText, kDefaultColorTabText);
    case COLOR_BACKGROUND_TAB_TEXT:
      return FindColor(kColorBackgroundTabText, kDefaultColorBackgroundTabText);
    case COLOR_BOOKMARK_TEXT:
      return FindColor(kColorBookmarkText, kDefaultColorBookmarkText);
    case COLOR_NTP_BACKGROUND:
      return (colors_.find(kColorNTPBackground) != colors_.end()) ?
          colors_[kColorNTPBackground] :
          kDefaultColorNTPBackground;
    case COLOR_NTP_TEXT:
      return FindColor(kColorNTPText, kDefaultColorNTPText);
    case COLOR_NTP_LINK:
      return FindColor(kColorNTPLink, kDefaultColorNTPLink);
    case COLOR_NTP_SECTION:
      return FindColor(kColorNTPSection, kDefaultColorNTPSection);
    case COLOR_NTP_SECTION_TEXT:
      return FindColor(kColorNTPSectionText, kDefaultColorNTPSectionText);
    case COLOR_NTP_SECTION_LINK:
      return FindColor(kColorNTPSectionLink, kDefaultColorNTPSectionLink);
    case COLOR_CONTROL_BACKGROUND:
      return FindColor(kColorControlBackground, kDefaultColorControlBackground);
    case COLOR_BUTTON_BACKGROUND:
      return FindColor(kColorButtonBackground, kDefaultColorButtonBackground);
    default:
      NOTREACHED() << "Unknown color requested";
  }

  // Return a debugging red color.
  return 0xffff0000;
}

bool BrowserThemeProvider::GetDisplayProperty(int id, int* result) {
  switch (id) {
    case NTP_BACKGROUND_ALIGNMENT:
      if (display_properties_.find(kDisplayPropertyNTPAlignment) !=
          display_properties_.end()) {
        *result = display_properties_[kDisplayPropertyNTPAlignment];
      } else {
        *result = kDefaultDisplayPropertyNTPAlignment;
      }
      return true;
    default:
      NOTREACHED() << "Unknown property requested";
  }
  return false;
}

bool BrowserThemeProvider::ShouldUseNativeFrame() {
  if (images_.find(IDR_THEME_FRAME) != images_.end())
    return false;
#if defined(OS_WIN)
  return win_util::ShouldUseVistaFrame();
#else
  return false;
#endif
}

bool BrowserThemeProvider::HasCustomImage(int id) {
  return (images_.find(id) != images_.end());
}

void BrowserThemeProvider::SetTheme(Extension* extension) {
  // Clear our image cache.
  ClearCaches();

  DCHECK(extension);
  DCHECK(extension->IsTheme());
  SetImageData(extension->GetThemeImages(),
               extension->path());
  SetColorData(extension->GetThemeColors());
  SetTintData(extension->GetThemeTints());
  SetDisplayPropertyData(extension->GetThemeDisplayProperties());
  GenerateFrameColors();
  GenerateFrameImages();

  SaveImageData(extension->GetThemeImages());
  SaveColorData();
  SaveTintData();
  SaveDisplayPropertyData();

  NotifyThemeChanged();
  UserMetrics::RecordAction(L"Themes_Installed", profile_);
}

void BrowserThemeProvider::UseDefaultTheme() {
  ClearAllThemeData();
  NotifyThemeChanged();
  UserMetrics::RecordAction(L"Themes_Reset", profile_);
}

bool BrowserThemeProvider::ReadThemeFileData(
    int id, std::vector<unsigned char>* raw_data) {
  if (images_.count(id)) {
    // First check to see if we have a registered theme extension and whether
    // it can handle this resource.
#if defined(OS_WIN)
    FilePath path = FilePath(UTF8ToWide(images_[id]));
#else
    FilePath path = FilePath(images_[id]);
#endif
    if (!path.empty()) {
      net::FileStream file;
      int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
      if (file.Open(path, flags) == net::OK) {
        int64 avail = file.Available();
        if (avail > 0 && avail < INT_MAX) {
          size_t size = static_cast<size_t>(avail);
          raw_data->resize(size);
          char* data = reinterpret_cast<char*>(&(raw_data->front()));
          if (file.ReadUntilComplete(data, size) == avail)
            return true;
        }
      }
    }
  }

  return false;
}

SkBitmap* BrowserThemeProvider::LoadThemeBitmap(int id) {
  DCHECK(CalledOnValidThread());
  // Attempt to find the image in our theme bundle.
  std::vector<unsigned char> raw_data, png_data;
  if (ReadThemeFileData(id, &raw_data)) {
    // Decode the PNG.
    int image_width = 0;
    int image_height = 0;

    if (!PNGDecoder::Decode(&raw_data.front(), raw_data.size(),
        PNGDecoder::FORMAT_BGRA, &png_data,
        &image_width, &image_height)) {
      NOTREACHED() << "Unable to decode theme image resource " << id;
      return NULL;
    }

    return PNGDecoder::CreateSkBitmapFromBGRAFormat(png_data,
                                                    image_width,
                                                    image_height);
  } else {
    // TODO(glen): File no-longer exists, we're out of date. We should
    // clear the theme (or maybe just the pref that points to this
    // image).
    return NULL;
  }
}

const std::string BrowserThemeProvider::GetTintKey(int id) {
  switch (id) {
    case TINT_FRAME:
      return kTintFrame;
    case TINT_FRAME_INACTIVE:
      return kTintFrameInactive;
    case TINT_FRAME_INCOGNITO:
      return kTintFrameIncognito;
    case TINT_FRAME_INCOGNITO_INACTIVE:
      return kTintFrameIncognitoInactive;
    case TINT_BUTTONS:
      return kTintButtons;
    case TINT_BACKGROUND_TAB:
      return kTintBackgroundTab;
    default:
      NOTREACHED() << "Unknown tint requested";
      return "";
  }
}

skia::HSL BrowserThemeProvider::GetDefaultTint(int id) {
  switch (id) {
    case TINT_FRAME:
      return kDefaultTintFrame;
    case TINT_FRAME_INACTIVE:
      return kDefaultTintFrameInactive;
    case TINT_FRAME_INCOGNITO:
      return kDefaultTintFrameIncognito;
    case TINT_FRAME_INCOGNITO_INACTIVE:
      return kDefaultTintFrameIncognitoInactive;
    case TINT_BUTTONS:
      return kDefaultTintButtons;
    case TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    default:
      skia::HSL result = {-1, -1, -1};
      return result;
  }
}

skia::HSL BrowserThemeProvider::GetTint(int id) {
  DCHECK(CalledOnValidThread());

  TintMap::iterator tint_iter = tints_.find(GetTintKey(id));
  if (tint_iter != tints_.end())
    return tint_iter->second;
  else
    return GetDefaultTint(id);
}

SkBitmap BrowserThemeProvider::TintBitmap(const SkBitmap& bitmap, int hsl_id) {
  return skia::ImageOperations::CreateHSLShiftedBitmap(bitmap, GetTint(hsl_id));
}

void BrowserThemeProvider::SetImageData(DictionaryValue* images_value,
                                        FilePath images_path) {
  images_.clear();

  if (!images_value)
    return;

  DictionaryValue::key_iterator iter = images_value->begin_keys();
  while (iter != images_value->end_keys()) {
    std::string val;
    if (images_value->GetString(*iter, &val)) {
      int id = ThemeResourcesUtil::GetId(WideToUTF8(*iter));
      if (id != -1) {
        if (!images_path.empty()) {
          images_[id] = WideToUTF8(images_path.AppendASCII(val)
              .ToWStringHack());
        } else {
          images_[id] = val;
        }
      }
    }
    ++iter;
  }
}

void BrowserThemeProvider::SetColorData(DictionaryValue* colors_value) {
  colors_.clear();

  if (!colors_value)
    return;

  DictionaryValue::key_iterator iter = colors_value->begin_keys();
  while (iter != colors_value->end_keys()) {
    ListValue* color_list;
    if (colors_value->GetList(*iter, &color_list) &&
        (color_list->GetSize() == 3 || color_list->GetSize() == 4)) {
      int r, g, b;
      color_list->GetInteger(0, &r);
      color_list->GetInteger(1, &g);
      color_list->GetInteger(2, &b);
      if (color_list->GetSize() == 4) {
        double alpha;
        color_list->GetReal(3, &alpha);
        colors_[WideToUTF8(*iter)] = SkColorSetARGB(
            static_cast<int>(alpha * 255), r, g, b);
      } else {
        colors_[WideToUTF8(*iter)] = SkColorSetRGB(r, g, b);
      }
    }
    ++iter;
  }
}

void BrowserThemeProvider::SetTintData(DictionaryValue* tints_value) {
  tints_.clear();

  if (!tints_value)
    return;

  DictionaryValue::key_iterator iter = tints_value->begin_keys();
  while (iter != tints_value->end_keys()) {
    ListValue* tint_list;
    if (tints_value->GetList(*iter, &tint_list) &&
        tint_list->GetSize() == 3) {
      skia::HSL hsl = { -1, -1, -1 };
      // TODO(glen): Make this work with integer values.
      tint_list->GetReal(0, &hsl.h);
      tint_list->GetReal(1, &hsl.s);
      tint_list->GetReal(2, &hsl.l);
      tints_[WideToUTF8(*iter)] = hsl;
    }
    ++iter;
  }
}

void BrowserThemeProvider::SetDisplayPropertyData(
    DictionaryValue* display_properties_value) {
  display_properties_.clear();

  if (!display_properties_value)
    return;

  DictionaryValue::key_iterator iter = display_properties_value->begin_keys();
  while (iter != display_properties_value->end_keys()) {
    // New tab page alignment.
    if (base::strcasecmp(WideToUTF8(*iter).c_str(),
        kDisplayPropertyNTPAlignment) == 0) {
      std::string val;
      if (display_properties_value->GetString(*iter, &val))
        display_properties_[kDisplayPropertyNTPAlignment] =
            StringToAlignment(val);
    }
    ++iter;
  }
}

// static
int BrowserThemeProvider::StringToAlignment(const std::string& alignment) {
  std::vector<std::wstring> split;
  SplitStringAlongWhitespace(UTF8ToWide(alignment), &split);

  std::vector<std::wstring>::iterator alignments = split.begin();
  int alignment_mask = 0;
  while (alignments != split.end()) {
    std::string comp = WideToUTF8(*alignments);
    const char* component = comp.c_str();

    if (base::strcasecmp(component, kAlignmentTop) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_TOP;
    else if (base::strcasecmp(component, kAlignmentBottom) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_BOTTOM;

    if (base::strcasecmp(component, kAlignmentLeft) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_LEFT;
    else if (base::strcasecmp(component, kAlignmentRight) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_RIGHT;
    alignments++;
  }
  return alignment_mask;
}

// static
std::string BrowserThemeProvider::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string;
  std::string horizontal_string;

  if (alignment & BrowserThemeProvider::ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & BrowserThemeProvider::ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & BrowserThemeProvider::ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & BrowserThemeProvider::ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  if (!vertical_string.empty() && !horizontal_string.empty())
    return vertical_string + " " + horizontal_string;
  else if (vertical_string.empty())
    return horizontal_string;
  else
    return vertical_string;
}

void BrowserThemeProvider::SetColor(const char* key, const SkColor& color) {
  colors_[kColorFrame] = color;
}

void BrowserThemeProvider::SetTint(const char* key, const skia::HSL& tint) {
  tints_[key] = tint;
}

void BrowserThemeProvider::GenerateFrameColors() {
  // Generate any secondary frame colors that weren't provided.
  skia::HSL frame_hsl = { 0, 0, 0 };
  skia::SkColorToHSL(GetColor(COLOR_FRAME), frame_hsl);

  if (colors_.find(kColorFrame) == colors_.end())
    colors_[kColorFrame] = HSLShift(frame_hsl, GetTint(TINT_FRAME));
  if (colors_.find(kColorFrameInactive) == colors_.end()) {
    colors_[kColorFrameInactive] =
        HSLShift(frame_hsl, GetTint(TINT_FRAME_INACTIVE));
  }
  if (colors_.find(kColorFrameIncognito) == colors_.end()) {
    colors_[kColorFrameIncognito] =
        HSLShift(frame_hsl, GetTint(TINT_FRAME_INCOGNITO));
  }
  if (colors_.find(kColorFrameIncognitoInactive) == colors_.end()) {
    colors_[kColorFrameIncognitoInactive] =
        HSLShift(frame_hsl, GetTint(TINT_FRAME_INCOGNITO_INACTIVE));
  }
}

void BrowserThemeProvider::GenerateFrameImages() {
  std::map<const int, int>::iterator iter = frame_tints_.begin();
  while (iter != frame_tints_.end()) {
    int id = iter->first;
    scoped_ptr<SkBitmap> frame;
    // If there's no frame image provided for the specified id, then load
    // the default provided frame. If that's not provided, skip this whole
    // thing and just use the default images.
    int base_id = (id == IDR_THEME_FRAME_INCOGNITO ||
                   id == IDR_THEME_FRAME_INCOGNITO_INACTIVE) ?
        IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;

    if (images_.find(id) != images_.end()) {
      frame.reset(LoadThemeBitmap(id));
    } else if (base_id != id && images_.find(base_id) != images_.end()) {
      frame.reset(LoadThemeBitmap(base_id));
    } else {
      // If the theme doesn't specify an image, then apply the tint to
      // the default frame. Note that the default theme provides default
      // bitmaps for all frame types, so this isn't strictly necessary
      // in the case where no tint is provided either.
      frame.reset(new SkBitmap(*rb_.GetBitmapNamed(IDR_THEME_FRAME)));
    }

    if (frame.get()) {
      SkBitmap* tinted = new SkBitmap(TintBitmap(*frame, iter->second));
      image_cache_[id] = tinted;
    }
    ++iter;
  }
}

void BrowserThemeProvider::ClearAllThemeData() {
  // Clear our image cache.
  ClearCaches();

  images_.clear();
  colors_.clear();
  tints_.clear();
  display_properties_.clear();

  SaveImageData(NULL);
  SaveColorData();
  SaveTintData();
  SaveDisplayPropertyData();
}

SkBitmap* BrowserThemeProvider::GenerateBitmap(int id) {
  if (id == IDR_THEME_TAB_BACKGROUND ||
      id == IDR_THEME_TAB_BACKGROUND_INCOGNITO) {
    // The requested image is a background tab. Get a frame to create the
    // tab against. As themes don't use the glass frame, we don't have to
    // worry about compositing them together, as our default theme provides
    // the necessary bitmaps.
    int base_id = (id == IDR_THEME_TAB_BACKGROUND) ?
        IDR_THEME_FRAME : IDR_THEME_FRAME_INCOGNITO;

    std::map<int, SkBitmap*>::iterator it = image_cache_.find(base_id);
    if (it != image_cache_.end()) {
      SkBitmap* frame = it->second;
      SkBitmap blurred =
          skia::ImageOperations::CreateBlurredBitmap(*frame, 5);
      SkBitmap* bg_tab =
          new SkBitmap(TintBitmap(blurred, TINT_BACKGROUND_TAB));
      return bg_tab;
    }
  }

  return NULL;
}

void BrowserThemeProvider::SaveImageData(DictionaryValue* images_value) {
  // Save our images data.
  DictionaryValue* pref_images =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeImages);
  pref_images->Clear();

  if (images_value) {
    DictionaryValue::key_iterator iter = images_value->begin_keys();
    while (iter != images_value->end_keys()) {
      std::string val;
      if (images_value->GetString(*iter, &val)) {
        int id = ThemeResourcesUtil::GetId(WideToUTF8(*iter));
        if (id != -1)
          pref_images->SetString(*iter, images_[id]);
      }
      ++iter;
    }
  }
}

void BrowserThemeProvider::SaveColorData() {
  // Save our color data.
  DictionaryValue* pref_colors =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeColors);
  pref_colors->Clear();
  if (colors_.size()) {
    ColorMap::iterator iter = colors_.begin();
    while (iter != colors_.end()) {
      SkColor rgb = (*iter).second;
      ListValue* rgb_list = new ListValue();
      rgb_list->Set(0, Value::CreateIntegerValue(SkColorGetR(rgb)));
      rgb_list->Set(1, Value::CreateIntegerValue(SkColorGetG(rgb)));
      rgb_list->Set(2, Value::CreateIntegerValue(SkColorGetB(rgb)));
      pref_colors->Set(UTF8ToWide((*iter).first), rgb_list);
      ++iter;
    }
  }
}

void BrowserThemeProvider::SaveTintData() {
  // Save our tint data.
  DictionaryValue* pref_tints =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeTints);
  pref_tints->Clear();
  if (tints_.size()) {
    TintMap::iterator iter = tints_.begin();
    while (iter != tints_.end()) {
      skia::HSL hsl = (*iter).second;
      ListValue* hsl_list = new ListValue();
      hsl_list->Set(0, Value::CreateRealValue(hsl.h));
      hsl_list->Set(1, Value::CreateRealValue(hsl.s));
      hsl_list->Set(2, Value::CreateRealValue(hsl.l));
      pref_tints->Set(UTF8ToWide((*iter).first), hsl_list);
      ++iter;
    }
  }
}

void BrowserThemeProvider::SaveDisplayPropertyData() {
  // Save our display property data.
  DictionaryValue* pref_display_properties =
      profile_->GetPrefs()->
          GetMutableDictionary(prefs::kCurrentThemeDisplayProperties);
  pref_display_properties->Clear();

  if (display_properties_.size()) {
    DisplayPropertyMap::iterator iter = display_properties_.begin();
    while (iter != display_properties_.end()) {
      if (base::strcasecmp((*iter).first.c_str(),
                           kDisplayPropertyNTPAlignment) == 0) {
        pref_display_properties->
            SetString(UTF8ToWide((*iter).first), AlignmentToString(
                (*iter).second));
      }
      ++iter;
    }
  }
}

void BrowserThemeProvider::NotifyThemeChanged() {
  // Redraw!
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::BROWSER_THEME_CHANGED,
                  NotificationService::AllSources(),
                  NotificationService::NoDetails());
}

void BrowserThemeProvider::LoadThemePrefs() {
  PrefService* prefs = profile_->GetPrefs();

  // TODO(glen): Figure out if any custom prefs were loaded, and if so
  //    UMA-log the fact that a theme was loaded.
  if (prefs->HasPrefPath(prefs::kCurrentThemeImages) ||
      prefs->HasPrefPath(prefs::kCurrentThemeColors) ||
      prefs->HasPrefPath(prefs::kCurrentThemeTints)) {
    // Our prefs already have the extension path baked in, so we don't need
    // to provide it.
    SetImageData(prefs->GetMutableDictionary(prefs::kCurrentThemeImages),
                 FilePath());
    SetColorData(prefs->GetMutableDictionary(prefs::kCurrentThemeColors));
    SetTintData(prefs->GetMutableDictionary(prefs::kCurrentThemeTints));
    GenerateFrameColors();
    GenerateFrameImages();
    UserMetrics::RecordAction(L"Themes_loaded", profile_);
  }
}

SkColor BrowserThemeProvider::FindColor(const char* id,
                                        SkColor default_color) {
  return (colors_.find(id) != colors_.end()) ? colors_[id] : default_color;
}

void BrowserThemeProvider::ClearCaches() {
  FreePlatformCaches();
  for (ImageCache::iterator i = image_cache_.begin();
       i != image_cache_.end(); i++) {
    delete i->second;
  }
  image_cache_.clear();
}

#if defined(TOOLKIT_VIEWS)
void BrowserThemeProvider::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif
