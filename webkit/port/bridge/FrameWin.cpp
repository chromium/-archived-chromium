/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#pragma warning(push, 0)
#include "Document.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "RenderFrame.h"
#include "RenderView.h"
#include "ScriptController.h"

#if USE(JSC)
#include "kjs_proxy.h"
#include "NP_jsobject.h"
#include "bindings/npruntime.h"
#include "runtime_root.h"
#include "runtime.h"
#endif
#if USE(V8)
#include "v8_npobject.h"
#include "npruntime_priv.h"
#endif

#include "webkit/glue/webplugin_impl.h"
#pragma warning(pop)

namespace WebCore {

void computePageRectsForFrame(WebCore::Frame* frame, const WebCore::IntRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, Vector<IntRect>& pages, int& outPageHeight)
{
    ASSERT(frame);

    pages.clear();
    outPageHeight = 0;

    if (!frame->document() || !frame->view() || !frame->document()->renderer())
        return;
 
    RenderView* root = static_cast<RenderView *>(frame->document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }
    
    float ratio = static_cast<float>(printRect.height()) / static_cast<float>(printRect.width());
 
    float pageWidth  = (float) root->docWidth();
    float pageHeight = pageWidth * ratio;
    outPageHeight = (int) pageHeight;   // this is the height of the page adjusted by margins
    pageHeight -= (headerHeight + footerHeight);

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight      = root->layer()->height();
    float docWidth       = root->layer()->width();
    float currPageWidth  = pageWidth / userScaleFactor;

    
    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0.0;
    do {
        float proposedBottom = std::min(docHeight, printedPagesHeight + pageHeight);
        frame->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);
       
        pages.append(IntRect(0,printedPagesHeight,currPageWidth,currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

DragImageRef Frame::dragImageForSelection()
{    
    if (selection()->isRange())
        return 0;  // TODO(pkasting): http://b/119669 Implement me!

    return 0;
}

} // namespace WebCore
