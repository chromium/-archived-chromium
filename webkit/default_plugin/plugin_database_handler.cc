// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/plugin_database_handler.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "third_party/libxml/include/libxml/parser.h"
#include "third_party/libxml/include/libxml/xpath.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/default_plugin/plugin_main.h"

using base::Time;
using base::TimeDelta;

PluginDatabaseHandler::PluginDatabaseHandler(
    PluginInstallerImpl& plugin_installer_instance)
    : plugin_downloads_file_(INVALID_HANDLE_VALUE),
      plugin_installer_instance_(plugin_installer_instance),
      ignore_plugin_db_data_(false) {
}

PluginDatabaseHandler::~PluginDatabaseHandler() {
  if (plugin_downloads_file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(plugin_downloads_file_);
    plugin_downloads_file_ = INVALID_HANDLE_VALUE;
  }
}

bool PluginDatabaseHandler::DownloadPluginsFileIfNeeded(
    const std::string& plugin_finder_url) {
  DCHECK(!plugin_finder_url.empty());
  // The time in days for which the plugins list is cached.
  // TODO(iyengar) Make this configurable.
  const int kPluginsListCacheTimeInDays = 3;

  plugin_finder_url_ = plugin_finder_url;

  PathService::Get(base::DIR_MODULE, &plugins_file_);
  plugins_file_ += L"\\chrome_plugins_file.xml";

  bool initiate_download = false;
  if (!file_util::PathExists(plugins_file_)) {
    initiate_download = true;
  } else {
    SYSTEMTIME creation_system_time = {0};
    if (!file_util::GetFileCreationLocalTime(plugins_file_,
                                             &creation_system_time)) {
      NOTREACHED();
      return false;
    }

    FILETIME creation_file_time = {0};
    ::SystemTimeToFileTime(&creation_system_time, &creation_file_time);

    FILETIME current_time = {0};
    ::GetSystemTimeAsFileTime(&current_time);

    Time file_time = Time::FromFileTime(creation_file_time);
    Time current_system_time = Time::FromFileTime(current_time);

    TimeDelta time_diff = file_time - current_system_time;
    if (time_diff.InDays() > kPluginsListCacheTimeInDays) {
      initiate_download = true;
    }
  }

  if (initiate_download) {
    DLOG(INFO) << "Initiating GetURLNotify on the plugin finder URL "
               << plugin_finder_url.c_str();

    plugin_installer_instance_.set_plugin_installer_state(
        PluginListDownloadInitiated);

    DCHECK(default_plugin::g_browser->geturlnotify);
    default_plugin::g_browser->geturlnotify(
        plugin_installer_instance_.instance(), plugin_finder_url.c_str(),
        NULL, NULL);
  } else {
    DLOG(INFO) << "Plugins file " << plugins_file_.c_str()
               << " already exists";
    plugin_downloads_file_ = ::CreateFile(plugins_file_.c_str(), GENERIC_READ,
                                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, NULL);
    if (plugin_downloads_file_ == INVALID_HANDLE_VALUE) {
      DLOG(INFO) << "Failed to open plugins file "
                 << plugins_file_.c_str() << " Error "
                 << ::GetLastError();
      NOTREACHED();
      return false;
    }
    // The URLNotify function contains all handling needed to parse the plugins
    // file and display the UI accordingly.
    plugin_installer_instance_.set_plugin_installer_state(
        PluginListDownloadInitiated);
    plugin_installer_instance_.URLNotify(plugin_finder_url.c_str(),
                                         NPRES_DONE);
  }
  return true;
}

