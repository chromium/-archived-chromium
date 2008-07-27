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
