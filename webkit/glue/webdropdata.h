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
//
// A struct for managing data being dropped on a webview.  This represents a
// union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef WEBKIT_GLUE_WEBDROPDATA_H__
#define WEBKIT_GLUE_WEBDROPDATA_H__

#include <string>
#include <vector>
#include "googleurl/src/gurl.h"

struct IDataObject;

struct WebDropData {
  // User is dropping a link on the webview.
  GURL url;
  std::wstring url_title;  // The title associated with |url|.

  // User is dropping one or more files on the webview.
  std::vector<std::wstring> filenames;

  // User is dragging plain text into the webview.
  std::wstring plain_text;

  // User is dragging MS HTML into the webview (e.g., out of IE).
  std::wstring cf_html;

  // User is dragging text/html into the webview (e.g., out of Firefox).
  std::wstring text_html;

  // User is dragging data from the webview (e.g., an image).
  std::wstring file_description_filename;
  std::string file_contents;

  // A reference to the underlying IDataObject.  This is a Windows drag and
  // drop specific object.  This should only be used by the test shell.
  IDataObject* data_object;

  // Helper method for converting Window's specific IDataObject to a WebDropData
  // object.
  static void PopulateWebDropData(IDataObject* data_object,
                                  WebDropData* drop_data);
};

#endif  // WEBKIT_GLUE_WEBDROPDATA_H__
