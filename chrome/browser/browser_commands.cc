// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"

#include <commdlg.h>
#include <shellapi.h>

#include "base/file_version_info.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/views/download_tab_view.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/interstitial_page_delegate.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/tab_restore_service.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/about_chrome_view.h"
#include "chrome/browser/views/bug_report_view.h"
#include "chrome/browser/views/clear_browsing_data.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/win_util.h"
#include "net/base/net_util.h"

#include "generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// Event handling and updating
//
void Browser::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  controller_.UpdateCommandEnabled(IDC_STOP, true);
  controller_.UpdateCommandEnabled(IDC_RELOAD, true);
  controller_.UpdateCommandEnabled(IDC_HOME,
                                   GetType() == BrowserType::TABBED_BROWSER);
  controller_.UpdateCommandEnabled(IDC_GO, true);
  controller_.UpdateCommandEnabled(IDC_NEWTAB, true);
  controller_.UpdateCommandEnabled(IDC_CLOSETAB, !IsApplication());
  controller_.UpdateCommandEnabled(IDC_NEWWINDOW, true);
  controller_.UpdateCommandEnabled(IDC_CLOSEWINDOW, true);
  controller_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, true);
  controller_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, true);
  controller_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, true);
  controller_.UpdateCommandEnabled(IDC_STAR, true);
  controller_.UpdateCommandEnabled(IDC_OPENURL, true);
  controller_.UpdateCommandEnabled(IDC_SHOWALLTABS_NEXT, true);
  controller_.UpdateCommandEnabled(IDC_SHOWALLTABS_PREV, true);
  controller_.UpdateCommandEnabled(IDC_SHOWALLTABS, true);
  controller_.UpdateCommandEnabled(IDC_CUT, true);
  controller_.UpdateCommandEnabled(IDC_COPY, true);
  controller_.UpdateCommandEnabled(IDC_PASTE, true);
  controller_.UpdateCommandEnabled(IDC_FIND, true);
  controller_.UpdateCommandEnabled(IDC_FIND_NEXT, true);
  controller_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, true);
  controller_.UpdateCommandEnabled(IDS_COMMANDS_REPORTBUG, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_JS_CONSOLE, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_PREV_TAB, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_0, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_1, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_2, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_3, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_4, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_5, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_6, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_7, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, true);
  controller_.UpdateCommandEnabled(IDC_VIEWSOURCE, true);
  controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUT, false);
  controller_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, true);
  controller_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  controller_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);
  controller_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, true);
  controller_.UpdateCommandEnabled(IDC_OPENFILE, true);
  controller_.UpdateCommandEnabled(IDC_TASKMANAGER, true);
  controller_.UpdateCommandEnabled(IDC_CLOSEPOPUPS, true);
  controller_.UpdateCommandEnabled(IDC_PRINT, true);
  controller_.UpdateCommandEnabled(IDC_COPY_URL, true);
  controller_.UpdateCommandEnabled(IDC_DUPLICATE, true);
  controller_.UpdateCommandEnabled(IDC_GOOFFTHERECORD, true);
  controller_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, true);
  controller_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);
  controller_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  controller_.UpdateCommandEnabled(IDC_ABOUT, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_BOOKMARKS_BAR, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88591, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_GB2312, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_BIG5HKSCS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_BIG5, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_THAI, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOREAN, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_SHIFTJIS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO2022JP, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_EUCJP, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885915, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_MACINTOSH, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88592, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1250, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88595, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1251, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOI8R, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOI8U, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88597, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1253, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88594, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885913, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1257, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88593, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885910, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885914, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885916, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88599, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1254, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88596, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1256, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88598, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1255, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1258, true);
  controller_.UpdateCommandEnabled(IDC_OPTIONS, true);
  controller_.UpdateCommandEnabled(IDC_CLOSE_WEB_APP,
                                   GetType() != BrowserType::TABBED_BROWSER);
  controller_.UpdateCommandEnabled(IDC_SHOW_AS_TAB,
                                   GetType() == BrowserType::BROWSER);
  controller_.UpdateCommandEnabled(
      IDC_RESTORE_TAB, (!profile_->IsOffTheRecord() &&
                        GetType() == BrowserType::TABBED_BROWSER));
  controller_.UpdateCommandEnabled(IDC_EXIT, true);
  // the debugger doesn't work in single process mode
  controller_.UpdateCommandEnabled(IDC_DEBUGGER,
      !RenderProcessHost::run_renderer_in_process());
  controller_.UpdateCommandEnabled(IDC_DEVELOPER, true);
  controller_.UpdateCommandEnabled(IDC_HELPMENU, true);
}

