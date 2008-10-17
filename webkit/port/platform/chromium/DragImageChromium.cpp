/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "DragImage.h"

#include "CachedImage.h"
#include "GraphicsContext.h"
#include "Image.h"

#if PLATFORM(WIN)
#include <windows.h>
#else
#include "NotImplemented.h"
#endif

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
// TODO(darin): DragImageRef should be changed to be a cross-platform
// container.  However, it may still make sense for its contents to be
// platform-dependent.
#if PLATFORM(WIN)
    if (!image)
        return IntSize();
    BITMAP b;
    GetObject(image, sizeof(BITMAP), &b);
    return IntSize(b.bmWidth, b.bmHeight);
#else
    return IntSize();
#endif
}

void deleteDragImage(DragImageRef image)
{
#if PLATFORM(WIN)
    if (image)
        ::DeleteObject(image);
#else
    notImplemented();
#endif
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    // FIXME
    return 0;
}
    
DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    //We don't do this on windows as the dragimage is blended by the OS
    return image;
}
        
DragImageRef createDragImageFromImage(Image* img)
{    
    // FIXME
    return 0;
}
    
DragImageRef createDragImageIconForCachedImage(CachedImage*)
{
    //FIXME: Provide icon for image type <rdar://problem/5015949>
    return 0;     
}
    
}
