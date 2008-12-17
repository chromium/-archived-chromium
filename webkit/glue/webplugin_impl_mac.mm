// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "wtf/ASCIICType.h"

#undef LOG
#include "webkit/glue/webplugin_impl.h"

// TODO(pinkerton): all of this needs to be filled in. webplugin_impl.cc has
// a lot of win32-specific code in it that needs to be re-factored.

//WebPluginContainer::WebPluginContainer(WebPluginImpl* impl);
//WebPluginContainer::~WebPluginContainer();
//WebCore::IntRect WebPluginContainer::windowClipRect() const;
//void WebPluginContainer::geometryChanged() const;
//void WebPluginContainer::setFrameGeometry(const WebCore::IntRect& rect);
//void WebPluginContainer::paint(WebCore::GraphicsContext*, const WebCore::IntRect& rect);
//void WebPluginContainer::setFocus();
//void WebPluginContainer::show();
//void WebPluginContainer::hide();
//void WebPluginContainer::handleEvent(WebCore::Event* event);

void WebPluginContainer::didReceiveResponse(const WebCore::ResourceResponse& response) {
  NSLog(@"WebPluginContainer::didReceiveResponse");
}

void WebPluginContainer::didReceiveData(const char *buffer, int length) {
  NSLog(@"WebPluginContainer::didReceiveData");
}

void WebPluginContainer::didFinishLoading() {
  NSLog(@"WebPluginContainer::didFinishLoading");
}

void WebPluginContainer::didFail(const WebCore::ResourceError&) {
  NSLog(@"WebPluginContainer::didFail");
}

NPObject* WebPluginContainer::GetPluginScriptableObject() {
  NSLog(@"WebPluginContainer::GetPluginScriptableObject");
}

WebCore::Widget* WebPluginImpl::Create(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebCore::Element* element,
                                       WebFrameImpl* frame,
                                       WebPluginDelegate* delegate,
                                       bool load_manually,
                                       const std::string& mime_type) {
  // TODO(pinkerton): delete delegate when stubbing out?
  NSLog(@"WebPluginImpl::Create");
  return NULL;
}
