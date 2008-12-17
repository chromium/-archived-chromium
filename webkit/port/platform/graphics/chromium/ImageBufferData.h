/*
 * Copyright (C) 2008 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageBufferData_h
#define ImageBufferData_h

#if PLATFORM(DARWIN)
// TODO(port): This #include isn't strictly kosher, but we're currently using
// the Mac font code from upstream WebKit, and we need to pick up their header.
#undef ImageBufferData_h
#include "third_party/WebKit/WebCore/platform/graphics/cg/ImageBufferData.h"
#else

#include "PlatformContextSkia.h"

#include "skia/ext/platform_canvas.h"

namespace WebCore {

class ImageBufferData {
public:
    ImageBufferData(const IntSize&);

    skia::PlatformCanvas m_canvas;

    // Must be second since this will refer to m_canvas.
    PlatformContextSkia m_platformContext;
};

}  // namespace WebCore

#endif

#endif  // ImageBufferData_h
