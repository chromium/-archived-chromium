// Copyright 2009, Google Inc.  All rights reserved.
// Portions of this file were adapted from the Mozilla project.
// See https://developer.mozilla.org/en/ActiveX_Control_for_Hosting_Netscape_Plug-ins_in_IE
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// File declaring StreamOperation class encapsulating basic support
// for the NPAPI GetURL streaming interface.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_STREAM_OPERATION_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_STREAM_OPERATION_H_

#include <atlwin.h>
#include <atlstr.h>
#include <urlmon.h>

#include "third_party/npapi/files/include/npupp.h"

class NPPluginProxy;

#define WM_NPP_NEWSTREAM      WM_USER
#define WM_NPP_ASFILE         WM_USER + 1
#define WM_NPP_DESTROYSTREAM  WM_USER + 2
#define WM_NPP_URLNOTIFY      WM_USER + 3
#define WM_NPP_WRITEREADY     WM_USER + 4
#define WM_NPP_WRITE          WM_USER + 5

#define WM_TEAR_DOWN          WM_USER + 10

// StreamOperation class used to provide a subset of the NPAPI GetURL* API.
// Class makes use of urlmon's IBindStatusCallback to receive notifications
// from urlmon as data is transferred.  Refer to the MSDN documentation
// for information on the usage model of IBindStatusCallback.
class ATL_NO_VTABLE StreamOperation :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CWindowImpl<StreamOperation, CWindow, CNullTraits>,
    public CComCoClass<StreamOperation, &CLSID_NULL>,
    public IBindStatusCallback {
 public:
  typedef CWindowImpl<StreamOperation, CWindow, CNullTraits> CWindowImplBase;

  StreamOperation();
  ~StreamOperation();

  // Assign/Retrieve the url from which to stream the data.
  void SetURL(const wchar_t* url) {
    url_ = url;
  }

  const ATL::CStringW& GetURL() const {
    return url_;
  }

  void SetFullURL(const wchar_t* url) {
    full_url_ = url;
  }

  const ATL::CStringW& GetFullURL() const {
    return full_url_;
  }

  // Returns the MIME-type of the data stream.
  const ATL::CStringW& GetContentType() const {
    return content_type_;
  }

  NPStream* GetNPStream() {
    return &np_stream_;
  }

  HANDLE GetThreadHandle() const {
    return thread_handle_;
  }

  // Assign the owning plugin pointer that spawned this operation.
  void SetOwner(NPPluginProxy* plugin) {
    owner_ = plugin;
  }

  // Assign/Retrieve the opaque NPAPI-provided callback data for the
  // stream-operation.
  void SetNotifyData(void *notify_data) {
    notify_data_ = notify_data;
  }

  void* GetNotifyData() {
    return notify_data_;
  }

  // Call to request that the streaming operation terminate early. As soon as
  // the streaming thread sees the request has been cancelled, it aborts its
  // binding.
  HRESULT RequestCancellation();

BEGIN_COM_MAP(StreamOperation)
  COM_INTERFACE_ENTRY(IBindStatusCallback)
END_COM_MAP()

  // To allow interaction with non-thread-safe NPAPI plug-in modules, the
  // streaming code uses Windows message pumps to serialize the interactions
  // calling back into the plug-in on the thread in which the plug-in resides.
  // When information about the state of the streaming request is provided
  // through a IBindStatusCallback routine, the thread will post a message
  // to the window created by the StreamOperation instance.  Because this
  // window will reside in the same thread as the calling plug-in, we are
  // guaranteed serialization and mutual exclusion of the handling of the
  // routines below.
BEGIN_MSG_MAP(StreamOperation)
  MESSAGE_HANDLER(WM_NPP_NEWSTREAM, OnNPPNewStream)
  MESSAGE_HANDLER(WM_NPP_ASFILE, OnNPPAsFile)
  MESSAGE_HANDLER(WM_NPP_DESTROYSTREAM, OnNPPDestroyStream)
  MESSAGE_HANDLER(WM_NPP_URLNOTIFY, OnNPPUrlNotify)
  MESSAGE_HANDLER(WM_NPP_WRITEREADY, OnNPPWriteReady)
  MESSAGE_HANDLER(WM_NPP_WRITE, OnNPPWrite)
  MESSAGE_HANDLER(WM_TEAR_DOWN, OnTearDown);
END_MSG_MAP()

  // Helper function called in response to WM_TEAR_DOWN to destroy class
  // resources on the appropriate thread.
  LRESULT OnTearDown(UINT uMsg,
                     WPARAM wParam,
                     LPARAM lParam,
                     BOOL& bHandled);

  // The following OnNPP... routines forward the respective notification to
  // the plugin that spawned the data transmission.
  LRESULT OnNPPNewStream(UINT uMsg,
                         WPARAM wParam,
                         LPARAM lParam,
                         BOOL& bHandled);

  LRESULT OnNPPDestroyStream(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled);

  LRESULT OnNPPAsFile(UINT uMsg,
                      WPARAM wParam,
                      LPARAM lParam,
                      BOOL& bHandled);

  LRESULT OnNPPUrlNotify(UINT uMsg,
                         WPARAM wParam,
                         LPARAM lParam,
                         BOOL& bHandled);

  LRESULT OnNPPWriteReady(UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL& bHandled);

  LRESULT OnNPPWrite(UINT uMsg,
                     WPARAM wParam,
                     LPARAM lParam,
                     BOOL& bHandled);

  // Methods implementing the IBindStatusCallback interface.  Refer to
  // the MSDN documentation for the expected behaviour of these routines.
  virtual HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dwReserved,
                                                   IBinding *pib);

  virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG *pnPriority);

  virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved);

  virtual HRESULT STDMETHODCALLTYPE OnProgress(ULONG ulProgress,
                                               ULONG ulProgressMax,
                                               ULONG ulStatusCode,
                                               LPCWSTR szStatusText);

  virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult,
                                                  LPCWSTR szError);

  virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *grfBINDF,
                                                BINDINFO *pbindinfo);

  virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grfBSCF,
                                                    DWORD dwSize,
                                                    FORMATETC *pformatetc,
                                                    STGMEDIUM *pstgmed);

  virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid,
                                                      IUnknown *punk);

  static HRESULT OpenURL(NPPluginProxy *owning_plugin, const wchar_t* url,
                         void *notify_data);

  virtual void OnFinalMessage(HWND hWnd);

  DECLARE_PROTECT_FINAL_CONSTRUCT();
 private:
  // Callback object for interacting with the urlmon streaming manager.
  ATL::CComPtr<IBinding> binding_;

  // The url from which the data is fetched, and the associated MIME-type.
  ATL::CStringW url_;
  ATL::CStringW full_url_;
  ATL::CStringW content_type_;

  // Back-pointer to the plug-in instance requesting the data transfer.
  NPPluginProxy *owner_;

  // Opaque data specified at request initiation that is passed back to the
  // plug-in during call-back invocation.
  void *notify_data_;

  NPStream np_stream_;

  int stream_size_;
  int stream_received_;

  // Cache of the type of stream requested by the plug-in.  May be one of:
  // NP_NORMAL, NP_ASFILE, NP_ASFILEONLY.
  int stream_type_;

  // Pointer to file handle used to save incoming data if the stream type is
  // NP_ASFILE or NP_ASFILEONLY.
  FILE* temp_file_;

  // Temporary file name.
  CStringW temp_file_name_;

  // Handle to the worker-thread where the streaming notifications are received.
  HANDLE thread_handle_;

  // Value used to indicate the streaming operation should stop processing
  // input data.
  bool cancel_requested_;

  static unsigned int __stdcall WorkerProc(void *worker_arguments);

  DISALLOW_COPY_AND_ASSIGN(StreamOperation);
};

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_STREAM_OPERATION_H_
