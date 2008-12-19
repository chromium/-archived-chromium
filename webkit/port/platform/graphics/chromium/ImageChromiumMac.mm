// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around Uniscribe that provides a reasonable API.

#include "config.h"

#include "BitmapImage.h"
#include "ChromiumBridge.h"
#include "Image.h"


namespace WebCore {

PassRefPtr<Image> Image::loadPlatformResource(const char* name)
{
    return ChromiumBridge::loadPlatformImageResource(name);
}

// TODO(port): These are temporary stubs, we need real implementations which
// may come in the form of ImageChromium.cpp.  The Windows Chromium
// implementation is currently in ImageSkia.cpp.
 
void BitmapImage::initPlatformData()
{
}

void BitmapImage::invalidatePlatformData()
{
}

}
