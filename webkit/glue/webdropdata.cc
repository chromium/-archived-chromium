// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include "base/clipboard_util.h"
#include "googleurl/src/gurl.h"
#include <shellapi.h>
#include <shlobj.h>

/* static */
void WebDropData::PopulateWebDropData(IDataObject* data_object,
                                      WebDropData* drop_data) {
  std::wstring url_str;
  if (ClipboardUtil::GetUrl(data_object, &url_str, &drop_data->url_title)) {
    GURL test_url(url_str);
    if (test_url.is_valid())
      drop_data->url = test_url;
  }
  ClipboardUtil::GetFilenames(data_object, &drop_data->filenames);
  ClipboardUtil::GetPlainText(data_object, &drop_data->plain_text);
  ClipboardUtil::GetCFHtml(data_object, &drop_data->cf_html);
  ClipboardUtil::GetTextHtml(data_object, &drop_data->text_html);
  ClipboardUtil::GetFileContents(data_object,
      &drop_data->file_description_filename, &drop_data->file_contents);
}

