/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "plugin/npapi_host_control/win/np_plugin_proxy.h"

#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include "base/scoped_ptr.h"
#include "plugin/npapi_host_control/win/module.h"
#include "plugin/npapi_host_control/win/np_browser_proxy.h"
#include "plugin/npapi_host_control/win/np_object_proxy.h"
#include "plugin/npapi_host_control/win/stream_operation.h"

namespace {

const wchar_t kPluginName[] = L"npo3dautoplugin.dll";
const wchar_t kAppDataPluginLocation[] =
    L"Mozilla\\plugins\\npo3dautoplugin.dll";

// Returns the path to the O3D plug-in located in the current user's
// Application Data directory.  Returns NULL on failure.
// Note:  The caller does not need to free the returned string.
const wchar_t* GetApplicationDataPluginPath() {
  static wchar_t kAppDataPath[MAX_PATH] = {0};
  HRESULT hr = SHGetFolderPath(0, CSIDL_APPDATA, NULL, 0, kAppDataPath);
  if (SUCCEEDED(hr)) {
    PathAppend(kAppDataPath, kAppDataPluginLocation);
    return kAppDataPath;
  } else {
    return NULL;
  }
}

// Returns a path to the O3D plug-in corresponding to the value of the
// MOZ_PLUGIN_PATH environment variable.  This variable is used to override
// the default directory where FireFox will search for plug-ins.
// Note:  The caller does not need to free the returned string.
const wchar_t* GetMozillaPluginPath() {
  static wchar_t kMozillaPluginPath[MAX_PATH] = {0};
  DWORD chars_written = GetEnvironmentVariable(L"MOZ_PLUGIN_PATH",
                                               kMozillaPluginPath,
                                               MAX_PATH);
  if (chars_written == 0) {
    return NULL;
  } else if (chars_written > MAX_PATH) {
    ATLASSERT(false && "MOZ_PLUGIN_PATH too large to represent a path.");
    return NULL;
  } else {
    PathAppend(kMozillaPluginPath, kPluginName);
    return kMozillaPluginPath;
  }
}

const wchar_t kProgramFilesPluginLocation[] =
    L"Mozilla Firefox\\plugins\\npo3dautoplugin.dll";

// Returns the path to the O3D plug-in located in the Program
// Files directory.  Returns NULL on failure.
// Note: The caller does not need to free the returned string.
const wchar_t* GetProgramFilesPluginPath() {
  static wchar_t kProgramFilesPath[MAX_PATH] = {0};
  HRESULT hr = SHGetFolderPath(0,
                               CSIDL_PROGRAM_FILES,
                               NULL,
                               0,
                               kProgramFilesPath);
  if (SUCCEEDED(hr)) {
    PathAppend(kProgramFilesPath, kProgramFilesPluginLocation);
    return kProgramFilesPath;
  } else {
    return NULL;
  }
}

// Helper class implementing RAII semantics for locking the ATL module.
class AutoModuleLock {
 public:
  AutoModuleLock() {
    NPAPIHostControlModule::LockModule();
  }
  ~AutoModuleLock() {
    NPAPIHostControlModule::UnlockModule();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(AutoModuleLock);
};

// Helper routine that populates nested scoped arrays of characters
// from std vectors of CStringA objects.  This routine is used to make
// a local copy of the name/value arguments to the plug-in instance,
// so that any local modifications on the arguments performed during
// plug-in initialization won't propagate to future instantiations of
// the plug-in.
void ConstructLocalPluginArgs(const std::vector<CStringA>& names,
                              const std::vector<CStringA>& values,
                              short* argc,
                              scoped_array<scoped_array<char> >* argn,
                              scoped_array<scoped_array<char> >* argv) {
  ATLASSERT(argc && argn && argv);
  ATLASSERT(names.size() == values.size());

  *argc = static_cast<short>(names.size());
  if (names.empty()) {
    argn->reset(NULL);
    argv->reset(NULL);
    return;
  }

  // Copy the contents of the name and value arrays to the scoped_array
  // parameters.
  argn->reset(new scoped_array<char>[*argc]);
  argv->reset(new scoped_array<char>[*argc]);
  for (int x = 0; x < *argc; ++x) {
    char* name = new char[names[x].GetLength() + 1];
    char* value = new char[values[x].GetLength() + 1];

    strcpy(name, static_cast<const char*>(names[x]));
    strcpy(value, static_cast<const char*>(values[x]));

    (*argn)[x].reset(name);
    (*argv)[x].reset(value);
  }
}

}  // unnamed namespace

int NPPluginProxy::kPluginInstanceCount = 0;

NPPluginProxy::NPPluginProxy()
    : browser_proxy_(NULL),
      scriptable_object_(NULL),
      NP_Initialize_(NULL),
      NP_GetEntryPoints_(NULL),
      NP_Shutdown_(NULL),
      plugin_module_(0) {
  npp_data_.ndata = npp_data_.pdata = NULL;
  memset(&plugin_funcs_, NULL, sizeof(plugin_funcs_));
}

NPPluginProxy::~NPPluginProxy() {
  // Serialize the destruction of instances so that there are no races on
  // the instance count, and library loads.
  AutoModuleLock lock;
  if (0 == --kPluginInstanceCount) {
    if (NP_Shutdown_) {
      NP_Shutdown_();
    }
  }

  FreeLibrary(plugin_module_);
  ATLASSERT(active_stream_ops_.empty() &&
            "Destruction of plugin proxy with still-pending streaming ops.");
}

bool NPPluginProxy::MapEntryPoints(HMODULE loaded_module) {
  // Initialize the function pointers to the plugin entry points.
  NP_Initialize_ = reinterpret_cast<NP_InitializeFunc>(
      GetProcAddress(loaded_module, "NP_Initialize"));
  NP_GetEntryPoints_ = reinterpret_cast<NP_GetEntryPointsFunc>(
      GetProcAddress(loaded_module, "NP_GetEntryPoints"));
  NP_Shutdown_ = reinterpret_cast<NP_ShutdownFunc>(
      GetProcAddress(loaded_module, "NP_Shutdown"));

  if (!NP_Initialize_ || !NP_GetEntryPoints_ || !NP_Shutdown_) {
    ATLASSERT(false && "NPAPI DLL exports not present.");
    return false;
  }

  // Plugin-initialization is to be performed once, at initial plug-in
  // loading time.  Note that this routine must be accessed serially to
  // protect against races on kPluginInstanceCount.
  if (0 == kPluginInstanceCount) {
    if (NPERR_NO_ERROR != NP_Initialize_(
            browser_proxy_->GetBrowserFunctions())) {
      ATLASSERT(false && "NPAPI initialization failure.");
      return false;
    }
  }
  ++kPluginInstanceCount;

  if (NPERR_NO_ERROR != NP_GetEntryPoints_(&plugin_funcs_)) {
    ATLASSERT(false && "Unknown failure getting NPAPI entry points.");
    return false;
  }

  plugin_module_ = loaded_module;
  return true;
}

bool NPPluginProxy::Init(NPBrowserProxy* browser_proxy,
                         const NPWindow& window,
                         const std::vector<CStringA>& argument_names,
                         const std::vector<CStringA>& argument_values) {
  ATLASSERT(plugin_module_ &&
            "Plugin module not loaded before initialization.");
  ATLASSERT(browser_proxy && "Browser environment required for plugin init.");
  browser_proxy_ = browser_proxy;

  // Store a pointer to the browser proxy instance in the netscape data
  // of the plugin data.  This will be the only access point to the browser
  // instance from within the NPBrowserProxy NPAPI functions.
  npp_data_.ndata = static_cast<void*>(browser_proxy_);

  scoped_array<scoped_array<char> > argn, argv;
  short argc;

  // Build a local-copy of the plug-in arguments, so that any modifications
  // on the name/value pairs will not be propagated to future instantiations.
  ConstructLocalPluginArgs(argument_names,
                           argument_values,
                           &argc,
                           &argn,
                           &argv);

  if (NPERR_NO_ERROR != plugin_funcs_.newp(
        "No mime type",
        GetNPP(),
        NP_EMBED,
        argc,
        reinterpret_cast<char**>(argn.get()),
        reinterpret_cast<char**>(argv.get()),
        NULL)) {
    NP_Shutdown_();
    ATLASSERT(false && "Unknown failure creating NPAPI plugin instance.");
    return false;
  }

  if (NPERR_NO_ERROR != plugin_funcs_.setwindow(
          GetNPP(),
          const_cast<NPWindow*>(&window))) {
    plugin_funcs_.destroy(GetNPP(), NULL);
    NP_Shutdown_();
    ATLASSERT(false  && "Unknown failure binding plugin window.");
    return false;
  }

  // We assume that the plugin is scripted, so get the scripting entry points
  // from the plugin.
  NPObject *np_object = NULL;
  if (NPERR_NO_ERROR != plugin_funcs_.getvalue(
          GetNPP(),
          NPPVpluginScriptableNPObject,
          static_cast<void*>(&np_object))) {
    plugin_funcs_.destroy(GetNPP(), NULL);
    NP_Shutdown_();
    ATLASSERT(false  && "Unable to initialize NPAPI scripting interface.");
    return false;
  }
  ATLASSERT(np_object);

  HRESULT hr = NPObjectProxy::CreateInstance(&scriptable_object_);
  ATLASSERT(SUCCEEDED(hr));

  scriptable_object_->SetBrowserProxy(browser_proxy_);
  scriptable_object_->SetHostedObject(np_object);

  browser_proxy_->RegisterNPObjectProxy(np_object, scriptable_object_);

  NPBrowserProxy::GetBrowserFunctions()->releaseobject(np_object);

  return true;
}

void NPPluginProxy::TearDown() {
  // Block until all stream operations requested by this plug-in have
  // completed.
  HRESULT hr;
  std::vector<HANDLE> stream_handles;
  for (int x = 0; x < active_stream_ops_.size(); ++x) {
    // Request that the stream finish early - so that large file transfers do
    // not block leaving the page.
    hr = active_stream_ops_[x]->RequestCancellation();
    ATLASSERT(SUCCEEDED(hr) &&
              "Failed to request cancellation of pending data stream.");
    stream_handles.push_back(active_stream_ops_[x]->GetThreadHandle());
  }

  static const unsigned int kWaitTimeOut = 120000;
  while (!stream_handles.empty()) {
    DWORD wait_code = MsgWaitForMultipleObjects(
        static_cast<DWORD>(stream_handles.size()),
        &stream_handles[0],
        FALSE,
        kWaitTimeOut,
        QS_ALLINPUT);
    wait_code -= WAIT_OBJECT_0;
    if (wait_code == stream_handles.size()) {
      MSG msg;
      GetMessage(&msg, NULL, 0, 0);
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else if (wait_code >= 0 && wait_code < stream_handles.size()) {
      // A thread has completed, so remove the handle and continue.
      stream_handles.erase(stream_handles.begin() + wait_code);
    } else {
      if (wait_code == WAIT_TIMEOUT + WAIT_OBJECT_0) {
        ATLASSERT(false &&
                  "Time-out waiting for completion of streaming operation.");
      } else {
        ATLASSERT(false &&
                  "Unknown error waiting on streaming operation completion.");
      }
      // There has been a catastropic error waiting for the pending transfers.
      // Kill all of the threads and leave the loop.
      // Note:  This approach will potentially leak resources allocated by
      // the plug-in, but it prevents access to stale data by the threads
      // once the plug-in has been unloaded.
      for (int x = 0; x < active_stream_ops_.size(); ++x) {
        BOOL thread_kill = TerminateThread(stream_handles[x], 0);
        ATLASSERT(thread_kill && "Failure killing stalled download thread.");
      }
      break;
    }
  }

  if (plugin_module_) {
    scriptable_object_ = NULL;
    plugin_funcs_.destroy(GetNPP(), NULL);
  }
}

void NPPluginProxy::RegisterStreamOperation(StreamOperation* stream_op) {
#ifndef NDEBUG
  StreamOpArray::iterator iter = std::find(active_stream_ops_.begin(),
                                           active_stream_ops_.end(),
                                           stream_op);
  ATLASSERT(iter == active_stream_ops_.end() &&
            "Duplicate registration of a StreamOperation.");
#endif
  active_stream_ops_.push_back(stream_op);
}

void NPPluginProxy::UnregisterStreamOperation(StreamOperation* stream_op) {
  StreamOpArray::iterator iter = std::find(active_stream_ops_.begin(),
                                           active_stream_ops_.end(),
                                           stream_op);
  ATLASSERT(iter != active_stream_ops_.end() &&
            "Unregistration of an unrecognized StreamOperation.");
  active_stream_ops_.erase(iter);
}

HRESULT NPPluginProxy::GetScriptableObject(
    INPObjectProxy** scriptable_object) const {
  ATLASSERT(scriptable_object);

  if (!scriptable_object_) {
    return E_FAIL;
  }

  *scriptable_object = scriptable_object_;
  (*scriptable_object)->AddRef();
  return S_OK;
}

HRESULT NPPluginProxy::Create(NPPluginProxy** proxy_instance) {
  ATLASSERT(proxy_instance);
  // Lock the module so that there are no races against the NP_Initialize
  // and NP_Shutdown calls.  NP_Initialize and NP_Shutdown parallel the
  // behaviour of DLL_PROCESS_ATTACH and DLL_PROCESS_DETACH.
  // We serialize all construction and destruction to ensure that any
  // plug-in initialization mimics this behaviour.
  AutoModuleLock lock;

  // First attempt to load the plug-in from the directory specified by the
  // MOZ_PLUGIN_PATH directory.
  HMODULE np_plugin = NULL;
  const wchar_t *plugin_path = GetMozillaPluginPath();
  if (plugin_path) {
    np_plugin = LoadLibrary(plugin_path);
  }

  if (!np_plugin) {
    // Attempt to load the plug-in from the installation directory.
    plugin_path = GetApplicationDataPluginPath();
    if (plugin_path) {
      np_plugin = LoadLibrary(plugin_path);
    }

    if (!np_plugin) {
      plugin_path = GetProgramFilesPluginPath();
      if (plugin_path) {
        np_plugin = LoadLibrary(plugin_path);
      }

      if (!np_plugin) {
        // As a last-ditch attempt, try to load the plug-in using the system
        // library path.
        np_plugin = LoadLibrary(kPluginName);
        if (!np_plugin) {
          ATLASSERT(false && "Unable to load plugin module.");
          return E_FAIL;
        }
      }
    }
  }

  // Load and initialize the plug-in with the current window settings.
  scoped_ptr<NPPluginProxy> plugin_proxy(new NPPluginProxy);
  if (!plugin_proxy->MapEntryPoints(np_plugin)) {
    FreeLibrary(np_plugin);
    return E_FAIL;
  }

  *proxy_instance = plugin_proxy.release();
  return S_OK;
}
