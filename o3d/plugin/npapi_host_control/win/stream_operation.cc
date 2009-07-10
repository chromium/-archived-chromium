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


#include "plugin/npapi_host_control/win/stream_operation.h"
#include "plugin/npapi_host_control/win/host_control.h"
#include "plugin/npapi_host_control/win/np_plugin_proxy.h"

namespace {

// The following classes are used to package arguments for interacting with
// the hosted NPAPI plug-in.
struct NPPDestroyStreamArgs {
  NPP npp_;
  NPStream *stream_;
  NPReason reason_;
  NPError *return_code_;
};

struct NPPNewStreamArgs {
  NPP npp_;
  NPMIMEType type_;
  NPStream *stream_;
  NPBool seekable_;
  uint16 *stype_;
  NPError *return_code_;
};

struct NPPAsFileArgs {
  NPP npp_;
  NPStream *stream_;
  const char* fname_;
};

struct NPPUrlNotifyArgs {
  NPP npp_;
  const char* url_;
  NPReason reason_;
  void *notify_data_;
};

struct NPPWriteReadyArgs {
  NPP npp_;
  NPStream *stream_;
  int32 *return_value_;
};

struct NPPWriteArgs {
  NPP npp_;
  NPStream *stream_;
  int32 offset_;
  int32 len_;
  void* buffer_;
  int32 *return_value_;
};

// Helper function that constructs the full url from a moniker associated with
// a base 'left-side' url prefix, and a stream operation.
HRESULT ConstructFullURLPath(const StreamOperation& stream_operation,
                             IMoniker* base_moniker,
                             CString* out_string) {
  ATLASSERT(base_moniker && out_string);

  HRESULT hr = S_OK;
  CComPtr<IMoniker> full_url_moniker;
  if (FAILED(hr = CreateURLMonikerEx(base_moniker,
                                     stream_operation.GetURL(),
                                     &full_url_moniker,
                                     URL_MK_UNIFORM))) {
    return hr;
  }

  // Determine if the monikers share a common prefix.  If they do, then
  // we can allow the data fetch to proceed - The same origin criteria has been
  // satisfied.
  CComPtr<IMoniker> prefix_moniker;
  bool urls_contain_prefix = false;
  hr = MonikerCommonPrefixWith(base_moniker, full_url_moniker, &prefix_moniker);
  if (SUCCEEDED(hr)) {
    urls_contain_prefix = true;
  }

  CComPtr<IBindCtx> bind_context;
  if (FAILED(hr = CreateBindCtx(0, &bind_context))) {
    return hr;
  }

  CComPtr<IMalloc> malloc_interface;
  if (FAILED(hr = CoGetMalloc(1, &malloc_interface))) {
    return hr;
  }

  LPOLESTR full_url_path = NULL;
  if (FAILED(hr = full_url_moniker->GetDisplayName(bind_context,
                                                   NULL,
                                                   &full_url_path))) {
    return hr;
  }

  if (!urls_contain_prefix) {
    // If the urls do not contain a common prefix, validate the access request
    // based on the fully qualified uri's.
    LPOLESTR base_path_name = NULL;
    if (FAILED(hr = base_moniker->GetDisplayName(bind_context,
                                                 NULL,
                                                 &base_path_name))) {
      malloc_interface->Free(full_url_path);
      return hr;
    }
  }

  *out_string = full_url_path;
  malloc_interface->Free(full_url_path);

  return S_OK;
}

// Helper routine implementing a custom version of SendMessage(...).
// The StreamingOperation class uses windows messages to communicate transfer
// notifications to the plug-in.  This is required so that the plug-in will
// receive notifications synchronously on the main-browser thread.
// SendMessage is not appropriate, because messages 'sent' to a window are NOT
// processed during DispatchMessage, but instead during GetMessage, PeekMessage
// and others.  Because the JScript engine periodically peeks the message
// queue during JavaScript evaluation, the plug-in would be notified, and
// potentially call back into the JavaScript environment causing unexpected
// reentrancy.
HRESULT CustomSendMessage(HWND window_handle, UINT message, LPARAM l_param) {
  // Mimic the behaviour of SendMessage by posting to the window, and then
  // blocking on an event.  Note that the message handlers must set
  // the event.
  HANDLE local_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!local_event) {
    return E_FAIL;
  }

