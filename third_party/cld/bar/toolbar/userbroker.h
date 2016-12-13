// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: Tom Wuttke (tw@google.com)
//
// This file defines the main userbroker api (see design doc at
// /eng/designdocs/navclient/toolbar-userbroker.html

#ifndef BAR_TOOLBAR_USERBROKER_H_
#define BAR_TOOLBAR_USERBROKER_H_

#include "cld/bar/common/installhelper.h"
#include "shared/closed/financial/rlz/win/lib/rlz_lib.h"

struct IUserBrokerProxy;

namespace user_broker {

// Call this to init the broker API from the broker or IE process.
// Must be called before any other user_broker function.
bool InitApi(bool set_is_broker);

// Call this to release resources hold by the broker before the toolbar
// is shut down.
void TearDown();

// Call this to inform the broker the current process has high privileges.
// On XP, this will always return true, and set a flag so that future calls
// to IsHighRights() will succeed.  On Vista, this will return false if the
// current process doesn't really have a high mandatory level.
bool DeclareHighRightsProcess();

// Get which integrity level the current process running at.  On Vista, this is
// accurate. On XP, this is inferred from InitAPI()/DeclareHighRightsProcess().
MANDATORY_LEVEL GetProcessIntegrity();

// True if the broker is operational.  Call this from the toolbar startup
// code to see if anything is wrong before making a toolbar object.
bool IsBrokerOK();

// Given a file path, resolve to an absolute path, and return true only if this
// path should be writeable by the toolbar userbroker or not.
bool ValidateToolbarFilePath(const TCHAR* path, TCHAR* full_path,
    bool create_directory);

// Given a reg path, resolve to an absolute reg path, and return true only if
// this reg key should be writeable by the toolbar userbroker or not.
bool ValidateToolbarRegistryPath(HKEY root, const TCHAR* path, REGSAM wow64);

bool IsUserBrokerProcess(HMODULE dll_instance);

// Returns the actual userbroker.exe file name when called from the toolbar.
CString GetUserStubExeFilename();

// These next functions are exact replicas of Win32 API functions - except
// that they live inside the user_broker namespace, and will dispatch
// the broker process if the operation requires writing.

// ValidateToolbarFilePath or ValidateToolbarRegistryPath are called
// before opening any handle for writing by the broker

LONG RegCreateKeyEx(HKEY key, const TCHAR* sub_key, DWORD reserved,
    TCHAR* key_class, DWORD options, REGSAM sam_desired, SECURITY_ATTRIBUTES*
    security_attributes, HKEY* result, DWORD* disposition);

LONG RegOpenKeyEx(HKEY key, const TCHAR* sub_key, DWORD options,
    REGSAM sam_desired, HKEY* result);

DWORD RegDeleteTree(HKEY key_root, const TCHAR* key_path, REGSAM wow64);

DWORD SHCreateDirectory(HWND window, const TCHAR* path);

HANDLE CreateFile(const TCHAR* file_name, DWORD desired_access,
    DWORD share_mode, SECURITY_ATTRIBUTES* security_attributes,
    DWORD creation_disposition, DWORD flags, HANDLE template_file);

BOOL DeleteFile(const TCHAR* file_name);

BOOL RemoveDirectory(const TCHAR*  path);

BOOL MoveFileEx(const TCHAR* file_name, const TCHAR* new_name,
    DWORD flags);

BOOL CopyFile(const TCHAR* file_name, const TCHAR* new_name,
    BOOL fail_if_exists);

// The UserBrokerProxy is a simple COM interface so that the search
// box module (which is outside of the toolbar code space) can use the broker
// from the host toolbar to write and delete user files.
HRESULT GetUserBrokerProxy(IUserBrokerProxy** broker_proxy);

bool ApplyPatch(const TCHAR* patch);

BOOL DeleteInUseFile(const TCHAR* file_name);

// Fixes the IE menus by deleted broken registry keys.
bool FixMenus();

// True if the current process has elevated rights
// (or if XP is simulating it via DeclareHighRightsProcess).
// InitApi must be called before this function.
inline bool IsHighRights() {
  return GetProcessIntegrity() >= MandatoryLevelHigh;
}

// True if the current process is in low rights protected mode
// (or if XP is simulating it via InitAPI(false)).
// InitApi must be called before this function.
inline bool IsLowRights() {
  return GetProcessIntegrity() <= MandatoryLevelLow;
}

// True if the current process is virtualized
// (or if XP is simulating it via InitAPI(false)).
// InitApi must be called before this function.
bool IsVirtualized();

// True if the current process will need to use the broker to do the sort of
// things that sometimes require using a broker (such as modifying shared
// registry keys and files).
// InitApi must be called before this function.
inline bool ShouldUseBroker() {
  return IsVirtualized() || IsLowRights();
}

HRESULT Uninstall(const TCHAR* user_sid, HWND skip_window, HANDLE* process);

// Wipe any virtualized files or reg keys from known toolbar paths
HRESULT CleanVirtualizedPaths();

// Enables toolbar in case user has disabled it specifically
void EnableGoogleToolbar();

// Finds all the interesting virtual folders via user broker and adds them
// to the set.
bool FindVirtualFolders(std::set<CString> *folder_set);

// Call the updater service to verify and execute this exe
HRESULT ExecuteGoogleSignedExeElevated(const TCHAR* exe, const TCHAR* args,
    HANDLE* process, bool allow_network_check);

// True if the google update service is available
bool IsUpdaterServiceAvailable();

// Send a crash report.  file_path indicates the minidump path, last_url
// contains the last URL accessed, and last_command contains the last
// command used. Returns S_OK if the default crash handler should still be used,
// S_FALSE if it should not, and E_FAIL in case of failure.
HRESULT SendCrashReport(LPCTSTR file_path,
                        LPCTSTR last_url,
                        int last_command,
                        bool silent);

// Clear all events reported by this product.
bool RlzClearAllProductEvents(rlz_lib::Product product, const WCHAR* sid);

// Parses RLZ related ping response. See comments to rlz_lib::ParsePingResponse.
bool RlzParsePingResponse(rlz_lib::Product product, const WCHAR* response,
                          const WCHAR* sid);

// Parses RLZ related ping response. See comments to rlz_lib::SetAccessPointRlz.
bool RlzSetAccessPointRlz(rlz_lib::AccessPoint point, const WCHAR* new_rlz,
                          const WCHAR* sid);

// Records an RLZ event. See rlz_lib::RecordProductEvent for more comments.
bool RlzRecordProductEvent(rlz_lib::Product product, rlz_lib::AccessPoint point,
                           rlz_lib::Event event_id, const WCHAR* sid);

// Adds RLS information to the RLZ library. See rlz_lib::RecordProductRls for
// more comments. Unlike rlz_lib::RecordProductRls, this function only accepts
// one access point because we only call it for one access point.
bool RlzRecordProductRls(rlz_lib::Product product, rlz_lib::AccessPoint point,
                         const WCHAR* rls_value, const WCHAR* sid);

// Parses the responses from the financial server.
// See rlz_lib::ParseFinancialPingResponse for more comments.
bool RlzParseFinancialPingResponse(rlz_lib::Product product,
                                   const WCHAR* response, const WCHAR* sid);

// Sets focus to window.
bool SetFocus(HWND hwnd);

// Compares text with content of IE's address bar.
bool IsEqualAddressBarText(HWND hwnd, const TCHAR* text);

// The following are QSB API wrappers. Currently QSB API requires medium
// integrity level to work so it can't be called directly from Toolbar.
// QSB API is also not thread safe in a way that Initialize/Uninitialize must
// be called in the same thread as all the rest functions. Thus API is
// initialized and uninitialized on each RPC call to any of API functions.
// See shared/quick_search_box/qsb_host_api/qsb_api.h for specific info about
// each function.
// TODO(avayvod): Implement load of QSB dll in user broker once so it's not
// loaded/unloaded on each API call.
bool QsbApiEnable(bool enable);
bool QsbApiIsEnabled();
bool QsbApiIsInstalled();
bool QsbApiIsTaskbarButtonEnabled();
bool QsbApiEnableTaskbarButton(bool enable);
bool QsbApiSetGoogleDomain(const TCHAR* google_domain);

// Registers histogram.
bool RegisterHistogram(int type,
                       const TCHAR* name,
                       int minimum,
                       int maximum,
                       int bucket_count,
                       int flags);

// Adds histogram value.
bool AddHistogramValue(const TCHAR* name,
                       int value);

// Returns HTML representation of accumulated histograms.
// Query could define prefix of histogram names (i.e. DNS) to be used in graph.
bool GetHistogramGraph(const TCHAR* query,
                       CString* graph_html);

// Returns current state of MetricsLog ready for UMA submission (XML & BZ2).
bool GetMetricsLogSubmission(CString* metrics_log);

// Passes current value of UsageStatsEnabled option to MetricsService.
bool UsageStatsEnable(bool enable);

// Use user_broker::CAtlFile if you want the create function brokered.
class CAtlFile : public ::CAtlFile {
 public:
  CAtlFile() {
  }

