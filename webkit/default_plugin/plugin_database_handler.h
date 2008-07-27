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

#ifndef WEBKIT_DEFAULT_PLUGIN_PLUGIN_DATABASE_HANDLER_H
#define WEBKIT_DEFAULT_PLUGIN_PLUGIN_DATABASE_HANDLER_H

#include <windows.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "bindings/npapi.h"

// Individual plugin details
struct PluginDetail {
  std::string mime_type;
  std::string download_url;
  std::wstring display_name;
  std::string language;
};

typedef std::vector<PluginDetail> PluginList;

class PluginInstallerImpl;
struct _xmlNode;

// This class handles download of the plugins database file from the plugin
// finder URL passed in. It also provides functionality to parse the plugins
// file and to locate the desired plugin mime type in the parsed plugin list.
// The format of the plugins databse file is as below:-
// <plugins>
//    <plugin>
//      <mime> </mime> (Mime type of the plugin)
//      <lang> </lang> (Supported language)
//      <url> </url>   (Link to the plugin installer)
//    </plugin>
//    <plugin>
//  </plugins>
class PluginDatabaseHandler {
 public:
  // plugin_installer_instance is a reference maintained to the current
  // PluginInstallerImpl instance.
  explicit PluginDatabaseHandler(
      PluginInstallerImpl& plugin_installer_instance);

  virtual ~PluginDatabaseHandler();

  // Downloads the plugins database file if needed.
  //
  // Parameters:
  // plugin_finder_url
  //   Specifies the http/https location of the chrome plugin finder.
  // Returns true on success.
  bool DownloadPluginsFileIfNeeded(const std::string& plugin_finder_url);

  // Writes data to the plugins database file.
  //
  // Parameters:
  // stream
  //   Pointer to the current stream.
  // offset
  //   Indicates the data offset.
  // buffer_length
  //   Specifies the length of the data buffer.
  // buffer
  //   Pointer to the actual buffer.
  // Returns the number of bytes actually written, 0 on error.
  int32 Write(NPStream* stream, int32 offset, int32 buffer_length,
              void* buffer);

  const std::wstring& plugins_file() const {
    return plugins_file_;
  }

  // Parses the XML file containing the list of available third-party
  // plugins and adds them to a list.
  // Returns true on success
  bool ParsePluginList();

  // Returns the plugin details for the third party plugin mime type passed in.
  //
  // Parameters:
  // mime_type
  //   Specifies the mime type of the desired third party plugin.
  // language
  //   Specifies the desired plugin language.
  // download_url
  //   Output parameter which contans the plugin download URL on success.
  // display_name
  //   Output parameter which contains the display name of the plugin on
  //   success.
  // Returns true if the plugin details were found.
  bool GetPluginDetailsForMimeType(const char* mime_type,
                                   const char* language,
                                   std::string* download_url,
                                   std::wstring* display_name);

  // Closes the handle to the plugin database file.
  //
  // Parameters:
  // delete_file
  //   Indicates if the plugin database file should be deleted.
  void Close(bool delete_file);

 protected:
  // Reads plugin information off an individual XML node.
  //
  // Parameters:
  // plugin_node
  //   Pointer to the plugin XML node.
  // plugin_detail
  //   Output parameter which contains the details of the plugin on success.
  // Returns true on success.
  bool ReadPluginInfo(_xmlNode* plugin_node, PluginDetail* plugin_detail);

 private:
  // Contains the full path of the downloaded plugins file containing
  // information about available third party plugin downloads.
  std::wstring plugins_file_;
  // Handle to the downloaded plugins file.
  HANDLE plugin_downloads_file_;
  // List of downloaded plugins. This is generated the first time the
  // plugins file is downloaded and parsed. Each item in the list is
  // of the type PluginDetail, and contains information about the third
  // party plugin like it's mime type, download link, etc.
  PluginList downloaded_plugins_list_;
  // We maintain a reference to the PluginInstallerImpl instance to be able
  // to achieve plugin installer state changes and to notify the instance
  // about download completions.
  PluginInstallerImpl& plugin_installer_instance_;
  // The plugin finder url
  std::string plugin_finder_url_;
  // Set if the current instance should ignore plugin data. This is required
  // if multiple null plugin instances attempt to download the plugin
  // database. In this case the first instance to create the plugins database
  // file locally writes to the file. The remaining instances ignore the
  // downloaded data.
  bool ignore_plugin_db_data_;
};

#endif  // WEBKIT_DEFAULT_PLUGIN_PLUGIN_DATABASE_DOWNLOAD_HANDLER_H