  if (!PostMessage(window_handle, message,
                   reinterpret_cast<WPARAM>(&local_event), l_param)) {
    CloseHandle(local_event);
    return E_FAIL;
  }

  HRESULT hr;
  static const unsigned int kWaitTimeOut = 120000;
  bool done = false;
  while (!done) {
    DWORD wait_code = MsgWaitForMultipleObjects(1,
                                                &local_event,
                                                FALSE,
                                                kWaitTimeOut,
                                                QS_ALLINPUT);
    switch (wait_code) {
      case WAIT_OBJECT_0:
        hr = S_OK;
        done = true;
        break;
      case WAIT_OBJECT_0 + 1:
        MSG msg;
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        break;
      case WAIT_TIMEOUT:
        // If the plug-in is busy processing JavaScript code, it's possible
        // that we may time-out here.  We don't break out of the loop, because
        // the event will eventually be signaled when the JS is done
        // processing.
        ATLASSERT(false && "Time out waiting for response from main thread.");
        break;
      default:
        ATLASSERT(false &&
                  "Critical failure waiting for response from main thread.");
        hr = E_FAIL;
        done = true;
        break;
    }
  }

  CloseHandle(local_event);
  return hr;
}

}  // unnamed namespace

StreamOperation::StreamOperation()
    : stream_size_(0),
      stream_received_(0),
      stream_type_(NP_NORMAL),
      temp_file_(NULL),
      thread_handle_(NULL),
      cancel_requested_(false) {
  memset(&np_stream_, 0, sizeof(np_stream_));
}

StreamOperation::~StreamOperation() {
}

