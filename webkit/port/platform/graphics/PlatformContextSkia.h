// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#ifndef PlatformContextSkia_h
#define PlatformContextSkia_h

#include "SkGraphicsContext.h"

namespace WebCore {
class GraphicsContext;
class GraphicsContextPlatformPrivate;
class PlatformScrollbar;
class ScrollView;
}

// This class is the "PlatformGraphicsContext" class when running under the
// Skia/Windows port. Name it PlatformContextSkia until we fix GraphicsContext.h.
class PlatformContextSkia : public SkGraphicsContext {
    // It initialize the back reference to GraphicsContextPlatformPrivate when the
    // PlatformContextSkia instance is associated with the GraphicsContext.
    friend class WebCore::GraphicsContextPlatformPrivate;
public:
    // For printing, there shouldn't be any canvas. canvas can be NULL.
    PlatformContextSkia(gfx::PlatformCanvasWin* canvas);
    ~PlatformContextSkia();

protected:
    // Make it accessible to friends.
    using SkGraphicsContext::setPaintContext;

private:
};

#endif  // PlatformContextSkia_h
