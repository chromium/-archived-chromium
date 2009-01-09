// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ICO_DECODER_H_
#define ICO_DECODER_H_

#include "BMPImageReader.h"
#include "PNGImageDecoder.h"

namespace WebCore {

// This class decodes the ICO and CUR image formats.
class ICOImageDecoder : public BMPImageReader
{
public:
    // See comments on |m_preferredIconSize| below.
    ICOImageDecoder(const IntSize& preferredIconSize)
        : m_preferredIconSize(preferredIconSize)
        , m_imageType(UNKNOWN)
    {
        m_andMaskState = NOT_YET_DECODED;
    }

    virtual String filenameExtension() const { return "ico"; }

    // BMPImageReader
    virtual void decodeImage(SharedBuffer* data);
    virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

    // ImageDecoder
    virtual bool isSizeAvailable() const;
    virtual IntSize size() const;

private:
    enum ImageType {
        UNKNOWN,
        BMP,
        PNG,
    };

    // These are based on the Windows ICONDIR and ICONDIRENTRY structs, but
    // with unnecessary entries removed.
    struct IconDirectory {
        uint16_t idCount;
    };
    struct IconDirectoryEntry {
        uint16_t bWidth;   // 16 bits so we can represent 256 as 256, not 0
        uint16_t bHeight;  // "
        uint16_t wBitCount;
        uint32_t dwImageOffset;
    };

    // Processes the ICONDIR at the beginning of the data.  Returns true if the
    // directory could be decoded.
    bool processDirectory(SharedBuffer* data);

    // Processes the ICONDIRENTRY records after the directory.  Keeps the
    // "best" entry as the one we'll decode.  Returns true if the entries could
    // be decoded.
    bool processDirectoryEntries(SharedBuffer* data);

    // Reads and returns a directory entry from the current offset into |data|.
    IconDirectoryEntry readDirectoryEntry(SharedBuffer* data);

    // Returns true if |entry| is a preferable icon entry to m_dirEntry.
    // Larger sizes, or greater bitdepths at the same size, are preferable.
    bool isBetterEntry(const IconDirectoryEntry& entry) const;

    // Called when the image to be decoded is a PNG rather than a BMP.
    // Instantiates a PNGImageDecoder, decodes the image, and copies the
    // results locally.
    void decodePNG(SharedBuffer* data);

    // The entry size we should prefer.  If this is empty, we choose the
    // largest available size.  If no entries of the desired size are
    // available, we pick the next larger size.
    IntSize m_preferredIconSize;

    // The headers for the ICO.
    IconDirectory m_directory;
    IconDirectoryEntry m_dirEntry;

    // The PNG decoder, if we need to use one.
    PNGImageDecoder m_pngDecoder;

    // What kind of image data is stored at the entry we're decoding.
    ImageType m_imageType;
};

}

#endif

