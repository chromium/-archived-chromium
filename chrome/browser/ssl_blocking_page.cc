// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_blocking_page.h"

#include "base/string_piece.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"

#include "generated_resources.h"

// static
SSLBlockingPage::SSLBlockingPageMap*
    SSLBlockingPage::tab_to_blocking_page_ =  NULL;

SSLBlockingPage::SSLBlockingPage(SSLManager::CertError* error,
                                 Delegate* delegate)
    : error_(error),
      delegate_(delegate),
      delegate_has_been_notified_(false),
      remove_last_entry_(true),
      created_nav_entry_(false) {
  InitSSLBlockingPageMap();
  // Remember the tab, because we might not be able to get to it later
  // via the error.
  tab_ = error->GetTabContents();
  DCHECK(tab_);

  // If there's already an interstitial in this tab, then we're about to
  // replace it.  We should be ok with just deleting the previous
  // SSLBlockingPage (not hiding it first), since we're about to be shown.
  SSLBlockingPageMap::const_iterator iter = tab_to_blocking_page_->find(tab_);
  if (iter != tab_to_blocking_page_->end()) {
    // Deleting the SSLBlockingPage will also remove it from the map.
    delete iter->second;

    // Since WebContents::InterstitialPageGone won't be called, we need
    // to clear the last NavigationEntry manually.
    tab_->controller()->RemoveLastEntry();
  }
  (*tab_to_blocking_page_)[tab_] = this;

  // Register notifications so we can delete ourself if the tab is closed.
  NotificationService::current()->AddObserver(this,
      NOTIFY_TAB_CLOSING,
      Source<NavigationController>(tab_->controller()));

  NotificationService::current()->AddObserver(this,
      NOTIFY_INTERSTITIAL_PAGE_CLOSED,
      Source<NavigationController>(tab_->controller()));

  // Register for DOM operations, this is how the blocking page notifies us of
  // what the user chooses.
  NotificationService::current()->AddObserver(this,
      NOTIFY_DOM_OPERATION_RESPONSE,
      Source<TabContents>(tab_));
}

SSLBlockingPage::~SSLBlockingPage() {
  NotificationService::current()->RemoveObserver(this,
      NOTIFY_TAB_CLOSING,
      Source<NavigationController>(tab_->controller()));

  NotificationService::current()->RemoveObserver(this,
      NOTIFY_INTERSTITIAL_PAGE_CLOSED,
      Source<NavigationController>(tab_->controller()));

  NotificationService::current()->RemoveObserver(this,
      NOTIFY_DOM_OPERATION_RESPONSE,
      Source<TabContents>(tab_));

  SSLBlockingPageMap::iterator iter =
      tab_to_blocking_page_->find(tab_);
  DCHECK(iter != tab_to_blocking_page_->end());
  tab_to_blocking_page_->erase(iter);

  if (!delegate_has_been_notified_) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

void SSLBlockingPage::Show() {
  // Let's build the html error page.
  DictionaryValue strings;
  SSLErrorInfo error_info = delegate_->GetSSLErrorInfo(error_);
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

  std::string html_text(jstemplate_builder::GetTemplateHtml(html,
                                                            &strings,
                                                            "template_root"));

  DCHECK(tab_->type() == TAB_CONTENTS_WEB);
  WebContents* tab = tab_->AsWebContents();
  const net::SSLInfo& ssl_info = error_->ssl_info();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      ssl_info.cert, tab->render_view_host()->process()->host_id());

  NavigationEntry* nav_entry = new NavigationEntry(TAB_CONTENTS_WEB);
  if (tab_->controller()->GetPendingEntryIndex() == -1) {
    // New navigation.
    // We set the page ID to max page id so to ensure the controller considers
    // this dummy entry a new one.  Because we'll remove the entry when the
    // interstitial is going away, it will not conflict with any future
    // navigations.
    created_nav_entry_ = true;
    nav_entry->set_page_id(tab_->GetMaxPageID() + 1);
    nav_entry->set_url(error_->request_url());
  } else {
    // Make sure to update the current entry ssl state to reflect the error.
    *nav_entry = *(tab_->controller()->GetPendingEntry());
  }
  nav_entry->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);

  nav_entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
  nav_entry->ssl().set_cert_id(cert_id);
  nav_entry->ssl().set_cert_status(ssl_info.cert_status);
  nav_entry->ssl().set_security_bits(ssl_info.security_bits);
  // The controller will own the entry.
  tab_->controller()->DidNavigateToEntry(nav_entry);
  tab->ShowInterstitialPage(html_text, NULL);
}

