// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

// These functions are used by the javascript access to the 
// netscape.plugins object.  See PluginInfoStore.h.
// They are also used by WebViewImpl to check if a plugin exists for a given
// MIME type.
//

#include "config.h"

#include "PluginData.h"
#pragma warning(push, 0)
#include "PluginInfoStore.h"
#pragma warning(pop)
#undef LOG

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "glue/glue_util.h"
#include "glue/webkit_glue.h"
#include "net/base/mime_util.h"

namespace WebCore {

// We cache the plugins ourselves, since if we're getting them from the another
// process GetPlugins() will be expensive.
static bool g_loaded_plugins = false;
static std::vector<WebPluginInfo> g_plugins;

void LoadPlugins(bool refresh)
{
    if (g_loaded_plugins && !refresh)
      return;

    g_loaded_plugins = true;
    webkit_glue::GetPlugins(refresh, &g_plugins);
}

// Returns a PluginInfo pointer.  Caller is responsible for
// deleting contents of the PluginInfo.
PluginInfo* PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned int index) 
{
    LoadPlugins(false);

    WebCore::PluginInfo* rv = new WebCore::PluginInfo();
    const WebPluginInfo& plugin = g_plugins[index];
    rv->name = webkit_glue::StdWStringToString(plugin.name);
    rv->desc = webkit_glue::StdWStringToString(plugin.desc);
    rv->file = webkit_glue::StdWStringToString(
        file_util::GetFilenameFromPath(plugin.file));
    for (size_t j = 0; j < plugin.mime_types.size(); ++ j) {
        WebCore::MimeClassInfo* new_mime = new WebCore::MimeClassInfo();
        const WebPluginMimeType& mime_type = plugin.mime_types[j];
        new_mime->desc = webkit_glue::StdWStringToString(mime_type.description);

        for (size_t k = 0; k < mime_type.file_extensions.size(); ++k) {
          if (new_mime->suffixes.length())
            new_mime->suffixes.append(",");

          new_mime->suffixes.append(webkit_glue::StdStringToString(
              mime_type.file_extensions[k]));
        }

        new_mime->type = webkit_glue::StdStringToString(mime_type.mime_type);
        new_mime->plugin = rv;
        rv->mimes.append(new_mime);
    }

    return rv;
}

unsigned PluginInfoStore::pluginCount() const 
{
    LoadPlugins(false);

    return g_plugins.size();
}

bool PluginInfoStore::supportsMIMEType(const WebCore::String &mime_type) 
{
    LoadPlugins(false);

    std::wstring converted_mime_type = webkit_glue::StringToStdWString(mime_type);

    for (size_t i = 0; i < g_plugins.size(); ++i) {
        for (size_t j = 0; j < g_plugins[i].mime_types.size(); ++j) {
            if (net::MatchesMimeType(
                    g_plugins[i].mime_types[j].mime_type,
                    base::SysWideToNativeMB(converted_mime_type))) {
                // Don't allow wildcard matches here as this will result in
                // plugins being instantiated in cases where they should not.
                // For e.g. clicking on a link which causes a file to be
                // downloaded, special mime types like text/xml, etc. In any
                // case the webkit codepaths which invoke this function don't
                // expect wildcard plugin matches.
                if (g_plugins[i].mime_types[j].mime_type == "*") {
                  continue;
                }
                return true;
            }
        }
    }

    return false;
}

void refreshPlugins(bool) 
{
    LoadPlugins(true);
}

String GetPluginMimeTypeFromExtension(const String& extension) {
    LoadPlugins(false);

    std::string mime_type;
    std::string extension_std = WideToUTF8(
        webkit_glue::StringToStdWString(extension));
#if !defined(__linux__)
    // TODO(port): unstub once we have plugin support for Linux
    for (size_t i = 0; i < g_plugins.size(); ++i) {
        if (NPAPI::PluginList::SupportsExtension(
                g_plugins[i], extension_std, &mime_type))
            break;
    }
#endif

    return webkit_glue::StdStringToString(mime_type);
}

} // namespace WebCore
