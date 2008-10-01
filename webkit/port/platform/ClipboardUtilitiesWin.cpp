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

FORMATETC* cfHDropFormat()
{
    static FORMATETC urlFormat = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

//Firefox text/html
static FORMATETC* texthtmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"text/html");
    static FORMATETC texthtmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &texthtmlFormat;
}

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

HGLOBAL createGlobalData(const String& str)
{
    HGLOBAL globalData = ::GlobalAlloc(GPTR, (str.length() + 1) * sizeof(UChar));
    if (!globalData)
        return 0;
    UChar* buffer = static_cast<UChar*>(::GlobalLock(globalData));
    memcpy(buffer, str.characters(), str.length() * sizeof(UChar));
    buffer[str.length()] = 0;
    ::GlobalUnlock(globalData);
    return globalData;
}

HGLOBAL createGlobalData(const Vector<char>& vector)
{
    HGLOBAL globalData = ::GlobalAlloc(GPTR, vector.size() + 1);
    if (!globalData)
        return 0;
    char* buffer = static_cast<char*>(::GlobalLock(globalData));
    memcpy(buffer, vector.data(), vector.size());
    buffer[vector.size()] = 0;
    ::GlobalUnlock(globalData);
    return globalData;
}

static void append(Vector<char>& vector, const char* string)
{
    vector.append(string, strlen(string));
}

static void append(Vector<char>& vector, const CString& string)
{
    vector.append(string.data(), string.length());
}

// Documentation for the CF_HTML format is available at http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
void markupToCF_HTML(const String& markup, const String& srcURL, Vector<char>& result)
{
    if (markup.isEmpty())
        return;

    #define MAX_DIGITS 10
    #define MAKE_NUMBER_FORMAT_1(digits) MAKE_NUMBER_FORMAT_2(digits)
    #define MAKE_NUMBER_FORMAT_2(digits) "%0" #digits "u"
    #define NUMBER_FORMAT MAKE_NUMBER_FORMAT_1(MAX_DIGITS)

    const char* header = "Version:0.9\n"
        "StartHTML:" NUMBER_FORMAT "\n"
        "EndHTML:" NUMBER_FORMAT "\n"
        "StartFragment:" NUMBER_FORMAT "\n"
        "EndFragment:" NUMBER_FORMAT "\n";
    const char* sourceURLPrefix = "SourceURL:";

    const char* startMarkup = "<HTML>\n<BODY>\n<!--StartFragment-->\n";
    const char* endMarkup = "\n<!--EndFragment-->\n</BODY>\n</HTML>";

    CString sourceURLUTF8 = srcURL == blankURL() ? "" : srcURL.utf8();
    CString markupUTF8 = markup.utf8();

    // calculate offsets
    unsigned startHTMLOffset = strlen(header) - strlen(NUMBER_FORMAT) * 4 + MAX_DIGITS * 4;
    if (sourceURLUTF8.length())
        startHTMLOffset += strlen(sourceURLPrefix) + sourceURLUTF8.length() + 1;
    unsigned startFragmentOffset = startHTMLOffset + strlen(startMarkup);
    unsigned endFragmentOffset = startFragmentOffset + markupUTF8.length();
    unsigned endHTMLOffset = endFragmentOffset + strlen(endMarkup);

    append(result, String::format(header, startHTMLOffset, endHTMLOffset, startFragmentOffset, endFragmentOffset).utf8());
    if (sourceURLUTF8.length()) {
        append(result, sourceURLPrefix);
        append(result, sourceURLUTF8);
        result.append('\n');
    }
    append(result, startMarkup);
    append(result, markupUTF8);
    append(result, endMarkup);

    #undef MAX_DIGITS
    #undef MAKE_NUMBER_FORMAT_1
    #undef MAKE_NUMBER_FORMAT_2
    #undef NUMBER_FORMAT
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
    static const char* const WindowsNewline("\r\n");
    str.replace(Newline, WindowsNewline);
}

void replaceNBSPWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}

FORMATETC* urlWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocatorW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* urlFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocator");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* plainTextFormat()
{
    static FORMATETC textFormat = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

FORMATETC* plainTextWFormat()
{
    static FORMATETC textFormat = {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

FORMATETC* filenameWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileNameW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* filenameFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileName");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

//MSIE HTML Format
FORMATETC* htmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"HTML Format");
    static FORMATETC htmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &htmlFormat;
}

FORMATETC* smartPasteFormat()
{
    static UINT cf = RegisterClipboardFormat(L"WebKit Smart Paste Format");
    static FORMATETC htmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &htmlFormat;
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
