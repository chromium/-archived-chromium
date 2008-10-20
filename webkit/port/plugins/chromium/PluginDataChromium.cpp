// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "PluginData.h"

#include "PluginInfoStore.h"

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

    PluginInfoStore c;
    for (size_t i = 0; i < plugins.size(); ++i) {
        PluginInfo* info = c.createPluginInfoForPluginAtIndex(i);
        m_plugins.append(info);
    }
}

void PluginData::refresh()
{
    // When next we initialize a PluginData, it'll be fresh.
    refreshData = true;
}

}
