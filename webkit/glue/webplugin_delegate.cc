// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#undef LOG

#include "webkit/glue/webplugin_delegate.h"

#include "base/logging.h"
#include "base/gfx/rect.h"

bool WebPluginDelegate::IsWindowless() const {
  NOTREACHED();
  return false;
}

const gfx::Rect& WebPluginDelegate::GetRect() const {
  NOTREACHED();
  static gfx::Rect dummy;
  return dummy;
}

const gfx::Rect& WebPluginDelegate::GetClipRect() const {
  NOTREACHED();
  return GetRect();
}

// Returns a combinaison of PluginQuirks.
int WebPluginDelegate::GetQuirks() const {
  NOTREACHED();
  return 0;
}