  // Base class takes ownership over the file handle.
  explicit CAtlFile(HANDLE hFile) : ::CAtlFile(hFile) {
  }

  // Same as the ATL function but uses the user_broker on CreateFile
  HRESULT Create(LPCTSTR szFilename,
                 DWORD dwDesiredAccess,
                 DWORD dwShareMode,
                 DWORD dwCreationDisposition,
                 DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
                 LPSECURITY_ATTRIBUTES lpsa = NULL,
                 HANDLE hTemplateFile = NULL) {
    ATLASSERT(m_h == NULL);

    HANDLE hFile = user_broker::CreateFile(szFilename,
                                           dwDesiredAccess,
                                           dwShareMode,
                                           lpsa,
                                           dwCreationDisposition,
                                           dwFlagsAndAttributes,
                                           hTemplateFile);
    if (hFile == INVALID_HANDLE_VALUE)
      return AtlHresultFromLastError();

    Attach(hFile);
    return S_OK;
  }
  DISALLOW_EVIL_CONSTRUCTORS(CAtlFile);
};

// A convenience function for setting Windows' last error based on an HRESULT
// received from the user broker server.  This function is used internally by
// the user broker and does not need to be used by users of the user broker
// (usually;^).
DWORD SetLastErrorFromAtlError(HRESULT result);

}  // namespace user_broker


#endif  // BAR_TOOLBAR_USERBROKER_H_
