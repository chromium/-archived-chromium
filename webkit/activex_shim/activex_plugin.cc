// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/activex_plugin.h"

#include <algorithm>

#include "base/fix_wp64.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/activex_shim/activex_util.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/activex_shim/npn_scripting.h"
#include "webkit/activex_shim/web_activex_site.h"

using std::string;
using std::wstring;

namespace activex_shim {

// Constant strings used in SetProp.
const wchar_t WNDPROP_ORIGINAL_WNDPROC[] = L"activexshim_orgwndproc";

ActiveXPlugin::ActiveXPlugin(NPP instance)
    : DispatchObject(NULL),
      npp_(instance),
      container_(NULL),
      windowless_(false),
      tried_activation_(false),
      control_activated_(false),
      activex_type_(ACTIVEX_GENERIC) {
  rect_.left = 0;
  rect_.top = 0;
  rect_.right = 0;
  rect_.bottom = 0;
}

ActiveXPlugin::~ActiveXPlugin() {
  // Releases all spawned Dispatch objects so that we won't have dangling
  // references.
  ReleaseSpawned();
}

// Firefox makes it pretty easy to distiguish between attrs and real params.
// it always places attrs first, then a pair with name "PARAM" and empty value.
// However, Chrome always put params first, then attrs. Need to figure out
// a way to handle them nicely.
void ActiveXPlugin::ProcessParams(int16 argc, char* argn[], char* argv[]) {
  // TODO(ruijiang): This list is not exhaustive yet. Add all possible
  // commmon attributes.
  static const char* const excluded_param_names[] = {
    "id", "name", "type", "class", "classid",
    "codebase", "width", "height" };

  // Handle parameters.
  for (int i = 0; i < argc; i++) {
    if (argn[i] == NULL)
      continue;

    ControlParam param;
    param.name = UTF8ToWide(argn[i]);
    // Sometimes browser will pass NULL when no value is present.
    if (argv[i] != NULL) {
      param.value = UTF8ToWide(argv[i]);
    }
    if (LowerCaseEqualsASCII(param.name, "classid")) {
      std::string clsid_ascii;
      if (GetClsidFromClassidAttribute(argv[i], &clsid_ascii)) {
        std::wstring raw_clsid = UTF8ToWide(clsid_ascii);
        clsid_ = wstring(L"{") + raw_clsid + L"}";
        activex_type_ = MapClassIdToType(clsid_ascii);
      }
    }
    if (LowerCaseEqualsASCII(param.name, "codebase")) {
      codebase_ = param.value;
    }
    bool ignore = false;
    for (int i = 0; i < arraysize(excluded_param_names); i++) {
      if (LowerCaseEqualsASCII(param.name, excluded_param_names[i])) {
        ignore = true;
        break;
      }
    }
    if (!ignore)
      params_.push_back(param);
  }
}

void ActiveXPlugin::ConvertForEmbeddedWmp() {
  clsid_ = L"{6bf52a52-394a-11d3-b153-00c04f79faa6}";
  ControlParam* existing_url_param = NULL;
  std::wstring src;
  // Find the src parameter and use it to add a new url parameter.
  // Find the volume parameter and setup defaults which make sense in
  // the Activex media player world.
  for (unsigned int i = 0; i < params_.size(); i++) {
    if (LowerCaseEqualsASCII(params_[i].name, "src")) {
      src = params_[i].value;
    } else if (LowerCaseEqualsASCII(params_[i].name, "url")) {
      existing_url_param = &params_[i];
    } else if (LowerCaseEqualsASCII(params_[i].name, "volume")) {
      // In the NPAPI media player world a volume value lesser than
      // -3000 turns off the volume. A volume value of 0 indicates
      // full volume. Translate these to their Activex counterparts.
      int specified_volume = 0;
      if (StringToInt(params_[i].value, &specified_volume)) {
        // Valid volume lies between 0 and -3000
        specified_volume = std::min (0, std::max(-3000, specified_volume));
        // Translate to a value between 0 and 100.
        int activex_volume = specified_volume / 30 + 100;
        params_[i].value = IntToWString(activex_volume);
      }
    }
  }

  if (!src.empty()) {
    if (existing_url_param == NULL)
      params_.push_back(ControlParam(L"url", src));
    else
      existing_url_param->value = src;
  }
}

NPError ActiveXPlugin::NPP_New(NPMIMEType plugin_type, int16 argc, char* argn[],
                               char* argv[], NPSavedData* saved) {
  ProcessParams(argc, argn, argv);

  // If mimetype is not activex, it must be windows media type. Do necessary
  // param conversion.
  if (!IsMimeTypeActiveX(plugin_type))
    ConvertForEmbeddedWmp();

  DCHECK(container_ == NULL);
  container_.reset(new NoRefIUnknownImpl<WebActiveXContainer>);
  // At this time we don't know the browser window yet.
  container_->Init(this);
  HRESULT hr = container_->CreateControlWithSite(clsid_.c_str());
  // TODO(ruijiang): We may still return OK, then show error inside the control
  // so that user may get a chance to install it.
  if (FAILED(hr))
    return NPERR_GENERIC_ERROR;

  // Does the control support windowless activation?
  // TODO(ruijiang): temporary disable windowless plugin cause it's not fully
  // working yet.
  if (false && container_->GetFirstSite()->inplace_object_windowless_ != NULL) {
    // TODO(ruijiang): Fix this. Right now Chrome will never return browser
    // window when plugin hasn't set NPPVpluginWindowBool to false yet.
    // Fix Chrome then we could remove this line.
    g_browser->setvalue(npp_, NPPVpluginWindowBool, false);
    // If we could get the container window successfully, we could go
    // windowless.
    HWND hwnd = NULL;
    g_browser->getvalue(npp_, NPNVnetscapeWindow, &hwnd);
    if (hwnd) {
      container_->set_container_wnd(hwnd);
      g_browser->setvalue(npp_, NPPVpluginWindowBool, false);
      windowless_ = true;
    } else {
      g_browser->setvalue(npp_, NPPVpluginWindowBool,
                          reinterpret_cast<void*>(true));
    }
  }

  // TODO(ruijiang): It is very common that controls query for the current url
  // during activation. In the current Chrome multi-process structure this
  // often causes deadlock (e.g. realplayer). Let's cache the url first while
  // looking for ways to solve deadlock.
  GetCurrentURL();
  return 0;
}

void SubclassWindow(HWND hwnd, WNDPROC wndproc) {
  LONG_PTR org_wndproc = SetWindowLongPtr(
      hwnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc));
  SetProp(hwnd, WNDPROP_ORIGINAL_WNDPROC,
          reinterpret_cast<HANDLE>(org_wndproc));
}

