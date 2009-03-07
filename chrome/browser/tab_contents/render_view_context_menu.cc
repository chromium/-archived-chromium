// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_clipboard_writer.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/views/options/fonts_languages_window_view.h"
#include "chrome/browser/views/page_info_window.h"
#include "chrome/common/win_util.h"
#endif

RenderViewContextMenu::RenderViewContextMenu(
    WebContents* web_contents,
    const ContextMenuParams& params)
    : params_(params),
      source_web_contents_(web_contents),
      profile_(web_contents->profile()) {
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenu::InitMenu(ContextNode node) {
  if (node.type & ContextNode::PAGE)
    AppendPageItems();
  if (node.type & ContextNode::FRAME)
    AppendFrameItems();
  if (node.type & ContextNode::LINK)
    AppendLinkItems();

  if (node.type & ContextNode::IMAGE) {
    if (node.type & ContextNode::LINK)
      AppendSeparator();
    AppendImageItems();
  }

  if (node.type & ContextNode::EDITABLE)
    AppendEditableItems();
  else if (node.type & ContextNode::SELECTION ||
           node.type & ContextNode::LINK)
    AppendCopyItem();

  if (node.type & ContextNode::SELECTION)
    AppendSearchProvider();
  AppendSeparator();
  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendDeveloperItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendLinkItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVELINKAS);

  if (params_.link_url.SchemeIs(chrome::kMailToScheme)) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION,
                   l10n_util::GetString(IDS_CONTENT_CONTEXT_COPYEMAILADDRESS));
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  }
}

void RenderViewContextMenu::AppendImageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendPageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_RELOAD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PRINT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendCopyItem() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(profile_);
  const TemplateURL* const default_provider =
      profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
  if (default_provider != NULL) {
    std::wstring label(l10n_util::GetStringF(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                       default_provider->short_name(),
                       l10n_util::TruncateString(params_.selection_text, 50)));
    AppendMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR, label);
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    AppendMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                   params_.dictionary_suggestions[i]);
  }
  if (params_.dictionary_suggestions.size() > 0)
    AppendSeparator();

  // If word is misspelled, give option for "Add to dictionary"
  if (!params_.misspelled_word.empty()) {
    if (params_.dictionary_suggestions.size() == 0) {
      AppendMenuItem(0,
          l10n_util::GetString(IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    AppendMenuItem(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY);
    AppendSeparator();
  }

  AppendMenuItem(IDS_CONTENT_CONTEXT_UNDO);
  AppendMenuItem(IDS_CONTENT_CONTEXT_REDO);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_CUT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PASTE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_DELETE);
  AppendSeparator();

  // Add Spell Check options sub menu.
  StartSubMenu(IDC_SPELLCHECK_MENU,
      l10n_util::GetString(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU));

  // Add Spell Check languages to sub menu.
  SpellChecker::Languages display_languages;
  SpellChecker::GetSpellCheckLanguagesToDisplayInContextMenu(profile_,
      &display_languages);
  DCHECK(display_languages.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::wstring app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < display_languages.size(); ++i) {
    std::wstring local_language(l10n_util::GetLocalName(
        display_languages[i], app_locale, true));
    AppendRadioMenuItem(IDC_SPELLCHECK_LANGUAGES_FIRST + i, local_language);
  }

  // Add item in the sub menu to pop up the fonts and languages options menu.
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add 'Check the spelling of this field' item in the sub menu.
  AppendCheckboxMenuItem(
      IDC_CHECK_SPELLING_OF_THIS_FIELD,
      l10n_util::GetString(IDS_CONTENT_CONTEXT_CHECK_SPELLING_OF_THIS_FIELD));

  FinishSubMenu();

  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SELECTALL);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsItemCommandEnabled(int id) const {
  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return true;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_BACK:
      return source_web_contents_->controller()->CanGoBack();

    case IDS_CONTENT_CONTEXT_FORWARD:
      return source_web_contents_->controller()->CanGoForward();

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      return IsDevCommandEnabled(id);

    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVELINKAS:
      return params_.link_url.is_valid() &&
             URLRequest::IsHandledURL(params_.link_url);

    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
      return params_.image_url.is_valid() &&
             URLRequest::IsHandledURL(params_.image_url);

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      // The images shown in the most visited thumbnails do not currently open
      // in a new tab as they should. Disabling this context menu option for
      // now, as a quick hack, before we resolve this issue (Issue = 2608).
      // TODO (sidchat): Enable this option once this issue is resolved.
      if (params_.image_url.scheme() == "chrome-ui")
        return false;
      return true;

    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.image_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      return SavePackage::IsSavableURL(source_web_contents_->GetURL());

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      return params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & ContextNode::CAN_UNDO);

    case IDS_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & ContextNode::CAN_REDO);

    case IDS_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & ContextNode::CAN_CUT);

    case IDS_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & ContextNode::CAN_COPY);

    case IDS_CONTENT_CONTEXT_PASTE:
      return !!(params_.edit_flags & ContextNode::CAN_PASTE);

    case IDS_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & ContextNode::CAN_DELETE);

    case IDS_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & ContextNode::CAN_SELECT_ALL);

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !source_web_contents_->profile()->IsOffTheRecord() &&
             params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      return !source_web_contents_->profile()->IsOffTheRecord() &&
             params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      return !params_.misspelled_word.empty();

    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
      return (source_web_contents_->controller()->GetActiveEntry() != NULL);

    case IDS_CONTENT_CONTEXT_RELOAD:
    case IDS_CONTENT_CONTEXT_COPYIMAGE:
    case IDS_CONTENT_CONTEXT_PRINT:
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
    case IDC_SPELLCHECK_MENU:
    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
      return true;

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
    case IDS_CONTENT_CONTEXT_PRINTFRAME:
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      return false;
  }
}

