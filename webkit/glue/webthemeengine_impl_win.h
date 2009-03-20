// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBTHEMEENGINE_IMPL_WIN_H_
#define WEBTHEMEENGINE_IMPL_WIN_H_

#include "third_party/WebKit/WebKit/chromium/public/win/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual void paintButton(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintMenuList(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarArrow(
      WebKit::WebCanvas*, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarThumb(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarTrack(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&, const WebKit::WebRect& align_rect);
  virtual void paintTextField(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&, WebKit::WebColor, bool fill_content_area,
      bool draw_edges);
  virtual void paintTrackbar(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
};

}  // namespace webkit_glue

#endif  // WEBTHEMEENGINE_IMPL_WIN_H_
