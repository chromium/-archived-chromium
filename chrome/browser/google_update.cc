// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_update.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/win_util.h"
#include "chrome/app/client_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "google_update_idl_i.c"

namespace {
// Check if the currently running instance can be updated by Google Update.
// Returns true only if the instance running is a Google Chrome
// distribution installed in a standard location.
bool CanUpdateCurrentChrome(const std::wstring& chrome_exe_path) {
#if !defined(GOOGLE_CHROME_BUILD)
  return false;
#else
  std::wstring user_exe_path = installer::GetChromeInstallPath(false);
  std::wstring machine_exe_path = installer::GetChromeInstallPath(true);
  std::transform(user_exe_path.begin(), user_exe_path.end(),
                 user_exe_path.begin(), tolower);
  std::transform(machine_exe_path.begin(), machine_exe_path.end(),
                 machine_exe_path.begin(), tolower);
  if (chrome_exe_path != user_exe_path &&
      chrome_exe_path != machine_exe_path ) {
    LOG(ERROR) << L"Google Update cannot update Chrome installed in a "
               << L"non-standard location: " << chrome_exe_path.c_str()
               << L". The standard location is: " << user_exe_path.c_str()
               << L" or " << machine_exe_path.c_str() << L".";
    return false;
  }

  return true;
#endif
}

// Creates an instance of a COM Local Server class using either plain vanilla
// CoCreateInstance, or using the Elevation moniker if running on Vista.
HRESULT CoCreateInstanceAsAdmin(REFCLSID class_id, REFIID interface_id,
                                void** interface_ptr) {
  if (!interface_ptr)
    return E_POINTER;

  // For Vista we need to instantiate the COM server via the elevation
  // moniker. This ensures that the UAC dialog shows up.
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    wchar_t class_id_as_string[MAX_PATH] = {0};
    StringFromGUID2(class_id, class_id_as_string,
                    arraysize(class_id_as_string));

    std::wstring elevation_moniker_name =
        StringPrintf(L"Elevation:Administrator!new:%s", class_id_as_string);

    BIND_OPTS3 bind_opts;
    memset(&bind_opts, 0, sizeof(bind_opts));

    bind_opts.cbStruct = sizeof(bind_opts);
    bind_opts.dwClassContext = CLSCTX_LOCAL_SERVER;
    return CoGetObject(elevation_moniker_name.c_str(), &bind_opts,
                       interface_id, reinterpret_cast<void**>(interface_ptr));
  }

  return CoCreateInstance(class_id, NULL, CLSCTX_LOCAL_SERVER,
                          interface_id,
                          reinterpret_cast<void**>(interface_ptr));
}


}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// The GoogleUpdateJobObserver COM class is responsible for receiving status
// reports from google Update. It keeps track of the progress as Google Update
// notifies us and ends the message loop we are spinning in once Google Update
// reports that it is done.
//
////////////////////////////////////////////////////////////////////////////////
class GoogleUpdateJobObserver
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IJobObserver {
 public:
  BEGIN_COM_MAP(GoogleUpdateJobObserver)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  GoogleUpdateJobObserver()
    : result_(UPGRADE_ERROR) {
  }
  virtual ~GoogleUpdateJobObserver() {}

  // Notifications from Google Update:
  STDMETHOD(OnShow)() {
    return S_OK;
  }
  STDMETHOD(OnCheckingForUpdate)() {
    result_ = UPGRADE_CHECK_STARTED;
    return S_OK;
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    result_ = UPGRADE_IS_AVAILABLE;
    new_version_ = version_string;
    return S_OK;
  }
  STDMETHOD(OnWaitingToDownload)() {
    return S_OK;
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    return S_OK;
  }
  STDMETHOD(OnWaitingToInstall)() {
    return S_OK;
  }
  STDMETHOD(OnInstalling)() {
    result_ = UPGRADE_STARTED;
    return S_OK;
  }
  STDMETHOD(OnPause)() {
    return S_OK;
  }
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    switch (code) {
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
      case COMPLETION_CODE_SUCCESS: {
        if (result_ == UPGRADE_STARTED)
          result_ = UPGRADE_SUCCESSFUL;
        else if (result_ == UPGRADE_CHECK_STARTED)
          result_ = UPGRADE_ALREADY_UP_TO_DATE;
        break;
      }
      default: {
        NOTREACHED();
        result_ = UPGRADE_ERROR;
        break;
      }
    }

    event_sink_ = NULL;

    // We no longer need to spin the message loop that we started spinning in
    // InitiateGoogleUpdateCheck.
    MessageLoop::current()->Quit();
    return S_OK;
  }
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    event_sink_ = event_sink;
    return S_OK;
  }

  // Returns the results of the update operation.
  STDMETHOD(GetResult)(GoogleUpdateUpgradeResult* result) {
    // Intermediary steps should never be reported to the client.
    DCHECK(result_ != UPGRADE_STARTED && result_ != UPGRADE_CHECK_STARTED);

    *result = result_;
    return S_OK;
  }

  // Returns which version Google Update found on the server (if a more
  // recent version was found). Otherwise, this will be blank.
  STDMETHOD(GetVersionInfo)(std::wstring* version_string) {
    *version_string = new_version_;
    return S_OK;
  }

 private:
  // The status/result of the Google Update operation.
  GoogleUpdateUpgradeResult result_;

  // The version string Google Update found.
  std::wstring new_version_;

  // Allows us control the upgrade process to a small degree. After OnComplete
  // has been called, this object can not be used.
  CComPtr<IProgressWndEvents> event_sink_;
};

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, public:

