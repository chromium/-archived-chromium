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

#include "config.h"

#include "ChromiumBridge.h"
#include "PluginData.h"
#include "PluginInfoStore.h"

namespace WebCore {

class PluginCache {
public:
    PluginCache() : m_loaded(false) { }

    ~PluginCache()
    {
        clear();
    }

    void load(bool refresh)
    {
        if (m_loaded) {
            if (!refresh)
                return;
            clear();
        } else {
            m_loaded = true;
        }
        ChromiumBridge::plugins(refresh, &m_plugins);
    }

    void clear()
    {
        for (size_t i = 0; i < m_plugins.size(); ++i)
            deleteAllValues(m_plugins[i]->mimes);
        deleteAllValues(m_plugins);
        m_plugins.clear();
    }

    // Returns a PluginInfo pointer.  Caller is responsible for
    // deleting contents of the PluginInfo.
    PluginInfo* createPluginInfoForPluginAtIndex(unsigned int index) 
    {
        load(false);

        WebCore::PluginInfo* rv = new WebCore::PluginInfo();
        const WebCore::PluginInfo* plugin = m_plugins[index];
        rv->name = plugin->name;
        rv->desc = plugin->desc;
        rv->file = plugin->file;
        for (size_t j = 0; j < plugin->mimes.size(); ++j) {
            WebCore::MimeClassInfo* newMime = new WebCore::MimeClassInfo();
            const WebCore::MimeClassInfo* mimeType = plugin->mimes[j];
            newMime->type = mimeType->type;
            newMime->desc = mimeType->desc;
            newMime->suffixes = mimeType->suffixes;
            newMime->plugin = rv;
            rv->mimes.append(newMime);
        }

        return rv;
    }

    unsigned pluginCount() 
    {
        load(false);

        return m_plugins.size();
    }

    bool supportsMIMEType(const WebCore::String &mime_type) 
    {
        load(false);

        for (size_t i = 0; i < m_plugins.size(); ++i) {
            for (size_t j = 0; j < m_plugins[i]->mimes.size(); ++j) {
                if (ChromiumBridge::matchesMIMEType(
                        m_plugins[i]->mimes[j]->type, mime_type)) {
                    // Don't allow wildcard matches here as this will result in
                    // plugins being instantiated in cases where they should
                    // not. For e.g. clicking on a link which causes a file to
                    // be downloaded, special mime types like text/xml, etc. In
                    // any case the webkit codepaths which invoke this function
                    // don't expect wildcard plugin matches.
                    if (m_plugins[i]->mimes[j]->type == "*")
                        continue;
                    return true;
                }
            }
        }

        return false;
    }

    String getPluginMimeTypeFromExtension(const String& extension) 
    {
#if !defined(__linux__)
        // TODO(port): unstub once we have plugin support for Linux

        load(false);

        for (size_t i = 0; i < m_plugins.size(); ++i) {
            PluginInfo* plugin = m_plugins[i];
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

private:
    bool m_loaded;
    Vector<PluginInfo*> m_plugins;
};

// We cache the plugins ourselves, since if we're getting them from the another
// process GetPlugins() will be expensive.
static PluginCache pluginCache;

PluginInfo* PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned int index) 
{
    return pluginCache.createPluginInfoForPluginAtIndex(index);
}

unsigned PluginInfoStore::pluginCount() const 
{
    return pluginCache.pluginCount();
}

bool PluginInfoStore::supportsMIMEType(const WebCore::String &mime_type) 
{
    return pluginCache.supportsMIMEType(mime_type);
}

void refreshPlugins(bool) 
{
    pluginCache.load(true);
}

String getPluginMimeTypeFromExtension(const String& extension) 
{
    return pluginCache.getPluginMimeTypeFromExtension(extension);
}

} // namespace WebCore
