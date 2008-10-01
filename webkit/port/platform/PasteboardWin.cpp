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

#include "ClipboardUtilitiesWin.h"
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

#include "base/clipboard_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

static UINT HTMLClipboardFormat = 0;
static UINT BookmarkClipboardFormat = 0;
static UINT WebSmartPasteFormat = 0;

Pasteboard* Pasteboard::generalPasteboard() 
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
{ 
    HTMLClipboardFormat = ClipboardUtil::GetHtmlFormat()->cfFormat;
    
    BookmarkClipboardFormat = ClipboardUtil::GetUrlWFormat()->cfFormat;
    
    WebSmartPasteFormat = ClipboardUtil::GetWebKitSmartPasteFormat()->cfFormat;
}

void Pasteboard::clear()
{
    webkit_glue::ClipboardClear();
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    clear();
    
    ExceptionCode ec = 0;
    webkit_glue::ClipboardWriteHTML(
        webkit_glue::StringToStdWString(
            createMarkup(selectedRange, 0, AnnotateForInterchange)),
        GURL(webkit_glue::StringToStdWString(
            selectedRange->startContainer(ec)->document()->url())));
    
    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    String str = frame->selectedText();
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    webkit_glue::ClipboardWriteText(webkit_glue::StringToStdWString(str));

    if (canSmartCopyOrDelete)
        webkit_glue::ClipboardWriteWebSmartPaste();
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    ASSERT(!url.isEmpty());

    clear();

    String title(titleStr);
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    // write to clipboard in format com.apple.safari.bookmarkdata to be able to paste into the bookmarks view with appropriate title
    webkit_glue::ClipboardWriteBookmark(webkit_glue::StringToStdWString(titleStr),
                                        webkit_glue::KURLToGURL(url));

    // write to clipboard in format CF_HTML to be able to paste into contenteditable areas as a link
    std::wstring link(webkit_glue::StringToStdWString(urlToMarkup(url, title)));
    webkit_glue::ClipboardWriteHTML(link, GURL());

    // bare-bones CF_UNICODETEXT support
    std::wstring spec(webkit_glue::StringToStdWString(url));
    webkit_glue::ClipboardWriteText(spec);
}

void Pasteboard::writeImage(Node* node, const KURL& url, const String& title)
{
    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = static_cast<RenderImage*>(node->renderer());
    CachedImage* cachedImage = static_cast<CachedImage*>(renderer->cachedImage());
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    clear();
    NativeImageSkia* bitmap = image->nativeImageForCurrentFrame();
    if (bitmap)
      webkit_glue::ClipboardWriteBitmap(*bitmap);
    if (!url.isEmpty()) {
        webkit_glue::ClipboardWriteBookmark(webkit_glue::StringToStdWString(title),
                                            webkit_glue::KURLToGURL(url));
        // write to clipboard in format com.apple.safari.bookmarkdata to be able to paste into the bookmarks view with appropriate title
        webkit_glue::ClipboardWriteBookmark(webkit_glue::StringToStdWString(title),
                                            webkit_glue::KURLToGURL(url));

        // write to clipboard in format CF_HTML to be able to paste into contenteditable areas as a an image
        std::wstring markup(webkit_glue::StringToStdWString(urlToImageMarkup(url, title)));
        webkit_glue::ClipboardWriteHTML(markup, GURL());

        // bare-bones CF_UNICODETEXT support
        std::wstring spec(webkit_glue::StringToStdWString(url.string()));
        webkit_glue::ClipboardWriteText(spec);
    }
}

bool Pasteboard::canSmartReplace()
{ 
    return webkit_glue::ClipboardIsFormatAvailable(WebSmartPasteFormat);
}

String Pasteboard::plainText(Frame* frame)
{
    if (webkit_glue::ClipboardIsFormatAvailable(CF_UNICODETEXT)) {
        std::wstring text;
        webkit_glue::ClipboardReadText(&text);
        if (!text.empty())
            return webkit_glue::StdWStringToString(text);
    }

    if (webkit_glue::ClipboardIsFormatAvailable(CF_TEXT)) {
        std::string text;
        webkit_glue::ClipboardReadAsciiText(&text);
        if (!text.empty())
            return webkit_glue::StdStringToString(text);
    }

    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    
    if (webkit_glue::ClipboardIsFormatAvailable(HTMLClipboardFormat)) {
        std::wstring markup;
        GURL src_url;
        webkit_glue::ClipboardReadHTML(&markup, &src_url);

        PassRefPtr<DocumentFragment> fragment =
            createFragmentFromMarkup(frame->document(),
                webkit_glue::StdWStringToString(markup),
                webkit_glue::StdStringToString(src_url.spec()));

        if (fragment)
            return fragment;
    }
     
    if (allowPlainText &&
        webkit_glue::ClipboardIsFormatAvailable(CF_UNICODETEXT)) {

        chosePlainText = true;
        std::wstring markup;
        webkit_glue::ClipboardReadText(&markup);
        RefPtr<DocumentFragment> fragment =
            createFragmentFromText(context.get(),
                                   webkit_glue::StdWStringToString(markup));

        if (fragment)
            return fragment.release();
    }

    if (allowPlainText && webkit_glue::ClipboardIsFormatAvailable(CF_TEXT)) {
        chosePlainText = true;
        std::string markup;
        webkit_glue::ClipboardReadAsciiText(&markup);
        RefPtr<DocumentFragment> fragment =
            createFragmentFromText(context.get(),
                                   webkit_glue::StdStringToString(markup));
        if (fragment)
            return fragment.release();
    }
    
    return 0;
}

} // namespace WebCore
