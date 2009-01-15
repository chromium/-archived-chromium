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
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "generated_resources.h"
#include "net/base/escape.h"


// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
#if defined(GOOGLE_CHROME_BUILD)
static const char* const kSbDiagnosticUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?site=%s&client=googlechrome";
#else
static const char* const kSbDiagnosticUrl =
    "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?site=%s&client=chromium";
#endif

static const char* const kSbReportPhishingUrl =
    "http://www.google.com/safebrowsing/report_error/";

static const wchar_t* const kSbDiagnosticHtml =
    L"<a href=\"\" onClick=\"sendCommand(4); return false;\" onMouseDown=\"return false;\">%ls</a>";

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingService* sb_service,
    const SafeBrowsingService::BlockingPageParam& param)
    : InterstitialPage(tab_util::GetWebContentsByID(
          param.render_process_host_id, param.render_view_id),
                       param.resource_type == ResourceType::MAIN_FRAME,
                       param.url),
      sb_service_(sb_service),
      client_(param.client),
      render_process_host_id_(param.render_process_host_id),
      render_view_id_(param.render_view_id),
      result_(param.result),
      proceed_(false),
      did_notify_(false),
      is_main_frame_(param.resource_type == ResourceType::MAIN_FRAME) {
  if (!is_main_frame_) {
    navigation_entry_index_to_remove_ =
        tab()->controller()->GetLastCommittedEntryIndex();
  } else {
    navigation_entry_index_to_remove_ = -1;
  }
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {
}

std::string SafeBrowsingBlockingPage::GetHTMLContents() {
  // Load the HTML page and create the template components.
  DictionaryValue strings;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::string html;

  if (result_ == SafeBrowsingService::URL_MALWARE) {
    std::wstring link = StringPrintf(kSbDiagnosticHtml,
        l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_DIAGNOSTIC_PAGE).
            c_str());

    strings.SetString(L"badURL", UTF8ToWide(url().host()));
    strings.SetString(L"title",
                      l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_TITLE));
    strings.SetString(L"headLine",
                      l10n_util::GetString(IDS_SAFE_BROWSING_MALWARE_HEADLINE));

    // Check to see if we're blocking the main page, or a sub-resource on the
    // main page.
    if (is_main_frame_) {
      strings.SetString(L"description1",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION1,
                                UTF8ToWide(url().host())));
      strings.SetString(L"description2",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION2,
                                link,
                                UTF8ToWide(url().host())));
    } else {
      strings.SetString(L"description1",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION4,
                                UTF8ToWide(tab()->GetURL().host()),
                                UTF8ToWide(url().host())));
      strings.SetString(L"description2",
          l10n_util::GetStringF(IDS_SAFE_BROWSING_MALWARE_DESCRIPTION5,
                                link,
                                UTF8ToWide(url().host())));
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
  } else {  // Phishing.
    strings.SetString(L"title",
                      l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_TITLE));
    strings.SetString(L"headLine",
        l10n_util::GetString(IDS_SAFE_BROWSING_PHISHING_HEADLINE));
    strings.SetString(L"description1",
        l10n_util::GetStringF(IDS_SAFE_BROWSING_PHISHING_DESCRIPTION1,
                              UTF8ToWide(url().host())));
    strings.SetString(L"description2",
        l10n_util::GetStringF(IDS_SAFE_BROWSING_PHISHING_DESCRIPTION2,
                              UTF8ToWide(url().host())));

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

  return jstemplate_builder::GetTemplateHtml(html, &strings, "template_root");
}

void SafeBrowsingBlockingPage::CommandReceived(const std::string& command) {
  if (command == "2") {
    // User pressed "Learn more".
    GURL url;
    if (result_ == SafeBrowsingService::URL_MALWARE) {
      url = GURL(l10n_util::GetString(IDS_LEARN_MORE_MALWARE_URL));
    } else if (result_ == SafeBrowsingService::URL_PHISHING) {
      url = GURL(l10n_util::GetString(IDS_LEARN_MORE_PHISHING_URL));
    } else {
      NOTREACHED();
    }
    tab()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::LINK);
    return;
  }
  if (command == "3") {
    // User pressed "Report error" for a phishing site.
    // Note that we cannot just put a link in the interstitial at this point.
    // It is not OK to navigate in the context of an interstitial page.
    DCHECK(result_ == SafeBrowsingService::URL_PHISHING);
    GURL report_url =
        safe_browsing_util::GeneratePhishingReportUrl(kSbReportPhishingUrl,
                                                      url().spec());
    Hide();
    tab()->OpenURL(report_url, GURL(), CURRENT_TAB, PageTransition::LINK);
    return;
  }
  if (command == "4") {
    // We're going to take the user to Google's SafeBrowsing diagnostic page.
    std::string diagnostic =
        StringPrintf(kSbDiagnosticUrl,
                     EscapeQueryParamValue(url().spec()).c_str());
    GURL diagnostic_url(diagnostic);
    diagnostic_url = google_util::AppendGoogleLocaleParam(diagnostic_url);
    DCHECK(result_ == SafeBrowsingService::URL_MALWARE);
    tab()->OpenURL(diagnostic_url, GURL(), CURRENT_TAB, PageTransition::LINK);
    return;
  }

  proceed_ = command == "1";

  if (proceed_)
    Proceed();
  else
    DontProceed();

  NotifyDone();
}

void SafeBrowsingBlockingPage::DontProceed() {
  if (navigation_entry_index_to_remove_ != -1) {
    tab()->controller()->RemoveEntryAtIndex(navigation_entry_index_to_remove_,
                                            NewTabUIURL());
  }
  InterstitialPage::DontProceed();
  // We are now deleted.
}

void SafeBrowsingBlockingPage::NotifyDone() {
  if (did_notify_)
    return;

  did_notify_ = true;

  base::Thread* io_thread = g_browser_process->io_thread();
  if (!io_thread)
    return;

  SafeBrowsingService::BlockingPageParam param;
  param.url = url();
  param.result = result_;
  param.client = client_;
  param.render_process_host_id = render_process_host_id_;
  param.render_view_id = render_view_id_;
  param.proceed = proceed_;
  io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      sb_service_,
      &SafeBrowsingService::OnBlockingPageDone,
      param));
}
