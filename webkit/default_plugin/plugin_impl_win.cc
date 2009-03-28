// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/plugin_impl_win.h"

#include <shellapi.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_strings.h"
#include "unicode/locid.h"
#include "webkit/default_plugin/activex_installer.h"
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/activex_shim/npn_scripting.h"
#include "webkit/default_plugin/default_plugin_shared.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/webkit_glue.h"

static const int TOOLTIP_MAX_WIDTH = 500;

PluginInstallerImpl::PluginInstallerImpl(int16 mode)
    : instance_(NULL),
      is_activex_(false),
      mode_(mode),
      plugin_install_stream_(NULL),
      plugin_installer_state_(PluginInstallerStateUndefined),
      enable_click_(false),
      icon_(NULL),
      bold_font_(NULL),
      regular_font_(NULL),
      underline_font_(NULL),
      tooltip_(NULL),
      activex_installer_(NULL),
      installation_job_monitor_thread_(
          new PluginInstallationJobMonitorThread()),
      plugin_database_handler_(*this),
      plugin_download_url_for_display_(false) {
}

PluginInstallerImpl::~PluginInstallerImpl() {
  installation_job_monitor_thread_->Stop();

  if (bold_font_)
    ::DeleteObject(bold_font_);

  if (underline_font_)
    ::DeleteObject(underline_font_);

  if (activex_installer_) {
    activex_installer_->Cleanup();
    activex_installer_->Release();
  }

  if (tooltip_)
    ::DestroyWindow(tooltip_);
}

bool PluginInstallerImpl::Initialize(HINSTANCE module_handle, NPP instance,
                                     NPMIMEType mime_type, int16 argc,
                                     char* argn[], char* argv[]) {
  DLOG(INFO) << __FUNCTION__ << " Mime Type : " << mime_type;
  DCHECK(instance != NULL);
  DCHECK(module_handle != NULL);

  instance_ = instance;
  mime_type_ = mime_type;

  // The clsid without the {} parentheses.
  std::string raw_activex_clsid;
  if (!ParseInstantiationArguments(mime_type, instance, argc, argn, argv,
                                   &raw_activex_clsid, &is_activex_,
                                   &activex_clsid_,
                                   &activex_codebase_,
                                   &plugin_download_url_,
                                   &plugin_finder_url_)) {
    DLOG(ERROR) << "Incorrect arguments passed to plugin";
    NOTREACHED();
    return false;
  }

  if (!installation_job_monitor_thread_->Initialize()) {
    DLOG(ERROR) << "Failed to initialize plugin install job";
    NOTREACHED();
    return false;
  }

  InitializeResources(module_handle);

  if (is_activex_) {
    // If the codebase is not from a whitelisted website, we do not allow
    // download.
    if (!activex_shim::IsCodebaseAllowed(raw_activex_clsid,
                                         activex_codebase_)) {
      activex_codebase_.clear();
      plugin_download_url_.clear();
    }

    if (!plugin_download_url_.empty()) {
      set_plugin_installer_state(PluginListDownloaded);
      DisplayAvailablePluginStatus();
      NotifyPluginStatus(default_plugin::MISSING_PLUGIN_AVAILABLE);
    } else {
      set_plugin_installer_state(PluginListDownloadFailed);
      DisplayStatus(IDS_DEFAULT_PLUGIN_NO_PLUGIN_AVAILABLE_MSG);
    }
  } else {
    DisplayStatus(IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG_NO_PLUGIN_NAME);
    plugin_database_handler_.DownloadPluginsFileIfNeeded(plugin_finder_url_);
  }

  return true;
}

void PluginInstallerImpl::Shutdown() {
  if (install_dialog_.IsWindow()) {
    install_dialog_.DestroyWindow();
  }
  if (IsWindow()) {
    DestroyWindow();
  }
}

void PluginInstallerImpl::NewStream(NPStream* stream) {
  plugin_install_stream_ = stream;
}

void PluginInstallerImpl::DestroyStream(NPStream* stream, NPError reason) {
  if (stream == plugin_install_stream_)
    plugin_install_stream_ = NULL;
}

