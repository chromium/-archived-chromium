// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

#include <CoreServices/CoreServices.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/password_manager/login_database_mac.h"

using webkit_glue::PasswordForm;

// Utility class to handle the details of constructing and running a keychain
// search from a set of attributes.
class KeychainSearch {
 public:
  KeychainSearch(const MacKeychain& keychain);
  ~KeychainSearch();

  // Sets up a keycahin search based on an non "null" (NULL for char*,
  // The appropriate "Any" entry for other types) arguments.
  //
  // IMPORTANT: Any paramaters passed in *must* remain valid for as long as the
  // KeychainSearch object, since the search uses them by reference.
  void Init(const char* server, const UInt32& port,
            const SecProtocolType& protocol,
            const SecAuthenticationType& auth_type, const char* security_domain,
            const char* path, const char* username);

  // Fills |items| with all Keychain items that match the Init'd search.
  // If the search fails for any reason, |items| will be unchanged.
  void FindMatchingItems(std::vector<SecKeychainItemRef>* matches);

 private:
  const MacKeychain* keychain_;
  SecKeychainAttributeList search_attributes_;
  SecKeychainSearchRef search_ref_;
};

KeychainSearch::KeychainSearch(const MacKeychain& keychain)
    : keychain_(&keychain), search_ref_(NULL) {
  search_attributes_.count = 0;
  search_attributes_.attr = NULL;
}

KeychainSearch::~KeychainSearch() {
  if (search_attributes_.attr) {
    free(search_attributes_.attr);
  }
}

void KeychainSearch::Init(const char* server, const UInt32& port,
                          const SecProtocolType& protocol,
                          const SecAuthenticationType& auth_type,
                          const char* security_domain, const char* path,
                          const char* username) {
  // Allocate enough to hold everything we might use.
  const unsigned int kMaxEntryCount = 7;
  search_attributes_.attr =
      static_cast<SecKeychainAttribute*>(calloc(kMaxEntryCount,
                                                sizeof(SecKeychainAttribute)));
  unsigned int entries = 0;
  // We only use search_attributes_ with SearchCreateFromAttributes, which takes
  // a "const SecKeychainAttributeList *", so we trust that they won't try
  // to modify the list, and that casting away const-ness is thus safe.
  if (server != NULL) {
    DCHECK(entries < kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecServerItemAttr;
    search_attributes_.attr[entries].length = strlen(server);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(server));
    ++entries;
  }
  if (port != kAnyPort) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecPortItemAttr;
    search_attributes_.attr[entries].length = sizeof(port);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(&port));
    ++entries;
  }
  if (protocol != kSecProtocolTypeAny) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecProtocolItemAttr;
    search_attributes_.attr[entries].length = sizeof(protocol);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(&protocol));
    ++entries;
  }
  if (auth_type != kSecAuthenticationTypeAny) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecAuthenticationTypeItemAttr;
    search_attributes_.attr[entries].length = sizeof(auth_type);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(&auth_type));
    ++entries;
  }
  if (security_domain != NULL && strlen(security_domain) > 0) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecSecurityDomainItemAttr;
    search_attributes_.attr[entries].length = strlen(security_domain);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(security_domain));
    ++entries;
  }
  if (path != NULL && strlen(path) > 0 && strcmp(path, "/") != 0) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecPathItemAttr;
    search_attributes_.attr[entries].length = strlen(path);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(path));
    ++entries;
  }
  if (username != NULL) {
    DCHECK(entries <= kMaxEntryCount);
    search_attributes_.attr[entries].tag = kSecAccountItemAttr;
    search_attributes_.attr[entries].length = strlen(username);
    search_attributes_.attr[entries].data =
        const_cast<void*>(reinterpret_cast<const void*>(username));
    ++entries;
  }
  search_attributes_.count = entries;
}

