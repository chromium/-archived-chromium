/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebImage_h
#define WebImage_h

#include "WebCommon.h"
#include "WebSize.h"

#if WEBKIT_USING_SKIA
class SkBitmap;
#endif

namespace WebKit {

    class WebImagePixels;
    class WebImagePrivate;

    // A container for an ARGB bitmap.
    class WebImage {
    public:
        ~WebImage() { reset(); }

        WebImage() : m_private(0) { }
        WebImage(const WebImage& image) : m_private(0) { assign(image); }

        WebImage& operator=(const WebImage& image)
        {
            assign(image);
            return *this;
        }

        WEBKIT_API void reset();
        WEBKIT_API WebSize size() const;

        WebImagePixels pixels() const;
        bool isNull() const { return m_private == 0; }

#if WEBKIT_USING_SKIA
        WebImage(const SkBitmap& bitmap) : m_private(0)
        {
            assign(bitmap);
        }

        WebImage& operator=(const SkBitmap& bitmap)
        {
            assign(bitmap);
            return *this;
        }

        WEBKIT_API operator SkBitmap() const;
#endif

    private:
        friend class WebImagePixels;

        WEBKIT_API void assign(const WebImage&);
        WEBKIT_API const void* lockPixels();
        WEBKIT_API void unlockPixels();

#if WEBKIT_USING_SKIA
        WEBKIT_API void assign(const SkBitmap&);
#endif

        WebImagePrivate* m_private;
    };

    class WebImagePixels {
    public:
        WebImagePixels(WebImage* image) : m_image(image)
        {
            m_data = m_image->lockPixels();
        }

        WebImagePixels(const WebImagePixels& other) : m_image(other.m_image)
        {
            m_data = m_image->lockPixels();
        }

        ~WebImagePixels() { m_image->unlockPixels(); }

        const void* get() const { return m_data; }
        operator const void*() const { return m_data; }

    private:
        WebImagePixels& operator=(const WebImagePixels&);

        WebImage* m_image;
        const void* m_data;
    };

    inline WebImagePixels WebImage::pixels() const
    {
        return WebImagePixels(const_cast<WebImage*>(this));
    }

} // namespace WebKit

#endif