bool Browser::SupportsCommand(int id) const {
  return controller_.SupportsCommand(id);
}

bool Browser::ExecuteWindowsAppCommand(int app_command) {
  switch (app_command) {
    case APPCOMMAND_BROWSER_BACKWARD:
      controller_.ExecuteCommand(IDC_BACK);
      return true;
    case APPCOMMAND_BROWSER_FORWARD:
      controller_.ExecuteCommand(IDC_FORWARD);
      return true;
    case APPCOMMAND_BROWSER_REFRESH:
      controller_.ExecuteCommand(IDC_RELOAD);
      return true;
    case APPCOMMAND_BROWSER_HOME:
      controller_.ExecuteCommand(IDC_HOME);
      return true;
    case APPCOMMAND_BROWSER_STOP:
      controller_.ExecuteCommand(IDC_STOP);
      return true;
    case APPCOMMAND_BROWSER_SEARCH:
      controller_.ExecuteCommand(IDC_FOCUS_SEARCH);
      return true;
    case APPCOMMAND_CLOSE:
      controller_.ExecuteCommand(IDC_CLOSETAB);
      return true;
    case APPCOMMAND_NEW:
      controller_.ExecuteCommand(IDC_NEWTAB);
      return true;
    case APPCOMMAND_OPEN:
      controller_.ExecuteCommand(IDC_OPENFILE);
      return true;
    case APPCOMMAND_PRINT:
      controller_.ExecuteCommand(IDC_PRINT);
      return true;

    // TODO(pkasting): http://b/1113069 Handle all these.
    case APPCOMMAND_HELP:
    case APPCOMMAND_SAVE:
    case APPCOMMAND_UNDO:
    case APPCOMMAND_REDO:
    case APPCOMMAND_COPY:
    case APPCOMMAND_CUT:
    case APPCOMMAND_PASTE:
    case APPCOMMAND_SPELL_CHECK:
    default:
      return false;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CommandController implementation
//

bool Browser::GetContextualLabel(int id, std::wstring* out) const {
  return false;
}

bool Browser::IsCommandEnabled(int id) const {
  switch (id) {
    case IDC_BACK: {
      NavigationController* nc = GetSelectedNavigationController();
      return nc ? nc->CanGoBack() : false;
    }
    case IDC_FORWARD: {
      NavigationController* nc = GetSelectedNavigationController();
      return nc ? nc->CanGoForward() : false;
    }
    case IDC_STOP: {
      TabContents* current_tab = GetSelectedTabContents();
      return (current_tab && current_tab->is_loading());
    }
    case IDC_CLOSETAB: {
      return !IsApplication();
    }
    default:
      return controller_.IsCommandEnabled(id);
  }
}

void Browser::ExecuteCommand(int id) {
  if (!IsCommandEnabled(id)) {
    NOTREACHED() << id;
    return;
  }
  // This might happen during QMU testing.
  if (!GetSelectedTabContents())
    return;

  switch (id) {
    case IDC_BACK:
      UserMetrics::RecordAction(L"Back", profile_);
      GoBack();
      break;

    case IDC_FORWARD:
      UserMetrics::RecordAction(L"Forward", profile_);
      GoForward();
      break;

    case IDC_RELOAD:
      UserMetrics::RecordAction(L"Reload", profile_);
      Reload();
      break;

    case IDC_HOME:
      UserMetrics::RecordAction(L"Home", profile_);
      Home();
      break;

    case IDC_STOP:
      UserMetrics::RecordAction(L"Stop", profile_);
      Stop();
      break;

    case IDC_GO:
      UserMetrics::RecordAction(L"Go", profile_);
      {
        LocationBarView* lbv = GetLocationBarView();
        if (lbv)
          lbv->location_entry()->model()->AcceptInput(CURRENT_TAB, false);
      }
      break;

    case IDC_NEWTAB:
      UserMetrics::RecordAction(L"NewTab", profile_);
      if (GetType() == BrowserType::TABBED_BROWSER) {
        AddBlankTab(true);
      } else {
        Browser* b = GetOrCreateTabbedBrowser();
        DCHECK(b);
        b->Show();
        b->MoveToFront(true);
        b->AddBlankTab(true);
      }
      break;

    case IDC_CLOSE_WEB_APP:
      UserMetrics::RecordAction(L"CloseWebApp", profile_);
      tabstrip_model_.CloseTabContentsAt(tabstrip_model_.selected_index());
      break;

    case IDC_CLOSETAB:
      UserMetrics::RecordAction(L"CloseTab_Accelerator", profile_);
      tabstrip_model_.CloseTabContentsAt(tabstrip_model_.selected_index());
      break;

    case IDC_NEWWINDOW:
      UserMetrics::RecordAction(L"NewWindow", profile_);
      Browser::OpenNewBrowserWindow(profile_->GetOriginalProfile(),
                                    SW_SHOWNORMAL);
      break;

    case IDC_CLOSEWINDOW:
      UserMetrics::RecordAction(L"CloseWindow", profile_);
      window_->Close();
      break;

    case IDC_FOCUS_LOCATION:
      UserMetrics::RecordAction(L"FocusLocation", profile_);
      {
        LocationBarView* lbv = GetLocationBarView();
        if (lbv) {
          AutocompleteEditView* aev = lbv->location_entry();
          aev->SetFocus();
          aev->SelectAll(true);
        }
      }
      break;

    case IDC_FOCUS_SEARCH:
      UserMetrics::RecordAction(L"FocusSearch", profile_);
      {
        LocationBarView* lbv = GetLocationBarView();
        if (lbv) {
          AutocompleteEditView* aev = lbv->location_entry();
          aev->model()->SetUserText(L"?");
          aev->SetFocus();
        }
      }
      break;

    case IDC_FOCUS_TOOLBAR:
      UserMetrics::RecordAction(L"FocusToolbar", profile_);
      {
        window_->FocusToolbar();
      }
      break;

    case IDC_STAR:
      UserMetrics::RecordAction(L"Star", profile_);
      StarCurrentTabContents();
      break;

    case IDC_OPENURL:
      UserMetrics::RecordAction(L"LoadURL", profile_);
      {
        LocationBarView* lbv = GetLocationBarView();
        if (lbv) {
          OpenURL(GURL(lbv->location_input()), lbv->disposition(),
                  lbv->transition());
        } else {
          OpenURL(GURL(), CURRENT_TAB, PageTransition::TYPED);
        }
      }
      break;

    // TODO(devint): http://b/issue?id=1117225 Cut, Copy, and Paste are always
    // enabled in the page menu regardless of whether the command will do
    // anything. When someone selects the menu item, we just act as if they hit
    // the keyboard shortcut for the command by sending the associated key press
    // to windows. The real fix to this bug is to disable the commands when they
    // won't do anything. We'll need something like an overall clipboard command
    // manager to do that.
    case IDC_CUT:
      UserMetrics::RecordAction(L"Cut", profile_);
      ui_controls::SendKeyPress(L'X', true, false, false);
      break;

    case IDC_COPY:
      UserMetrics::RecordAction(L"Copy", profile_);
      ui_controls::SendKeyPress(L'C', true, false, false);
      break;

    case IDC_PASTE:
      UserMetrics::RecordAction(L"Paste", profile_);
      ui_controls::SendKeyPress(L'V', true, false, false);
      break;

    case IDC_FIND:
      UserMetrics::RecordAction(L"Find", profile_);
      OpenFindInPageWindow();
      break;

    case IDC_FIND_NEXT:
      UserMetrics::RecordAction(L"FindNext", profile_);
      if (!AdvanceFindSelection(true))
        OpenFindInPageWindow();
      break;

    case IDC_FIND_PREVIOUS:
      UserMetrics::RecordAction(L"FindPrevious", profile_);
      if (!AdvanceFindSelection(false))
        OpenFindInPageWindow();
      break;

    case IDS_COMMANDS_REPORTBUG:
      UserMetrics::RecordAction(L"ReportBug", profile_);
      OpenBugReportDialog();
      break;

    case IDC_SELECT_NEXT_TAB:
      UserMetrics::RecordAction(L"SelectNextTab", profile_);
      tabstrip_model_.SelectNextTab();
      break;

    case IDC_SELECT_PREV_TAB:
      UserMetrics::RecordAction(L"SelectPrevTab", profile_);
      tabstrip_model_.SelectPreviousTab();
      break;

    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7: {
      int new_index = id - IDC_SELECT_TAB_0;
      if (new_index < tab_count()) {
        tabstrip_model_.SelectTabContentsAt(new_index, true);
        UserMetrics::RecordAction(L"SelectNumberedTab", profile_);
      }
      break;
    }

    case IDC_SELECT_LAST_TAB:
      UserMetrics::RecordAction(L"SelectLastTab", profile_);
      tabstrip_model_.SelectLastTab();
      break;

    case IDC_VIEWSOURCE: {
      UserMetrics::RecordAction(L"ViewSource", profile_);

      TabContents* current_tab = GetSelectedTabContents();
      NavigationEntry* entry =
          current_tab->controller()->GetLastCommittedEntry();
      if (entry) {
        GURL url("view-source:" + entry->url().spec());
        AddTabWithURL(url, PageTransition::LINK, true, NULL);
      }
      break;
    }

    case IDC_SHOW_JS_CONSOLE: {
      UserMetrics::RecordAction(L"ShowJSConsole", profile_);
      TabContents* current_tab = GetSelectedTabContents();
      if (current_tab && current_tab->AsWebContents()) {
        WebContents* wc = current_tab->AsWebContents();
        wc->render_view_host()->ShowJavaScriptConsole();
      }
      break;
    }

    case IDC_CREATE_SHORTCUT: {
      UserMetrics::RecordAction(L"CreateShortcut", profile_);
      WebContents* contents = this->GetSelectedTabContents()->AsWebContents();
      if (contents)
        contents->CreateShortcut();
      break;
    }

    case IDC_GOOFFTHERECORD: {
      Browser::OpenNewBrowserWindow(profile_->GetOffTheRecordProfile(),
                                    SW_SHOWNORMAL);
      break;
    }

    case IDC_VIEW_PASSWORDS: {
      PasswordManagerView::Show(profile_);
      break;
    }

    case IDC_IMPORT_SETTINGS: {
      UserMetrics::RecordAction(L"Import_ShowDlg", profile_);
      OpenImportSettingsDialog();
      break;
    }

    case IDC_CLEAR_BROWSING_DATA: {
      UserMetrics::RecordAction(L"ClearBrowsingData_ShowDlg", profile_);
      OpenClearBrowsingDataDialog();
      break;
    }

    case IDC_ABOUT: {
      UserMetrics::RecordAction(L"AboutChrome", profile_);
      ChromeViews::Window::CreateChromeWindow(
          GetTopLevelHWND(),
          gfx::Rect(),
          new AboutChromeView(profile_))->Show();
      break;
    }

    case IDC_EDIT_SEARCH_ENGINES: {
      UserMetrics::RecordAction(L"EditSearchEngines", profile_);
      OpenKeywordEditor();
      break;
    }

    case IDC_ZOOM_PLUS: {
      UserMetrics::RecordAction(L"ZoomPlus", profile_);
      TabContents* current_tab = GetSelectedTabContents();
      if (current_tab->AsWebContents()) {
        current_tab->AsWebContents()->render_view_host()->AlterTextSize(
          text_zoom::TEXT_LARGER);
      }
      break;
    }

    case IDC_ZOOM_MINUS: {
      UserMetrics::RecordAction(L"ZoomMinus", profile_);
      TabContents* current_tab = GetSelectedTabContents();
      if (current_tab->AsWebContents()) {
        current_tab->AsWebContents()->render_view_host()->AlterTextSize(
          text_zoom::TEXT_SMALLER);
      }
      break;
    }

    case IDC_ZOOM_NORMAL: {
      UserMetrics::RecordAction(L"ZoomNormal", profile_);
      TabContents* current_tab = GetSelectedTabContents();
      if (current_tab->AsWebContents()) {
        current_tab->AsWebContents()->render_view_host()->AlterTextSize(
          text_zoom::TEXT_STANDARD);
      }
      break;
    }

    case IDC_OPENFILE: {
      UserMetrics::RecordAction(L"OpenFile", profile_);
      if (!select_file_dialog_.get())
        select_file_dialog_ = SelectFileDialog::Create(this);
      select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                      L"", L"", GetTopLevelHWND(), NULL);
      break;
    }

    case IDC_TASKMANAGER:
      UserMetrics::RecordAction(L"TaskManager", profile_);
      TaskManager::Open();
      break;

    case IDC_CLOSEPOPUPS:
      UserMetrics::RecordAction(L"CloseAllSuppressedPopups", profile_);
      GetSelectedTabContents()->CloseAllSuppressedPopups();
      break;

    case IDC_PRINT: {
      UserMetrics::RecordAction(L"PrintPreview", profile_);
      WebContents* const web_contents =
          GetSelectedTabContents()->AsWebContents();
      DCHECK(web_contents);
      web_contents->PrintPreview();
      break;
    }

    case IDC_COPY_URL:
      UserMetrics::RecordAction(L"CopyURLToClipBoard", profile_);
      CopyCurrentURLToClipBoard();
      break;

    case IDC_SAVEPAGE: {
      UserMetrics::RecordAction(L"SavePage", profile_);
      TabContents* current_tab = GetSelectedTabContents();
      if (current_tab) {
        WebContents* web_tab = current_tab->AsWebContents();
        DCHECK(web_tab);
        web_tab->OnSavePage();
      }
      break;
    }

    case IDC_ENCODING_AUTO_DETECT: {
      UserMetrics::RecordAction(L"AutoDetectChange", profile_);
      encoding_auto_detect_.SetValue(!encoding_auto_detect_.GetValue());
      // Reload the page so we can try to auto-detect the charset.
      Reload();
      break;
    }

    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GB2312:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5HKSCS:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_ISO88599:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258: {
      UserMetrics::RecordAction(L"OverrideEncoding", profile_);
      const std::wstring cur_encoding_name =
          CharacterEncoding::GetCanonicalEncodingNameByCommandId(id);
      TabContents* current_tab = GetSelectedTabContents();
      if (!cur_encoding_name.empty() && current_tab)
        current_tab->set_encoding(cur_encoding_name);
      // Update user recently selected encoding list.
      std::wstring new_selected_encoding_list;
      if (CharacterEncoding::UpdateRecentlySelectdEncoding(
              profile_->GetPrefs()->GetString(prefs::kRecentlySelectedEncoding),
              id,
              &new_selected_encoding_list))
        profile_->GetPrefs()->SetString(prefs::kRecentlySelectedEncoding,
                                        new_selected_encoding_list);
       break;
    }

    case IDC_DUPLICATE:
      UserMetrics::RecordAction(L"Duplicate", profile_);
      DuplicateContentsAt(selected_index());
      break;

    case IDC_SHOW_BOOKMARKS_BAR: {
      UserMetrics::RecordAction(L"ShowBookmarksBar", profile_);

      // Invert the current pref.
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(prefs::kShowBookmarkBar,
          !prefs->GetBoolean(prefs::kShowBookmarkBar));
      prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());

      // And notify the notification service.
      Source<Profile> source(profile_);
      NotificationService::current()->Notify(
          NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED, source,
          NotificationService::NoDetails());
      break;
    }

    case IDC_SHOW_HISTORY:
      UserMetrics::RecordAction(L"ShowHistory", profile_);
      ShowNativeUI(HistoryTabUI::GetURL());
      break;

    case IDC_SHOW_DOWNLOADS:
      UserMetrics::RecordAction(L"ShowDownloads", profile_);
      ShowNativeUI(DownloadTabUI::GetURL());
      break;

    case IDC_OPTIONS:
      UserMetrics::RecordAction(L"ShowOptions", profile_);
      ShowOptionsWindow(OPTIONS_PAGE_DEFAULT, OPTIONS_GROUP_NONE, profile_);
      break;

    case IDC_DEBUGGER:
      UserMetrics::RecordAction(L"Debugger", profile_);
      OpenDebuggerWindow();
      break;

    case IDC_SHOW_AS_TAB:
      UserMetrics::RecordAction(L"ShowAsTab", profile_);
      ConvertToTabbedBrowser();
      break;

    case IDC_RESTORE_TAB: {
      UserMetrics::RecordAction(L"RestoreTab", profile_);
      TabRestoreService* service = profile_->GetTabRestoreService();
      if (!service)
        break;

      const TabRestoreService::Tabs& tabs = service->tabs();
      if (tabs.empty() || tabs.front().from_last_session)
        break;

      const TabRestoreService::HistoricalTab& tab = tabs.front();
      AddRestoredTab(tab.navigations, tab.current_navigation_index, true);
      service->RemoveHistoricalTabById(tab.id);
      break;
    }

    case IDC_EXIT:
      BrowserList::CloseAllBrowsers(true);
      break;

    case IDC_HELPMENU: {
      GURL help_url(l10n_util::GetString(IDS_HELP_CONTENT_URL));
      AddTabWithURL(help_url, PageTransition::AUTO_BOOKMARK, true, NULL);
      break;
    }

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id <<
        " from window " << GetTopLevelHWND();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Command Execution
//

void Browser::GoBack() {
  // If we are showing an interstitial, just hide it.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab) {
    WebContents* web_contents = current_tab->AsWebContents();
    // If we are showing an interstitial page, we don't need to navigate back
    // to the previous page as it is already available in a render view host
    // of the WebContents.  This makes the back more snappy and avoids potential
    // reloading of POST pages.
    if (web_contents && web_contents->showing_interstitial_page()) {
      // Let the delegate decide (if any) if it wants to handle the back
      // navigation (it may have extra things to do).
     if (web_contents->interstitial_page_delegate() &&
         web_contents->interstitial_page_delegate()->GoBack()) {
       return;
     }
     // TODO(jcampan): #1283764 once we refactored and only use the
     //                interstitial delegate, the code below should go away.
      NavigationEntry* prev_nav_entry = web_contents->controller()->
          GetEntryAtOffset(-1);
      DCHECK(prev_nav_entry);
      // We do a normal back if:
      // - the page is not a WebContents, its TabContents might have to be
      //   recreated.
      // - we have not yet visited that navigation entry (typically session
      //   restore), in which case the page is not already available.
      if (prev_nav_entry->tab_type() == TAB_CONTENTS_WEB &&
          !prev_nav_entry->restored()) {
        // It is the job of the code that shows the interstitial to listen for
        // notifications of the interstitial getting hidden and appropriately
        // dealing with the navigation entries.
        web_contents->HideInterstitialPage(false, false);
        return;
      }
    }
  }
  NavigationController* nc = GetSelectedNavigationController();
  if (nc && nc->CanGoBack())
    nc->GoBack();
}

void Browser::GoForward() {
  NavigationController* nc = GetSelectedNavigationController();
  if (nc && nc->CanGoForward())
    nc->GoForward();
}

void Browser::Stop() {
  // TODO(mpcomplete): make this more abstracted.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->AsWebContents())
    current_tab->AsWebContents()->Stop();
}

void Browser::Reload() {
  // If we are showing an interstitial, treat this as an OpenURL.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab) {
    WebContents* web_contents = current_tab->AsWebContents();
    if (web_contents && web_contents->showing_interstitial_page()) {
      NavigationEntry* entry = current_tab->controller()->GetActiveEntry();
      DCHECK(entry);  // Should exist if interstitial is showing.
      OpenURL(entry->url(), CURRENT_TAB, PageTransition::RELOAD);
      return;
    }
  }

  if (current_tab) {
    // As this is caused by a user action, give the focus to the page.
    current_tab->Focus();
    current_tab->controller()->Reload();
  }
}

