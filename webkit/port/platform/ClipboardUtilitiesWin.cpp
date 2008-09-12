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
#include "ClipboardUtilitiesWin.h"

#include "KURL.h"
#include "CString.h"
#include "DocumentFragment.h"
#include "markup.h"
#include "PlatformString.h"
#include "TextEncoding.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <wininet.h>    // for INTERNET_MAX_URL_LENGTH

#include "base/clipboard_util.h"

namespace WebCore {

HGLOBAL createGlobalData(const KURL& url, const String& title)
{
    String mutableURL(url.string());
    String mutableTitle(title);
    SIZE_T size = mutableURL.length() + mutableTitle.length() + 2;  // +1 for "\n" and +1 for null terminator
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size * sizeof(UChar));

    if (cbData) {
        PWSTR buffer = (PWSTR)::GlobalLock(cbData);
        swprintf_s(buffer, size, L"%s\n%s", mutableURL.charactersWithNullTermination(), mutableTitle.isNull() ? L"" : mutableTitle.charactersWithNullTermination());
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

HGLOBAL createGlobalData(String str)
{   
    SIZE_T size = (str.length() + 1) * sizeof(UChar);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size);
    if (cbData) {
        void* buffer = ::GlobalLock(cbData);
        memcpy(buffer, str.charactersWithNullTermination(), size);
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

HGLOBAL createGlobalData(CString str)
{
    SIZE_T size = str.length() * sizeof(char);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size + 1);
    if (cbData) {
        char* buffer = static_cast<char*>(::GlobalLock(cbData));
        memcpy(buffer, str.data(), size);
        buffer[size] = 0;
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

// Documentation for the CF_HTML format is available at http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
DeprecatedCString markupToCF_HTML(const String& markup, const String& srcURL)
{
    if (!markup.length())
        return DeprecatedCString();

    DeprecatedCString cf_html        ("Version:0.9");
    DeprecatedCString startHTML      ("\nStartHTML:");
    DeprecatedCString endHTML        ("\nEndHTML:");
    DeprecatedCString startFragment  ("\nStartFragment:");
    DeprecatedCString endFragment    ("\nEndFragment:");
    DeprecatedCString sourceURL      ("\nSourceURL:");

    bool shouldFillSourceURL = !srcURL.isEmpty() && (srcURL != "about:blank");
    if (shouldFillSourceURL)
        sourceURL.append(srcURL.utf8().data());

    DeprecatedCString startMarkup    ("\n<HTML>\n<BODY>\n<!--StartFragment-->\n");
    DeprecatedCString endMarkup      ("\n<!--EndFragment-->\n</BODY>\n</HTML>");

    // calculate offsets
    const unsigned UINT_MAXdigits = 10; // number of digits in UINT_MAX in base 10
    unsigned startHTMLOffset = cf_html.length() + startHTML.length() + endHTML.length() + startFragment.length() + endFragment.length() + (shouldFillSourceURL ? sourceURL.length() : 0) + (4*UINT_MAXdigits);
    unsigned startFragmentOffset = startHTMLOffset + startMarkup.length();
    CString markupUTF8 = markup.utf8();
    unsigned endFragmentOffset = startFragmentOffset + markupUTF8.length();
    unsigned endHTMLOffset = endFragmentOffset + endMarkup.length();

    // fill in needed data
    startHTML.append(String::format("%010u", startHTMLOffset).deprecatedString().utf8());
    endHTML.append(String::format("%010u", endHTMLOffset).deprecatedString().utf8());
    startFragment.append(String::format("%010u", startFragmentOffset).deprecatedString().utf8());
    endFragment.append(String::format("%010u", endFragmentOffset).deprecatedString().utf8());
    startMarkup.append(markupUTF8.data());

    // create full cf_html string from the fragments
    cf_html.append(startHTML);
    cf_html.append(endHTML);
    cf_html.append(startFragment);
    cf_html.append(endFragment);
    if (shouldFillSourceURL)
        cf_html.append(sourceURL);
    cf_html.append(startMarkup);
    cf_html.append(endMarkup);

    return cf_html;
}

String urlToMarkup(const KURL& url, const String& title)
{
    String markup("<a href=\"");
    markup.append(url.string());
    markup.append("\">");
    markup.append(title);
    markup.append("</a>");
    return markup;
}

String urlToImageMarkup(const KURL& url, const String& altStr) {
    String markup("<img src=\"");
    markup.append(url.string());
    markup.append("\"");
    if (!altStr.isEmpty()) {
        markup.append(" alt=\"");
        // TODO(dglazkov): escape string
        markup.append(altStr);
        markup.append("\"");
    }
    markup.append("/>");
    return markup;
}

void replaceNewlinesWithWindowsStyleNewlines(String& str)
{
    static const UChar Newline = '\n';
    static const String WindowsNewline("\r\n");
    str.replace(Newline, WindowsNewline);
}

void replaceNBSPWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}


PassRefPtr<DocumentFragment> fragmentFromFilenames(Document*, const IDataObject*)
{
    //FIXME: We should be able to create fragments from files
    return 0;
}

bool containsFilenames(const IDataObject*)
{
    //FIXME: We'll want to update this once we can produce fragments from files
    return false;
}

//Convert a String containing CF_HTML formatted text to a DocumentFragment
PassRefPtr<DocumentFragment> fragmentFromCF_HTML(Document* doc, const String& cf_html)
{        
    // obtain baseURL if present
    String srcURLStr("sourceURL:");
    String srcURL;
    unsigned lineStart = cf_html.find(srcURLStr, 0, false);
    if (lineStart != -1) {
        unsigned srcEnd = cf_html.find("\n", lineStart, false);
        unsigned srcStart = lineStart+srcURLStr.length();
        String rawSrcURL = cf_html.substring(srcStart, srcEnd-srcStart);
        replaceNBSPWithSpace(rawSrcURL);
        srcURL = rawSrcURL.stripWhiteSpace();
    }

    // find the markup between "<!--StartFragment -->" and "<!--EndFragment -->", accounting for browser quirks
    unsigned markupStart = cf_html.find("<html", 0, false);
    unsigned tagStart = cf_html.find("startfragment", markupStart, false);
    unsigned fragmentStart = cf_html.find('>', tagStart) + 1;
    unsigned tagEnd = cf_html.find("endfragment", fragmentStart, false);
    unsigned fragmentEnd = cf_html.reverseFind('<', tagEnd);
    String markup = cf_html.substring(fragmentStart, fragmentEnd - fragmentStart).stripWhiteSpace();

    return createFragmentFromMarkup(doc, markup, srcURL);
}


PassRefPtr<DocumentFragment> fragmentFromHTML(Document* doc, IDataObject* data) 
{
    if (!doc || !data)
        return 0;

    STGMEDIUM store;
    String html;
    String srcURL;
    if (SUCCEEDED(data->GetData(ClipboardUtil::GetHtmlFormat(), &store))) {
        //MS HTML Format parsing
        char* data = (char*)GlobalLock(store.hGlobal);
        SIZE_T dataSize = ::GlobalSize(store.hGlobal);
        String cf_html(UTF8Encoding().decode(data, dataSize));         
        GlobalUnlock(store.hGlobal);
        ReleaseStgMedium(&store); 
        if (PassRefPtr<DocumentFragment> fragment = fragmentFromCF_HTML(doc, cf_html))
            return fragment;
    } 
    if (SUCCEEDED(data->GetData(ClipboardUtil::GetTextHtmlFormat(), &store))) {
        //raw html
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        html = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        return createFragmentFromMarkup(doc, html, srcURL);
    } 

    return 0;
}

bool containsHTML(IDataObject* data)
{
    return SUCCEEDED(data->QueryGetData(ClipboardUtil::GetTextHtmlFormat())) ||
        SUCCEEDED(data->QueryGetData(ClipboardUtil::GetHtmlFormat()));
}

} // namespace WebCore