void KeychainSearch::FindMatchingItems(std::vector<SecKeychainItemRef>* items) {
  OSStatus result = keychain_->SearchCreateFromAttributes(
      NULL, kSecInternetPasswordItemClass, &search_attributes_, &search_ref_);

  if (result != noErr) {
    LOG(ERROR) << "Keychain lookup failed with error " << result;
    return;
  }

  SecKeychainItemRef keychain_item;
  while (keychain_->SearchCopyNext(search_ref_, &keychain_item) == noErr) {
    // Consumer is responsible for deleting the forms when they are done.
    items->push_back(keychain_item);
  }

  keychain_->Free(search_ref_);
  search_ref_ = NULL;
}

#pragma mark -

// TODO(stuartmorgan): Convert most of this to private helpers in
// MacKeychainPaswordFormAdapter once it has sufficient higher-level public
// methods to provide test coverage.
namespace internal_keychain_helpers {

// Takes a PasswordForm's signon_realm and parses it into its component parts,
// which are returned though the appropriate out parameters.
// Returns true if it can be successfully parsed, in which case all out params
// that are non-NULL will be set. If there is no port, port will be 0.
// If the return value is false, the state of the our params is undefined.
//
// TODO(stuartmorgan): signon_realm for proxies is not yet supported.
bool ExtractSignonRealmComponents(const std::string& signon_realm,
                                  std::string* server, int* port,
                                  bool* is_secure,
                                  std::string* security_domain) {
  // The signon_realm will be the Origin portion of a URL for an HTML form,
  // and the same but with the security domain as a path for HTTP auth.
  GURL realm_as_url(signon_realm);
  if (!realm_as_url.is_valid()) {
    return false;
  }

  if (server)
    *server = realm_as_url.host();
  if (is_secure)
    *is_secure = realm_as_url.SchemeIsSecure();
  if (port)
    *port = realm_as_url.has_port() ? atoi(realm_as_url.port().c_str()) : 0;
  if (security_domain) {
    // Strip the leading '/' off of the path to get the security domain.
    *security_domain = realm_as_url.path().substr(1);
  }
  return true;
}

// Returns a URL built from the given components. To create a URL without a
// port, pass kAnyPort for the |port| parameter.
GURL URLFromComponents(bool is_secure, const std::string& host, int port,
                       const std::string& path) {
  GURL::Replacements url_components;
  std::string scheme(is_secure ? "https" : "http");
  url_components.SetSchemeStr(scheme);
  url_components.SetHostStr(host);
  std::string port_string;  // Must remain in scope until after we do replacing.
  if (port != kAnyPort) {
    std::ostringstream port_stringstream;
    port_stringstream << port;
    port_string = port_stringstream.str();
    url_components.SetPortStr(port_string);
  }
  url_components.SetPathStr(path);

  GURL url("http://dummy.com");  // ReplaceComponents needs a valid URL.
  return url.ReplaceComponents(url_components);
}

// Converts a Keychain time string to a Time object, returning true if
// time_string_bytes was parsable. If the return value is false, the value of
// |time| is unchanged.
bool TimeFromKeychainTimeString(const char* time_string_bytes,
                                unsigned int byte_length,
                                base::Time* time) {
  DCHECK(time);

  char* time_string = static_cast<char*>(malloc(byte_length + 1));
  memcpy(time_string, time_string_bytes, byte_length);
  time_string[byte_length] = '\0';
  base::Time::Exploded exploded_time;
  bzero(&exploded_time, sizeof(exploded_time));
  // The time string is of the form "yyyyMMddHHmmss'Z", in UTC time.
  int assignments = sscanf(time_string, "%4d%2d%2d%2d%2d%2dZ",
                           &exploded_time.year, &exploded_time.month,
                           &exploded_time.day_of_month, &exploded_time.hour,
                           &exploded_time.minute, &exploded_time.second);
  free(time_string);

  if (assignments == 6) {
    *time = base::Time::FromUTCExploded(exploded_time);
    return true;
  }
  return false;
}

// Returns the Keychain SecAuthenticationType type corresponding to |scheme|.
SecAuthenticationType AuthTypeForScheme(PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::SCHEME_HTML:   return kSecAuthenticationTypeHTMLForm;
    case PasswordForm::SCHEME_BASIC:  return kSecAuthenticationTypeHTTPBasic;
    case PasswordForm::SCHEME_DIGEST: return kSecAuthenticationTypeHTTPDigest;
    case PasswordForm::SCHEME_OTHER:  return kSecAuthenticationTypeDefault;
  }
  NOTREACHED();
  return kSecAuthenticationTypeDefault;
}

