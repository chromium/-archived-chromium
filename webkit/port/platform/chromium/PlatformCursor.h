// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef PlatformCursor_h
#define PlatformCursor_h

#include "Image.h"
#include "IntPoint.h"
#include "RefPtr.h"

namespace WebCore {
    class PlatformCursor {
    public:
        enum Type {
            typePointer,
            typeCross,
            typeHand,
            typeIBeam,
            typeWait,
            typeHelp,
            typeEastResize,
            typeNorthResize,
            typeNorthEastResize,
            typeNorthWestResize,
            typeSouthResize,
            typeSouthEastResize,
            typeSouthWestResize,
            typeWestResize,
            typeNorthSouthResize,
            typeEastWestResize,
            typeNorthEastSouthWestResize,
            typeNorthWestSouthEastResize,
            typeColumnResize,
            typeRowResize,
            typeMiddlePanning,
            typeEastPanning,
            typeNorthPanning,
            typeNorthEastPanning,
            typeNorthWestPanning,
            typeSouthPanning,
            typeSouthEastPanning,
            typeSouthWestPanning,
            typeWestPanning,
            typeMove,
            typeVerticalText,
            typeCell,
            typeContextMenu,
            typeAlias,
            typeProgress,
            typeNoDrop,
            typeCopy,
            typeNone,
            typeNotAllowed,
            typeZoomIn,
            typeZoomOut,
            typeCustom
        };

        // Cursor.h assumes that it can initialize us to 0.
        explicit PlatformCursor(int type = 0) : m_type(typePointer) {}
        
        PlatformCursor(Type type) : m_type(type) {}

        PlatformCursor(Image* image, const IntPoint& hotSpot)
            : m_image(image)
            , m_hotSpot(hotSpot)
            , m_type(typeCustom) {}

        PassRefPtr<Image> customImage() const { return m_image; }
        const IntPoint& hotSpot() const { return m_hotSpot; }
        Type type() const { return m_type; }

    private:
        RefPtr<Image> m_image;
        IntPoint m_hotSpot;
        Type m_type;
    };
}

#endif
