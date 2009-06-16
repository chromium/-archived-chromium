// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

#include <CoreServices/CoreServices.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/keychain_mac.h"

using webkit_glue::PasswordForm;

namespace internal_keychain_helpers {

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

// The time string is of the form "yyyyMMddHHmmss'Z", in UTC time.
bool TimeFromKeychainTimeString(const char* time_string_bytes,
                                unsigned int byte_length,
                                base::Time* time) {
  DCHECK(time);

  char* time_string = static_cast<char*>(malloc(byte_length + 1));
  memcpy(time_string, time_string_bytes, byte_length);
  time_string[byte_length] = '\0';
  base::Time::Exploded exploded_time;
  bzero(&exploded_time, sizeof(exploded_time));
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

PasswordForm::Scheme SchemeForAuthType(SecAuthenticationType auth_type) {
  switch (auth_type) {
    case kSecAuthenticationTypeHTMLForm:   return PasswordForm::SCHEME_HTML;
    case kSecAuthenticationTypeHTTPBasic:  return PasswordForm::SCHEME_BASIC;
    case kSecAuthenticationTypeHTTPDigest: return PasswordForm::SCHEME_DIGEST;
    default:                               return PasswordForm::SCHEME_OTHER;
  }
}

// Searches |keychain| for all items usable for the given signon_realm, and
// puts them in |items|. The caller is responsible for calling keychain->Free
// on each of them when it is finished with them.
void FindMatchingKeychainItems(const MacKeychain& keychain,
                               const std::string& signon_realm,
                               PasswordForm::Scheme scheme,
                               std::vector<SecKeychainItemRef>* items) {
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
    return;
  }

  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type =
      internal_keychain_helpers::AuthTypeForScheme(scheme);

  internal_keychain_helpers::KeychainSearch keychain_search(keychain);
  keychain_search.Init(server.c_str(), port, protocol, auth_type,
                       (scheme == PasswordForm::SCHEME_HTML) ?
                           NULL : security_domain.c_str(),
                       NULL, NULL);
  keychain_search.FindMatchingItems(items);
}

void FindMatchingKeychainItem(const MacKeychain& keychain,
                              const PasswordForm& form,
                              SecKeychainItemRef* match) {
  DCHECK(match);
  *match = NULL;

  // We don't store blacklist entries in the keychain, so the answer to "what
  // Keychain item goes with this form" is always "nothing" for blacklists.
  if (form.blacklisted_by_user) {
    return;
  }

  // Construct a keychain search based on all the unique attributes.
  std::string server;
  std::string security_domain;
  int port;
  bool is_secure;
  if (!internal_keychain_helpers::ExtractSignonRealmComponents(
          form.signon_realm, &server, &port, &is_secure, &security_domain)) {
    // TODO(stuartmorgan): Proxies will currently fail here, since their
    // signon_realm is not a URL. We need to detect the proxy case and handle
    // it specially.
    return;
  }

  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type =
      internal_keychain_helpers::AuthTypeForScheme(form.scheme);
  std::string path = form.origin.path();
  std::string username = WideToUTF8(form.username_value);

  internal_keychain_helpers::KeychainSearch keychain_search(keychain);
  keychain_search.Init(server.c_str(), port, protocol, auth_type,
                       (form.scheme == PasswordForm::SCHEME_HTML) ?
                           NULL : security_domain.c_str(),
                       path.c_str(), username.c_str());

  std::vector<SecKeychainItemRef> matches;
  keychain_search.FindMatchingItems(&matches);

  if (matches.size() > 0) {
    *match = matches[0];
    // Free everything we aren't returning.
    for (std::vector<SecKeychainItemRef>::iterator i = matches.begin() + 1;
         i != matches.end(); ++i) {
      keychain.Free(*i);
    }
  }
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
        form->scheme = internal_keychain_helpers::SchemeForAuthType(auth_type);
        break;
      }
      case kSecSecurityDomainItemAttr:
        security_domain.assign(static_cast<const char *>(attr.data),
                               attr.length);
        break;
      case kSecCreationDateItemAttr:
        // The only way to get a date out of Keychain is as a string. Really.
        // (The docs claim it's an int, but the header is correct.)
        internal_keychain_helpers::TimeFromKeychainTimeString(
            static_cast<char*>(attr.data), attr.length, &form->date_created);
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

  form->origin = internal_keychain_helpers::URLFromComponents(form->ssl_valid,
                                                              server, port,
                                                              path);
  // TODO(stuartmorgan): Handle proxies, which need a different signon_realm
  // format.
  form->signon_realm = form->origin.GetOrigin().spec();
  if (form->scheme != PasswordForm::SCHEME_HTML) {
    form->signon_realm.append(security_domain);
  }
  return true;
}

}  // internal_keychain_helpers

#pragma mark -

PasswordStoreMac::PasswordStoreMac(MacKeychain* keychain)
    : keychain_(keychain) {
  DCHECK(keychain_.get());
}

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
  std::vector<SecKeychainItemRef> keychain_items;

  internal_keychain_helpers::FindMatchingKeychainItems(
      *keychain_, request->form.signon_realm, request->form.scheme,
      &keychain_items);

  std::vector<PasswordForm*> forms;

  for (std::vector<SecKeychainItemRef>::iterator i = keychain_items.begin();
       i != keychain_items.end(); ++i) {
    // Consumer is responsible for deleting the forms when they are done...
    PasswordForm* form = new PasswordForm();
    SecKeychainItemRef keychain_item = *i;
    if (internal_keychain_helpers::FillPasswordFormFromKeychainItem(
            *keychain_, keychain_item, form)) {
      forms.push_back(form);
    }
    // ... but we need to clean up the keychain item.
    keychain_->Free(keychain_item);
  }

  NotifyConsumer(request, forms);
}