// Returns the PasswordForm Scheme corresponding to |auth_type|.
PasswordForm::Scheme SchemeForAuthType(SecAuthenticationType auth_type) {
  switch (auth_type) {
    case kSecAuthenticationTypeHTMLForm:   return PasswordForm::SCHEME_HTML;
    case kSecAuthenticationTypeHTTPBasic:  return PasswordForm::SCHEME_BASIC;
    case kSecAuthenticationTypeHTTPDigest: return PasswordForm::SCHEME_DIGEST;
    default:                               return PasswordForm::SCHEME_OTHER;
  }
}

SecKeychainItemRef MatchingKeychainItem(const MacKeychain& keychain,
                                        const PasswordForm& form) {
  // We don't store blacklist entries in the keychain, so the answer to "what
  // Keychain item goes with this form" is always "nothing" for blacklists.
  if (form.blacklisted_by_user) {
    return NULL;
  }

  // Construct a keychain search based on all the unique attributes.
  std::string server;
  std::string security_domain;
  int port;
  bool is_secure;
  if (!ExtractSignonRealmComponents(form.signon_realm, &server, &port,
                                    &is_secure, &security_domain)) {
    // TODO(stuartmorgan): Proxies will currently fail here, since their
    // signon_realm is not a URL. We need to detect the proxy case and handle
    // it specially.
    return NULL;
  }

  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type = AuthTypeForScheme(form.scheme);
  std::string path = form.origin.path();
  std::string username = WideToUTF8(form.username_value);

  KeychainSearch keychain_search(keychain);
  keychain_search.Init(server.c_str(), port, protocol, auth_type,
                       (form.scheme == PasswordForm::SCHEME_HTML) ?
                           NULL : security_domain.c_str(),
                       path.c_str(), username.c_str());

  std::vector<SecKeychainItemRef> matches;
  keychain_search.FindMatchingItems(&matches);

  if (matches.size() == 0) {
    return NULL;
  }
  // Free all items after the first, since we won't be returning them.
  for (std::vector<SecKeychainItemRef>::iterator i = matches.begin() + 1;
       i != matches.end(); ++i) {
    keychain.Free(*i);
  }
  return matches[0];
}

