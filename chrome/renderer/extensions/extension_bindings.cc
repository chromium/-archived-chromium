// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_bindings.h"

#include "base/values.h"
#include "chrome/common/render_messages.h"

#define BIND_METHOD(name) BindMethod(#name, &ExtensionBindings::name)
ExtensionBindings::ExtensionBindings() {
  BIND_METHOD(getTestString);
}
#undef BIND_METHOD

void ExtensionBindings::getTestString(
    const CppArgumentList& args, CppVariant* result) {
  result->Set("This is a placeholder string.  It's here to hold places.");
}
