// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/page_info_window_mac.h"

#include <Security/Security.h>
#include <SecurityInterface/SFCertificatePanel.h>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/time_format.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"

using base::Time;

PageInfoWindow* PageInfoWindow::Factory() {
  // The controller will clean itself up after the NSWindow it manages closes.
  // We do not manage it as it owns us.
  PageInfoWindowController* controller =
      [[PageInfoWindowController alloc] init];
  return [controller pageInfo];
}

PageInfoWindowMac::PageInfoWindowMac(PageInfoWindowController* controller)
    : PageInfoWindow(), controller_(controller) {
}

PageInfoWindowMac::~PageInfoWindowMac() {
}

void PageInfoWindowMac::Show() {
  [[controller_ window] makeKeyAndOrderFront:nil];
}

void PageInfoWindowMac::ShowCertDialog(int) {
  DCHECK(cert_id_ != 0);
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert);
  if (!cert.get()) {
    // The certificate was not found. Could be that the renderer crashed before
    // we displayed the page info.
    return;
  }

  SecCertificateRef cert_mac = cert->os_cert_handle();
  if (!cert_mac)
    return;

  [[SFCertificatePanel sharedCertificatePanel]
      beginSheetForWindow:[controller_ window]
            modalDelegate:nil
           didEndSelector:NULL
              contextInfo:NULL
              // This is cast to id because we get compiler errors about an
              // OpaqueSecCertificateRef* being converted to an ObjC class.
              // It's a CF-type so it's toll-free bridged and casting to id
              // is OK.
             certificates:[NSArray arrayWithObject:(id)cert_mac]
                showGroup:YES
  ];
}

