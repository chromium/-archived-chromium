// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_theme_source.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/gfx/png_encoder.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/theme_resources_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/views/bookmark_bar_view.h"
#endif

// Path for the New Tab CSS. When we get more than a few of these, we should
// use a resource map rather than hard-coded strings.
static const char* kNewTabCSSPath = "css/newtab.css";

static string16 SkColorToRGBAString(SkColor color) {
  return WideToUTF16(l10n_util::GetStringF(IDS_RGBA_CSS_FORMAT_STRING,
      IntToWString(SkColorGetR(color)),
      IntToWString(SkColorGetG(color)),
      IntToWString(SkColorGetB(color)),
      DoubleToWString(SkColorGetA(color) / 255.0)));
}

static std::string StripQueryParams(const std::string& path) {
  GURL path_url = GURL(std::string(chrome::kChromeUIScheme) + "://" +
                       std::string(chrome::kChromeUIThemePath) + "/" + path);
  return path_url.path().substr(1);  // path() always includes a leading '/'.
}

////////////////////////////////////////////////////////////////////////////////
// DOMUIThemeSource, public:

DOMUIThemeSource::DOMUIThemeSource(Profile* profile)
    : DataSource(chrome::kChromeUIThemePath, MessageLoop::current()),
      profile_(profile) {
}

void DOMUIThemeSource::StartDataRequest(const std::string& path,
                                        int request_id) {
  // Our path may include cachebuster arguments, so trim them off.
  std::string uncached_path = StripQueryParams(path);

  if (strcmp(uncached_path.c_str(), kNewTabCSSPath) == 0) {
    SendNewTabCSS(request_id);
    return;
  } else {
    int resource_id = ThemeResourcesUtil::GetId(uncached_path);
    if (resource_id != -1) {
      SendThemeBitmap(request_id, resource_id);
      return;
    }
  }
  // We don't have any data to send back.
  SendResponse(request_id, NULL);
}

std::string DOMUIThemeSource::GetMimeType(const std::string& path) const {
  std::string uncached_path = StripQueryParams(path);

  if (strcmp(uncached_path.c_str(), kNewTabCSSPath) == 0)
    return "text/css";
  return "image/png";
}

void DOMUIThemeSource::SendResponse(int request_id, RefCountedBytes* data) {
  ChromeURLDataManager::DataSource::SendResponse(request_id, data);
}

////////////////////////////////////////////////////////////////////////////////
// DOMUIThemeSource, private:

void DOMUIThemeSource::SendNewTabCSS(int request_id) {
  ThemeProvider* tp = profile_->GetThemeProvider();
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND);
  SkColor color_text = tp->GetColor(BrowserThemeProvider::COLOR_NTP_TEXT);
  SkColor color_link = tp->GetColor(BrowserThemeProvider::COLOR_NTP_LINK);
  SkColor color_section =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION);
  SkColor color_section_text =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_TEXT);
  SkColor color_section_link =
      tp->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_LINK);

  // Generate the replacements.
  std::vector<string16> subst;

  // Cache-buster for background.
  subst.push_back(UTF8ToUTF16(IntToString(static_cast<int>(
      base::Time::Now().ToDoubleT()))));

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));
  subst.push_back(UTF8ToUTF16(GetNewTabBackgroundCSS(false)));
  subst.push_back(UTF8ToUTF16(GetNewTabBackgroundCSS(true)));
  subst.push_back(SkColorToRGBAString(color_text));
  subst.push_back(SkColorToRGBAString(color_link));
  subst.push_back(SkColorToRGBAString(color_section));
  subst.push_back(SkColorToRGBAString(color_section_text));
  subst.push_back(SkColorToRGBAString(color_section_link));

  // Get our template.
  static const StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_NEW_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  string16 format_string = ASCIIToUTF16(new_tab_theme_css.as_string());
  const std::string css_string = UTF16ToASCII(ReplaceStringPlaceholders(
      format_string, subst, NULL));

  // Convert to a format appropriate for sending.
  scoped_refptr<RefCountedBytes> css_bytes(new RefCountedBytes);
  css_bytes->data.resize(css_string.size());
  std::copy(css_string.begin(), css_string.end(), css_bytes->data.begin());

  // Send.
  SendResponse(request_id, css_bytes);
}

void DOMUIThemeSource::SendThemeBitmap(int request_id, int resource_id) {
  ThemeProvider* tp = profile_->GetThemeProvider();
  DCHECK(tp);

  SkBitmap* image = tp->GetBitmapNamed(resource_id);
  DCHECK(image);
  std::vector<unsigned char> png_bytes;
  PNGEncoder::EncodeBGRASkBitmap(*image, false, &png_bytes);

  scoped_refptr<RefCountedBytes> image_data =
      new RefCountedBytes(png_bytes);
  SendResponse(request_id, image_data);
}

std::string DOMUIThemeSource::GetNewTabBackgroundCSS(bool bar_attached) {
  int alignment;
  profile_->GetThemeProvider()->GetDisplayProperty(
      BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT, &alignment);

  if (bar_attached)
    return BrowserThemeProvider::AlignmentToString(alignment);

  // The bar is detached, so we must offset the background by the bar size
  // if it's a top-aligned bar.
#if defined(OS_WIN)
  int offset = BookmarkBarView::kNewtabBarHeight;
#else
  int offset = 0;
#endif

  if (alignment & BrowserThemeProvider::ALIGN_TOP) {
    if (alignment & BrowserThemeProvider::ALIGN_LEFT)
      return "0% " + IntToString(-offset) + "px";
    else if (alignment & BrowserThemeProvider::ALIGN_RIGHT)
      return "100% " + IntToString(-offset) + "px";
    return IntToString(-offset) + "px";
  }
  return BrowserThemeProvider::AlignmentToString(alignment);
}