// Unsubclass a window that has been subclassed by us (has the property
// WNDPROP_ORIGINAL_WNDPROC)
void UnsubclassWindow(HWND hwnd) {
  WNDPROC org_wndproc = static_cast<WNDPROC>(
      GetProp(hwnd, WNDPROP_ORIGINAL_WNDPROC));
  // Either this window has already been unsubclassed or it is not subclassed
  // by us.
  if (org_wndproc == NULL)
    return;
  SetWindowLongPtr(hwnd,
                   GWL_WNDPROC,
                   reinterpret_cast<LONG_PTR>(org_wndproc));
  RemoveProp(hwnd, WNDPROP_ORIGINAL_WNDPROC);
}

// Window procedure to subclass Window created by control.
LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg,
                              WPARAM wparam, LPARAM lparam) {
  WNDPROC org_wndproc = static_cast<WNDPROC>(
      GetProp(hwnd, WNDPROP_ORIGINAL_WNDPROC));
  switch(msg) {
    case WM_KEYDOWN:
      if (wparam == VK_TAB) {
        // TODO(ruijiang): Handle the tab key to transfer focus back to browser.
        // HWND hparent = GetParent(hwnd);
        // HWND hparent2 = GetParent(hparent);
        // PostMessage(hparent2, WM_KEYDOWN, wparam, lparam);
        // return 0;
      }
      break;
    case WM_DESTROY:
      // Unsubclass myself.
      UnsubclassWindow(hwnd);
      break;
  }
  if (org_wndproc != NULL)
    return CallWindowProc(org_wndproc, hwnd, msg, wparam, lparam);
  else
    return 0;
}

