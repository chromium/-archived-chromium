// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/views/login_view.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "views/window/dialog_delegate.h"

using views::LoginView;
using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerWin

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerWin : public LoginHandler,
                        public base::RefCountedThreadSafe<LoginHandlerWin>,
                        public views::DialogDelegate {
 public:
  LoginHandlerWin(URLRequest* request, MessageLoop* ui_loop)
      : dialog_(NULL),
        handled_auth_(false),
        request_(request),
        request_loop_(MessageLoop::current()),
        ui_loop_(ui_loop),
        password_manager_(NULL) {
    DCHECK(request_) << "LoginHandler constructed with NULL request";

    AddRef();  // matched by ReleaseLater.
    if (!ResourceDispatcherHost::RenderViewForRequest(request_,
                                                      &render_process_host_id_,
                                                      &tab_contents_id_)) {
      NOTREACHED();
    }
  }

  ~LoginHandlerWin() {
  }

  void set_login_view(LoginView* login_view) {
    login_view_ = login_view;
  }

  // views::DialogDelegate methods:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    if (button == MessageBoxFlags::DIALOGBUTTON_OK)
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
          this, &LoginHandlerWin::CancelAuthDeferred));
      SendNotifications();
    }
  }
  virtual void DeleteDelegate() {
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

  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           std::wstring explanation) {
    DCHECK(MessageLoop::current() == ui_loop_);

    LoginView* view = new LoginView(explanation);

    // Set the model for the login view. The model (password manager) is owned
    // by the view's parent TabContents, so natural destruction order means we
    // don't have to worry about calling SetModel(NULL), because the view will
    // be deleted before the password manager.
    view->SetModel(manager);

    set_login_view(view);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    dialog_ = GetTabContentsForLogin()->CreateConstrainedDialog(this);
    SendNotifications();
  }

  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) {
    password_form_ = form;
  }

  virtual void SetPasswordManager(PasswordManager* password_manager) {
    password_manager_ = password_manager;
  }

  virtual TabContents* GetTabContentsForLogin() {
    DCHECK(MessageLoop::current() == ui_loop_);

    return tab_util::GetTabContentsByID(render_process_host_id_,
                                        tab_contents_id_);
  }

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
        this, &LoginHandlerWin::CloseContentsDeferred));
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerWin::SendNotifications));
    request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerWin::SetAuthDeferred, username, password));
  }

  virtual void CancelAuth() {
    if (WasAuthHandled(true))
      return;

    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerWin::CloseContentsDeferred));
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerWin::SendNotifications));
    request_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &LoginHandlerWin::CancelAuthDeferred));
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
  friend class LoginPrompt;

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
    TabContents* requesting_contents = GetTabContentsForLogin();
    if (!requesting_contents)
      return;

    NavigationController* controller = &requesting_contents->controller();

    if (!WasAuthHandled(false)) {
      LoginNotificationDetails details(this);
      service->Notify(NotificationType::AUTH_NEEDED,
                      Source<NavigationController>(controller),
                      Details<LoginNotificationDetails>(&details));
    } else {
      service->Notify(NotificationType::AUTH_SUPPLIED,
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
  // Can be null if the TabContents is not a TabContents.
  // This should only be accessed on the ui_loop_.
  PasswordManager* password_manager_;

  // Cached from the URLRequest, in case it goes NULL on us.
  int render_process_host_id_;
  int tab_contents_id_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerWin);
};

// static
LoginHandler* LoginHandler::Create(URLRequest* request, MessageLoop* ui_loop) {
  return new LoginHandlerWin(request, ui_loop);
}
