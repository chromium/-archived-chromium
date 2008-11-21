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

#include "ChromiumBridge.h"
#include "PluginData.h"
#if COMPILER(MSVC)
__pragma(warning(push, 0))
#endif
#include "PluginInfoStore.h"
#if COMPILER(MSVC)
__pragma(warning(pop))
#endif
#undef LOG

namespace WebCore {

// We cache the plugins ourselves, since if we're getting them from the another
// process GetPlugins() will be expensive.
static bool g_loaded_plugins = false;
static Vector<PluginInfo*> g_plugins;

void LoadPlugins(bool refresh)
{
    if (g_loaded_plugins && !refresh)
        return;

    if (g_loaded_plugins) {
        for (size_t i = 0; i < g_plugins.size(); ++i)
            deleteAllValues(g_plugins[i]->mimes);
        deleteAllValues(g_plugins);
        g_plugins.clear();
    }

    g_loaded_plugins = true;
    // these are leaked at shutdown
    ChromiumBridge::getPlugins(refresh, &g_plugins);
}

// Returns a PluginInfo pointer.  Caller is responsible for
// deleting contents of the PluginInfo.
PluginInfo* PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned int index) 
{
    LoadPlugins(false);

    WebCore::PluginInfo* rv = new WebCore::PluginInfo();
    const WebCore::PluginInfo* plugin = g_plugins[index];
    rv->name = plugin->name;
    rv->desc = plugin->desc;
    rv->file = plugin->file;
    for (size_t j = 0; j < plugin->mimes.size(); ++j) {
        WebCore::MimeClassInfo* new_mime = new WebCore::MimeClassInfo();
        const WebCore::MimeClassInfo* mime_type = plugin->mimes[j];
        new_mime->type = mime_type->type;
        new_mime->desc = mime_type->desc;
        new_mime->suffixes = mime_type->suffixes;
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

    for (size_t i = 0; i < g_plugins.size(); ++i) {
        for (size_t j = 0; j < g_plugins[i]->mimes.size(); ++j) {
            if (ChromiumBridge::matchesMIMEType(g_plugins[i]->mimes[j]->type, 
                                                mime_type)) {
                // Don't allow wildcard matches here as this will result in
                // plugins being instantiated in cases where they should not.
                // For e.g. clicking on a link which causes a file to be
                // downloaded, special mime types like text/xml, etc. In any
                // case the webkit codepaths which invoke this function don't
                // expect wildcard plugin matches.
                if (g_plugins[i]->mimes[j]->type == "*")
                    continue;
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

String GetPluginMimeTypeFromExtension(const String& extension) 
{
#if !defined(__linux__)
    // TODO(port): unstub once we have plugin support for Linux
    LoadPlugins(false);

    for (size_t i = 0; i < g_plugins.size(); ++i) {
        PluginInfo* plugin = g_plugins[i];
        for (size_t j = 0; j < plugin->mimes.size(); ++j) {
            MimeClassInfo* mime = plugin->mimes[j];
            Vector<String> extensions;
            mime->suffixes.split(",", extensions);
            for (size_t k = 0; k < extensions.size(); ++k) {
                if (extension == extensions[k])
                    return mime->type;
            }
        }
    }
#endif

    return String();
}

} // namespace WebCore