bool PluginInstallerImpl::WriteReady(NPStream* stream) {
  bool ready_to_accept_data = false;

  if (plugin_installer_state() != PluginListDownloadInitiated) {
    DCHECK(default_plugin::g_browser);
    DCHECK(default_plugin::g_browser->destroystream);
    // We don't want any data, kill the stream.
    default_plugin::g_browser->destroystream(instance_, stream, NPRES_DONE);
  }
  if (0 == lstrcmpiA(stream->url, plugin_finder_url_.c_str())) {
    ready_to_accept_data = true;
  }
  return ready_to_accept_data;
}

int32 PluginInstallerImpl::Write(NPStream* stream, int32 offset,
                                 int32 buffer_length, void* buffer) {
  if (plugin_installer_state() != PluginListDownloadInitiated) {
    DCHECK(default_plugin::g_browser);
    DCHECK(default_plugin::g_browser->destroystream);
    // We don't want any data, kill the stream.
    default_plugin::g_browser->destroystream(instance_, stream, NPRES_DONE);
    return 0;
  }

  return plugin_database_handler_.Write(stream, offset, buffer_length, buffer);
}

void PluginInstallerImpl::ClearDisplay() {
  enable_click_ = false;
  command_.clear();
  optional_additional_message_.clear();
  get_plugin_link_message_.clear();
}

void PluginInstallerImpl::RefreshDisplay() {
  if (!IsWindow())
    return;
  UpdateToolTip();

  InvalidateRect(NULL, TRUE);
  UpdateWindow();
}

bool PluginInstallerImpl::CreateToolTip() {
  DWORD ex_styles = IsRTLLayout() ? WS_EX_LAYOUTRTL : 0;
  tooltip_ = CreateWindowEx(ex_styles, TOOLTIPS_CLASS, NULL,
                            WS_POPUP | TTS_ALWAYSTIP,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            m_hWnd, NULL, NULL, NULL);
 if (!tooltip_)
   return false;

  // Associate the ToolTip with the tool.
  TOOLINFO tool_info = {0};
  tool_info.cbSize = sizeof(tool_info);
  tool_info.hwnd = m_hWnd;
  tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  tool_info.uId = reinterpret_cast<UINT_PTR>(m_hWnd);
  tool_info.lpszText = NULL;
  SendMessage(tooltip_, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&tool_info));
  SendMessage(tooltip_, TTM_SETMAXTIPWIDTH, 0, TOOLTIP_MAX_WIDTH);
  return true;
}

void PluginInstallerImpl::UpdateToolTip() {
  if (tooltip_ == NULL)
    return;
  std::wstring tip = command_;
  if (!optional_additional_message_.empty())
    tip += std::wstring(L"\n\r") + optional_additional_message_;

  TOOLINFO tool_info = {0};
  tool_info.cbSize = sizeof(tool_info);
  tool_info.hwnd = m_hWnd;
  tool_info.uFlags = TTF_IDISHWND;
  tool_info.uId = reinterpret_cast<UINT_PTR>(m_hWnd);
  tool_info.lpszText = const_cast<LPWSTR>(tip.c_str());
  SendMessage(tooltip_, TTM_UPDATETIPTEXT, 0, (LPARAM)&tool_info);
}

void PluginInstallerImpl::DisplayAvailablePluginStatus() {
  ClearDisplay();
  enable_click_ = true;
  command_ = ReplaceStringForPossibleEmptyReplacement(
      IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG,
      IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG_NO_PLUGIN_NAME,
      plugin_name_);
  optional_additional_message_ =
      webkit_glue::GetLocalizedString(IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG_2);
  get_plugin_link_message_ = webkit_glue::GetLocalizedString(
      IDS_DEFAULT_PLUGIN_GET_THE_PLUGIN_BTN_MSG);
  RefreshDisplay();
}

void PluginInstallerImpl::DisplayStatus(int message_resource_id) {
  ClearDisplay();
  command_ =
      webkit_glue::GetLocalizedString(message_resource_id);
  RefreshDisplay();
}

void PluginInstallerImpl::DisplayPluginDownloadFailedStatus() {
  ClearDisplay();
  command_ = webkit_glue::GetLocalizedString(
      IDS_DEFAULT_PLUGIN_DOWNLOAD_FAILED_MSG);
  command_ = ReplaceStringPlaceholders(
      command_,
      ASCIIToWide(plugin_download_url_),
      NULL);
  RefreshDisplay();
}

