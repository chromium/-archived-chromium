/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "Pasteboard.h"

#include "ChromiumBridge.h"
#include "ClipboardUtilitiesChromium.h"
#include "CString.h"
#include "DocumentFragment.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "Image.h"
#include "KURL.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Range.h"
#include "RenderImage.h"
#include "TextEncoding.h"
#include "markup.h"

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
{
}

void Pasteboard::clear()
{
    // The ScopedClipboardWriter class takes care of clearing the clipboard's
    // previous contents.
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    String html = createMarkup(selectedRange, 0, AnnotateForInterchange);
    ExceptionCode ec = 0;
    KURL url = selectedRange->startContainer(ec)->document()->url();
    String plainText = frame->selectedText();
#if PLATFORM(WIN_OS)
    replaceNewlinesWithWindowsStyleNewlines(plainText);
#endif
    replaceNBSPWithSpace(plainText);

    ChromiumBridge::clipboardWriteSelection(html, url, plainText,
                                            canSmartCopyOrDelete);
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    ASSERT(!url.isEmpty());

    String title(titleStr);
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    ChromiumBridge::clipboardWriteURL(url, title);
}

void Pasteboard::writeImage(Node* node, const KURL& url, const String& title)
{
    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = static_cast<RenderImage*>(node->renderer());
    CachedImage* cachedImage = static_cast<CachedImage*>(renderer->cachedImage());
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    NativeImageSkia* bitmap = 0;
#if !PLATFORM(CG)
    bitmap = image->nativeImageForCurrentFrame();
#endif
    ChromiumBridge::clipboardWriteImage(bitmap, url, title);
}

bool Pasteboard::canSmartReplace()
{
    return ChromiumBridge::clipboardIsFormatAvailable(
        PasteboardPrivate::WebSmartPasteFormat);
}

String Pasteboard::plainText(Frame* frame)
{
    return ChromiumBridge::clipboardReadPlainText();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    if (ChromiumBridge::clipboardIsFormatAvailable(
        PasteboardPrivate::HTMLFormat)) {
        String markup;
        KURL src_url;
        ChromiumBridge::clipboardReadHTML(&markup, &src_url);

        RefPtr<DocumentFragment> fragment =
            createFragmentFromMarkup(frame->document(), markup, src_url);

        if (fragment)
            return fragment.release();
    }

    if (allowPlainText) {
        String markup = ChromiumBridge::clipboardReadPlainText();
        if (!markup.isEmpty()) {
            chosePlainText = true;
            RefPtr<DocumentFragment> fragment =
                createFragmentFromText(context.get(), markup);
            if (fragment)
                return fragment.release();
        }
    }

    return 0;
}

} // namespace WebCore
