// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing webkit's settings.
//
// Adding new values to this class probably involves updating
// WebViewImpl::SetPreferences, common/render_messages.h, and
// browser/profile.cc.

#ifndef WEBKIT_GLUE_WEBPREFERENCES_H__
#define WEBKIT_GLUE_WEBPREFERENCES_H__

#include <string>
#include "googleurl/src/gurl.h"

struct WebPreferences {
  std::wstring standard_font_family;
  std::wstring fixed_font_family;
  std::wstring serif_font_family;
  std::wstring sans_serif_font_family;
  std::wstring cursive_font_family;
  std::wstring fantasy_font_family;
  int default_font_size;
  int default_fixed_font_size;
  int minimum_font_size;
  int minimum_logical_font_size;
  std::wstring default_encoding;
  bool javascript_enabled;
  bool web_security_enabled;
  bool javascript_can_open_windows_automatically;
  bool loads_images_automatically;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool developer_extras_enabled;
  std::wstring inspector_settings;
  bool shrinks_standalone_images_to_fit;
  bool uses_universal_detector;
  bool text_areas_are_resizable;
  bool java_enabled;
  bool allow_scripts_to_close_windows;
  bool uses_page_cache;
  bool remote_fonts_enabled;
  bool xss_auditor_enabled;

  // TODO(tc): User style sheets will not work in chrome because it tries to
  // load the style sheet using a request without a frame.
  bool user_style_sheet_enabled;
  GURL user_style_sheet_location;

  // We try to keep the default values the same as the default values in
  // chrome, except for the cases where it would require lots of extra work for
  // the embedder to use the same default value.
  WebPreferences()
      : standard_font_family(L"Times New Roman"),
        fixed_font_family(L"Courier New"),
        serif_font_family(L"Times New Roman"),
        sans_serif_font_family(L"Arial"),
        cursive_font_family(L"Script"),
        fantasy_font_family(),  // Not sure what to use on Windows.
        default_font_size(16),
        default_fixed_font_size(13),
        minimum_font_size(1),
        minimum_logical_font_size(6),
        default_encoding(L"ISO-8859-1"),
        javascript_enabled(true),
        web_security_enabled(true),
        javascript_can_open_windows_automatically(true),
        loads_images_automatically(true),
        plugins_enabled(true),
        dom_paste_enabled(false),  // enables execCommand("paste")
        developer_extras_enabled(false),  // Requires extra work by embedder
        inspector_settings(L""),
        shrinks_standalone_images_to_fit(true),
        uses_universal_detector(false),  // Disabled: page cycler regression
        text_areas_are_resizable(true),
        java_enabled(true),
        allow_scripts_to_close_windows(false),
        uses_page_cache(false),
        remote_fonts_enabled(false),
        xss_auditor_enabled(false),
        user_style_sheet_enabled(false) {
  }
};

#endif  // WEBKIT_GLUE_WEBPREFERENCES_H__
