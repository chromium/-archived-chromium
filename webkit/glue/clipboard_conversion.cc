// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/clipboard_conversion.h"

#include "build/build_config.h"

#include "ChromiumDataObject.h"
#include "ClipboardUtilitiesChromium.h"
#include "KURL.h"
#include "SharedBuffer.h"
#include <wtf/Vector.h>

#include "webkit/glue/glue_util.h"

namespace webkit_glue {

WebDropData ChromiumDataObjectToWebDropData(
    WebCore::ChromiumDataObject* data_object) {
  WebDropData drop_data;
  drop_data.url = KURLToGURL(data_object->url);
  drop_data.url_title = StringToStdWString(data_object->urlTitle);

  for (size_t i = 0; i < data_object->filenames.size(); ++i) {
    drop_data.filenames.push_back(StringToStdWString(
        data_object->filenames[i]));
  }

  drop_data.plain_text = StringToStdWString(data_object->plainText);

  drop_data.text_html = StringToStdWString(data_object->textHtml);
  drop_data.html_base_url = KURLToGURL(data_object->htmlBaseUrl);

  drop_data.file_description_filename = StringToStdWString(
      data_object->fileContentFilename);
  if (data_object->fileContent) {
    drop_data.file_contents.assign(data_object->fileContent->data(),
        data_object->fileContent->size());
  }

  return drop_data;
}

PassRefPtr<WebCore::ChromiumDataObject> WebDropDataToChromiumDataObject(
    const WebDropData& drop_data) {
  RefPtr<WebCore::ChromiumDataObject> data_object =
      WebCore::ChromiumDataObject::create();
  data_object->url = GURLToKURL(drop_data.url);
  data_object->urlTitle = StdWStringToString(drop_data.url_title);

  for (size_t i = 0; i < drop_data.filenames.size(); ++i) {
    data_object->filenames.append(StdWStringToString(drop_data.filenames[i]));
  }

  data_object->plainText = StdWStringToString(drop_data.plain_text);

  data_object->textHtml = StdWStringToString(drop_data.text_html);
  data_object->htmlBaseUrl = GURLToKURL(drop_data.html_base_url);

  data_object->fileContentFilename = StdWStringToString(
      drop_data.file_description_filename);
  if (!drop_data.file_contents.empty()) {
    data_object->fileContent =
        WebCore::SharedBuffer::create(drop_data.file_contents.data(),
                                      drop_data.file_contents.size());
  }

  return data_object;
}

}  // namespace webkit_glue
