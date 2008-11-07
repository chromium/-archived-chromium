// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper methods for converting WebDropData objects to
// WebKit ChromiumDataObject and back.
#ifndef WEBKIT_GLUE_CLIPBOARD_CONVERSION_H_
#define WEBKIT_GLUE_CLIPBOARD_CONVERSION_H_

#include "webkit/glue/webdropdata.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {
class ChromiumDataObject;
}

namespace webkit_glue {

WebDropData ChromiumDataObjectToWebDropData(
    WebCore::ChromiumDataObject* data_object);

PassRefPtr<WebCore::ChromiumDataObject> WebDropDataToChromiumDataObject(
    const WebDropData& drop_data);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_CLIPBOARD_CONVERSION_H_
