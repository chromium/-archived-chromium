// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APIS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APIS_H_

#include <string>

class RenderViewHost;

// ExtensionAPIHandler is the top-level entry point for extension callbacks
// in the browser process. It lives on the UI thread.
class ExtensionAPIHandler {
 public:
  ExtensionAPIHandler(RenderViewHost* render_view_host);

  // Handle a request to perform some synchronous API.
  // TODO(aa): args should be a Value object.
  void HandleRequest(const std::string& name, const std::string& args,
                     int callback_id);

 private:
  // TODO(aa): Once there can be APIs that are asynchronous wrt the browser's UI
  // thread, we may have to have to do something about this raw pointer.
  RenderViewHost* render_view_host_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APIS_H_
