// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "generated_resources.h"
#include "net/base/escape.h"


// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
// TODO(paulg): Change 'googleclient' to a proper client name before launch.
static const char* const kSbDiagnosticUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?site=%ls&client=googleclient";

static const char* const kSbReportPhishingUrl =
    "http://www.google.com/safebrowsing/report_error/";

static const wchar_t* const kSbDiagnosticHtml =
    L"<a href=\"\" onClick=\"sendCommand(4); return false;\">%ls</a>";

// Created on the io_thread.
SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingService* sb_service,
    SafeBrowsingService::Client* client,
    int render_process_host_id,
    int render_view_id,
    const GURL& url,
    ResourceType::Type resource_type,
    SafeBrowsingService::UrlCheckResult result)
    : sb_service_(sb_service),
      client_(client),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      url_(url),
      result_(result),
      proceed_(false),
      tab_(NULL),
      controller_(NULL),
      delete_pending_(false),
      is_main_frame_(resource_type == ResourceType::MAIN_FRAME),
      created_temporary_entry_(false) {
}

// Deleted on the io_thread.
SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {
}

void SafeBrowsingBlockingPage::DisplayBlockingPage() {
  TabContents* tab = tab_util::GetTabContentsByID(render_process_host_id_,
                                                  render_view_id_);
  if (!tab || tab->type() != TAB_CONTENTS_WEB) {
    NotifyDone();
    return;
  }

  tab_ = tab;
  controller_ = tab->controller();

  // Register for notifications of events from this tab.
  NotificationService* ns = NotificationService::current();
  DCHECK(ns);
  ns->AddObserver(this, NOTIFY_TAB_CLOSING,
                  Source<NavigationController>(controller_));
  ns->AddObserver(this, NOTIFY_DOM_OPERATION_RESPONSE,
                  Source<TabContents>(tab_));

  // Hold an extra reference to ourself until the interstitial is gone.
  AddRef();

  WebContents* web_contents = tab->AsWebContents();

  // Load the HTML page and create the template components.
  DictionaryValue strings;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::string html;

  if (result_ == SafeBrowsingService::URL_MALWARE) {
    std::wstring link = StringPrintf(kSbDiagnosticHtml,
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_DIAGNOSTIC_PAGE).c_str());

    strings.SetString(L"badURL", UTF8ToWide(url_.host()));
    strings.SetString(L"title",
                      l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_TITLE));
    strings.SetString(L"headLine",
                      l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_HEADLINE));

    // Check to see if we're blocking the main page, or a sub-resource on the
    // main page.
    GURL top_url = tab_->GetURL();
    if (top_url == url_) {
      strings.SetString(L"description1",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION1,
                                UTF8ToWide(url_.host())));
      strings.SetString(L"description2",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION2,
                                link,
                                UTF8ToWide(url_.host())));
    } else {
      strings.SetString(L"description1",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION4,
                                UTF8ToWide(top_url.host()),
                                UTF8ToWide(url_.host())));
      strings.SetString(L"description2",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION5,
                                link,
                                UTF8ToWide(url_.host())));
    }

    strings.SetString(L"description3",
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION3));
    strings.SetString(L"confirm_text",
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION_AGREE));
    strings.SetString(L"continue_button",
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_PROCEED_BUTTON));
    strings.SetString(L"back_button",
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_BACK_BUTTON));
    strings.SetString(L"textdirection",
        (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
        L"rtl" : L"ltr");
    html = rb.GetDataResource(IDR_SAFE_BROWSING_MALWARE_BLOCK);
  } else {
    strings.SetString(L"title",
                      l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_TITLE));
    strings.SetString(L"headLine",
        l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_HEADLINE));
    strings.SetString(L"description1",
        l10n_util::GetStringF(IDS_SAFE_BROWSING_PHISHING_DESCRIPTION1,
                              UTF8ToWide(url_.host())));
    strings.SetString(L"description2",
        l10n_util::GetStringF(IDS_SAFE_BROWSING_PHISHING_DESCRIPTION2,
                              UTF8ToWide(url_.host())));

    strings.SetString(L"continue_button",
        l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_PROCEED_BUTTON));
    strings.SetString(L"back_button",
        l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_BACK_BUTTON));
    strings.SetString(L"report_error",
        l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_REPORT_ERROR));
    strings.SetString(L"textdirection",
        (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
        L"rtl" : L"ltr");
    html = rb.GetDataResource(IDR_SAFE_BROWSING_PHISHING_BLOCK);
  }

  std::string html_page(jstemplate_builder::GetTemplateHtml(html,
                                                            &strings,
                                                            "template_root"));

  // If the malware is the actual main frame and we have no pending entry
  // (typically the navigation was initiated by the page), we create a fake
  // navigation entry (so the location bar shows the page's URL).
  if (is_main_frame_ && tab_->controller()->GetPendingEntryIndex() == -1) {
    // New navigation.
    NavigationEntry* nav_entry = new NavigationEntry(TAB_CONTENTS_WEB);

    // We set the page ID to max page id so to ensure the controller considers
    // this dummy entry a new one.  Because we'll remove the entry when the
    // interstitial is going away, it will not conflict with any future
    // navigations.
    nav_entry->set_page_id(tab_->GetMaxPageID() + 1);
    nav_entry->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);
    nav_entry->set_url(url_);
    tab_->controller()->DidNavigateToEntry(nav_entry);
    created_temporary_entry_ = true;
  }

  // Show the interstitial page.
  web_contents->ShowInterstitialPage(html_page, this);
}

