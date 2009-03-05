// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DEFAULT_PLUGIN_PLUGIN_DATABASE_HANDLER_H
#define WEBKIT_DEFAULT_PLUGIN_PLUGIN_DATABASE_HANDLER_H

#include <windows.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi.h"

// Individual plugin details
struct PluginDetail {
  // List of mime types supported by the plugin.
  std::vector<std::string> mime_types;
  // The URL where the plugin can be downloaded from.
  std::string download_url;
  // The display name for the plugin.
  std::wstring display_name;
  // Language of the plugin installer. (en-us, etc).
  std::string language;
  // Indicates if the download URL points to an exe or to a URL which
  // needs to be displayed in a tab.
  bool download_url_for_display;
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
//      <mime_types> </mime_types> (semicolon separated list of mime types
//                                  supported by the plugin)
//      <lang> </lang> (Supported language)
//      <url> </url>   (Link to the plugin installer)
//      <displayurl> 0 </displayurl> (Indicates if the URL is a display URL.
//                                    defaults to 0.
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
  // download_url_for_display
  //   Output parameter which indicates if the plugin URL points to an exe
  //   or not.
  // Returns true if the plugin details were found.
  bool GetPluginDetailsForMimeType(const char* mime_type,
                                   const char* language,
                                   std::string* download_url,
                                   std::wstring* display_name,
                                   bool* download_url_for_display);

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