void Browser::Home() {
  GURL homepage_url = GetHomePage();
  GetSelectedTabContents()->controller()->LoadURL(
      homepage_url, PageTransition::AUTO_BOOKMARK);
}

void Browser::StarCurrentTabContents() {
  TabContents* tab = GetSelectedTabContents();
  if (!tab || !tab->AsWebContents())
    return;

  WebContents* rvh = tab->AsWebContents();
  BookmarkModel* model = tab->profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  NavigationEntry* entry = rvh->controller()->GetActiveEntry();
  if (!entry)
    return;  // Can't star if there is no URL.
  const GURL& url = entry->display_url();
  if (url.is_empty() || !url.is_valid())
    return;

  if (window_->GetStarButton()) {
    if (!window_->GetStarButton()->is_bubble_showing()) {
      const bool newly_bookmarked = !model->IsBookmarked(url);
      if (newly_bookmarked) {
        model->SetURLStarred(url, entry->title(), true);
        if (!model->IsBookmarked(url)) {
          // Starring failed. This shouldn't happen.
          NOTREACHED();
          return;
        }
      }
      window_->GetStarButton()->ShowStarBubble(url, newly_bookmarked);
    }
  } else if (model->IsBookmarked(url)) {
    // If we can't find the star button and the user wanted to unstar it,
    // go ahead and unstar it without showing the bubble.
    model->SetURLStarred(url, std::wstring(), false);
  }
}