void SafeBrowsingBlockingPage::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_TAB_CLOSING:
      HandleClose();
      break;
    case NOTIFY_DOM_OPERATION_RESPONSE:
      Continue(Details<DomOperationNotificationDetails>(details)->json());
      break;
    default:
      NOTREACHED();
  }
}

void SafeBrowsingBlockingPage::InterstitialClosed() {
  HandleClose();
}

bool SafeBrowsingBlockingPage::GoBack() {
  WebContents* web_contents = tab_->AsWebContents();
  NavigationEntry* prev_entry =
      web_contents->controller()->GetEntryAtOffset(-1);

  if (!prev_entry) {
    // Nothing to go to, default to about:blank.  Navigating will cause the
    // interstitial to hide which will trigger "this" to be deleted.
    tab_->controller()->LoadURL(GURL("about:blank"),
                                PageTransition::AUTO_BOOKMARK);
  } else if (prev_entry->tab_type() != TAB_CONTENTS_WEB ||
             prev_entry->restored() ||
             !is_main_frame_) {
    // We do navigate back if any of these is true:
    // - the page is not a WebContents, its TabContents might have to be
    //   recreated.
    // - we have not yet visited that navigation entry (typically session
    //   restore), in which case the page is not already available.
    // - the interstitial was triggered by a sub-resource.  In that case we
    //   really need to navigate, just hiding the interstitial would show the
    //   page containing the bad resource, and we don't want that.
    web_contents->controller()->GoBack();
  } else {
    // Otherwise, the user was viewing a page and navigated to a URL that was
    // interrupted by an interstitial. Thus, we can just hide the interstitial
    // and show the page the user was on before.
    web_contents->HideInterstitialPage(false, false);
  }

  // WARNING: at this point we are now either deleted or pending deletion from
  //          the IO thread.

  // Remove the navigation entry for the malware page.  Note that we always
  // remove the entry even if we did not create it as it has been flagged as
  // malware and we don't want the user navigating back to it.
  web_contents->controller()->RemoveLastEntry();

  return true;
}

void SafeBrowsingBlockingPage::Continue(const std::string& user_action) {
  TabContents* tab = tab_util::GetTabContentsByID(render_process_host_id_,
                                                  render_view_id_);
  DCHECK(tab);
  WebContents* web = tab->AsWebContents();
  if (user_action == "2") {
    // User pressed "Learn more".
    GURL url;
    if (result_ == SafeBrowsingService::URL_MALWARE) {
      url = GURL(l10n_util::GetString(IDS_LEARN_MORE_MALWARE_URL));
    } else if (result_ == SafeBrowsingService::URL_PHISHING) {
      url = GURL(l10n_util::GetString(IDS_LEARN_MORE_PHISHING_URL));
    } else {
      NOTREACHED();
    }
    web->OpenURL(url, CURRENT_TAB, PageTransition::LINK);
    return;
  }
  if (user_action == "3") {
    // User pressed "Report error" for a phishing site.
    // Note that we cannot just put a link in the interstitial at this point.
    // It is not OK to navigate in the context of an interstitial page.
    DCHECK(result_ == SafeBrowsingService::URL_PHISHING);
    GURL report_url =
        safe_browsing_util::GeneratePhishingReportUrl(kSbReportPhishingUrl,
                                                      url_.spec());
    web->OpenURL(report_url, CURRENT_TAB, PageTransition::LINK);
    return;
  }
  if (user_action == "4") {
    // We're going to take the user to Google's SafeBrowsing diagnostic page.
    std::string diagnostic =
        StringPrintf(kSbDiagnosticUrl,
                     EscapeQueryParamValue(url_.spec()).c_str());
    GURL diagnostic_url(diagnostic);
    diagnostic_url = google_util::AppendGoogleLocaleParam(diagnostic_url);
    DCHECK(result_ == SafeBrowsingService::URL_MALWARE);
    web->OpenURL(diagnostic_url, CURRENT_TAB, PageTransition::LINK);
    return;
  }

  proceed_ = user_action == "1";

  if (proceed_) {
    // We are continuing, if we have created a temporary navigation entry,
    // delete it as a new will be created on navigation.
    if (created_temporary_entry_)
      web->controller()->RemoveLastEntry();
    if (is_main_frame_)
      web->HideInterstitialPage(true, true);
    else
      web->HideInterstitialPage(false, false);
  } else {
    GoBack();
  }

  NotifyDone();
}

void SafeBrowsingBlockingPage::HandleClose() {
  NotificationService* ns = NotificationService::current();
  DCHECK(ns);
  ns->RemoveObserver(this, NOTIFY_TAB_CLOSING,
                     Source<NavigationController>(controller_));
  ns->RemoveObserver(this, NOTIFY_DOM_OPERATION_RESPONSE,
                     Source<TabContents>(tab_));

  NotifyDone();
  Release();
}

void SafeBrowsingBlockingPage::NotifyDone() {
  if (delete_pending_)
    return;

  delete_pending_ = true;

  if (tab_ && tab_->AsWebContents()) {
    // Ensure the WebContents does not keep a pointer to us.
    tab_->AsWebContents()->set_interstitial_delegate(NULL);
  }

  base::Thread* io_thread = g_browser_process->io_thread();
  if (!io_thread)
    return;

  io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      sb_service_,
      &SafeBrowsingService::OnBlockingPageDone,
      this, client_, proceed_));
}

