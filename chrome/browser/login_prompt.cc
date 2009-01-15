// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"

#include "base/command_line.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/controller.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/views/login_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/dialog_delegate.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

#include "generated_resources.h"

using namespace std;
using views::LoginView;

class LoginHandlerImpl;

// Helper to remove the ref from an URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an URLRequest.
static void ResetLoginHandlerForRequest(URLRequest* request) {
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  if (!info)
    return;

  info->login_handler = NULL;
}

// ----------------------------------------------------------------------------
// LoginHandlerImpl

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerImpl : public LoginHandler,
                         public base::RefCountedThreadSafe<LoginHandlerImpl>,
                         public views::DialogDelegate {
 public:
  LoginHandlerImpl(URLRequest* request, MessageLoop* ui_loop)
      : dialog_(NULL),
        handled_auth_(false),
        request_(request),
        request_loop_(MessageLoop::current()),
        ui_loop_(ui_loop),
        password_manager_(NULL) {
    DCHECK(request_) << "LoginHandler constructed with NULL request";

    AddRef();  // matched by ReleaseLater.
    if (!tab_util::GetTabContentsID(request_, &render_process_host_id_,
                                    &tab_contents_id_)) {
      NOTREACHED();
    }
  }

  ~LoginHandlerImpl() {
  }

  // Initialize the UI part of the LoginHandler.
  // Scary thread safety note: This can potentially be called *after* SetAuth
  // or CancelAuth (say, if the request was cancelled before the UI thread got
  // control).  However, that's OK since any UI interaction in those functions
  // will occur via an InvokeLater on the UI thread, which is guaranteed
  // to happen after this is called (since this was InvokeLater'd first).
  void InitWithDialog(ConstrainedWindow* dlg) {
    DCHECK(MessageLoop::current() == ui_loop_);
    dialog_ = dlg;
    SendNotifications();
  }

  // Returns the WebContents that needs authentication.
  WebContents* GetWebContentsForLogin() {
    DCHECK(MessageLoop::current() == ui_loop_);

    return tab_util::GetWebContentsByID(render_process_host_id_,
                                        tab_contents_id_);
  }

  void set_login_view(LoginView* login_view) {
    login_view_ = login_view;
  }

  void set_password_form(const PasswordForm& form) {
    password_form_ = form;
  }

  void set_password_manager(PasswordManager* password_manager) {
    password_manager_ = password_manager;
  }

  // views::DialogDelegate methods:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const {
    if (button == DIALOGBUTTON_OK)
      return l10n_util::GetString(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
    return DialogDelegate::GetDialogButtonLabel(button);
  }
  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(IDS_LOGIN_DIALOG_TITLE);
  }
  virtual void WindowClosing() {
    DCHECK(MessageLoop::current() == ui_loop_);

    // Reference is no longer valid.
    dialog_ = NULL;

    if (!WasAuthHandled(true)) {
      request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &LoginHandlerImpl::CancelAuthDeferred));
      SendNotifications();
    }

    // Delete this object once all InvokeLaters have been called.
    request_loop_->ReleaseSoon(FROM_HERE, this);
  }
  virtual bool Cancel() {
    DCHECK(MessageLoop::current() == ui_loop_);
    DCHECK(dialog_) << "LoginHandler invoked without being attached";
    CancelAuth();
    return true;
  }
  virtual bool Accept() {
    DCHECK(MessageLoop::current() == ui_loop_);
    DCHECK(dialog_) << "LoginHandler invoked without being attached";
    SetAuth(login_view_->GetUsername(), login_view_->GetPassword());
    return true;
  }
  virtual views::View* GetContentsView() {
    return login_view_;
  }

  // LoginHandler:
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password) {
    if (WasAuthHandled(true))
      return;

    // Tell the password manager the credentials were submitted / accepted.
    if (password_manager_) {
      password_form_.username_value = username;
      password_form_.password_value = password;
      password_manager_->ProvisionallySavePassword(password_form_);
    }

    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::CloseContentsDeferred));
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::SendNotifications));
    request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::SetAuthDeferred, username, password));
  }

  virtual void CancelAuth() {
    if (WasAuthHandled(true))
      return;

    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::CloseContentsDeferred));
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::SendNotifications));
    request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerImpl::CancelAuthDeferred));
  }

  virtual void OnRequestCancelled() {
    DCHECK(MessageLoop::current() == request_loop_) <<
        "Why is OnRequestCancelled called from the UI thread?";

    // Reference is no longer valid.
    request_ = NULL;

    // Give up on auth if the request was cancelled.
    CancelAuth();
  }

 private:

  // Calls SetAuth from the request_loop.
  void SetAuthDeferred(const std::wstring& username,
                       const std::wstring& password) {
    DCHECK(MessageLoop::current() == request_loop_);

    if (request_) {
      request_->SetAuth(username, password);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Calls CancelAuth from the request_loop.
  void CancelAuthDeferred() {
    DCHECK(MessageLoop::current() == request_loop_);

    if (request_) {
      request_->CancelAuth();
      // Verify that CancelAuth does destroy the request via our delegate.
      DCHECK(request_ != NULL);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Closes the view_contents from the UI loop.
  void CloseContentsDeferred() {
    DCHECK(MessageLoop::current() == ui_loop_);

    // The hosting ConstrainedWindow may have been freed.
    if (dialog_)
      dialog_->CloseConstrainedWindow();
  }

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  // If |set_handled| is true, it will mark authentication as handled.
  bool WasAuthHandled(bool set_handled) {
    AutoLock lock(handled_auth_lock_);
    bool was_handled = handled_auth_;
    if (set_handled)
      handled_auth_ = true;
    return was_handled;
  }

  // Notify observers that authentication is needed or received.  The automation
  // proxy uses this for testing.
  void SendNotifications() {
    DCHECK(MessageLoop::current() == ui_loop_);

    NotificationService* service = NotificationService::current();
    WebContents* requesting_contents = GetWebContentsForLogin();
    if (!requesting_contents)
      return;

    NavigationController* controller = requesting_contents->controller();

    if (!WasAuthHandled(false)) {
      LoginNotificationDetails details(this);
      service->Notify(NOTIFY_AUTH_NEEDED,
                      Source<NavigationController>(controller),
                      Details<LoginNotificationDetails>(&details));
    } else {
      service->Notify(NOTIFY_AUTH_SUPPLIED,
                      Source<NavigationController>(controller),
                      NotificationService::NoDetails());
    }
  }

  // True if we've handled auth (SetAuth or CancelAuth has been called).
  bool handled_auth_;
  Lock handled_auth_lock_;

  // The ConstrainedWindow that is hosting our LoginView.
  // This should only be accessed on the ui_loop_.
  ConstrainedWindow* dialog_;

  // The MessageLoop of the thread that the ChromeViewContents lives in.
  MessageLoop* ui_loop_;

  // The request that wants login data.
  // This should only be accessed on the request_loop_.
  URLRequest* request_;

  // The MessageLoop of the thread that the URLRequest lives in.
  MessageLoop* request_loop_;

  // The LoginView that contains the user's login information
  LoginView* login_view_;

  // The PasswordForm sent to the PasswordManager. This is so we can refer to it
  // when later notifying the password manager if the credentials were accepted
  // or rejected.
  // This should only be accessed on the ui_loop_.
  PasswordForm password_form_;

  // Points to the password manager owned by the TabContents requesting auth.
  // Can be null if the TabContents is not a WebContents.
  // This should only be accessed on the ui_loop_.
  PasswordManager* password_manager_;

  // Cached from the URLRequest, in case it goes NULL on us.
  int render_process_host_id_;
  int tab_contents_id_;

  DISALLOW_EVIL_CONSTRUCTORS(LoginHandlerImpl);
};


// ----------------------------------------------------------------------------
// LoginDialogTask

// This task is run on the UI thread and creates a constrained window with
// a LoginView to prompt the user.  The response will be sent to LoginHandler,
// which then routes it to the URLRequest on the I/O thread.
class LoginDialogTask : public Task {
 public:
  LoginDialogTask(net::AuthChallengeInfo* auth_info, LoginHandlerImpl* handler)
      : auth_info_(auth_info), handler_(handler) {
  }
  virtual ~LoginDialogTask() {
  }

  void Run() {
    WebContents* parent_contents = handler_->GetWebContentsForLogin();
    if (!parent_contents) {
      // The request was probably cancelled.
      return;
    }

    wstring explanation = l10n_util::GetStringF(IDS_LOGIN_DIALOG_DESCRIPTION,
                                                auth_info_->host,
                                                auth_info_->realm);
    LoginView* view = new LoginView(explanation);
    // Tell the password manager to look for saved passwords. There is only
    // a password manager when dealing with a WebContents type.
    if (parent_contents->type() == TAB_CONTENTS_WEB) {
      PasswordManager* password_manager =
          parent_contents->AsWebContents()->GetPasswordManager();
      // Set the model for the login view. The model (password manager) is owned
      // by the view's parent TabContents, so natural destruction order means we
      // don't have to worry about calling SetModel(NULL), because the view will
      // be deleted before the password manager.
      view->SetModel(password_manager);
      std::vector<PasswordForm> v;
      MakeInputForPasswordManager(parent_contents->GetURL(), &v);
      password_manager->PasswordFormsSeen(v);
      handler_->set_password_manager(password_manager);
    }

    handler_->set_login_view(view);
    ConstrainedWindow* dialog =
        parent_contents->CreateConstrainedDialog(handler_, view);
    handler_->InitWithDialog(dialog);
  }

 private:
  // Helper to create a PasswordForm and stuff it into a vector as input
  // for PasswordManager::PasswordFormsSeen, the hook into PasswordManager.
  void MakeInputForPasswordManager(
      const GURL& origin_url,
      std::vector<PasswordForm>* password_manager_input) {
    PasswordForm dialog_form;
    if (LowerCaseEqualsASCII(auth_info_->scheme, "basic")) {
      dialog_form.scheme = PasswordForm::SCHEME_BASIC;
    } else if (LowerCaseEqualsASCII(auth_info_->scheme, "digest")) {
      dialog_form.scheme = PasswordForm::SCHEME_DIGEST;
    } else {
      dialog_form.scheme = PasswordForm::SCHEME_OTHER;
    }
    dialog_form.origin = origin_url;
    // TODO(timsteele): Shouldn't depend on HttpKey since a change to the
    // format would result in not being able to retrieve existing logins
    // for a site. Refactor HttpKey behavior to be more reusable.
    dialog_form.signon_realm =
        net::AuthCache::HttpKey(dialog_form.origin, *auth_info_);
    password_manager_input->push_back(dialog_form);
    // Set the password form for the handler (by copy).
    handler_->set_password_form(dialog_form);
  }

  // Info about who/where/what is asking for authentication.
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  // Where to send the authentication when obtained.
  // This is owned by the ResourceDispatcherHost that invoked us.
  LoginHandlerImpl* handler_;

  DISALLOW_EVIL_CONSTRUCTORS(LoginDialogTask);
};

// ----------------------------------------------------------------------------
// Public API

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop) {
  LoginHandlerImpl* handler = new LoginHandlerImpl(request, ui_loop);
  ui_loop->PostTask(FROM_HERE, new LoginDialogTask(auth_info, handler));
  return handler;
}