void PageInfoWindowMac::Init(Profile* profile,
                             const GURL& url,
                             const NavigationEntry::SSLStatus& ssl,
                             NavigationEntry::PageType page_type,
                             bool show_history,
                             gfx::NativeView parent) {
  // These wstring's will be converted to NSString's and passed to the
  // window controller when we're done figuring out what text should go in them.
  std::wstring identity_msg;
  std::wstring connection_msg;

  // Set all the images to "good" mode.
  [controller_ setIdentityImg:[controller_ goodImg]];
  [controller_ setConnectionImg:[controller_ goodImg]];

  cert_id_ = ssl.cert_id();
  scoped_refptr<net::X509Certificate> cert;

  // Identity section
  std::wstring identity_title;
  std::wstring subject_name(UTF8ToWide(url.host()));
  bool empty_subject_name = subject_name.empty();
  if (empty_subject_name) {
    subject_name.assign(
      l10n_util::GetString(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }
  if (page_type == NavigationEntry::NORMAL_PAGE && cert_id_ &&
      CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert) &&
      !net::IsCertStatusError(ssl.cert_status())) {
    // OK HTTPS page
    if ((ssl.cert_status() & net::CERT_STATUS_IS_EV) != 0) {
      DCHECK(!cert->subject().organization_names.empty());
      identity_title =
        l10n_util::GetStringF(IDS_PAGE_INFO_EV_IDENTITY_TITLE,
          UTF8ToWide(cert->subject().organization_names[0]),
          UTF8ToWide(url.host()));
      // An EV cert is required to have a city (localityName) and country but
      // state is "if any".
      DCHECK(!cert->subject().locality_name.empty());
      DCHECK(!cert->subject().country_name.empty());
      std::wstring locality;
      if (!cert->subject().state_or_province_name.empty()) {
        locality = l10n_util::GetStringF(
            IDS_PAGEINFO_ADDRESS,
            UTF8ToWide(cert->subject().locality_name),
            UTF8ToWide(cert->subject().state_or_province_name),
            UTF8ToWide(cert->subject().country_name));
      } else {
        locality = l10n_util::GetStringF(
            IDS_PAGEINFO_PARTIAL_ADDRESS,
            UTF8ToWide(cert->subject().locality_name),
            UTF8ToWide(cert->subject().country_name));
      }
      DCHECK(!cert->subject().organization_names.empty());
      identity_msg.assign(l10n_util::GetStringF(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV,
          UTF8ToWide(cert->subject().organization_names[0]),
          locality,
          UTF8ToWide(PageInfoWindow::GetIssuerName(cert->issuer()))));
    } else {
      // Non EV OK HTTPS.
      if (empty_subject_name)
        identity_title.clear();  // Don't display any title.
      else
        identity_title.assign(subject_name);
      std::wstring issuer_name(UTF8ToWide(
          PageInfoWindow::GetIssuerName(cert->issuer())));
      if (issuer_name.empty()) {
        issuer_name.assign(
            l10n_util::GetString(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
      } else {
        identity_msg.assign(
            l10n_util::GetStringF(IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY,
                                  issuer_name));
      }
    }
  } else {
    // Bad HTTPS.
    identity_msg.assign(
        l10n_util::GetString(IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    [controller_ setIdentityImg:[controller_ badImg]];
  }

  // Connection section.
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  if (ssl.security_bits() <= 0) {
    [controller_ setConnectionImg:[controller_ badImg]];
    connection_msg.assign(
        l10n_util::GetStringF(
            IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
            subject_name));
  } else if (ssl.security_bits() < 80) {
    [controller_ setConnectionImg:[controller_ badImg]];
    connection_msg.assign(
        l10n_util::GetStringF(
            IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
            subject_name));
  } else {
    connection_msg.assign(
        l10n_util::GetStringF(
            IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
            subject_name,
            IntToWString(ssl.security_bits())));
    if (ssl.has_mixed_content()) {
      [controller_ setConnectionImg:[controller_ badImg]];
      connection_msg.assign(
          l10n_util::GetStringF(
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
              connection_msg,
              l10n_util::GetString(
                  IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_MIXED_CONTENT_WARNING)));
    } else if (ssl.has_unsafe_content()) {
      [controller_ setConnectionImg:[controller_ badImg]];
      connection_msg.assign(
          l10n_util::GetStringF(
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
              connection_msg,
              l10n_util::GetString(
                  IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_BAD_HTTPS_WARNING)));
    }
  }

  // We've figured out the messages that we want to appear in the page info
  // window and we now hand them up to the NSWindowController, which binds them
  // to the Cocoa view.
  [controller_ setIdentityMsg:base::SysWideToNSString(identity_msg)];
  [controller_ setConnectionMsg:base::SysWideToNSString(connection_msg)];

  // Request the number of visits.
  HistoryService* history = profile->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (show_history && history) {
    history->GetVisitCountToHost(
        url,
        &request_consumer_,
        NewCallback(this, &PageInfoWindowMac::OnGotVisitCountToHost));
  }

  // By default, assume that we don't have certificate information to show.
  [controller_ setEnableCertButton:NO];
  if (cert_id_) {
    // Don't bother showing certificates if there isn't one. Gears runs with no
    // os root certificate.
    if (cert.get() && cert->os_cert_handle()) {
      [controller_ setEnableCertButton:YES];
    }
  }
}

void PageInfoWindowMac::OnGotVisitCountToHost(HistoryService::Handle handle,
                                              bool found_visits,
                                              int count,
                                              Time first_visit) {
  if (!found_visits) {
    // This indicates an error, such as the page wasn't http/https; do nothing.
    return;
  }

  // We have history information, so show the box and extend the window frame.
  [controller_ setShowHistoryBox:YES];

  bool visited_before_today = false;
  if (count) {
    Time today = Time::Now().LocalMidnight();
    Time first_visit_midnight = first_visit.LocalMidnight();
    visited_before_today = (first_visit_midnight < today);
  }

  if (!visited_before_today) {
    [controller_ setHistoryImg:[controller_ badImg]];
    [controller_ setHistoryMsg:
        base::SysWideToNSString(
            l10n_util::GetString(
              IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY))];
  } else {
    [controller_ setHistoryImg:[controller_ goodImg]];
    [controller_ setHistoryMsg:
        base::SysWideToNSString(
            l10n_util::GetStringF(
                IDS_PAGE_INFO_SECURITY_TAB_VISITED_BEFORE_TODAY,
                base::TimeFormatShortDate(first_visit)))];
  }
}
