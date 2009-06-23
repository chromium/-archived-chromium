// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_

#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/time.h"
#include "chrome/browser/keychain_mac.h"

namespace internal_keychain_helpers {

// Takes a PasswordForm's signon_realm and parses it into its component parts,
// which are returned though the appropriate out parameters.
// Returns true if it can be successfully parsed, in which case all out params
// that are non-NULL will be set. If there is no port, port will be 0.
// If the return value is false, the state of the our params is undefined.
bool ExtractSignonRealmComponents(const std::string& signon_realm,
                                  std::string* server, int* port,
                                  bool* is_secure,
                                  std::string* security_domain);

// Returns a URL built from the given components. To create a URL without a
// port, pass kAnyPort for the |port| parameter.
GURL URLFromComponents(bool is_secure, const std::string& host, int port,
                       const std::string& path);

// Converts a Keychain time string to a Time object, returning true if
// time_string_bytes was parsable. If the return value is false, the value of
// |time| is unchanged.
bool TimeFromKeychainTimeString(const char* time_string_bytes,
                                unsigned int byte_length,
                                base::Time* time);

// Returns the Keychain SecAuthenticationType type corresponding to |scheme|.
SecAuthenticationType AuthTypeForScheme(
    webkit_glue::PasswordForm::Scheme scheme);

// Returns the PasswordForm Scheme corresponding to |auth_type|.
webkit_glue::PasswordForm::Scheme SchemeForAuthType(
    SecAuthenticationType auth_type);

// Searches |keychain| for all items usable for the given signon_realm, and
// puts them in |items|. The caller is responsible for calling keychain->Free
// on each of them when it is finished with them.
void FindMatchingKeychainItems(const MacKeychain& keychain,
                               const std::string& signon_realm,
                               webkit_glue::PasswordForm::Scheme scheme,
                               std::vector<SecKeychainItemRef>* items);

// Searches |keychain| for the specific keychain entry matching the given form,
// and returns it (or NULL if no match is found).
// The caller is responsible for calling keychain->Free on the returned item.
SecKeychainItemRef FindMatchingKeychainItem(
    const MacKeychain& keychain, const webkit_glue::PasswordForm& form);

// Sets the fields of |form| based on the keychain data from |keychain_item|.
// Fields that can't be determined from |keychain_item| will be unchanged.
//
// IMPORTANT: This function can cause the OS to trigger UI (to allow access to
// the keychain item if we aren't trusted for the item), and block until the UI
// is dismissed.
//
// If excessive prompting for access to other applications' keychain items
// becomes an issue, the password storage API will need to be refactored to
// allow the password to be retrieved later (accessing other fields doesn't
// require authorization).
bool FillPasswordFormFromKeychainItem(const MacKeychain& keychain,
                                      const SecKeychainItemRef& keychain_item,
                                      webkit_glue::PasswordForm* form);

// Creates a new keychain entry from |form|, or updates the password of an
// existing keychain entry if there is a collision. Returns true if a keychain
// entry was successfully added/updated.
bool AddKeychainEntryForForm(const MacKeychain& keychain,
                             const webkit_glue::PasswordForm& form);

// Changes the password for keychain_item to |password|; returns true if the
// password was successfully changed.
bool SetKeychainItemPassword(const MacKeychain& keychain,
                             const SecKeychainItemRef& keychain_item,
                             const std::string& password);

// Returns true if the two given forms match based on signon_reaml, scheme, and
// username_value, and are thus suitable for merging (see MergePasswordForms).
// If this returns true, and path_matches is non-NULL, *path_matches will be set
// based on whether the full origin matches as well.
bool FormsMatchForMerge(const webkit_glue::PasswordForm& form_a,
                        const webkit_glue::PasswordForm& form_b,
                        bool* path_matches);

// Populates merged_forms by combining the password data from keychain_forms and
// the metadata from database_forms, removing used entries from the two source
// lists.
//
// On return, database_forms and keychain_forms will have only unused
// entries; for database_forms that means entries for which no corresponding
// password can be found (and which aren't blacklist entries), but for
// keychain_forms it's only entries we explicitly choose not to use (e.g.,
// blacklist entries from other browsers). Keychain entries that we have no
// database matches for will still end up in merged_forms, since they have
// enough information to be used as imported passwords.
void MergePasswordForms(std::vector<webkit_glue::PasswordForm*>* keychain_forms,
                        std::vector<webkit_glue::PasswordForm*>* database_forms,
                        std::vector<webkit_glue::PasswordForm*>* merged_forms);

}  // internal_keychain_helpers

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_INTERNAL_H_
