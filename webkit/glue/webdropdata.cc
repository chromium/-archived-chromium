// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"

using WebKit::WebData;
using WebKit::WebDragData;
using WebKit::WebString;
using WebKit::WebVector;

WebDropData::WebDropData(const WebDragData& drag_data)
    : identity(0),
      url(drag_data.url()),
      url_title(drag_data.urlTitle()),
      file_extension(drag_data.fileExtension()),
      plain_text(drag_data.plainText()),
      text_html(drag_data.htmlText()),
      html_base_url(drag_data.htmlBaseURL()),
      file_description_filename(drag_data.fileContentFileName()) {
  if (drag_data.hasFileNames()) {
    WebVector<WebString> fileNames;
    drag_data.fileNames(fileNames);
    for (size_t i = 0; i < fileNames.size(); ++i)
      filenames.push_back(fileNames[i]);
  }
  WebData contents = drag_data.fileContent();
  if (!contents.isEmpty())
    file_contents.assign(contents.data(), contents.size());
}

WebDragData WebDropData::ToDragData() const {
  WebDragData result;
  result.initialize();
  result.setURL(url);
  result.setURLTitle(url_title);
  result.setFileExtension(file_extension);
  result.setFileNames(filenames);
  result.setPlainText(plain_text);
  result.setHTMLText(text_html);
  result.setHTMLBaseURL(html_base_url);
  result.setFileContentFileName(file_description_filename);
  result.setFileContent(WebData(file_contents.data(), file_contents.size()));
  return result;
}
