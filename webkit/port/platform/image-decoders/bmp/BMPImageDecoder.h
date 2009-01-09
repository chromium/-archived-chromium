// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BMP_DECODER_H_
#define BMP_DECODER_H_

#include "BMPImageReader.h"

namespace WebCore {

// This class decodes the BMP image format.
class BMPImageDecoder : public BMPImageReader
{
public:
    virtual String filenameExtension() const { return "bmp"; }

    // BMPImageReader
    virtual void decodeImage(SharedBuffer* data);

private:
    // Processes the file header at the beginning of the data.  Returns true if
    // the file header could be decoded.
    bool processFileHeader(SharedBuffer* data);
};

}

#endif