void StreamOperation::OnFinalMessage(HWND hWnd) {
  CWindowImplBase::OnFinalMessage(hWnd);
  if (owner_) {
    owner_->UnregisterStreamOperation(this);
  }

  // The binding holds a reference to the stream operation, which forms
  // a cyclic reference chain.  Release the binding so that both objects
  // can be destroyed.
  binding_ = NULL;

  // The object has an artificially boosted reference count to ensure that it
  // stays alive as long as it may receive window messages.  Release this
  // reference here, and potentially free the instance.
  Release();
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnStartBinding(DWORD dwReserved,
                                                          IBinding *pib) {
  binding_ = pib;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::GetPriority(LONG *pnPriority) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnLowResource(DWORD reserved) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnProgress(ULONG ulProgress,
                                                      ULONG ulProgressMax,
                                                      ULONG ulStatusCode,
                                                      LPCWSTR szStatusText) {
  if (cancel_requested_) {
    binding_->Abort();
    return S_OK;
  }

  // Capture URL re-directs and MIME-type status notifications.
  switch (ulStatusCode) {
    case BINDSTATUS_BEGINDOWNLOADDATA:
    case BINDSTATUS_REDIRECTING:
      url_ = szStatusText;
      break;
    case BINDSTATUS_MIMETYPEAVAILABLE:
      content_type_ = szStatusText;
      break;
    default:
      break;
  }

  // Track the current progress of the streaming transfer.
  stream_size_ = ulProgressMax;
  stream_received_ = ulProgress;

  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnStopBinding(HRESULT hresult,
                                                         LPCWSTR szError) {
  NPReason reason = SUCCEEDED(hresult) ? NPRES_DONE : NPRES_NETWORK_ERR;
  USES_CONVERSION;

  // Notify the calling plug-in that the transfer has completed.
  if (stream_type_ == NP_ASFILE || stream_type_ == NP_ASFILEONLY) {
    if (temp_file_) {
      fclose(temp_file_);
    }

    if (reason == NPRES_DONE) {
      NPPAsFileArgs arguments = {
        owner_->GetNPP(),
        GetNPStream(),
        W2A(temp_file_name_)
      };
      CustomSendMessage(m_hWnd, WM_NPP_ASFILE,
                        reinterpret_cast<LPARAM>(&arguments));
    }
  }

  if (reason == NPRES_DONE) {
    NPError error_return;
    NPPDestroyStreamArgs destroy_stream_args = {
      owner_->GetNPP(),
      GetNPStream(),
      reason,
      &error_return
    };
    CustomSendMessage(m_hWnd, WM_NPP_DESTROYSTREAM,
                      reinterpret_cast<LPARAM>(&destroy_stream_args));
    ATLASSERT(NPERR_NO_ERROR == error_return);
  }

  NPPUrlNotifyArgs url_args = {
    owner_->GetNPP(),
    W2A(url_),
    reason,
    GetNotifyData()
  };
  CustomSendMessage(m_hWnd, WM_NPP_URLNOTIFY,
                    reinterpret_cast<LPARAM>(&url_args));

  // Clear the intermediate file from the cache.
  _wremove(temp_file_name_);
  temp_file_name_ = L"";

  // The operation has completed, so tear-down the intermediate window, and
  // exit the worker thread.
  CustomSendMessage(m_hWnd, WM_TEAR_DOWN, 0);
  PostQuitMessage(0);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::GetBindInfo(DWORD *grfBINDF,
                                                       BINDINFO *pbindinfo) {
  // Request an asynchronous transfer of the data.
  *grfBINDF = BINDF_ASYNCHRONOUS |  BINDF_ASYNCSTORAGE |
              BINDF_GETNEWESTVERSION;

  int cbSize = pbindinfo->cbSize;
  memset(pbindinfo, 0, cbSize);
  pbindinfo->cbSize = cbSize;
  pbindinfo->dwBindVerb = BINDVERB_GET;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnDataAvailable(
    DWORD grfBSCF,
    DWORD dwSize,
    FORMATETC *pformatetc,
    STGMEDIUM *pstgmed) {
  if (pstgmed->tymed != TYMED_ISTREAM || !pstgmed->pstm) {
    return S_OK;
  }

  // Don't bother processing any data if the stream has been canceled.
  if (cancel_requested_) {
    binding_->Abort();
    return S_OK;
  }

  // Notify the plugin that a new stream has been opened.
  if (grfBSCF & BSCF_FIRSTDATANOTIFICATION) {
    USES_CONVERSION;
    np_stream_.url = W2CA(url_);
    np_stream_.end = stream_size_;
    np_stream_.lastmodified = 0;
    np_stream_.notifyData = GetNotifyData();

    uint16 stream_type = NP_NORMAL;

    NPError np_error;
    NPPNewStreamArgs new_stream_args = {
      owner_->GetNPP(),
      const_cast<char*>(W2CA(GetContentType())),
      GetNPStream(),
      FALSE,
      &stream_type,
      &np_error
    };
    CustomSendMessage(m_hWnd, WM_NPP_NEWSTREAM,
                      reinterpret_cast<LPARAM>(&new_stream_args));
    if (np_error != NPERR_NO_ERROR) {
      return E_FAIL;
    }

    // Cache the stream type requested by the plug-in.
    stream_type_ = stream_type;
  }

  if (grfBSCF & BSCF_INTERMEDIATEDATANOTIFICATION ||
      grfBSCF & BSCF_LASTDATANOTIFICATION) {
    // Read all of the available data, and pass it to the plug-in, if requested.
    HRESULT hr;
    char local_data[16384];
    int bytes_received_total = 0;
    // If a large number of bytes have been received, then this loop can
    // take a long time to complete - which will block the user from leaving
    // the page as the plug-in waits for all transfers to complete.  We
    // add a check on cancel_requested_ to allow for early bail-out.
    while (bytes_received_total < dwSize &&
           !cancel_requested_) {
      int bytes_to_read = dwSize - bytes_received_total;
      unsigned long bytes_read = 0;

      if (bytes_to_read > sizeof(local_data)) {
        bytes_to_read = sizeof(local_data);
      }

      if (stream_type_ == NP_NORMAL || stream_type_ == NP_ASFILE) {
        int32 bytes_to_accept;
        NPPWriteReadyArgs write_ready_args = {
          owner_->GetNPP(),
          GetNPStream(),
          &bytes_to_accept
        };
        CustomSendMessage(m_hWnd, WM_NPP_WRITEREADY,
                          reinterpret_cast<LPARAM>(&write_ready_args));

        if (bytes_to_read > bytes_to_accept) {
          bytes_to_read = bytes_to_accept;
        }
      }

      // If the plug-in has indicated that it is not prepared to read any data,
      // then bail early.
      if (bytes_to_read == 0) {
        break;
      }

      hr = pstgmed->pstm->Read(local_data, bytes_to_read, &bytes_read);
      if (FAILED(hr) || S_FALSE == hr) {
        break;
      }

      // Pass the data to the plug-in.
      if (stream_type_ == NP_NORMAL || stream_type_ == NP_ASFILE) {
        int consumed_bytes;
        NPPWriteArgs write_args = {
          owner_->GetNPP(),
          GetNPStream(),
          bytes_received_total,
          bytes_read,
          local_data,
          &consumed_bytes
        };
        CustomSendMessage(m_hWnd, WM_NPP_WRITE,
                          reinterpret_cast<LPARAM>(&write_args));
        ATLASSERT(consumed_bytes == bytes_read);
      }

      if (stream_type_ == NP_ASFILE || stream_type_ == NP_ASFILEONLY) {
        // If the plug-in requested access to the data through a file, then
        // create a temporary file and write the data to it.
        if (!temp_file_) {
          temp_file_name_= _wtempnam(NULL, L"npapi_host_temp");
          _wfopen_s(&temp_file_, temp_file_name_, L"wb");
        }
        fwrite(local_data, bytes_read, 1, temp_file_);
      }
      bytes_received_total += bytes_read;
    }
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamOperation::OnObjectAvailable(REFIID riid,
                                                             IUnknown *punk) {
  return S_OK;
}

HRESULT StreamOperation::OpenURL(NPPluginProxy *owning_plugin,
                                 const wchar_t *url,
                                 void *notify_data) {
  // The StreamOperation instance is created with a ref-count of zero,
  // so we explicitly attach a CComPtr to the object to boost the count, and
  // manage the lifetime of the object.
  CComObject<StreamOperation> *stream_ptr;
  CComObject<StreamOperation>::CreateInstance(&stream_ptr);
  CComPtr<CComObject<StreamOperation> > stream_object = stream_ptr;
  if (!stream_object) {
    return E_OUTOFMEMORY;
  }

  CComPtr<CHostControl> host_control =
      owning_plugin->browser_proxy()->GetHostingControl();
  CComPtr<IMoniker> base_url_moniker = host_control->GetURLMoniker();

  stream_object->SetURL(url);
  stream_object->SetNotifyData(notify_data);
  stream_object->SetOwner(owning_plugin);

  CString full_path;
  HRESULT hr;
  if (FAILED(hr = ConstructFullURLPath(*stream_object,
                                       base_url_moniker,
                                       &full_path))) {
    return hr;
  }

  stream_object->SetFullURL(full_path);

  // Create an object window on this thread that will be sent messages when
  // something happens on the worker thread.
  HWND temporary_window = stream_object->Create(HWND_DESKTOP);
  ATLASSERT(temporary_window);
  if (!temporary_window) {
    return E_FAIL;
  }

  // Artificially increment the reference count of the stream_object instance
  // to ensure that the object will not be deleted until WM_NC_DESTROY is
  // processed and OnFinalMessage is invoked.
  // Note:  The operator-> is not used, because it returns a type overloading
  // the public access of AddRef/Release.
  (*stream_object).AddRef();

  stream_object->thread_handle_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     0,
                     WorkerProc,
                     static_cast<void*>(stream_object),
                     CREATE_SUSPENDED,
                     NULL));
  ATLASSERT(stream_object->thread_handle_);
  if (!stream_object->thread_handle_) {
    stream_object->DestroyWindow();
    return E_FAIL;
  }

  owning_plugin->RegisterStreamOperation(stream_object);
  if (!ResumeThread(stream_object->thread_handle_)) {
    owning_plugin->UnregisterStreamOperation(stream_object);
    stream_object->DestroyWindow();
    // If the thread never resumed, then we can safely terminate it here - it
    // has not had a chance to allocate any resources that would be leaked.
    TerminateThread(stream_object->thread_handle_, 0);
    return E_FAIL;
  }

  return S_OK;
}

unsigned int __stdcall StreamOperation::WorkerProc(void* worker_arguments) {
  CComObject<StreamOperation> *stream_object =
      static_cast<CComObject<StreamOperation> *>(worker_arguments);
  ATLASSERT(stream_object);

  // Initialize the COM run-time for this new thread.
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hr) && "Failure to initialize worker COM apartment.");
  if (FAILED(hr)) {
    CustomSendMessage(stream_object->m_hWnd, WM_TEAR_DOWN, 0);
    CoUninitialize();
    return 0;
  }

  {
    // Get the ActiveX control so the request is within the context of the
    // plugin. Among other things, this lets the browser reject file:// uris
    // when the page is loaded over http://.
    CComPtr<IUnknown> caller;
    stream_object->owner_->browser_proxy()->GetHostingControl()->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&caller));

    // Note that the OnStopBinding(...) routine, which is always called, will
    // post WM_QUIT to this thread.
    hr = URLOpenStream(caller, stream_object->GetFullURL(), 0,
                       static_cast<IBindStatusCallback*>(stream_object));

    // Pump messages until WM_QUIT arrives
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  CoUninitialize();
  return 0;
}

LRESULT StreamOperation::OnNPPNewStream(UINT uMsg,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        BOOL& bHandled) {
  NPPNewStreamArgs *args = reinterpret_cast<NPPNewStreamArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    *args->return_code_ = owner_->GetPluginFunctions()->newstream(
        args->npp_,
        args->type_,
        args->stream_,
        args->seekable_,
        args->stype_);
  } else {
    *args->return_code_ = NPERR_GENERIC_ERROR;
  }

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnNPPDestroyStream(UINT uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            BOOL& bHandled) {
  NPPDestroyStreamArgs *args = reinterpret_cast<NPPDestroyStreamArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    *args->return_code_ = owner_->GetPluginFunctions()->destroystream(
        args->npp_,
        args->stream_,
        args->reason_);
  } else {
    *args->return_code_ = NPERR_NO_ERROR;
  }

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnNPPAsFile(UINT uMsg,
                                     WPARAM wParam,
                                     LPARAM lParam,
                                     BOOL& bHandled) {
  NPPAsFileArgs *args = reinterpret_cast<NPPAsFileArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    owner_->GetPluginFunctions()->asfile(args->npp_, args->stream_,
                                         args->fname_);
  }

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnNPPUrlNotify(UINT uMsg,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        BOOL& bHandled) {
  NPPUrlNotifyArgs *args = reinterpret_cast<NPPUrlNotifyArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    owner_->GetPluginFunctions()->urlnotify(args->npp_, args->url_,
                                            args->reason_, args->notify_data_);
  }
  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnNPPWriteReady(UINT uMsg,
                                         WPARAM wParam,
                                         LPARAM lParam,
                                         BOOL& bHandled) {
  NPPWriteReadyArgs *args = reinterpret_cast<NPPWriteReadyArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    *args->return_value_ = owner_->GetPluginFunctions()->writeready(
        args->npp_,
        args->stream_);
  } else {
    // Indicate to the download thread that 0 bytes are ready to be received.
    *args->return_value_ = 0;
  }

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnNPPWrite(UINT uMsg,
                                    WPARAM wParam,
                                    LPARAM lParam,
                                    BOOL& bHandled) {
  NPPWriteArgs *args = reinterpret_cast<NPPWriteArgs*>(lParam);
  ATLASSERT(args);

  // If the stream was canceled, don't pass the notification to the plug-in.
  if (!cancel_requested_) {
    *args->return_value_ = owner_->GetPluginFunctions()->write(
        args->npp_,
        args->stream_,
        args->offset_,
        args->len_,
        args->buffer_);
  } else {
    *args->return_value_ = args->len_;
  }

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

LRESULT StreamOperation::OnTearDown(UINT uMsg,
                                    WPARAM wParam,
                                    LPARAM lParam,
                                    BOOL& bHandled) {
  // DestroyWindow must be called on the same thread as where the window was
  // constructed, so make the call here.
  DestroyWindow();

  if (wParam) {
    HANDLE* event_handle = reinterpret_cast<HANDLE*>(wParam);
    SetEvent(*event_handle);
  }
  return 0;
}

HRESULT StreamOperation::RequestCancellation() {
  ATLASSERT(binding_ &&
            "Cancellation request on a stream that has not been bound.");
  cancel_requested_ = true;
  return S_OK;
}