// NPP API Processing.
NPError ActiveXPlugin::NPP_SetWindow(NPWindow* window) {
  if (window->type != NPWindowTypeWindow &&
      window->type != NPWindowTypeDrawable)
    return NPERR_GENERIC_ERROR;

  // Remember the window position. This position is relative to the browser.
  rect_.left = window->x;
  rect_.top = window->y;
  rect_.right = rect_.left + window->width;
  rect_.bottom = rect_.top + window->height;

  RECT client;
  client.left = 0;
  client.top = 0;
  client.right = window->width;
  client.bottom = window->height;

  // This happens when we did not create the container because we do not
  // allow initialization of certain ActiveX objects.
  if (container_ == NULL)
    return NPERR_GENERIC_ERROR;

  if (!tried_activation_) {
    // Do not try activation again.
    tried_activation_ = true;

    // For windowed controls we need to get the plugin window.
    if (window->type == NPWindowTypeWindow)
      container_->set_container_wnd(static_cast<HWND>(window->window));
    WebActiveXSite* site = container_->GetFirstSite();
    if (site == NULL)
      return NPERR_GENERIC_ERROR;
    POINT pos;
    if (windowless()) {
      pos.x = window->x;
      pos.y = window->y;
    } else {
      pos.x = 0;
      pos.y = 0;
    }
    HRESULT hr = site->ActivateControl(pos.x, pos.y, window->width,
                                       window->height, params_);
    if (FAILED(hr))
      return NPERR_GENERIC_ERROR;

    // We are done with activation.
    control_activated_ = true;

    if (window->type == NPWindowTypeWindow) {
      HWND hwnd = static_cast<HWND>(window->window);
      // The window some browser (FF) created does not clip children. It will
      // cause blinking of the control area during resizing, clicking etc.
      SetWindowLong(hwnd, GWL_STYLE,
                    GetWindowLong(hwnd, GWL_STYLE) | WS_CLIPCHILDREN |
                    WS_CLIPSIBLINGS);
      // If the control has a window, we need to subclass it.
      CComQIPtr<IOleWindow> ole_window = container_->GetFirstControl();
      if (ole_window != NULL) {
        HWND control_wnd = NULL;
        hr = ole_window->GetWindow(&control_wnd);
        if (SUCCEEDED(hr)) {
          SubclassWindow(control_wnd, ControlWindowProc);
        }
      }
    }
    return 0;
  } else if (control_activated_) {
    WebActiveXSite* site = container_->GetFirstSite();
    DCHECK(site != NULL);
    if (window->type == NPWindowTypeWindow) {
      site->SetRect(&client);
    } else {
      site->SetRect(&rect_);
    }
    return 0;
  } else {
    return NPERR_GENERIC_ERROR;
  }
}

NPError ActiveXPlugin::NPP_NewStream(NPMIMEType type, NPStream* stream,
                                     NPBool seekable, uint16* stype) {
  return 0;
}

NPError ActiveXPlugin::NPP_DestroyStream(NPStream* stream, NPReason reason) {
  return 0;
}

int32 ActiveXPlugin::NPP_WriteReady(NPStream* stream) {
  // TODO(ruijiang): Now returns an arbitary value. Will handle it later.
  return 65536;
}

int32 ActiveXPlugin::NPP_Write(NPStream* stream, int32 offset, int32 len,
                               void* buffer) {
  // TODO(ruijiang): Pretend we have processed it. Otherwise FireFox will
  // pretty much deadlock.
  return len;
}

void ActiveXPlugin::NPP_StreamAsFile(NPStream* stream, const char* fname) {
}

void ActiveXPlugin::NPP_Print(NPPrint* platformPrint) {
}

int16 ActiveXPlugin::NPP_HandleEvent(void* event) {
  if (!control_activated_)
    return NPERR_GENERIC_ERROR;

  NPEvent* evt = static_cast<NPEvent*>(event);
  // TODO(ruijiang): Handle various events here for windowless control.
  switch (evt->event) {
    case WM_PAINT:
      return HandlePaintEvent(
          reinterpret_cast<HDC>(static_cast<LONG_PTR>(evt->wParam)),
          reinterpret_cast<NPRect*>(static_cast<LONG_PTR>(evt->lParam)));
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_KEYUP:
    case WM_KEYDOWN:
    case WM_SETFOCUS:
      return HandleInputEvent(evt->event, evt->wParam, evt->lParam);
    case WM_SETCURSOR:
      // TODO(ruijiang): seems we are not getting this message
      break;
    case WM_KILLFOCUS:
      // TODO(ruijiang): We are not getting this message yet.
      break;
    default:
      break;
  }

  return 0;
}

