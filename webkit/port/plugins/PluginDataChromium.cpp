// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "PluginData.h"

#undef LOG
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

static bool refreshData = false;

void PluginData::initPlugins()
{
    std::vector<WebPluginInfo> plugins;
    if (!webkit_glue::GetPlugins(refreshData, &plugins))
        return;
    refreshData = false;

    for (size_t i = 0; i < plugins.size(); ++i) {
        const WebPluginInfo& sourceInfo = plugins[i];

        PluginInfo* info = new PluginInfo;
        info->name = webkit_glue::StdWStringToString(sourceInfo.name);
        info->file = webkit_glue::StdWStringToString(sourceInfo.file);
        info->desc = webkit_glue::StdWStringToString(sourceInfo.desc);

        for (size_t j = 0; j < sourceInfo.mime_types.size(); ++j) {
            const WebPluginMimeType& mimeType = sourceInfo.mime_types[j];

            MimeClassInfo* mime = new MimeClassInfo;
            mime->type = webkit_glue::StdStringToString(mimeType.mime_type);
            mime->desc = webkit_glue::StdWStringToString(mimeType.description);

            for (size_t k = 0; k < mimeType.file_extensions.size(); ++k) {
                if (k > 0)
                    mime->suffixes += ",";
                mime->suffixes += webkit_glue::StdStringToString(mimeType.file_extensions[k]);
            }
            info->mimes.append(mime);
        }
        m_plugins.append(info);
    }
}

void PluginData::refresh()
{
    // When next we initialize a PluginData, it'll be fresh.
    refreshData = true;
}

}