bool FillPasswordFormFromKeychainItem(const MacKeychain& keychain,
                                      const SecKeychainItemRef& keychain_item,
                                      PasswordForm* form) {
  DCHECK(form);

  SecKeychainAttributeInfo attrInfo;
  UInt32 tags[] = { kSecAccountItemAttr,
                    kSecServerItemAttr,
                    kSecPortItemAttr,
                    kSecPathItemAttr,
                    kSecProtocolItemAttr,
                    kSecAuthenticationTypeItemAttr,
                    kSecSecurityDomainItemAttr,
                    kSecCreationDateItemAttr,
                    kSecNegativeItemAttr };
  attrInfo.count = arraysize(tags);
  attrInfo.tag = tags;
  attrInfo.format = NULL;

  SecKeychainAttributeList *attrList;
  UInt32 password_length;
  void* password_data;
  OSStatus result = keychain.ItemCopyAttributesAndData(keychain_item, &attrInfo,
                                                       NULL, &attrList,
                                                       &password_length,
                                                       &password_data);

  if (result != noErr) {
    // We don't log errSecAuthFailed because that just means that the user
    // chose not to allow us access to the item.
    if (result != errSecAuthFailed) {
      LOG(ERROR) << "Keychain data load failed: " << result;
    }
    return false;
  }

  UTF8ToWide(static_cast<const char *>(password_data), password_length,
             &(form->password_value));

  int port = kAnyPort;
  std::string server;
  std::string security_domain;
  std::string path;
  for (unsigned int i = 0; i < attrList->count; i++) {
    SecKeychainAttribute attr = attrList->attr[i];
    if (!attr.data) {
      continue;
    }
    switch (attr.tag) {
      case kSecAccountItemAttr:
        UTF8ToWide(static_cast<const char *>(attr.data), attr.length,
                   &(form->username_value));
        break;
      case kSecServerItemAttr:
        server.assign(static_cast<const char *>(attr.data), attr.length);
        break;
      case kSecPortItemAttr:
        port = *(static_cast<UInt32*>(attr.data));
        break;
      case kSecPathItemAttr:
        path.assign(static_cast<const char *>(attr.data), attr.length);
        break;
      case kSecProtocolItemAttr:
      {
        SecProtocolType protocol = *(static_cast<SecProtocolType*>(attr.data));
        // TODO(stuartmorgan): Handle proxy types
        form->ssl_valid = (protocol == kSecProtocolTypeHTTPS);
        break;
      }
      case kSecAuthenticationTypeItemAttr:
      {
        SecAuthenticationType auth_type =
            *(static_cast<SecAuthenticationType*>(attr.data));
        form->scheme = SchemeForAuthType(auth_type);
        break;
      }
      case kSecSecurityDomainItemAttr:
        security_domain.assign(static_cast<const char *>(attr.data),
                               attr.length);
        break;
      case kSecCreationDateItemAttr:
        // The only way to get a date out of Keychain is as a string. Really.
        // (The docs claim it's an int, but the header is correct.)
        TimeFromKeychainTimeString(static_cast<char*>(attr.data), attr.length,
                                   &form->date_created);
        break;
      case kSecNegativeItemAttr:
        Boolean negative_item = *(static_cast<Boolean*>(attr.data));
        if (negative_item) {
          form->blacklisted_by_user = true;
        }
        break;
    }
  }
  keychain.ItemFreeAttributesAndData(attrList, password_data);

  // kSecNegativeItemAttr doesn't seem to actually be in widespread use. In
  // practice, other browsers seem to use a "" or " " password (and a special
  // user name) to indicated blacklist entries.
  if (form->password_value.empty() || form->password_value == L" ") {
    form->blacklisted_by_user = true;
  }

  form->origin = URLFromComponents(form->ssl_valid, server, port, path);
  // TODO(stuartmorgan): Handle proxies, which need a different signon_realm
  // format.
  form->signon_realm = form->origin.GetOrigin().spec();
  if (form->scheme != PasswordForm::SCHEME_HTML) {
    form->signon_realm.append(security_domain);
  }
  return true;
}

bool FormsMatchForMerge(const PasswordForm& form_a, const PasswordForm& form_b,
                        bool* path_matches) {
  // We never merge blacklist entries between our store and the keychain.
  if (form_a.blacklisted_by_user || form_b.blacklisted_by_user) {
    return false;
  }
  bool matches = form_a.scheme == form_b.scheme &&
                 form_a.signon_realm == form_b.signon_realm &&
                 form_a.username_value == form_b.username_value;
  if (matches && path_matches) {
    *path_matches = (form_a.origin == form_b.origin);
  }
  return matches;
}

// Returns an the best match for |form| from |keychain_forms|, or NULL if there
// is no suitable match.
PasswordForm* BestKeychainFormForForm(
    const PasswordForm& base_form,
    const std::vector<PasswordForm*>* keychain_forms) {
  PasswordForm* partial_match = NULL;
  for (std::vector<PasswordForm*>::const_iterator i = keychain_forms->begin();
       i != keychain_forms->end(); ++i) {
    // TODO(stuartmorgan): We should really be scoring path matches and picking
    // the best, rather than just checking exact-or-not (although in practice
    // keychain items with paths probably came from us).
    bool path_matches = false;
    if (FormsMatchForMerge(base_form, *(*i), &path_matches)) {
      if (path_matches) {
        return *i;
      } else if (!partial_match) {
        partial_match = *i;
      }
    }
  }
  return partial_match;
}

