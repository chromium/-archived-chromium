// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/tools/test_shell/mock_webclipboard_impl.h"

#include "base/clipboard.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "webkit/api/public/WebImage.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;
using WebKit::WebURL;

bool MockWebClipboardImpl::isFormatAvailable(Format format) {
  switch (format) {
    case FormatHTML:
      return !m_htmlText.isEmpty();

    case FormatSmartPaste:
      return m_writeSmartPaste;

    default:
      NOTREACHED();
      return false;
  }
  return true;
}

WebKit::WebString MockWebClipboardImpl::readPlainText() {
  return m_plainText;
}

WebKit::WebString MockWebClipboardImpl::readHTML(WebKit::WebURL* url) {
  return m_htmlText;
}

void MockWebClipboardImpl::writeHTML(
    const WebKit::WebString& htmlText, const WebKit::WebURL& url,
    const WebKit::WebString& plainText, bool writeSmartPaste) {
  m_htmlText = htmlText;
  m_plainText = plainText;
  m_writeSmartPaste = writeSmartPaste;
}

void MockWebClipboardImpl::writeURL(
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  m_htmlText = UTF8ToUTF16(
      webkit_glue::WebClipboardImpl::URLToMarkup(url, title));
  m_plainText = UTF8ToUTF16(url.spec());
  m_writeSmartPaste = false;
}

void MockWebClipboardImpl::writeImage(const WebKit::WebImage& image,
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  if (!image.isNull()) {
    m_htmlText = UTF8ToUTF16(
        webkit_glue::WebClipboardImpl::URLToImageMarkup(url, title));
    m_plainText = m_htmlText;
    m_writeSmartPaste = false;
  }
}