void PluginInstallerImpl::URLNotify(const char* url, NPReason reason) {
  DCHECK(plugin_installer_state_ == PluginListDownloadInitiated);

  if (plugin_installer_state_ == PluginListDownloadInitiated) {
    bool plugin_available = false;
    if (reason == NPRES_DONE) {
      DLOG(INFO) << "Received Done notification for plugin list download";
      set_plugin_installer_state(PluginListDownloaded);
      plugin_database_handler_.ParsePluginList();
      if (plugin_database_handler_.GetPluginDetailsForMimeType(
              mime_type_.c_str(), desired_language_.c_str(),
              &plugin_download_url_, &plugin_name_,
              &plugin_download_url_for_display_)) {
        plugin_available = true;
      } else {
        set_plugin_installer_state(PluginListDownloadedPluginNotFound);
      }

      plugin_database_handler_.Close(false);
    } else {
      DLOG(WARNING) << "Failed to download plugin list";
      set_plugin_installer_state(PluginListDownloadFailed);

      plugin_database_handler_.Close(true);
    }

    if (plugin_available) {
      DLOG(INFO) << "Plugin available for mime type " << mime_type_;
      DisplayAvailablePluginStatus();
      NotifyPluginStatus(default_plugin::MISSING_PLUGIN_AVAILABLE);
    } else {
      DLOG(WARNING) << "No plugin available for mime type " << mime_type_;
      DisplayStatus(IDS_DEFAULT_PLUGIN_NO_PLUGIN_AVAILABLE_MSG);
    }
  }

  default_plugin::g_browser->destroystream(instance_, plugin_install_stream_,
                                           NPRES_DONE);
}

int16 PluginInstallerImpl::NPP_HandleEvent(void* event) {
  // This is a hack. The renderer will send a direct custom message to ask for
  // installation.
  NPEvent* npp_event = static_cast<NPEvent*>(event);
  if (npp_event->event == kInstallMissingPluginMessage) {
    // We could get this message because InfoBar may not be in sync with our
    // internal processing. So we need to check the status.
    if (plugin_installer_state() == PluginListDownloaded) {
      ShowInstallDialog();
    }
  }
  return 0;
}

std::wstring PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
    int message_id_with_placeholders,
    int messsage_id_without_placeholders,
    const std::wstring& replacement_string) {
  // If the replacement_string is not empty, load the string identified by
  // the resource id message_id_with_placeholders, and replace the
  // placeholder with the replacement_string. Otherwise return the string
  // identified by resource id messsage_id_without_placeholders.
  if (replacement_string.empty()) {
    return webkit_glue::GetLocalizedString(messsage_id_without_placeholders);
  } else {
    std::wstring string_with_placeholders =
        webkit_glue::GetLocalizedString(message_id_with_placeholders);
    return ReplaceStringPlaceholders(string_with_placeholders,
                                     replacement_string, NULL);
  }
}

bool PluginInstallerImpl::SetWindow(HWND parent_window) {
  if (!::IsWindow(parent_window)) {
    // No window created yet. Ignore this call.
    if (!IsWindow())
      return true;
    // Parent window has been destroyed.
    Shutdown();
    return true;
  }

  RECT parent_rect = {0};

  if (IsWindow()) {
    ::GetClientRect(parent_window, &parent_rect);
    SetWindowPos(NULL, &parent_rect, SWP_SHOWWINDOW);
    return true;
  }
  // First time in -- no window created by plugin yet.
  ::GetClientRect(parent_window, &parent_rect);
  Create(parent_window, parent_rect, NULL, WS_CHILD | WS_BORDER);
  DCHECK(IsWindow());
  installation_job_monitor_thread_->set_plugin_window(m_hWnd);

  CreateToolTip();
  UpdateToolTip();

  UpdateWindow();
  ShowWindow(SW_SHOW);

  return true;
}