void SSLBlockingPage::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_TAB_CLOSING:
    case NOTIFY_INTERSTITIAL_PAGE_CLOSED: {
      // We created a navigation entry for the interstitial, remove it.
      // Note that we don't remove the entry if we are closing all tabs so that
      // the last entry is kept for the restoring on next start-up.
      Browser* browser = Browser::GetBrowserForController(tab_->controller(),
                                                          NULL);
      if (remove_last_entry_ &&
          !browser->tabstrip_model()->closing_all()) {
        tab_->controller()->RemoveLastEntry();
      }
      delete this;
      break;
    }
    case NOTIFY_DOM_OPERATION_RESPONSE: {
      std::string json =
          Details<DomOperationNotificationDetails>(details)->json();
      if (json == "1") {
        Proceed();
      } else {
        DontProceed();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void SSLBlockingPage::Proceed() {
  // We hide the interstitial page first as allowing the certificate will
  // resume the request and we want the WebContents back to showing the
  // non interstitial page (otherwise the request completion messages may
  // confuse the WebContents if it is still showing the interstitial
  // page).
  DCHECK(tab_->type() == TAB_CONTENTS_WEB);
  tab_->AsWebContents()->HideInterstitialPage(true, true);

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();

  // Do not remove the navigation entry if we have not created it explicitly
  // as in such cases (session restore) the controller would not create a new
  // entry on navigation since the page id is less than max page id.
  if (!created_nav_entry_)
    remove_last_entry_ = false;
}

void SSLBlockingPage::DontProceed() {
  NotifyDenyCertificate();

  // We are navigating, remove the current entry before we mess with it.
  remove_last_entry_ = false;
  tab_->controller()->RemoveLastEntry();

  NavigationEntry* entry = tab_->controller()->GetActiveEntry();
  if (!entry) {
    // Nothing to go to, default to about:blank.  Navigating will cause the
    // interstitial to hide which will trigger "this" to be deleted.
    tab_->controller()->LoadURL(GURL("about:blank"),
                                PageTransition::AUTO_BOOKMARK);
  } else if (entry->tab_type() != TAB_CONTENTS_WEB) {
    // Not a WebContent, reload it so to recreate the TabContents for it.
    tab_->controller()->Reload();
  } else {
    DCHECK(tab_->type() == TAB_CONTENTS_WEB);
    if (entry->restored()) {
      // If this page was restored, it is not available, we have to navigate to
      // it.
      tab_->controller()->GoToOffset(0);
    } else {
      tab_->AsWebContents()->HideInterstitialPage(false, false);
    }
  }
  // WARNING: we are now deleted!
}

// static
void SSLBlockingPage::InitSSLBlockingPageMap() {
  if (!tab_to_blocking_page_)
    tab_to_blocking_page_ = new SSLBlockingPageMap;
}

void SSLBlockingPage::NotifyDenyCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnDenyCertificate(error_);
  delegate_has_been_notified_ = true;
}

void SSLBlockingPage::NotifyAllowCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnAllowCertificate(error_);
  delegate_has_been_notified_ = true;
}

// static
SSLBlockingPage* SSLBlockingPage::GetSSLBlockingPage(
    TabContents* tab_contents) {
  InitSSLBlockingPageMap();
  SSLBlockingPageMap::const_iterator iter =
      tab_to_blocking_page_->find(tab_contents);
  if (iter == tab_to_blocking_page_->end())
    return NULL;

  return iter->second;
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

