// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_js_object.h"

#include "chrome/renderer/render_view.h"

ExternalJSObject::ExternalJSObject() : render_view_(NULL) {
  BindMethod("AddSearchProvider", &ExternalJSObject::AddSearchProvider);
}

void ExternalJSObject::AddSearchProvider(const CppArgumentList& args,
                                         CppVariant* result) {
  DCHECK(render_view_);
  result->SetNull();

  if (render_view_) {
    if (args.size() < 1 || !args[0].isString())
      return;
    render_view_->AddSearchProvider(args[0].ToString());
  }
}