void PluginInstallerImpl::DownloadPlugin() {
  set_plugin_installer_state(PluginDownloadInitiated);

  DLOG(INFO) << "Initiating download for plugin URL "
             << plugin_download_url_.c_str();

  DisplayStatus(IDS_DEFAULT_PLUGIN_DOWNLOADING_PLUGIN_MSG);

  if (is_activex_) {
    if (activex_installer_ == NULL) {
      CComObject<ActiveXInstaller>::CreateInstance(&activex_installer_);
      activex_installer_->AddRef();
    }
    activex_installer_->StartDownload(activex_clsid_, activex_codebase_, m_hWnd,
                                      kActivexInstallResult);
  } else {
    if (!plugin_download_url_for_display_) {
      webkit_glue::DownloadUrl(plugin_download_url_, m_hWnd);
    } else {
      default_plugin::g_browser->geturl(instance(),
                                        plugin_download_url_.c_str(),
                                        "_blank");
      set_plugin_installer_state(PluginInstallerLaunchSuccess);
      DisplayStatus(IDS_DEFAULT_PLUGIN_REFRESH_PLUGIN_MSG);
      enable_click_ = true;
      RefreshDisplay();
    }
  }
}

void PluginInstallerImpl::DownloadCancelled() {
  DisplayAvailablePluginStatus();
}

LRESULT PluginInstallerImpl::OnEraseBackGround(UINT message, WPARAM wparam,
                                               LPARAM lparam, BOOL& handled) {
  HDC paint_device_context = reinterpret_cast<HDC>(wparam);
  RECT erase_rect = {0};
  ::GetClipBox(paint_device_context, &erase_rect);
  HBRUSH brush = ::CreateSolidBrush(RGB(252, 235, 162));
  DCHECK(brush);
  ::FillRect(paint_device_context, &erase_rect, brush);
  ::DeleteObject(brush);
  return 1;
}

// Use ICU in order to determine whether the locale is right-to-left.
//
// TODO(idana) bug# 1246452: there is already code in
// chrome/common/l10n_util.h/cc that uses ICU to determine the locale
// direction. We don't currently use this code since code in WebKit should not
// depend on code in Chrome. We can fix this by pulling (at least part of) the
// l10n_util functionality up into the Base module.
bool PluginInstallerImpl::IsRTLLayout() const {
  const Locale& locale = Locale::getDefault();
  const char* lang = locale.getLanguage();

  // Check only for Arabic and Hebrew languages for now.
  if (strcmp(lang, "ar") == 0 || strcmp(lang, "he") == 0) {
    return true;
  }
  return false;
}