void Browser::OpenFindInPageWindow() {
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->AsWebContents())
    current_tab->AsWebContents()->OpenFindInPageWindow(*this);
}

void Browser::AdoptFindWindow(TabContents* tab_contents) {
  if (tab_contents->AsWebContents())
    tab_contents->AsWebContents()->ReparentFindWindow(GetTopLevelHWND());
}

bool Browser::AdvanceFindSelection(bool forward_direction) {
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->AsWebContents()) {
    current_tab->AsWebContents()->AdvanceFindSelection(forward_direction);
  }

  return false;
}

void Browser::OpenDebuggerWindow() {
#ifndef CHROME_DEBUGGER_DISABLED
  TabContents* current_tab = GetSelectedTabContents();
  if (!current_tab)
    return;

  if (current_tab->AsWebContents()) {
    // Only one debugger instance can exist at a time right now.
    // TODO(erikkay): need an alert, dialog, something
    // or better yet, fix the one instance limitation
    if (!DebuggerWindow::DoesDebuggerExist()) {
      debugger_window_ = new DebuggerWindow();
    }
    debugger_window_->Show(current_tab);
  }
#endif
}

void Browser::OpenKeywordEditor() {
  KeywordEditorView::Show(profile());
}

void Browser::OpenImportSettingsDialog() {
  ChromeViews::Window::CreateChromeWindow(GetTopLevelHWND(), gfx::Rect(),
                                          new ImporterView(profile_))->Show();
}

