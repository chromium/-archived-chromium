/*
 * Copyright (C) 2008 Google Inc.  All rights reserved.
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
#include "ThemeHelperChromiumWin.h"

#include "FloatRect.h"
#include "GraphicsContext.h"

namespace WebCore {

ThemeHelperWin::ThemeHelperWin(GraphicsContext* context,
                               const IntRect& rect)
    : m_orgContext(context)
    , m_orgMatrix(context->getCTM())
    , m_orgRect(rect)
{
    if (m_orgMatrix.b() != 0 ||  // Y skew
        m_orgMatrix.c() != 0) {  // X skew
        // Complicated effects, make a copy and draw the bitmap there.
        m_type = COPY;
        m_rect.setSize(rect.size());

        m_newBuffer.set(ImageBuffer::create(rect.size(), false).release());

        // Theme drawing messes with the transparency.
        // TODO(brettw) Ideally, we would leave this transparent, but I was
        // having problems with button drawing, so we fill with white. Buttons
        // looked good with transparent here and no fixing up of the alpha
        // later, but text areas didn't. This makes text areas look good but
        // gives buttons a white halo. Is there a way to fix this? I think
        // buttons actually have antialised edges which is just not possible
        // to handle on a transparent background given that it messes with the
        // alpha channel.
        FloatRect newContextRect(0, 0, rect.width(), rect.height());
        GraphicsContext* newContext = m_newBuffer->context();
        newContext->setFillColor(Color::white);
        newContext->fillRect(newContextRect);

        return;
    }

    if (m_orgMatrix.a() != 1.0 ||  // X scale
        m_orgMatrix.d() != 1.0) {  // Y scale
        // Only a scaling is applied.
        m_type = SCALE;

        // Save the transformed coordinates to draw.
        m_rect = m_orgMatrix.mapRect(rect);
        
        m_orgContext->save();
        m_orgContext->concatCTM(m_orgContext->getCTM().inverse());
        return;
    }

    // Nothing interesting.
    m_rect = rect;
    m_type = ORIGINAL;
}

ThemeHelperWin::~ThemeHelperWin()
{
    switch (m_type) {
    case SCALE:
        m_orgContext->restore();
        break;
    case COPY: {
        // Copy the duplicate bitmap with our control to the original canvas.
        FloatRect destRect(m_orgRect);
        m_newBuffer->context()->platformContext()->canvas()->
            getTopPlatformDevice().fixupAlphaBeforeCompositing();
        m_orgContext->drawImage(m_newBuffer->image(), destRect);
        break;
    }
    case ORIGINAL:
        break;
    }
}

}  // namespace WebCore
