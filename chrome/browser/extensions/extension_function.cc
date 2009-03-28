// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "chrome/browser/extensions/extension_function_dispatcher.h"

void ExtensionFunction::SendResponse(bool success) {
  if (success) {
    dispatcher_->SendResponse(this);
  } else {
    // TODO(aa): In case of failure, send the error message to an error
    // callback.
  }
}