void MergePasswordForms(std::vector<PasswordForm*>* keychain_forms,
                        std::vector<PasswordForm*>* database_forms,
                        std::vector<PasswordForm*>* merged_forms) {
  std::set<PasswordForm*> used_keychain_forms;
  for (std::vector<PasswordForm*>::iterator i = database_forms->begin();
       i != database_forms->end();) {
    PasswordForm* db_form = *i;
    bool use_form = false;
    if (db_form->blacklisted_by_user) {
      // Blacklist entries aren't merged, so just take it directly.
      use_form = true;
    } else {
      // Check for a match in the keychain list.
      PasswordForm* best_match = BestKeychainFormForForm(*db_form,
                                                         keychain_forms);
      if (best_match) {
        used_keychain_forms.insert(best_match);
        db_form->password_value = best_match->password_value;
        use_form = true;
      }
    }
    if (use_form) {
      merged_forms->push_back(db_form);
      i = database_forms->erase(i);
    } else {
      ++i;
    }
  }
  // Find any remaining keychain entries that we want, and clear out everything
  // we used.
  for (std::vector<PasswordForm*>::iterator i = keychain_forms->begin();
       i != keychain_forms->end();) {
    PasswordForm* keychain_form = *i;
    if (keychain_form->blacklisted_by_user) {
      ++i;
    } else {
      if (used_keychain_forms.find(keychain_form) == used_keychain_forms.end())
        merged_forms->push_back(keychain_form);
      else
        delete keychain_form;
      i = keychain_forms->erase(i);
    }
  }
}

}  // namespace internal_keychain_helpers

#pragma mark -

MacKeychainPasswordFormAdapter::MacKeychainPasswordFormAdapter(
    MacKeychain* keychain) : keychain_(keychain) {
}

// Returns PasswordForms for each keychain entry matching |form|.
// Caller is responsible for deleting the returned forms.
std::vector<PasswordForm*>
    MacKeychainPasswordFormAdapter::PasswordsMatchingForm(
        const PasswordForm& query_form) {
  std::vector<SecKeychainItemRef> keychain_items =
      MatchingKeychainItems(query_form.signon_realm, query_form.scheme);

  std::vector<PasswordForm*> keychain_forms =
      CreateFormsFromKeychainItems(keychain_items);
  for (std::vector<SecKeychainItemRef>::iterator i = keychain_items.begin();
       i != keychain_items.end(); ++i) {
    keychain_->Free(*i);
  }
  return keychain_forms;
}

bool MacKeychainPasswordFormAdapter::AddLogin(const PasswordForm& form) {
  std::string server;
  std::string security_domain;
  int port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
           form.signon_realm, &server, &port, &is_secure, &security_domain)) {
    return false;
  }
  std::string username = WideToUTF8(form.username_value);
  std::string password = WideToUTF8(form.password_value);
  std::string path = form.origin.path();
  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  OSStatus result = keychain_->AddInternetPassword(
      NULL, server.size(), server.c_str(),
      security_domain.size(), security_domain.c_str(),
      username.size(), username.c_str(),
      path.size(), path.c_str(),
      port, protocol, internal_keychain_helpers::AuthTypeForScheme(form.scheme),
      password.size(), password.c_str(), NULL);

  // If we collide with an existing item, find and update it instead.
  if (result == errSecDuplicateItem) {
    SecKeychainItemRef existing_item =
        internal_keychain_helpers::MatchingKeychainItem(*keychain_, form);
    if (!existing_item) {
      return false;
    }
    bool changed = SetKeychainItemPassword(existing_item, password);
    keychain_->Free(existing_item);
    return changed;
  }

  return (result == noErr);
}

