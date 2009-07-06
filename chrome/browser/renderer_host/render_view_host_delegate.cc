// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_view_host_delegate.h"

#include "base/gfx/rect.h"
#include "chrome/common/renderer_preferences.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webpreferences.h"

RenderViewHostDelegate::View* RenderViewHostDelegate::GetViewDelegate() const {
  return NULL;
}

RenderViewHostDelegate::Save* RenderViewHostDelegate::GetSaveDelegate() const {
  return NULL;
}

const GURL& RenderViewHostDelegate::GetURL() const {
  return GURL::EmptyGURL();
}

TabContents* RenderViewHostDelegate::GetAsTabContents() {
  return NULL;
}

GURL RenderViewHostDelegate::GetAlternateErrorPageURL() const {
  return GURL();
}

RendererPreferences RenderViewHostDelegate::GetRendererPrefs() const {
  return RendererPreferences();
}

WebPreferences RenderViewHostDelegate::GetWebkitPrefs() {
  return WebPreferences();
}

bool RenderViewHostDelegate::CanBlur() const {
  return true;
}

gfx::Rect RenderViewHostDelegate::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool RenderViewHostDelegate::IsExternalTabContainer() const {
  return false;
}