int16 ActiveXPlugin::HandlePaintEvent(HDC dc, NPRect* invalid_area) {
  // Chrome sets world transform by a certain offset in some cases, e.g.,
  // clicking on the control. This will cause unfortunate effect on ActiveX
  // control, because some will try to adjust the drawing rect and reset the
  // window/view point origin to 0. However, they are not aware of the new
  // SetWolrTransform feature. Thus causing drawing off the real control area
  // (see atlctl.h: CComControlBase::OnDrawAdvanced)
  // On the other hand, FireFox never changes the origins. I've spent hours
  // figuring out what went wrong...
  int saved = SaveDC(dc);

  POINT offset;
  offset.x = offset.y = 0;
  // Easy way to figure out the difference between world and device.
  LPtoDP(dc, &offset, 1);
  RECT rc = rect_;
  OffsetRect(&rc, offset.x, offset.y);

  // Reset everything so that device page has the same origin as the world.
  SetWindowOrgEx(dc, 0, 0, NULL);
  SetViewportOrgEx(dc, 0, 0, NULL);
  if (GetGraphicsMode(dc) == GM_ADVANCED) {
    XFORM f;
    if (GetWorldTransform(dc, &f)) {
      f.eDx = 0;
      f.eDy = 0;
      SetWorldTransform(dc, &f);
    }
  }

  WebActiveXSite* site = container_->GetFirstSite();
  if (site->view_object_ != NULL)
    site->view_object_->Draw(DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc,
                             reinterpret_cast<RECTL*>(&rc), NULL, NULL, 0);

  RestoreDC(dc, saved);
  return 0;
}

int16 ActiveXPlugin::HandleInputEvent(uint32 msg, uint32 wparam,
                                      uint32 lparam) {
  WebActiveXSite* site = container_->GetFirstSite();
  if (site->inplace_object_windowless_ == NULL)
    return 0;
  LRESULT result;
  HRESULT hr = site->inplace_object_windowless_->OnWindowMessage(
      msg, wparam, lparam, &result);
  return 0;
}

void ActiveXPlugin::NPP_URLNotify(const char* url, NPReason reason,
                                  void* notifyData) {
}

NPError ActiveXPlugin::NPP_GetValue(NPPVariable variable, void* value) {
  if (variable == NPPVpluginScriptableNPObject) {
    *(static_cast<void**>(value)) = GetScriptableNPObject();
    return 0;
  }
  return NPERR_GENERIC_ERROR;
}

NPError ActiveXPlugin::NPP_SetValue(NPNVariable variable, void* value) {
  // No setable value yet.
  return NPERR_GENERIC_ERROR;
}

void ActiveXPlugin::Draw(HDC dc, RECT* lprc, RECT* lpclip) {
  // TODO(ruijiang): Temporary. Fix this later.
  int ret = FillRect(dc, lprc,
                     static_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));
  TextOut(dc, lprc->left, lprc->top, _T("HelloWorld"), 5);
}

IDispatch* ActiveXPlugin::GetDispatch() {
  if (container_ == NULL)
    return NULL;
  IUnknown* unk = container_->GetFirstControl();
  CComQIPtr<IDispatch> disp = unk;
  if (disp == NULL)
    return NULL;
  IDispatch* res = disp.Detach();
  res->Release();
  return res;
}

NPNScriptableObject ActiveXPlugin::GetWindow() {
  if (!window_.IsValid()) {
    NPObject* object = NULL;
    g_browser->getvalue(npp_, NPNVWindowNPObject, &object);
    window_ = NPNScriptableObject(npp_, object);
  }
  return window_;
}

std::wstring ActiveXPlugin::GetCurrentURL() {
  if (url_.size())
    return url_;
  url_ = GetWindow().GetObjectProperty("document").GetStringProperty("URL");
  return url_;
}

std::wstring ActiveXPlugin::ResolveURL(const std::wstring& url) {
  // TODO(ruijiang): consider the base element of document.
  std::wstring doc_url = GetCurrentURL();
  GURL base(doc_url);
  GURL ret = base.Resolve(url);
  return UTF8ToWide(ret.spec());
}


}  // namespace activex_shim