std::vector<PasswordForm*>
    MacKeychainPasswordFormAdapter::CreateFormsFromKeychainItems(
        const std::vector<SecKeychainItemRef>& items) {
  std::vector<PasswordForm*> keychain_forms;
  for (std::vector<SecKeychainItemRef>::const_iterator i = items.begin();
       i != items.end(); ++i) {
    PasswordForm* form = new PasswordForm();
    if (internal_keychain_helpers::FillPasswordFormFromKeychainItem(*keychain_,
                                                                    *i, form)) {
      keychain_forms.push_back(form);
    }
  }
  return keychain_forms;
}

// Searches |keychain| for all items usable for the given signon_realm, and
// returns them. The caller is responsible for calling keychain->Free
// on each of them when it is finished with them.
std::vector<SecKeychainItemRef>
    MacKeychainPasswordFormAdapter::MatchingKeychainItems(
        const std::string& signon_realm, PasswordForm::Scheme scheme) {
  std::vector<SecKeychainItemRef> matches;
  // Construct a keychain search based on the signon_realm and scheme.
  std::string server;
  std::string security_domain;
  int port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
           signon_realm, &server, &port, &is_secure, &security_domain)) {
    // TODO(stuartmorgan): Proxies will currently fail here, since their
    // signon_realm is not a URL. We need to detect the proxy case and handle
    // it specially.
    return matches;
  }

  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type =
      internal_keychain_helpers::AuthTypeForScheme(scheme);

  KeychainSearch keychain_search(*keychain_);
  keychain_search.Init(server.c_str(), port, protocol, auth_type,
                       (scheme == PasswordForm::SCHEME_HTML) ?
                           NULL : security_domain.c_str(),
                       NULL, NULL);
  keychain_search.FindMatchingItems(&matches);
  return matches;
}

bool MacKeychainPasswordFormAdapter::SetKeychainItemPassword(
    const SecKeychainItemRef& keychain_item, const std::string& password) {
  OSStatus result = keychain_->ItemModifyAttributesAndData(keychain_item, NULL,
                                                           password.size(),
                                                           password.c_str());
  return (result == noErr);
}

#pragma mark -

PasswordStoreMac::PasswordStoreMac(MacKeychain* keychain,
                                   LoginDatabaseMac* login_db)
    : keychain_(keychain), login_metadata_db_(login_db) {
  DCHECK(keychain_.get());
  DCHECK(login_metadata_db_.get());
}

PasswordStoreMac::~PasswordStoreMac() {}

void PasswordStoreMac::AddLoginImpl(const PasswordForm& form) {
  NOTIMPLEMENTED();
}

void PasswordStoreMac::UpdateLoginImpl(const PasswordForm& form) {
  NOTIMPLEMENTED();
}

void PasswordStoreMac::RemoveLoginImpl(const PasswordForm& form) {
  NOTIMPLEMENTED();
}

void PasswordStoreMac::GetLoginsImpl(GetLoginsRequest* request) {
  MacKeychainPasswordFormAdapter keychainAdapter(keychain_.get());
  std::vector<PasswordForm*> keychain_forms =
      keychainAdapter.PasswordsMatchingForm(request->form);

  std::vector<PasswordForm*> database_forms;
  login_metadata_db_->GetLogins(request->form, &database_forms);

  std::vector<PasswordForm*> merged_forms;
  internal_keychain_helpers::MergePasswordForms(&keychain_forms,
                                                &database_forms,
                                                &merged_forms);

  // Clean up any orphaned database entries.
  for (std::vector<PasswordForm*>::iterator i = database_forms.begin();
       i != database_forms.end(); ++i) {
    login_metadata_db_->RemoveLogin(**i);
  }
  // Delete the forms we aren't returning.
  STLDeleteElements(&database_forms);
  STLDeleteElements(&keychain_forms);

  NotifyConsumer(request, merged_forms);
}
