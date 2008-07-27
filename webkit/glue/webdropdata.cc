// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  // data_object used by the test_shell.
  drop_data->data_object = data_object;
}