int32 PluginDatabaseHandler::Write(NPStream* stream, int32 offset,
                                   int32 buffer_length, void* buffer) {
  if (ignore_plugin_db_data_) {
    return buffer_length;
  }

  if (plugin_downloads_file_ == INVALID_HANDLE_VALUE) {
    DLOG(INFO) << "About to create plugins file " << plugins_file_.c_str();
    plugin_downloads_file_ = CreateFile(plugins_file_.c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (plugin_downloads_file_ == INVALID_HANDLE_VALUE) {
      unsigned long error = ::GetLastError();
      if (error == ERROR_SHARING_VIOLATION) {
        // File may have been downloaded by another plugin instance on this
        // page.
        plugin_downloads_file_ = ::CreateFile(
            plugins_file_.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (plugin_downloads_file_ != INVALID_HANDLE_VALUE) {
          ignore_plugin_db_data_ = true;
          return buffer_length;
        }
      }

      DLOG(INFO) << "Failed to create plugins file "
                 << plugins_file_.c_str() << " Error "
                 << ::GetLastError();
      NOTREACHED();
      return 0;
    }
  }

  unsigned long bytes_written = 0;
  if (0 == lstrcmpiA(stream->url, plugin_finder_url_.c_str())) {
    DCHECK(plugin_downloads_file_ != INVALID_HANDLE_VALUE);

    WriteFile(plugin_downloads_file_, buffer, buffer_length, &bytes_written,
              NULL);
    DCHECK(buffer_length == bytes_written);
  }
  return bytes_written;
}


bool PluginDatabaseHandler::ParsePluginList() {
  if (plugin_downloads_file_ == INVALID_HANDLE_VALUE) {
    DLOG(WARNING) << "Invalid plugins file";
    NOTREACHED();
    return false;
  }

  bool parse_result = false;

  std::string plugins_file = WideToUTF8(plugins_file_.c_str());
  xmlDocPtr plugin_downloads_doc = xmlParseFile(plugins_file.c_str());
  if (plugin_downloads_doc == NULL) {
    DLOG(WARNING) << "Failed to parse plugins file " << plugins_file.c_str();
    return parse_result;
  }

  xmlXPathContextPtr context = NULL;
  xmlXPathObjectPtr plugins_result = NULL;

  do {
    context = xmlXPathNewContext(plugin_downloads_doc);
    if (context == NULL) {
      DLOG(WARNING) << "Failed to retrieve XPath context";
      NOTREACHED();
      parse_result = false;
      break;
    }

    plugins_result =
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//plugin"),
                               context);
    if ((plugins_result == NULL) ||
         xmlXPathNodeSetIsEmpty(plugins_result->nodesetval)) {
      DLOG(WARNING) << "Failed to find XPath //plugin";
      NOTREACHED();
      parse_result = false;
      break;
    }

    xmlNodeSetPtr plugin_list = plugins_result->nodesetval;
    for (int plugin_index = 0; plugin_index < plugin_list->nodeNr;
         ++plugin_index) {
      PluginDetail plugin_detail;
      if (!ReadPluginInfo(plugin_list->nodeTab[plugin_index]->children,
                          &plugin_detail)) {
        DLOG(ERROR) << "Failed to read plugin details at index "
                    << plugin_index;
        break;
      }
      downloaded_plugins_list_.push_back(plugin_detail);
    }
    if (downloaded_plugins_list_.size())
      parse_result = true;
  } while (0);

  xmlXPathFreeContext(context);
  xmlXPathFreeObject(plugins_result);
  xmlFreeDoc(plugin_downloads_doc);
  xmlCleanupParser();
  DLOG(INFO) << "Parse plugins file result " << parse_result;
  return parse_result;
}

bool PluginDatabaseHandler::GetPluginDetailsForMimeType(
    const char* mime_type, const char* language,
    std::string* download_url, std::wstring* plugin_name,
    bool* download_url_for_display) {
  if (!mime_type || !language || !download_url || !plugin_name ||
      !download_url_for_display) {
    NOTREACHED();
    return false;
  }

  PluginList::iterator plugin_index;
  for (plugin_index = downloaded_plugins_list_.begin();
       plugin_index != downloaded_plugins_list_.end();
       ++plugin_index) {
    PluginDetail& current_plugin = *plugin_index;

    std::vector<std::string>::iterator mime_type_index;
    for (mime_type_index = current_plugin.mime_types.begin();
         mime_type_index != current_plugin.mime_types.end();
         ++mime_type_index) {
      if ((0 == lstrcmpiA(mime_type, (*mime_type_index).c_str())) &&
          (0 == lstrcmpiA(language, current_plugin.language.c_str()))) {
        *download_url = current_plugin.download_url;
        *plugin_name = current_plugin.display_name;
        *download_url_for_display = current_plugin.download_url_for_display;
        return true;
      }
    }
  }
  return false;
}

void PluginDatabaseHandler::Close(bool delete_file) {
  if (plugin_downloads_file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(plugin_downloads_file_);
    plugin_downloads_file_ = INVALID_HANDLE_VALUE;
    if (delete_file) {
      ::DeleteFile(plugins_file_.c_str());
      plugins_file_.clear();
    }
  }
}

bool PluginDatabaseHandler::ReadPluginInfo(_xmlNode* plugin_node,
                                           PluginDetail* plugin_detail) {
  static const char kMimeTypeSeparator = ';';

  if (!plugin_node) {
    NOTREACHED();
    return false;
  }

  _xmlNode* plugin_mime_type = plugin_node->next;
  if ((plugin_mime_type == NULL) ||
      (plugin_mime_type->next == NULL)) {
    DLOG(WARNING) << "Failed to find mime type node in file";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_mime_type_vals = plugin_mime_type->children;
  if (plugin_mime_type_vals == NULL) {
    DLOG(WARNING) << "Invalid mime type";
    NOTREACHED();
    return false;
  }
  // Skip the first child of each node as it is the text element
  // for that node.
  _xmlNode* plugin_lang_node = plugin_mime_type->next->next;
  if ((plugin_lang_node == NULL) ||
      (plugin_lang_node->next == NULL)) {
    DLOG(WARNING) << "Failed to find plugin language node";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_lang_val = plugin_lang_node->children;
  if (plugin_lang_val == NULL) {
    DLOG(WARNING) << "Invalid plugin language";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_name_node = plugin_lang_node->next->next;
  if ((plugin_name_node == NULL) ||
      (plugin_name_node->next == NULL)) {
    DLOG(WARNING) << "Failed to find plugin name node";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_name_val = plugin_name_node->children;
  if (plugin_name_val == NULL) {
    DLOG(WARNING) << "Invalid plugin name";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_download_url_node = plugin_name_node->next->next;
  if (plugin_download_url_node == NULL) {
    DLOG(WARNING) << "Failed to find plugin URL node";
    NOTREACHED();
    return false;
  }
  _xmlNode* plugin_download_url_val = plugin_download_url_node->children;
  if (plugin_download_url_val == NULL) {
    DLOG(WARNING) << "Invalid plugin URL";
    NOTREACHED();
    return false;
  }

  // Look for the DisplayUrl node. By default every URL is treated as an
  // executable URL.
  plugin_detail->download_url_for_display = false;

  _xmlNode* plugin_download_url_for_display_node =
      plugin_download_url_node->next->next;
  if (plugin_download_url_for_display_node) {
    _xmlNode* url_for_display_val =
        plugin_download_url_for_display_node->children;
    if (url_for_display_val) {
      int url_for_display = 0;
      StringToInt(reinterpret_cast<const char*>(url_for_display_val->content),
                  &url_for_display);
      if (url_for_display != 0)
        plugin_detail->download_url_for_display = true;
    }
  }

  plugin_detail->display_name =
      UTF8ToWide(reinterpret_cast<const char*>(plugin_name_val->content));
  plugin_detail->download_url =
      reinterpret_cast<const char*>(plugin_download_url_val->content);

  SplitString(reinterpret_cast<const char*>(plugin_mime_type_vals->content),
              kMimeTypeSeparator, &plugin_detail->mime_types);

  plugin_detail->language =
      reinterpret_cast<const char*>(plugin_lang_val->content);

  return true;
}
