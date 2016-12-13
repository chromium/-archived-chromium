// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/web_activex_container.h"

#include "webkit/activex_shim/activex_plugin.h"
#include "webkit/activex_shim/activex_util.h"
#include "webkit/activex_shim/npn_scripting.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/activex_shim/web_activex_site.h"

namespace activex_shim {

// WebActiveXContainer
WebActiveXContainer::WebActiveXContainer()
    : plugin_(NULL),
      container_wnd_(NULL) {
}

WebActiveXContainer::~WebActiveXContainer() {
  // Don't do anything here. Do everything in FinalRelease.
}

void WebActiveXContainer::Init(ActiveXPlugin* plugin) {
  plugin_ = plugin;
}

void WebActiveXContainer::FinalRelease() {
  container_wnd_ = NULL;
  for (unsigned int i = 0; i < sites_.size(); i++) {
    delete sites_[i];
  }
  sites_.clear();
}

// IUnknown
HRESULT STDMETHODCALLTYPE WebActiveXContainer::QueryInterface(
    REFIID iid,
    void** object) {
  *object = NULL;
  if (iid == IID_IUnknown) {
    *object = static_cast<IUnknown*>(static_cast<IOleContainer*>(this));
  } else if (iid == IID_IDispatch) {
    *object = static_cast<IDispatch*>(static_cast<IHTMLDocument2Impl*>(this));
  } else if (iid == IID_IParseDisplayName) {
    *object = static_cast<IParseDisplayName*>(this);
  } else if (iid == IID_IOleContainer) {
    *object = static_cast<IOleContainer*>(this);
  } else if (iid == IID_IOleWindow) {
    *object = static_cast<IOleWindow*>(this);
  } else if (iid == IID_IOleInPlaceUIWindow) {
    *object = static_cast<IOleInPlaceUIWindow*>(this);
  } else if (iid == IID_IOleInPlaceFrame) {
    *object = static_cast<IOleInPlaceFrame*>(this);
  } else if (iid == IID_IHTMLDocument) {
    *object = static_cast<IHTMLDocument*>(this);
  } else if (iid == IID_IHTMLDocument2) {
    *object = static_cast<IHTMLDocument2*>(this);
  } else if (iid == IID_IWebBrowser) {
    *object = static_cast<IWebBrowser*>(this);
  } else if (iid == IID_IWebBrowserApp) {
    *object = static_cast<IWebBrowserApp*>(this);
  } else if (iid == IID_IWebBrowser2) {
    *object = static_cast<IWebBrowser2*>(this);
  } else if (iid == IID_IBindHost) {
    *object = static_cast<IBindHost*>(this);
  }
  TRACK_QUERY_INTERFACE(iid, *object != NULL);
  return (*object != NULL) ? S_OK : E_NOINTERFACE;
}

// IParseDisplayName
HRESULT STDMETHODCALLTYPE WebActiveXContainer::ParseDisplayName(
    IBindCtx* bc,
    LPOLESTR display_name,
    ULONG* cheaten,
    IMoniker** moniker) {
  TRACK_METHOD();
  // Do not support this.
  return E_NOTIMPL;
}

// IOleContainer
HRESULT STDMETHODCALLTYPE WebActiveXContainer::EnumObjects(DWORD flags,
    IEnumUnknown** ppenum) {
  TRACK_METHOD();
  // Do not support enumeration.
  return E_NOTIMPL;
};

HRESULT STDMETHODCALLTYPE WebActiveXContainer::LockContainer(BOOL lock) {
  TRACK_METHOD();
  // Do not allow locking container.
  return E_NOTIMPL;
};

// IOleWindow

HRESULT STDMETHODCALLTYPE WebActiveXContainer::GetWindow(HWND* wnd) {
  TRACK_METHOD();
  *wnd = container_wnd();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::ContextSensitiveHelp(
    BOOL enter_mode) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

// IOleInPlaceUIWindow
HRESULT STDMETHODCALLTYPE WebActiveXContainer::GetBorder(LPRECT border) {
  TRACK_METHOD();
  // Does not allow tool bar etc.
  return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::RequestBorderSpace(
    LPCBORDERWIDTHS border_widths) {
  TRACK_METHOD();
  return INPLACE_E_NOTOOLSPACE;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::SetBorderSpace(
    LPCBORDERWIDTHS border_widths) {
  TRACK_METHOD();
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::SetActiveObject(
    IOleInPlaceActiveObject* active_object,
    LPCOLESTR obj_name) {
  TRACK_METHOD();
  // Ignore whatever.
  return S_OK;
}

// IOleInPlaceFrame

HRESULT STDMETHODCALLTYPE WebActiveXContainer::InsertMenus(
    HMENU hmenu_shared,
    LPOLEMENUGROUPWIDTHS menu_widths) {
  TRACK_METHOD();
  // No menu is allowed.
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::SetMenu(
    HMENU hmenu_shared,
    HOLEMENU hole_menu,
    HWND active_object) {
  TRACK_METHOD();
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::RemoveMenus(
    HMENU hmenu_shared) {
  TRACK_METHOD();
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::SetStatusText(
    LPCOLESTR status_text) {
  TRACK_METHOD();
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::EnableModeless(BOOL enable) {
  TRACK_METHOD();
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::TranslateAccelerator(
    LPMSG msg,
    WORD id) {
  TRACK_METHOD();
  // TODO(ruijiang): Link it with browser message processing.
  // Returning S_FALSE means: The keystroke was not used.
  return S_FALSE;
}

// IHTMLDocument2
HRESULT STDMETHODCALLTYPE WebActiveXContainer::get_URL(BSTR* p) {
  TRACK_METHOD();
  std::wstring url = plugin_->GetCurrentURL();
  *p = SysAllocString(url.c_str());
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::get_cookie(BSTR* p) {
  TRACK_METHOD();
  *p = SysAllocString(plugin_->GetWindow().
      GetObjectProperty("document").GetStringProperty("cookie").c_str());
  return S_OK;
}

// IWebBrowser
HRESULT STDMETHODCALLTYPE WebActiveXContainer::Navigate(
    BSTR url,
    VARIANT* flags,
    VARIANT* target_frame_name,
    VARIANT* post_data,
    VARIANT* headers) {
  TRACK_METHOD();
  // TODO(ruijiang): See if we need to handle the optional params.
  plugin_->GetWindow().Invoke("open", "%s%s", url, L"_blank");
  return S_OK;
}

// IWebBrowserApp
HRESULT STDMETHODCALLTYPE WebActiveXContainer::get_LocationURL(
    BSTR* location_url) {
  return get_URL(location_url);
}

// IBindHost

// Flash uses CreateMoniker to get the url of the movie its "movie" parameter
// points to. We must implement this otherwise Flash will not work.
HRESULT STDMETHODCALLTYPE WebActiveXContainer::CreateMoniker(
    LPOLESTR szName,
    IBindCtx* bc,
    IMoniker** mk,
    DWORD reserved) {
  TRACK_METHOD();
  std::wstring url = plugin_->ResolveURL(szName);
  HRESULT hr = CreateURLMoniker(NULL, url.c_str(), mk);
  return hr;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::MonikerBindToStorage(
    IMoniker* mk,
    IBindCtx* bc,
    IBindStatusCallback* bsc,
    REFIID riid,
    void** obj) {
  // QuickTime uses this function to get the movie data from the container.
  TRACK_METHOD();
  IBindCtx* new_bc = NULL;
  // Logic is a little tricky here. When bc is passed in, we should use that;
  // otherwise we need to create a new one and release it after done.
  IBindCtx* bc_to_use = bc;
  if (bc == NULL) {
    CreateBindCtx(0, &new_bc);
    if (!new_bc)
      return E_FAIL;
    bc_to_use = new_bc;
  }
  // We must register the callback otherwise control will not know when new
  // data arrive.
  if (bsc)
    RegisterBindStatusCallback(bc_to_use, bsc, NULL, 0);
  HRESULT hr = mk->BindToStorage(bc_to_use, NULL, riid, obj);
  if (new_bc)
    new_bc->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE WebActiveXContainer::MonikerBindToObject(
    IMoniker* mk,
    IBindCtx* bc,
    IBindStatusCallback* bsc,
    REFIID riid,
    void** obj) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

HRESULT WebActiveXContainer::CreateControlWithSite(const wchar_t* clsid) {
  HRESULT hr;
  CLSID id;
  hr = CLSIDFromString(const_cast<LPOLESTR>(clsid), &id);
  if (FAILED(hr))
    return hr;

  IUnknown* control;
  hr = CoCreateInstance(id, NULL, CLSCTX_INPROC_SERVER, __uuidof(IUnknown),
                        (LPVOID*)&control);
  if (FAILED(hr))
    return hr;

  // First try to get the IObjectSafety interface and get/set options.
  // If failed then see if the object has registered itself as safe.
  unsigned long safety_flag = GetAndSetObjectSafetyOptions(control);
  if (!(safety_flag & SAFE_FOR_INITIALIZING) ||
      !(safety_flag & SAFE_FOR_SCRIPTING)) {
    safety_flag = GetRegisteredObjectSafetyOptions(id);
    if (!(safety_flag & SAFE_FOR_INITIALIZING) ||
        !(safety_flag & SAFE_FOR_SCRIPTING))
      return E_FAIL;
  }

  // Create client site and pass it to the control.
  WebActiveXSite* site = new NoRefIUnknownImpl<WebActiveXSite>;
  // From now on, we should assume site will take care of the lifecycle
  // of control.
  site->Init(this, control);
  control = NULL;

  sites_.push_back(site);
  return S_OK;
}

bool WebActiveXContainer::OnWindowMessage(UINT msg, WPARAM wparam,
                                          LPARAM lparam, LRESULT* result) {
  HRESULT hr;
  for (unsigned int i = 0; i < sites_.size(); i++) {
    WebActiveXSite* site = sites_[i];
    CComQIPtr<IOleInPlaceObjectWindowless> windowless = site->control_;
    if (windowless == NULL)
      continue;

    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
      POINT pt;
      pt.x = LOWORD(lparam);
      pt.y = HIWORD(lparam);
      if (PtInRect(&site->rect_, pt)) {
        hr = windowless->OnWindowMessage(msg, wparam, lparam, result);
        if (hr == S_OK)
          return true;
      }
    }
  }

  // We did not consume the message. The caller can now process it.
  return false;
}

IUnknown* WebActiveXContainer::GetFirstControl() {
  if (sites_.size()) {
    return sites_[0]->control_;
  } else {
    return NULL;
  }
}

WebActiveXSite* WebActiveXContainer::GetFirstSite() {
  if (sites_.size()) {
    return sites_[0];
  } else {
    return NULL;
  }
}

}  // namespace activex_shim
