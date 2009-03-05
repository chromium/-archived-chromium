// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_PLUGIN_H__
#define WEBKIT_ACTIVEX_SHIM_ACTIVEX_PLUGIN_H__

#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/activex_shim/dispatch_object.h"
#include "webkit/activex_shim/npn_scripting.h"
#include "webkit/activex_shim/web_activex_container.h"
#include "webkit/activex_shim/web_activex_site.h"

namespace activex_shim {

// ActiveXPlugin, a host for ActiveX control. There is one ActiveXPlugin object
// for each ActiveX control. It handles NPAPI calls from the browser side
// and is responsible for most activities of the plugin.
class ActiveXPlugin : public DispatchObject {
 public:
  explicit ActiveXPlugin(NPP instance);
  ~ActiveXPlugin();

  // NPP API Processing.
  NPError NPP_New(NPMIMEType plugin_type, int16 argc, char* argn[],
                  char* argv[], NPSavedData* saved);
  NPError NPP_SetWindow(NPWindow* window);
  NPError NPP_NewStream(NPMIMEType type, NPStream* stream,
                        NPBool seekable, uint16* stype);
  NPError NPP_DestroyStream(NPStream* stream, NPReason reason);
  int32 NPP_WriteReady(NPStream* stream);
  int32 NPP_Write(NPStream* stream, int32 offset, int32 len, void* buffer);
  void NPP_StreamAsFile(NPStream* stream, const char* fname);
  void NPP_Print(NPPrint* platformPrint);
  int16 NPP_HandleEvent(void* event);
  void NPP_URLNotify(const char* url, NPReason reason, void* notifyData);
  NPError NPP_GetValue(NPPVariable variable, void* value);
  NPError NPP_SetValue(NPNVariable variable, void* value);

  void Draw(HDC dc, RECT* lprc, RECT* lpclip);

  // Get scriptable window object from the browser.
  NPNScriptableObject GetWindow();
  // Retrieves the URL of current webpage from the browser.
  std::wstring GetCurrentURL();
  // Resolves the relative URL (could be already an absolute URL too) to return
  // full URL based on current document's URL and base.
  std::wstring ResolveURL(const std::wstring& url);
  bool windowless() { return windowless_; }
  NPP npp() { return npp_; }
  ActiveXTypes activex_type() { return activex_type_; }

 private:
  // Process parameters passed in from browser.
  void ProcessParams(int16 argc, char* argn[], char* argv[]);
  // For handling wmp mime type, we need to initialize params differently
  // (change src to corresponding param for wmp control), and set clsid to wmp.
  void ConvertForEmbeddedWmp();

  // Overrided functions of base class: DispatchObject.
  virtual IDispatch* GetDispatch();
  virtual NPP GetNPP() { return npp_; }
  virtual bool NPObjectOwnsMe() { return false; }

  // Event handlers for windowless plugin.
  int16 HandlePaintEvent(HDC dc, NPRect* invalid_area);
  int16 HandleInputEvent(uint32 msg, uint32 wparam, uint32 lparam);

  // Related NPP instance.
  NPP npp_;
  // Position of the control relative to the browser.
  RECT rect_;
  // Initialization parameters from param tags and object tag.
  std::vector<ControlParam> params_;
  // Clsid of the activex object.
  std::wstring clsid_;
  std::wstring codebase_;
  // At this point every plugin has one container. It simplifies things.
  scoped_ptr<WebActiveXContainer> container_;
  // True if the control supports windowless and we are creating a windowless
  // plugin.
  bool windowless_;
  // Have we ever tried to activate the control.
  bool tried_activation_;
  // Whether we have successfully created the control and activated it.
  bool control_activated_;
  // Initially this is not initialized. We assign it on the first call to
  // GetWindow, then we will keep this copy.
  NPNScriptableObject window_;
  ActiveXTypes activex_type_;
  // Cache of the current url. It is used very frequently.
  std::wstring url_;

  DISALLOW_EVIL_CONSTRUCTORS(ActiveXPlugin);
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_PLUGIN_H__