void Browser::OpenBugReportDialog() {
  // Retrieve the URL for the current tab (if any) and tell the BugReportView
  TabContents* current_tab = GetSelectedTabContents();
  if (!current_tab)
    return;

  BugReportView* bug_report_view = new BugReportView(profile_, current_tab);

  if (current_tab->controller()->GetLastCommittedEntry()) {
    if (current_tab->type() == TAB_CONTENTS_WEB) {
      // URL for the current page
      bug_report_view->SetUrl(
          current_tab->controller()->GetActiveEntry()->url());
    }
  }

  // retrieve the application version info
  std::wstring version;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    version = version_info->product_name() + L" - " +
    version_info->file_version() +
    L" (" + version_info->last_change() + L")";
  }
  bug_report_view->set_version(version);

  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins)
  std::vector<unsigned char> *screenshot_png = new std::vector<unsigned char>;
  win_util::GrabWindowSnapshot(GetTopLevelHWND(), screenshot_png);
  // the BugReportView takes ownership of the png data, and will dispose of
  // it in its destructor.
  bug_report_view->set_png_data(screenshot_png);

  // Create and show the dialog
  ChromeViews::Window::CreateChromeWindow(GetTopLevelHWND(), gfx::Rect(),
                                          bug_report_view)->Show();
}

