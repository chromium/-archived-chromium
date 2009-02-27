// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_H__
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_H__

#include "chrome/common/ipc_message.h"
#include "chrome/renderer/dom_ui_bindings.h"

// ExtensionBindings is the class backing the "extension" object
// accessible from JavaScript.
class ExtensionBindings : public DOMBoundBrowserObject {
 public:
  ExtensionBindings();
  virtual ~ExtensionBindings() {}

  // Methods exposed to JavaScript.
  void getTestString(const CppArgumentList& args, CppVariant* result);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionBindings);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_BINDINGS_H__