LRESULT PluginInstallerImpl::OnPaint(UINT message, WPARAM wparam, LPARAM lparam,
                                     BOOL& handled) {
  PAINTSTRUCT paint_struct = {0};
  BeginPaint(&paint_struct);

  int save_dc_context = SaveDC(paint_struct.hdc);
  // The drawing order is as below:-
  // 1. The Get plugin link at the top left corner.
  // 2. The plugin icon.
  // 3. The text describing user actions to the right of the icon.
  SIZE get_plugin_link_extents = {0};
  HFONT old_font =
      reinterpret_cast<HFONT>(SelectObject(paint_struct.hdc, regular_font_));

  GetTextExtentPoint32(paint_struct.hdc, get_plugin_link_message_.c_str(),
                       static_cast<int>(get_plugin_link_message_.length()),
                       &get_plugin_link_extents);

  POINT device_point = {0};
  device_point.x = get_plugin_link_extents.cx;
  device_point.y = get_plugin_link_extents.cy;
  LPtoDP(paint_struct.hdc, &device_point, 1);

  RECT text_rect = {0};
  text_rect.left   = 5;
  text_rect.right  = text_rect.left + device_point.x;
  text_rect.top    = 5;
  text_rect.bottom = text_rect.top + device_point.y;

  RECT client_rect = {0};
  GetClientRect(&client_rect);

  int icon_width = GetSystemMetrics(SM_CXICON);
  int icon_height = GetSystemMetrics(SM_CYICON);

  int x = (client_rect.right / 2) - icon_width / 2;
  int y = (client_rect.bottom / 2) - icon_height / 2;

  DCHECK(icon_);
  DrawIcon(paint_struct.hdc, x, y, icon_);

  int old_mode = SetBkMode(paint_struct.hdc, TRANSPARENT);
  COLORREF old_color = SetTextColor(paint_struct.hdc, RGB(0, 0, 255));

  // If the UI layout is right-to-left, we need to mirror the position of the
  // link text and also make sure that the text is aligned to the right.
  DWORD draw_text_flags = DT_VCENTER;
  if (IsRTLLayout()) {
    draw_text_flags |= DT_RTLREADING | DT_RIGHT;
    int text_width = text_rect.right - text_rect.left;
    int client_width = client_rect.right - client_rect.left;
    text_rect.left = client_width - text_rect.left - text_width;
    text_rect.right = text_rect.left + text_width;
  } else {
    draw_text_flags |= DT_LEFT;
  }

  DrawText(paint_struct.hdc, get_plugin_link_message_.c_str(),
           static_cast<int>(get_plugin_link_message_.length()),
           &text_rect, draw_text_flags);

  SIZE command_extents = {0};
  GetTextExtentPoint32(paint_struct.hdc, command_.c_str(),
                       static_cast<int>(command_.length()), &command_extents);

  int y_origin = y + (icon_height) + 10;
  int x_origin = x - command_extents.cx / 2 + icon_width / 2;

  // If the window is too small then no point in attempting to display plugin
  // information. In any case this info is displayed when the user hovers
  // over the plugin window.
  int window_width = client_rect.right - client_rect.left;
  int window_height = client_rect.bottom - client_rect.top;

  if ((x_origin + command_extents.cx <= window_width) &&
    (y_origin + command_extents.cy <= window_height)) {
    PaintUserActionInformation(paint_struct.hdc, x_origin, y_origin);
  }

  RestoreDC(paint_struct.hdc, save_dc_context);
  EndPaint(&paint_struct);
  return 0;
}

void PluginInstallerImpl::PaintUserActionInformation(HDC paint_dc,
                                                     int x_origin,
                                                     int y_origin) {
  SelectObject(paint_dc, regular_font_);

  SIZE command_extents = {0};
  GetTextExtentPoint32(paint_dc, command_.c_str(),
                       static_cast<int>(command_.length()), &command_extents);
  POINT device_point = {0};
  device_point.x = command_extents.cx;
  device_point.y = command_extents.cy;
  LPtoDP(paint_dc, &device_point, 1);

  RECT text_rect = {0};
  text_rect.left   = x_origin;
  text_rect.right  = text_rect.left + device_point.x;
  text_rect.top    = y_origin;
  text_rect.bottom = text_rect.top + device_point.y;

  SetTextColor(paint_dc, RGB(0, 0, 0));

  // Make sure we pass the right BiDi flags to DrawText if the UI layout is
  // right-to-left.
  UINT additional_dt_flags = IsRTLLayout() ? DT_RTLREADING : 0;
  DrawText(paint_dc, command_.c_str(),
           static_cast<int>(command_.length()), &text_rect,
           DT_CENTER | DT_VCENTER | additional_dt_flags);

  if (!optional_additional_message_.empty()) {
    SelectObject(paint_dc, regular_font_);

    SIZE optional_text_extents = {0};
    int optional_message_length =
          static_cast<int>(optional_additional_message_.length());
    GetTextExtentPoint32(paint_dc,
                         optional_additional_message_.c_str(),
                         optional_message_length,
                         &optional_text_extents);
    POINT device_point = {0};
    device_point.x = optional_text_extents.cx;
    device_point.y = optional_text_extents.cy;
    LPtoDP(paint_dc, &device_point, 1);

    text_rect.right  = text_rect.left + device_point.x;
    text_rect.top = y_origin + command_extents.cy + 2;
    text_rect.bottom = text_rect.top + device_point.y;

    DrawText(paint_dc, optional_additional_message_.c_str(),
             static_cast<int>(optional_additional_message_.length()),
             &text_rect, DT_CENTER | DT_VCENTER | additional_dt_flags);
  }
}

void PluginInstallerImpl::ShowInstallDialog() {
  enable_click_ = false;
  install_dialog_.Initialize(this, plugin_name_);
  install_dialog_.Create(m_hWnd, NULL);
  install_dialog_.ShowWindow(SW_SHOW);
}

