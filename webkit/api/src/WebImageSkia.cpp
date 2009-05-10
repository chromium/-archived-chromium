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

#include "config.h"
#include "WebImage.h"

#include <SkBitmap.h>

namespace WebKit {

class WebImagePrivate : public SkBitmap {
public:
    WebImagePrivate(const SkBitmap& bitmap) : SkBitmap(bitmap) { }
};

void WebImage::reset()
{
    delete m_private;
    m_private = 0;
}

WebSize WebImage::size() const
{
    if (!m_private)
        return WebSize();

    return WebSize(m_private->width(), m_private->height());
}

void WebImage::assign(const WebImage& image)
{
    if (m_private)
        delete m_private;

    if (image.m_private)
        m_private = new WebImagePrivate(*image.m_private);
    else
        m_private = 0;
}

WebImage::operator SkBitmap() const
{
    if (!m_private)
        return SkBitmap();

    return *m_private;
}

void WebImage::assign(const SkBitmap& bitmap)
{
    if (m_private)
        delete m_private;

    m_private = new WebImagePrivate(bitmap);
}

const void* WebImage::lockPixels()
{
    m_private->lockPixels();
    return static_cast<void*>(m_private->getPixels());
}

void WebImage::unlockPixels()
{
    m_private->unlockPixels();
}

} // namespace WebKit
