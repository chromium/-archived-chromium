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
  bool javascript_can_open_windows_automatically;
  bool loads_images_automatically;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool developer_extras_enabled;
  bool shrinks_standalone_images_to_fit;
  bool uses_universal_detector;
  bool text_areas_are_resizable;
  bool dashboard_compatibility_mode;
  bool java_enabled;

  // TODO(tc): User style sheets will not work in chrome because it tries to
  // load the style sheet using a request without a frame.
  bool user_style_sheet_enabled;
  GURL user_style_sheet_location;

  std::string user_agent;

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
        javascript_can_open_windows_automatically(true),
        loads_images_automatically(true),
        plugins_enabled(true),
        dom_paste_enabled(false),  // enables execCommand("paste")
        developer_extras_enabled(false),  // Requires extra work by embedder
        shrinks_standalone_images_to_fit(true),
        uses_universal_detector(false),  // Disabled: page cycler regression
        user_style_sheet_enabled(false),
        text_areas_are_resizable(true),
        dashboard_compatibility_mode(false),
        java_enabled(true) {
  }
};

#endif  // WEBKIT_GLUE_WEBPREFERENCES_H__