LRESULT PluginInstallerImpl::OnLButtonDown(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  if (!enable_click_)
    return 0;
  if (plugin_installer_state() == PluginListDownloaded) {
    ShowInstallDialog();
    NotifyPluginStatus(default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD);
  } else if (plugin_installer_state_ == PluginInstallerLaunchSuccess) {
    DCHECK(default_plugin::g_browser);
    DCHECK(default_plugin::g_browser->geturl);
    default_plugin::g_browser->geturl(
        instance_,
        "javascript:navigator.plugins.refresh(true)",
        "_self");
    default_plugin::g_browser->geturl(
        instance_,
        "javascript:window.location.reload(true)",
        "_self");
  }

  return 0;
}

LRESULT PluginInstallerImpl::OnSetCursor(UINT message, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
  if (enable_click_) {
    ::SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND)));
    return 1;
  }
  handled = FALSE;
  return 0;
}

LRESULT PluginInstallerImpl::OnRefreshPlugins(UINT message, WPARAM wparam,
                                              LPARAM lparam, BOOL& handled) {
  DCHECK(default_plugin::g_browser);
  DCHECK(default_plugin::g_browser->geturl);
  default_plugin::g_browser->geturl(
      instance_,
      "javascript:navigator.plugins.refresh(true)",
      "_self");
  default_plugin::g_browser->geturl(
      instance_,
      "javascript:window.location.reload(true)",
      "_self");
  return 0;
}

LRESULT PluginInstallerImpl::OnCopyData(UINT message, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  COPYDATASTRUCT* download_file_info =
      reinterpret_cast<COPYDATASTRUCT*>(lparam);
  if (!download_file_info || !download_file_info->dwData) {
    DLOG(WARNING) << "Failed to download plugin";
    set_plugin_installer_state(PluginDownloadFailed);
    DisplayPluginDownloadFailedStatus();
  } else {
    DLOG(INFO) << "Successfully downloaded plugin installer";
    set_plugin_installer_state(PluginDownloadCompleted);

    std::wstring file_path =
        reinterpret_cast<const wchar_t*>(download_file_info->lpData);

    std::wstring current_directory =
        file_util::GetDirectoryFromPath(file_path);

    SHELLEXECUTEINFO shell_execute_info = {0};
    shell_execute_info.cbSize = sizeof(shell_execute_info);
    shell_execute_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    shell_execute_info.lpFile = file_path.c_str();
    shell_execute_info.lpDirectory = current_directory.c_str();
    shell_execute_info.nShow = SW_SHOW;

    if (!ShellExecuteEx(&shell_execute_info)) {
      DLOG(WARNING) << "Failed to launch plugin installer "
                    << file_path.c_str() << " Error "
                    << ::GetLastError();
      set_plugin_installer_state(PluginInstallerLaunchFailure);
      DisplayStatus(IDS_DEFAULT_PLUGIN_INSTALLATION_FAILED_MSG);
      NOTREACHED();
    } else {
      DLOG(INFO) << "Successfully launched plugin installer";
      set_plugin_installer_state(PluginInstallerLaunchSuccess);
      installation_job_monitor_thread_->AssignProcessToJob(
          shell_execute_info.hProcess);
      DisplayStatus(IDS_DEFAULT_PLUGIN_REFRESH_PLUGIN_MSG);
      enable_click_ = true;
      RefreshDisplay();
    }
  }
  return 0;
}

LRESULT PluginInstallerImpl::OnActiveXInstallResult(UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam,
                                                    BOOL& handled) {
  handled = TRUE;

  if (SUCCEEDED(wparam)) {
    set_plugin_installer_state(PluginInstallerLaunchSuccess);
    DisplayStatus(IDS_DEFAULT_PLUGIN_REFRESH_PLUGIN_MSG);
    PostMessage(kRefreshPluginsMessage, 0, 0);
  } else if ((wparam == INET_E_UNKNOWN_PROTOCOL) ||
             (wparam == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND))) {
    set_plugin_installer_state(PluginDownloadFailed);
    DisplayPluginDownloadFailedStatus();
  } else {
    set_plugin_installer_state(PluginInstallerLaunchFailure);
    DisplayStatus(IDS_DEFAULT_PLUGIN_INSTALLATION_FAILED_MSG);
  }
  return 0;
}

