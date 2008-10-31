// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "ChromiumBridge.h"

#include "HostWindow.h"
#include "PlatformWidget.h"
#include "ScrollView.h"
#include "Widget.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

static PlatformWidget ToPlatform(Widget* widget) {
  return widget ? widget->root()->hostWindow()->platformWindow() : 0;
}

// Screen ---------------------------------------------------------------------

int ChromiumBridge::screenDepth(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).depth;
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).depth_per_component;
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).is_monochrome;
}

IntRect ChromiumBridge::screenRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToPlatform(widget)).rect);
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToPlatform(widget)).available_rect);
}

}  // namespace WebCore
