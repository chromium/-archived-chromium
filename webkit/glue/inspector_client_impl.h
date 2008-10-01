// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H__
#define WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H__

#include "InspectorClient.h"
#include "base/ref_counted.h"

class WebNodeHighlight;
class WebView;

class WebInspectorClient : public WebCore::InspectorClient {
public:
  WebInspectorClient(WebView*);

  // InspectorClient
  virtual void inspectorDestroyed();

  virtual WebCore::Page* createPage();
  virtual WebCore::String localizedStringsURL();
  virtual void showWindow();
  virtual void closeWindow();
  virtual bool windowVisible();

  virtual void attachWindow();
  virtual void detachWindow();

  virtual void setAttachedWindowHeight(unsigned height);

  virtual void highlight(WebCore::Node*);
  virtual void hideHighlight();

  virtual void inspectedURLChanged(const WebCore::String& newURL);

private:
  ~WebInspectorClient();

  // The WebView of the page being inspected; gets passed to the constructor
  scoped_refptr<WebView> inspected_web_view_;

  // The WebView of the Inspector popup window
  WebView* inspector_web_view_;
};

#endif // WEBKIT_GLUE_INSPECTOR_CLIENT_IMPL_H__

