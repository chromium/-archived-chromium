// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"

WebKitContext::WebKitContext(const FilePath& data_path, bool is_incognito)
    : data_path_(data_path),
      is_incognito_(is_incognito) {
}

WebKitContext::~WebKitContext() {
}
