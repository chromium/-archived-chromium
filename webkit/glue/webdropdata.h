// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