bool RenderViewContextMenu::ItemIsChecked(int id) const {
  // Check box for 'Check the Spelling of this field'.
  if (id == IDC_CHECK_SPELLING_OF_THIS_FIELD)
    return params_.spellcheck_enabled;

  // Don't bother getting the display language vector if this isn't a spellcheck
  // language.
  if ((id < IDC_SPELLCHECK_LANGUAGES_FIRST) ||
      (id >= IDC_SPELLCHECK_LANGUAGES_LAST))
    return false;

  SpellChecker::Languages display_languages;
  return SpellChecker::GetSpellCheckLanguagesToDisplayInContextMenu(
      source_web_contents_->profile(), &display_languages) ==
      (id - IDC_SPELLCHECK_LANGUAGES_FIRST);
}

void RenderViewContextMenu::ExecuteItemCommand(int id) {
  // Check to see if one of the spell check language ids have been clicked.
  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    const size_t language_number = id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    SpellChecker::Languages display_languages;
    SpellChecker::GetSpellCheckLanguagesToDisplayInContextMenu(
        source_web_contents_->profile(), &display_languages);
    if (language_number < display_languages.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
          source_web_contents_->profile()->GetPrefs(), NULL);
      dictionary_language.SetValue(display_languages[language_number]);
    }

    return;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURL(params_.link_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(params_.link_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    // TODO(paulg): Prompt the user for file name when saving links and images.
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url =
          (id == IDS_CONTENT_CONTEXT_SAVELINKAS ? params_.link_url :
                                                  params_.image_url);
      DownloadManager* dlm =
          source_web_contents_->profile()->GetDownloadManager();
      dlm->DownloadUrl(url, referrer, source_web_contents_);
      break;
    }

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.link_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      WriteURLToClipboard(params_.image_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      OpenURL(params_.image_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_BACK:
      source_web_contents_->controller()->GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      source_web_contents_->controller()->GoForward();
      break;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      source_web_contents_->OnSavePage();
      break;

    case IDS_CONTENT_CONTEXT_RELOAD:
      source_web_contents_->controller()->Reload(true);
      break;

    case IDS_CONTENT_CONTEXT_PRINT:
      source_web_contents_->PrintPreview();
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
      OpenURL(GURL("view-source:" + params_.page_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO: {
#if defined(OS_WIN)
      NavigationEntry* nav_entry =
          source_web_contents_->controller()->GetActiveEntry();
      PageInfoWindow::CreatePageInfo(
          source_web_contents_->profile(),
          nav_entry,
          source_web_contents_->GetContentNativeView(),
          PageInfoWindow::SECURITY);
#else
     // TODO(port): port PageInfoWindow.
     NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_VIEWPAGEINFO";
#endif
      break;
    }

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
      OpenURL(params_.frame_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      OpenURL(params_.frame_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      OpenURL(params_.frame_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
#if defined(OS_WIN)
      win_util::MessageBox(NULL, L"Context Menu Action", L"Save Frame As",
                           MB_OK);
#else
      // TODO(port): message box equivalent
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_SAVEFRAMEAS";
#endif
      break;

    case IDS_CONTENT_CONTEXT_PRINTFRAME:
#if defined(OS_WIN)
      win_util::MessageBox(NULL, L"Context Menu Action", L"Print Frame",
                           MB_OK);
#else
      // TODO(port): message box equivalent
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_PRINTFRAME";
#endif
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      OpenURL(GURL("view-source:" + params_.frame_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO: {
      // Deserialize the SSL info.
      NavigationEntry::SSLStatus ssl;
      if (!params_.security_info.empty()) {
        int cert_id, cert_status, security_bits;
        SSLManager::DeserializeSecurityInfo(params_.security_info,
                                            &cert_id,
                                            &cert_status,
                                            &security_bits);
        ssl.set_cert_id(cert_id);
        ssl.set_cert_status(cert_status);
        ssl.set_security_bits(security_bits);
      }
      PageInfoWindow::CreateFrameInfo(
          source_web_contents_->profile(),
          params_.frame_url,
          ssl,
          source_web_contents_->GetContentNativeView(),
          PageInfoWindow::SECURITY);
      break;
    }

    case IDS_CONTENT_CONTEXT_UNDO:
      source_web_contents_->render_view_host()->Undo();
      break;

    case IDS_CONTENT_CONTEXT_REDO:
      source_web_contents_->render_view_host()->Redo();
      break;

    case IDS_CONTENT_CONTEXT_CUT:
      source_web_contents_->render_view_host()->Cut();
      break;

    case IDS_CONTENT_CONTEXT_COPY:
      source_web_contents_->render_view_host()->Copy();
      break;

    case IDS_CONTENT_CONTEXT_PASTE:
      source_web_contents_->render_view_host()->Paste();
      break;

    case IDS_CONTENT_CONTEXT_DELETE:
      source_web_contents_->render_view_host()->Delete();
      break;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->render_view_host()->SelectAll();
      break;

    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR: {
      const TemplateURL* const default_provider = source_web_contents_->
          profile()->GetTemplateURLModel()->GetDefaultSearchProvider();
      DCHECK(default_provider);  // The context menu should not contain this
                                 // item when there is no provider.
      const TemplateURLRef* const search_url = default_provider->url();
      DCHECK(search_url->SupportsReplacement());
      OpenURL(GURL(search_url->ReplaceSearchTerms(*default_provider,
          params_.selection_text, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
          std::wstring())), NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;
    }

    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
      source_web_contents_->render_view_host()->Replace(
          params_.dictionary_suggestions[id - IDC_SPELLCHECK_SUGGESTION_0]);
      break;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      source_web_contents_->render_view_host()->ToggleSpellCheck();
      break;
    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      source_web_contents_->render_view_host()->AddToDictionary(
          params_.misspelled_word);
      break;

    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS: {
#if defined(OS_WIN)
      FontsLanguagesWindowView* window_ = new FontsLanguagesWindowView(
          source_web_contents_->profile());
      views::Window::CreateChromeWindow(
          source_web_contents_->GetContentNativeView(),
          gfx::Rect(), window_)->Show();
      window_->SelectLanguagesTab();
#else
      // TODO(port): need views::Window
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS";
#endif
      break;
    }

    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      break;
  }
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAlwaysEnableDevTools))
    return true;

  NavigationEntry *active_entry =
      source_web_contents_->controller()->GetActiveEntry();
  if (!active_entry)
    return false;

  // Don't inspect HTML dialogs.
  if (source_web_contents_->type() == TAB_CONTENTS_HTML_DIALOG)
    return false;

  // Don't inspect view source.
  if (active_entry->IsViewSourceMode())
    return false;

  // Don't inspect inspector, new tab UI, etc.
  if (active_entry->url().SchemeIs(chrome::kChromeUIScheme))
    return false;

  // Don't inspect about:network, about:memory, etc.
  // However, we do want to inspect about:blank, which is often
  // used by ordinary web pages.
  if (active_entry->display_url().SchemeIs(chrome::kAboutScheme) &&
      !LowerCaseEqualsASCII(active_entry->display_url().path(), "blank"))
    return false;

  // Don't enable the web inspector if JavaScript is disabled
  if (id == IDS_CONTENT_CONTEXT_INSPECTELEMENT) {
    PrefService* prefs = source_web_contents_->profile()->GetPrefs();
    if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line.HasSwitch(switches::kDisableJavaScript))
      return false;
  }

  return true;
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::OpenURL(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  source_web_contents_->OpenURL(url, GURL(), disposition, transition);
}

void RenderViewContextMenu::CopyImageAt(int x, int y) {
  source_web_contents_->render_view_host()->CopyImageAt(x, y);
}

void RenderViewContextMenu::Inspect(int x, int y) {
  source_web_contents_->render_view_host()->InspectElementAt(x, y);
}

void RenderViewContextMenu::WriteTextToClipboard(
    const string16& text) {
  ClipboardService* clipboard = g_browser_process->clipboard_service();

  if (!clipboard)
    return;

  ScopedClipboardWriter scw(clipboard);
  scw.WriteText(text);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  if (url.SchemeIs(chrome::kMailToScheme))
    WriteTextToClipboard(UTF8ToUTF16(url.path()));
  else
    WriteTextToClipboard(UTF8ToUTF16(url.spec()));
}