GoogleUpdate::GoogleUpdate()
    : listener_(NULL) {
}

GoogleUpdate::~GoogleUpdate() {
}

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, views::DialogDelegate implementation:

void GoogleUpdate::CheckForUpdate(bool install_if_newer) {
  // We need to shunt this request over to InitiateGoogleUpdateCheck and have
  // it run in the file thread.
  MessageLoop* file_loop = g_browser_process->file_thread()->message_loop();
  file_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
      &GoogleUpdate::InitiateGoogleUpdateCheck,
      install_if_newer, MessageLoop::current()));
}

// Adds/removes a listener. Only one listener is maintained at the moment.
void GoogleUpdate::AddStatusChangeListener(
    GoogleUpdateStatusListener* listener) {
  DCHECK(!listener_);
  listener_ = listener;
}

void GoogleUpdate::RemoveStatusChangeListener() {
  listener_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, private:

bool GoogleUpdate::InitiateGoogleUpdateCheck(bool install_if_newer,
                                             MessageLoop* main_loop) {

  std::wstring chrome_exe_path;
  if (!PathService::Get(base::DIR_EXE, &chrome_exe_path)) {
    NOTREACHED();
    return false;
  }

  std::transform(chrome_exe_path.begin(), chrome_exe_path.end(),
                 chrome_exe_path.begin(), tolower);

  if (!CanUpdateCurrentChrome(chrome_exe_path)) {
    main_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
        &GoogleUpdate::ReportResults, UPGRADE_ERROR,
        CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY));
    return false;
  }

  CComObject<GoogleUpdateJobObserver>* job_observer;
  HRESULT hr =
      CComObject<GoogleUpdateJobObserver>::CreateInstance(&job_observer);
  if (hr != S_OK) {
    return ReportFailure(hr, GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED,
                         main_loop);
  }

  CComPtr<IJobObserver> job_holder(job_observer);

  CComPtr<IGoogleUpdate> on_demand;

  if (InstallUtil::IsPerUserInstall(chrome_exe_path.c_str())) {
    hr = on_demand.CoCreateInstance(CLSID_OnDemandUserAppsClass);
  } else {
    // The Update operation needs Admin privileges for writing
    // to %ProgramFiles%. On Vista we need to elevate before instantiating
    // the updater instance.
    if (!install_if_newer) {
      hr = on_demand.CoCreateInstance(CLSID_OnDemandMachineAppsClass);
    } else {
      hr = CoCreateInstanceAsAdmin(CLSID_OnDemandMachineAppsClass,
                                   IID_IGoogleUpdate,
                                   reinterpret_cast<void**>(&on_demand));
    }
  }

  if (hr != S_OK)
    return ReportFailure(hr, GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, main_loop);

  if (!install_if_newer)
    hr = on_demand->CheckForUpdate(google_update::kChromeGuid, job_observer);
  else
    hr = on_demand->Update(google_update::kChromeGuid, job_observer);

  if (hr != S_OK)
    return ReportFailure(hr, GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR,
                         main_loop);

  // We need to spin the message loop while Google Update is running so that it
  // can report back to us through GoogleUpdateJobObserver. This message loop
  // will terminate once Google Update sends us the completion status
  // (success/error). See OnComplete().
  MessageLoop::current()->Run();

  GoogleUpdateUpgradeResult results;
  hr = job_observer->GetResult(&results);
  if (hr != S_OK)
    return ReportFailure(hr, GOOGLE_UPDATE_GET_RESULT_CALL_FAILED, main_loop);

  if (results == UPGRADE_ERROR)
    return ReportFailure(hr, GOOGLE_UPDATE_ERROR_UPDATING, main_loop);

  hr = job_observer->GetVersionInfo(&version_available_);
  if (hr != S_OK)
    return ReportFailure(hr, GOOGLE_UPDATE_GET_VERSION_INFO_FAILED, main_loop);

  main_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
      &GoogleUpdate::ReportResults, results, GOOGLE_UPDATE_NO_ERROR));
  job_holder = NULL;
  on_demand = NULL;
  return true;
}

void GoogleUpdate::ReportResults(GoogleUpdateUpgradeResult results,
                                 GoogleUpdateErrorCode error_code) {
  // If we get an error, then error code must not be blank, and vice versa.
  DCHECK(results == UPGRADE_ERROR ? error_code != GOOGLE_UPDATE_NO_ERROR :
                                    error_code == GOOGLE_UPDATE_NO_ERROR);
  if (listener_)
    listener_->OnReportResults(results, error_code, version_available_);
}

bool GoogleUpdate::ReportFailure(HRESULT hr, GoogleUpdateErrorCode error_code,
                                 MessageLoop* main_loop) {
  NOTREACHED() << "Communication with Google Update failed: " << hr
               << " error: " << error_code;
  main_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
      &GoogleUpdate::ReportResults, UPGRADE_ERROR, error_code));
  return false;
}
