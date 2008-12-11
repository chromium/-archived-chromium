// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

// We need access to the private member of ImageSource.  Hack it locally
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#define private protected
#include "ImageSource.h"
#undef private

namespace WebCore {

class ImageSourceSkia : public ImageSource {
public:
    // This is a special-purpose routine for the favicon decoder, which is used
    // to specify a particular icon size for the ICOImageDecoder to prefer
    // decoding.  Note that not all favicons are ICOs, so this won't
    // necessarily do anything differently than ImageSource::setData().
    //
    // Passing an empty IntSize for |preferredIconSize| here is exactly
    // equivalent to just calling ImageSource::setData().  See also comments in
    // ICOImageDecoder.cpp.
    void setData(SharedBuffer* data,
                 bool allDataReceived,
                 const IntSize& preferredIconSize);
};

}