void Browser::OpenClearBrowsingDataDialog() {
    ChromeViews::Window::CreateChromeWindow(
        GetTopLevelHWND(),
        gfx::Rect(),
        new ClearBrowsingDataView(profile_))->Show();
}

void Browser::RunSimpleFrameMenu(const CPoint& pt, HWND hwnd) {
  bool for_popup = !IsApplication();
  EncodingMenuControllerDelegate d(this, &controller_);

  // The menu's anchor point is different based on the UI layout.
  Menu::AnchorPoint anchor;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    anchor = Menu::TOPRIGHT;
  else
    anchor = Menu::TOPLEFT;

  Menu m(&d, anchor, hwnd);
  m.AppendMenuItemWithLabel(IDC_BACK,
                            l10n_util::GetString(IDS_CONTENT_CONTEXT_BACK));
  m.AppendMenuItemWithLabel(IDC_FORWARD,
                            l10n_util::GetString(
                                IDS_CONTENT_CONTEXT_FORWARD));
  m.AppendMenuItemWithLabel(IDC_RELOAD,
                            l10n_util::GetString(IDS_APP_MENU_RELOAD));
  m.AppendSeparator();
  m.AppendMenuItemWithLabel(IDC_DUPLICATE,
                            l10n_util::GetString(IDS_APP_MENU_DUPLICATE));
  m.AppendMenuItemWithLabel(IDC_COPY_URL,
                            l10n_util::GetString(IDS_APP_MENU_COPY_URL));
  if (for_popup) {
    m.AppendMenuItemWithLabel(IDC_SHOW_AS_TAB,
                              l10n_util::GetString(IDS_SHOW_AS_TAB));
  }
  m.AppendMenuItemWithLabel(IDC_NEWTAB,
                            l10n_util::GetString(IDS_APP_MENU_NEW_WEB_PAGE));
  m.AppendSeparator();
  m.AppendMenuItemWithLabel(IDC_CUT, l10n_util::GetString(IDS_CUT));
  m.AppendMenuItemWithLabel(IDC_COPY, l10n_util::GetString(IDS_COPY));
  m.AppendMenuItemWithLabel(IDC_PASTE, l10n_util::GetString(IDS_PASTE));
  m.AppendSeparator();
  m.AppendMenuItemWithLabel(IDC_FIND, l10n_util::GetString(IDS_FIND_IN_PAGE));
  m.AppendMenuItemWithLabel(IDC_SAVEPAGE, l10n_util::GetString(IDS_SAVEPAGEAS));
  m.AppendMenuItemWithLabel(IDC_PRINT, l10n_util::GetString(IDS_PRINT));
  m.AppendSeparator();
  Menu* zoom_menu = m.AppendSubMenu(IDC_ZOOM,
                                    l10n_util::GetString(IDS_ZOOM));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_PLUS,
                                     l10n_util::GetString(IDS_ZOOM_PLUS));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_NORMAL,
                                     l10n_util::GetString(IDS_ZOOM_NORMAL));
  zoom_menu->AppendMenuItemWithLabel(IDC_ZOOM_MINUS,
                                     l10n_util::GetString(IDS_ZOOM_MINUS));

  // Create encoding menu.
  Menu* encoding_menu = m.AppendSubMenu(IDC_ENCODING,
                                        l10n_util::GetString(IDS_ENCODING));
  EncodingMenuControllerDelegate::BuildEncodingMenu(profile_, encoding_menu);

  m.AppendSeparator();
  m.AppendMenuItemWithLabel(IDC_CLOSE_WEB_APP, l10n_util::GetString(IDS_CLOSE));
  m.RunMenuAt(pt.x, pt.y);
}

