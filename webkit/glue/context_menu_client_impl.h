// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_MENU_CLIENT_IMPL_H__
#define WEBKIT_GLUE_CONTEXT_MENU_CLIENT_IMPL_H__

#include "build/build_config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ContextMenuClient.h"
MSVC_POP_WARNING();

class WebViewImpl;

// Handles window-level notifications from WebCore on behalf of a WebView.
class ContextMenuClientImpl : public WebCore::ContextMenuClient {
public:
  ContextMenuClientImpl(WebViewImpl* webview) : webview_(webview) {
  }

  virtual ~ContextMenuClientImpl();
  virtual void contextMenuDestroyed();

  virtual WebCore::PlatformMenuDescription getCustomMenuFromDefaultItems(
      WebCore::ContextMenu*);
  virtual void contextMenuItemSelected(WebCore::ContextMenuItem*,
                                       const WebCore::ContextMenu*);

  virtual void downloadURL(const WebCore::KURL&);
  virtual void copyImageToClipboard(const WebCore::HitTestResult&);
  virtual void searchWithGoogle(const WebCore::Frame*);
  virtual void lookUpInDictionary(WebCore::Frame*);
  virtual void speak(const WebCore::String&);
  virtual bool isSpeaking();
  virtual void stopSpeaking();
  virtual bool shouldIncludeInspectElementItem();

#if defined(OS_MACOSX)
  virtual void searchWithSpotlight();
#endif
private:
  WebViewImpl* webview_;  // weak pointer
};

#endif // WEBKIT_GLUE_CONTEXT_MENU_CLIENT_IMPL_H__
