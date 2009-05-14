// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/histogram.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

enum SSLBlockingPageEvent {
  SHOW,
  PROCEED,
  DONT_PROCEED,
};

void RecordSSLBlockingPageStats(SSLBlockingPageEvent event) {
  static LinearHistogram histogram("interstial.ssl", 0, 2, 3);
  histogram.SetFlags(kUmaTargetedHistogramFlag);
  histogram.Add(event);
}

}  // namespace

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(SSLCertErrorHandler* handler,
                                 Delegate* delegate)
    : InterstitialPage(handler->GetTabContents(), true, handler->request_url()),
      handler_(handler),
      delegate_(delegate),
      delegate_has_been_notified_(false) {
  RecordSSLBlockingPageStats(SHOW);
}

SSLBlockingPage::~SSLBlockingPage() {
  if (!delegate_has_been_notified_) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

std::string SSLBlockingPage::GetHTMLContents() {
  // Let's build the html error page.
  DictionaryValue strings;
  SSLErrorInfo error_info = delegate_->GetSSLErrorInfo(handler_);
  strings.SetString(L"title",
                    l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_TITLE));
  strings.SetString(L"headLine", error_info.title());
  strings.SetString(L"description", error_info.details());

  strings.SetString(L"moreInfoTitle",
                    l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_TITLE));
  SetExtraInfo(&strings, error_info.extra_information());

  strings.SetString(L"proceed",
                    l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_PROCEED));
  strings.SetString(L"exit",
                    l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_EXIT));

  strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       L"rtl" : L"ltr");

  static const StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SSL_ROAD_BLOCK_HTML));

  return jstemplate_builder::GetTemplateHtml(html, &strings, "template_root");
}

void SSLBlockingPage::UpdateEntry(NavigationEntry* entry) {
  const net::SSLInfo& ssl_info = handler_->ssl_info();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      ssl_info.cert, tab()->render_view_host()->process()->pid());

  entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
  entry->ssl().set_cert_id(cert_id);
  entry->ssl().set_cert_status(ssl_info.cert_status);
  entry->ssl().set_security_bits(ssl_info.security_bits);
  NotificationService::current()->Notify(
      NotificationType::SSL_VISIBLE_STATE_CHANGED,
      Source<NavigationController>(&tab()->controller()),
      NotificationService::NoDetails());
}

void SSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "1") {
    Proceed();
  } else {
    DontProceed();
  }
}

void SSLBlockingPage::Proceed() {
  RecordSSLBlockingPageStats(PROCEED);

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();

  // This call hides and deletes the interstitial.
  InterstitialPage::Proceed();
}

void SSLBlockingPage::DontProceed() {
  RecordSSLBlockingPageStats(DONT_PROCEED);

  NotifyDenyCertificate();
  InterstitialPage::DontProceed();
}

void SSLBlockingPage::NotifyDenyCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnDenyCertificate(handler_);
  delegate_has_been_notified_ = true;
}

void SSLBlockingPage::NotifyAllowCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnAllowCertificate(handler_);
  delegate_has_been_notified_ = true;
}

// static
void SSLBlockingPage::SetExtraInfo(
    DictionaryValue* strings,
    const std::vector<std::wstring>& extra_info) {
  DCHECK(extra_info.size() < 5);  // We allow 5 paragraphs max.
  const std::wstring keys[5] = {
      L"moreInfo1", L"moreInfo2", L"moreInfo3", L"moreInfo4", L"moreInfo5"
  };
  int i;
  for (i = 0; i < static_cast<int>(extra_info.size()); i++) {
    strings->SetString(keys[i], extra_info[i]);
  }
  for (;i < 5; i++) {
    strings->SetString(keys[i], L"");
  }
}
