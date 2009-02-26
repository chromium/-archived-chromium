// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing data being dropped on a webview.  This represents a
// union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef WEBKIT_GLUE_WEBDROPDATA_H_
#define WEBKIT_GLUE_WEBDROPDATA_H_

#include <string>
#include <vector>
#include "googleurl/src/gurl.h"

struct IDataObject;

struct WebDropData {
  // Construct with a given drag identity.  Note: identity is an int32 because
  // it is passed over the renderer NPAPI interface to gears.
  explicit WebDropData(int32 drag_identity) : identity(drag_identity) {}
  int32 identity;

  // For default constructions, use drag |identity| 0.
  WebDropData() : identity(0) {}

  // User is dragging a link into the webview.
  GURL url;
  std::wstring url_title;  // The title associated with |url|.

  // File extension for dragging images from a webview to the desktop.
  std::wstring file_extension;

  // User is dropping one or more files on the webview.
  std::vector<std::wstring> filenames;

  // User is dragging plain text into the webview.
  std::wstring plain_text;

  // User is dragging text/html into the webview (e.g., out of Firefox). 
  // |html_base_url| is the URL that the html fragment is taken from (used to
  // resolve relative links).  It's ok for |html_base_url| to be empty.
  std::wstring text_html;
  GURL html_base_url;

  // User is dragging data from the webview (e.g., an image).
  std::wstring file_description_filename;
  std::string file_contents;

  // Helper method for converting Window's specific IDataObject to a WebDropData
  // object.  TODO(tc): Move this to the browser side since it's Windows
  // specific and no longer used in webkit.
  static void PopulateWebDropData(IDataObject* data_object,
                                  WebDropData* drop_data);
};

#endif  // WEBKIT_GLUE_WEBDROPDATA_H_