void Browser::CopyCurrentURLToClipBoard() {
  TabContents* tc = GetSelectedTabContents();
  DCHECK(tc);

  std::string url = tc->GetURL().spec();

  if (!::OpenClipboard(NULL)) {
    NOTREACHED();
    return;
  }

  if (::EmptyClipboard()) {
    HGLOBAL text = ::GlobalAlloc(GMEM_MOVEABLE, url.size() + 1);
    LPSTR ptr = static_cast<LPSTR>(::GlobalLock(text));
    memcpy(ptr, url.c_str(), url.size());
    ptr[url.size()] = '\0';
    ::GlobalUnlock(text);

    ::SetClipboardData(CF_TEXT, text);
  }

  if (!::CloseClipboard()) {
    NOTREACHED();
  }
}

bool Browser::CanDuplicateContentsAt(int index) {
  TabContents* contents = GetTabContentsAt(index);
  DCHECK(contents);

  NavigationController* nc = contents->controller();
  return nc ? (nc->active_contents() && nc->GetLastCommittedEntry()) : false;
}

void Browser::DuplicateContentsAt(int index) {
  TabContents* contents = GetTabContentsAt(index);
  TabContents* new_contents = NULL;
  DCHECK(contents);

  if (type_ == BrowserType::TABBED_BROWSER) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    new_contents = contents->controller()->Clone(
        GetTopLevelHWND())->active_contents();
    // If you duplicate a tab that is not selected, we need to make sure to
    // select the tab being duplicated so that DetermineInsertionIndex returns
    // the right index (if tab 5 is selected and we right-click tab 1 we want
    // the new tab to appear in index position 2, not 6).
    if (tabstrip_model_.selected_index() != index)
      tabstrip_model_.SelectTabContentsAt(index, true);
    tabstrip_model_.AddTabContents(new_contents, index + 1,
                                   PageTransition::LINK, true);
  } else {
    Browser* new_browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile(),
                                       BrowserType::APPLICATION, app_name_);

    // We need to show the browser now. Otherwise HWNDViewContainer assumes
    // the tab contents is invisible and won't size it.
    new_browser->Show();

    // The page transition below is only for the purpose of inserting the tab.
    new_contents = new_browser->AddTabWithNavigationController(
        contents->controller()->Clone(new_browser->GetTopLevelHWND()),
        PageTransition::LINK);

    new_browser->MoveToFront(true);
  }

  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(new_contents->controller());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Browser, SelectFileDialog::Listener implementation:

void Browser::FileSelected(const std::wstring& path, void* params) {
  GURL file_url = net::FilePathToFileURL(path);
  if (!file_url.is_empty())
    OpenURL(file_url, CURRENT_TAB, PageTransition::TYPED);
}

