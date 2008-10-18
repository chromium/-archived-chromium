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

// Modified from Apple's version to not directly call any windows methods as
// they may not be available to us in the multiprocess 

#include "config.h"
#include "DragData.h"

#if PLATFORM(WIN_OS)
#include "ClipboardWin.h"
#include "ClipboardUtilitiesWin.h"
#include "WCDataObject.h"
#endif

#include "DocumentFragment.h"
#include "KURL.h"
#include "PlatformString.h"
#include "Markup.h"

#undef LOG
#include "base/file_util.h"
#include "base/string_util.h"
#include "net/base/base64.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"

namespace {

bool containsHTML(const WebDropData& drop_data) {
    std::wstring html;
    return drop_data.cf_html.length() > 0
        || drop_data.text_html.length() > 0;
}

}

namespace WebCore {

PassRefPtr<Clipboard> DragData::createClipboard(ClipboardAccessPolicy policy) const
{
// TODO(darin): Invent ClipboardChromium and use that instead.
#if PLATFORM(WIN_OS)
    WCDataObject* data;
    WCDataObject::createInstance(&data);
    RefPtr<ClipboardWin> clipboard = ClipboardWin::create(true, data, policy);
    // The clipboard keeps a reference to the WCDataObject, so we can release
    // our reference to it.
    data->Release();

    return clipboard.release();
#else
    return PassRefPtr<Clipboard>(0);
#endif
}

bool DragData::containsURL() const
{
    return m_platformDragData->url.is_valid();
}

String DragData::asURL(String* title) const
{
    if (!m_platformDragData->url.is_valid())
        return String();
 
    // |title| can be NULL
    if (title)
        *title = webkit_glue::StdWStringToString(m_platformDragData->url_title);
    return webkit_glue::StdStringToString(m_platformDragData->url.spec());
}

bool DragData::containsFiles() const
{
    return !m_platformDragData->filenames.empty();
}

void DragData::asFilenames(Vector<String>& result) const
{
    for (size_t i = 0; i < m_platformDragData->filenames.size(); ++i)
        result.append(webkit_glue::StdWStringToString(m_platformDragData->filenames[i]));
}

bool DragData::containsPlainText() const
{
    return !m_platformDragData->plain_text.empty();
}

String DragData::asPlainText() const
{
    return webkit_glue::StdWStringToString(
        m_platformDragData->plain_text);
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::canSmartReplace() const
{
    // Mimic the situations in which mac allows drag&drop to do a smart replace.
    // This is allowed whenever the drag data contains a 'range' (ie.,
    // ClipboardWin::writeRange is called).  For example, dragging a link
    // should not result in a space being added.
    return !m_platformDragData->cf_html.empty() &&
           !m_platformDragData->plain_text.empty() &&
           !m_platformDragData->url.is_valid();
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText() || containsURL()
        || ::containsHTML(*m_platformDragData)
        || containsColor();
}

PassRefPtr<DocumentFragment> DragData::asFragment(Document* doc) const
{     
    /*
     * Order is richest format first. On OSX this is:
     * * Web Archive
     * * Filenames
     * * HTML
     * * RTF
     * * TIFF
     * * PICT
     */

     // TODO(tc): Disabled because containsFilenames is hardcoded to return
     // false.  We need to implement fragmentFromFilenames when this is
     // re-enabled in Apple's win port.
     //if (containsFilenames())
     //    if (PassRefPtr<DocumentFragment> fragment = fragmentFromFilenames(doc, m_platformDragData))
     //        return fragment;

     if (!m_platformDragData->cf_html.empty()) {
         RefPtr<DocumentFragment> fragment = fragmentFromCF_HTML(doc,
             webkit_glue::StdWStringToString(m_platformDragData->cf_html));
         return fragment;
     }

     if (!m_platformDragData->text_html.empty()) {
         String url;
         RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(doc,
             webkit_glue::StdWStringToString(m_platformDragData->text_html), url);
         return fragment;
     }

     return 0;
}

Color DragData::asColor() const
{
    return Color();
}

}
