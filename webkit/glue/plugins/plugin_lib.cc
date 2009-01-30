// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace NPAPI
{

const char kPluginLibrariesLoadedCounter[] = "PluginLibrariesLoaded";
const char kPluginInstancesActiveCounter[] = "PluginInstancesActive";

// A list of all the instantiated plugins.
static std::vector<scoped_refptr<PluginLib> >* g_loaded_libs;

PluginLib* PluginLib::CreatePluginLib(const FilePath& filename) {
  // We can only have one PluginLib object per plugin as it controls the per
  // instance function calls (i.e. NP_Initialize and NP_Shutdown).  So we keep
  // a map of PluginLib objects.
  if (!g_loaded_libs)
    g_loaded_libs = new std::vector<scoped_refptr<PluginLib> >;

  for (size_t i = 0; i < g_loaded_libs->size(); ++i) {
    if ((*g_loaded_libs)[i]->plugin_info().path == filename)
      return (*g_loaded_libs)[i];
  }

  WebPluginInfo info;
  const PluginEntryPoints* entry_points = NULL;
  if (!PluginList::ReadPluginInfo(filename, &info, &entry_points))
    return NULL;

  return new PluginLib(info, entry_points);
}

void PluginLib::UnloadAllPlugins() {
  if (g_loaded_libs) {
    for (size_t i = 0; i < g_loaded_libs->size(); ++i)
      (*g_loaded_libs)[i]->Unload();

    delete g_loaded_libs;
    g_loaded_libs = NULL;
  }
}

void PluginLib::ShutdownAllPlugins() {
  if (g_loaded_libs) {
    for (size_t i = 0; i < g_loaded_libs->size(); ++i)
      (*g_loaded_libs)[i]->Shutdown();
  }
}

PluginLib::PluginLib(const WebPluginInfo& info,
                     const PluginEntryPoints* entry_points)
    : web_plugin_info_(info),
      library_(0),
      initialized_(false),
      saved_data_(0),
      instance_count_(0) {
  StatsCounter(kPluginLibrariesLoadedCounter).Increment();
  memset((void*)&plugin_funcs_, 0, sizeof(plugin_funcs_));
  g_loaded_libs->push_back(this);

  if (entry_points) {
    internal_ = true;
    entry_points_ = *entry_points;
  } else {
    internal_ = false;
    // We will read the entry points from the plugin directly.
    memset(&entry_points_, 0, sizeof(entry_points_));
  }
}

PluginLib::~PluginLib() {
  StatsCounter(kPluginLibrariesLoadedCounter).Decrement();
  if (saved_data_ != 0) {
    // TODO - delete the savedData object here
  }
}

NPPluginFuncs* PluginLib::functions() {
  return &plugin_funcs_;
}

NPError PluginLib::NP_Initialize() {
  if (initialized_)
    return NPERR_NO_ERROR;

  if (!Load())
    return NPERR_MODULE_LOAD_FAILED_ERROR;

  PluginHost* host = PluginHost::Singleton();
  if (host == 0)
    return NPERR_GENERIC_ERROR;

#if defined(OS_LINUX)
  NPError rv = entry_points_.np_initialize(host->host_functions(),
                                           &plugin_funcs_);
#else
  NPError rv = entry_points_.np_initialize(host->host_functions());
#endif
  initialized_ = (rv == NPERR_NO_ERROR);
  return rv;
}

void PluginLib::NP_Shutdown(void) {
  DCHECK(initialized_);
  entry_points_.np_shutdown();
}

PluginInstance* PluginLib::CreateInstance(const std::string& mime_type) {
  PluginInstance* new_instance = new PluginInstance(this, mime_type);
  instance_count_++;
  StatsCounter(kPluginInstancesActiveCounter).Increment();
  DCHECK(new_instance != 0);
  return new_instance;
}

void PluginLib::CloseInstance() {
  StatsCounter(kPluginInstancesActiveCounter).Decrement();
  instance_count_--;
  // If a plugin is running in its own process it will get unloaded on process
  // shutdown.
  if ((instance_count_ == 0) &&
       webkit_glue::IsPluginRunningInRendererProcess()) {
    Unload();
    for (size_t i = 0; i < g_loaded_libs->size(); ++i) {
      if ((*g_loaded_libs)[i].get() == this) {
        g_loaded_libs->erase(g_loaded_libs->begin() + i);
        break;
      }
    }
    if (g_loaded_libs->empty()) {
      delete g_loaded_libs;
      g_loaded_libs = NULL;
    }
  }
}

bool PluginLib::Load() {
  bool rv = false;
  NativeLibrary library = 0;

  if (!internal_) {
    if (library_ != 0)
      return rv;

    library = LoadNativeLibrary(web_plugin_info_.path);
    if (library == 0)
      return rv;

    rv = true;  // assume success now

    entry_points_.np_initialize =
        (NP_InitializeFunc)GetFunctionPointerFromNativeLibrary(library,
            FUNCTION_NAME("NP_Initialize"));
    if (entry_points_.np_initialize == 0)
      rv = false;

#if !defined(OS_LINUX)
    entry_points_.np_getentrypoints =
        (NP_GetEntryPointsFunc)GetFunctionPointerFromNativeLibrary(library,
            FUNCTION_NAME("NP_GetEntryPoints"));
    if (entry_points_.np_getentrypoints == 0)
      rv = false;
#endif

    entry_points_.np_shutdown =
        (NP_ShutdownFunc)GetFunctionPointerFromNativeLibrary(library,
            FUNCTION_NAME("NP_Shutdown"));
    if (entry_points_.np_shutdown == 0)
      rv = false;
  } else {
    rv = true;
  }

  if (rv) {
    plugin_funcs_.size = sizeof(plugin_funcs_);
    plugin_funcs_.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
#if !defined(OS_LINUX)
    if (entry_points_.np_getentrypoints(&plugin_funcs_) != NPERR_NO_ERROR)
      rv = false;
#else
    // On Linux, we get the plugin entry points during NP_Initialize.
#endif
  }

  if (!internal_) {
    if (rv)
      library_ = library;
    else
      UnloadNativeLibrary(library);
  }

  return rv;
}

// This class implements delayed NP_Shutdown and FreeLibrary on the plugin dll.
class FreePluginLibraryTask : public Task {
 public:
  FreePluginLibraryTask(PluginLib::NativeLibrary library,
                        NP_ShutdownFunc shutdown_func)
      : library_(library),
        NP_Shutdown_(shutdown_func) {
  }

  ~FreePluginLibraryTask() {}

  void Run() {
    if (NP_Shutdown_)
      NP_Shutdown_();

    if (library_) {
      PluginLib::UnloadNativeLibrary(library_);
      library_ = NULL;
    }
  }

 private:
  PluginLib::NativeLibrary library_;
  NP_ShutdownFunc NP_Shutdown_;
  DISALLOW_EVIL_CONSTRUCTORS(FreePluginLibraryTask);
};

void PluginLib::Unload() {
  if (!internal_ && library_) {
    // In case of single process mode, a plugin can delete itself
    // by executing a script. So delay the unloading of the library
    // so that the plugin will have a chance to unwind.
    bool defer_unload = webkit_glue::IsPluginRunningInRendererProcess();

#if USE(JSC)
    // The plugin NPAPI instances may still be around. Delay the
    // NP_Shutdown and FreeLibrary calls at least till the next
    // peek message.
    defer_unload = true;
#endif

    if (defer_unload) {
      FreePluginLibraryTask* free_library_task =
          new FreePluginLibraryTask(library_, entry_points_.np_shutdown);
      MessageLoop::current()->PostTask(FROM_HERE, free_library_task);
    } else {
      Shutdown();
      UnloadNativeLibrary(library_);
    }

    library_ = 0;
  }
}

void PluginLib::Shutdown() {
  if (initialized_ && !internal_) {
    NP_Shutdown();
    initialized_ = false;
  }
}

}  // namespace NPAPI
