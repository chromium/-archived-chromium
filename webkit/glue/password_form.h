// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PASSWORD_FORM_H__
#define WEBKIT_GLUE_PASSWORD_FORM_H__

#include <string>
#include <map>

#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace webkit_glue {

// The PasswordForm struct encapsulates information about a login form,
// which can be an HTML form or a dialog with username/password text fields.
//
// The Web Data database stores saved username/passwords and associated form
// metdata using a PasswordForm struct, typically one that was created from
// a parsed HTMLFormElement or LoginDialog, but the saved entries could have
// also been created by imported data from another browser.
//
// The PasswordManager implements a fuzzy-matching algorithm to compare saved
// PasswordForm entries against PasswordForms that were created from a parsed
// HTML or dialog form. As one might expect, the more data contained in one
// of the saved PasswordForms, the better the job the PasswordManager can do
// in matching it against the actual form it was saved on, and autofill
// accurately. But it is not always possible, especially when importing from
// other browsers with different data models, to copy over all the information
// about a particular "saved password entry" to our PasswordForm
// representation.
//
// The field descriptions in the struct specification below are
// intended to describe which fields are not strictly required when adding a saved
// password entry to the database and how they can affect the matching process.

struct PasswordForm {
  // Enum to differentiate between HTML form based authentication, and dialogs
  // using basic or digest schemes. Default is SCHEME_HTML. Only PasswordForms
  // of the same Scheme will be matched/autofilled against each other.
  enum Scheme {
    SCHEME_HTML,
    SCHEME_BASIC,
    SCHEME_DIGEST,
    SCHEME_OTHER
  } scheme;

  // The "Realm" for the sign-on (scheme, host, port for SCHEME_HTML, and
  // contains the HTTP realm for dialog-based forms).
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // The URL (minus query parameters) containing the form. This is the primary
  // data used by the PasswordManager to decide (in longest matching prefix
  // fashion) whether or not a given PasswordForm result from the database is a
  // good fit for a particular form on a page, so it must not be empty.
  GURL origin;

  // The action target of the form. This is the primary data used by the
  // PasswordManager for form autofill; that is, the action of the saved
  // credentials must match the action of the form on the page to be autofilled.
  // If this is empty / not available, it will result in a "restricted"
  // IE-like autofill policy, where we wait for the user to type in his
  // username before autofilling the password. In these cases, after successful
  // login the action URL will automatically be assigned by the
  // PasswordManager.
  //
  // When parsing an HTML form, this must always be set.
  GURL action;

  // The name of the submit button used. Optional; only used in scoring
  // of PasswordForm results from the database to make matches as tight as
  // possible.
  //
  // When parsing an HTML form, this must always be set.
  std::wstring submit_element;

  // The name of the username input element. Optional (improves scoring).
  //
  // When parsing an HTML form, this must always be set.
  std::wstring username_element;

  // The username. Optional.
  //
  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  std::wstring username_value;

  // The name of the password input element, Optional (improves scoring).
  //
  // When parsing an HTML form, this must always be set.
  std::wstring password_element;

  // The password. Required.
  //
  // When parsing an HTML form, this is typically empty.
  std::wstring password_value;

  // If the form was a change password form, the name of the
  // 'old password' input element. Optional.
  std::wstring old_password_element;

  // The old password. Optional.
  std::wstring old_password_value;

  // Whether or not this login was saved under an HTTPS session with a valid
  // SSL cert. We will never match or autofill a PasswordForm where
  // ssl_valid == true with a PasswordForm where ssl_valid == false. This means
  // passwords saved under HTTPS will never get autofilled onto an HTTP page.
  // When importing, this should be set to true if the page URL is HTTPS, thus
  // giving it "the benefit of the doubt" that the SSL cert was valid when it
  // was saved. Default to false.
  bool ssl_valid;

  // True if this PasswordForm represents the last username/password login the
  // user selected to log in to the site. If there is only one saved entry for
  // the site, this will always be true, but when there are multiple entries
  // the PasswordManager ensures that only one of them has a preferred bit set
  // to true. Default to false.
  //
  // When parsing an HTML form, this is not used.
  bool preferred;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blacklisted_by_user;

  PasswordForm()
      : scheme(SCHEME_HTML),
        ssl_valid(false),
        preferred(false),
        blacklisted_by_user(false) {
  }
};

// Map username to PasswordForm* for convenience. See password_form_manager.h.
typedef std::map<std::wstring, PasswordForm*> PasswordFormMap;

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_FORM_H__
