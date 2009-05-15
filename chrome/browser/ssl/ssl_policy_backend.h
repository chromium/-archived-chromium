// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "net/base/x509_certificate.h"
#include "webkit/api/public/WebConsoleMessage.h"

class NavigationController;
class SSLHostState;
class Task;

class SSLPolicyBackend {
 public:
  explicit SSLPolicyBackend(NavigationController* controller);
  virtual ~SSLPolicyBackend();

  // Ensure that the specified message is displayed to the user.  This will
  // display an InfoBar at the top of the associated tab.
  virtual void ShowMessage(const std::wstring& msg);

  // Same as ShowMessage but also contains a link that when clicked run the
  // specified task.  The SSL Manager becomes the owner of the task.
  virtual void ShowMessageWithLink(const std::wstring& msg,
                                   const std::wstring& link_text,
                                   Task* task);

  // Records that a host is "broken," that is, the origin for that host has been
  // contaminated with insecure content, either via HTTP or via HTTPS with a
  // bad certificate.
  virtual void MarkHostAsBroken(const std::string& host, int pid);

  // Returns whether the specified host was marked as broken.
  virtual bool DidMarkHostAsBroken(const std::string& host, int pid) const;

  // Logs a message to the console of the page.
  virtual void AddMessageToConsole(const string16& message,
                                   const WebKit::WebConsoleMessage::Level&);

  // Records that |cert| is permitted to be used for |host| in the future.
  virtual void DenyCertForHost(net::X509Certificate* cert,
                               const std::string& host);

  // Records that |cert| is not permitted to be used for |host| in the future.
  virtual void AllowCertForHost(net::X509Certificate* cert,
                                const std::string& host);

  // Queries whether |cert| is allowed or denied for |host|.
  virtual net::X509Certificate::Policy::Judgment QueryPolicy(
      net::X509Certificate* cert, const std::string& host);

  // Allow mixed content to be visible (non filtered).
  virtual void AllowMixedContentForHost(const std::string& host);

  // Returns whether the specified host is allowed to show mixed content.
  virtual bool DidAllowMixedContentForHost(const std::string& host) const;

  // Reloads the tab.
  virtual void Reload();

  // Shows the pending messages (in info-bars) if any.
  virtual void ShowPendingMessages();

  // Clears any pending messages.
  virtual void ClearPendingMessages();

 private:
  // SSLMessageInfo contains the information necessary for displaying a message
  // in an info-bar.
  struct SSLMessageInfo {
   public:
    explicit SSLMessageInfo(const std::wstring& text)
        : message(text),
          action(NULL) { }

    SSLMessageInfo(const std::wstring& message,
                   const std::wstring& link_text,
                   Task* action)
        : message(message), link_text(link_text), action(action) { }

    // Overridden so that std::find works.
    bool operator==(const std::wstring& other_message) const {
      // We are uniquing SSLMessageInfo by their message only.
      return message == other_message;
    }

    std::wstring message;
    std::wstring link_text;
    Task* action;
  };

  // Dispatch NotificationType::SSL_INTERNAL_STATE_CHANGED notification.
  void DispatchSSLInternalStateChanged();

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // SSL state specific for each host.
  SSLHostState* ssl_host_state_;

  // The list of messages that should be displayed (in info bars) when the page
  // currently loading had loaded.
  std::vector<SSLMessageInfo> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicyBackend);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