std::string PluginInstallerImpl::ResolveURL(NPP instance,
                                            const std::string& relative_url) {
  // The NPAPI functions may not be available if this function is called
  // as a result of a unit test.
  if (default_plugin::g_browser) {
    NPObject* object = NULL;
    default_plugin::g_browser->getvalue(instance, NPNVWindowNPObject, &object);
    activex_shim::NPNScriptableObject window(instance, object);
    std::wstring url =
        window.GetObjectProperty("document").GetStringProperty("URL");
    GURL base(url);
    return base.Resolve(relative_url).spec();
  }
  return relative_url;
}

bool PluginInstallerImpl::InitializeResources(HINSTANCE module_handle) {
  DCHECK(icon_ == NULL);
  DCHECK(regular_font_ == NULL);
  DCHECK(bold_font_ == NULL);
  DCHECK(underline_font_ == NULL);

  icon_ = LoadIcon(module_handle, MAKEINTRESOURCE(IDI_DEFAULT_PLUGIN_ICON));
  DCHECK(icon_ != NULL);

  desired_language_ = "en-us";
  regular_font_ = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  DCHECK(regular_font_ != NULL);

  LOGFONT font_info = {0};
  GetObject(regular_font_, sizeof(LOGFONT), &font_info);
  font_info.lfWeight |= FW_BOLD;
  bold_font_ = CreateFontIndirect(&font_info);
  DCHECK(bold_font_ != NULL);

  font_info.lfUnderline = TRUE;
  underline_font_ = CreateFontIndirect(&font_info);
  DCHECK(underline_font_ != NULL);
  return true;
}

bool PluginInstallerImpl::ParseInstantiationArguments(
    NPMIMEType mime_type,
    NPP instance,
    int16 argc,
    char* argn[],
    char* argv[],
    std::string* raw_activex_clsid,
    bool* is_activex,
    std::string* activex_clsid,
    std::string* activex_codebase,
    std::string* plugin_download_url,
    std::string* plugin_finder_url) {

  if (!raw_activex_clsid || !is_activex || !activex_clsid ||
      !plugin_download_url || !plugin_finder_url || !activex_codebase) {
    NOTREACHED();
    return false;
  }

  *is_activex = false;

  bool valid_mime_type = (mime_type != NULL ? strlen(mime_type) > 0 : false);

  for (int i = 0; i < argc; ++i) {
    // We should only look for activex installation if the mime type passed in
    // is not valid. In any case this code will be taken out when we remove
    // the activex shim.
    if (!valid_mime_type && LowerCaseEqualsASCII(argn[i], "classid") &&
        activex_shim::GetClsidFromClassidAttribute(argv[i],
                                                   raw_activex_clsid)) {
      *is_activex = true;
      *activex_clsid = std::string("{") + *raw_activex_clsid + "}";
    }
    if (LowerCaseEqualsASCII(argn[i], "codebase")) {
      *activex_codebase = ResolveURL(instance, argv[i]);
      size_t pos = activex_codebase->find('#');
      if (pos != std::string::npos)
        *plugin_download_url = activex_codebase->substr(0, pos);
      else
        *plugin_download_url = *activex_codebase;
    }
  }

  if (!*is_activex) {
    if (!valid_mime_type || !instance) {
      DLOG(WARNING) << __FUNCTION__ << " Invalid parameters passed in";
      NOTREACHED();
      return false;
    }

    if (!webkit_glue::GetPluginFinderURL(plugin_finder_url)) {
      NOTREACHED();
      DLOG(WARNING) << __FUNCTION__ << " Failed to get the plugin finder URL";
      return false;
    }

    DLOG(INFO) << "Plugin finder URL is " << plugin_finder_url->c_str();
  }

  return true;
}

void PluginInstallerImpl::NotifyPluginStatus(int status) {
  default_plugin::g_browser->getvalue(
    instance_,
    static_cast<NPNVariable>(
        default_plugin::kMissingPluginStatusStart + status),
    NULL);
}
