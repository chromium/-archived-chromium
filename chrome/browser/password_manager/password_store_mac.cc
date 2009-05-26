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

namespace internal_keychain_helpers {

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

  const char* server_c_str = server.c_str();
  UInt32 port_uint = port;
  SecProtocolType protocol = is_secure ? kSecProtocolTypeHTTPS
                                       : kSecProtocolTypeHTTP;
  SecAuthenticationType auth_type =
      internal_keychain_helpers::AuthTypeForScheme(scheme);
  const char* security_domain_c_str = security_domain.c_str();

  // kSecSecurityDomainItemAttr must be last, so that it can be "removed" when
  // not applicable.
  SecKeychainAttribute attributes[] = {
    { kSecServerItemAttr, strlen(server_c_str),
      const_cast<void*>(reinterpret_cast<const void*>(server_c_str)) },
    { kSecPortItemAttr, sizeof(port_uint), static_cast<void*>(&port_uint) },
    { kSecProtocolItemAttr, sizeof(protocol), static_cast<void*>(&protocol) },
    { kSecAuthenticationTypeItemAttr, sizeof(auth_type),
      static_cast<void*>(&auth_type) },
    { kSecSecurityDomainItemAttr, strlen(security_domain_c_str),
      const_cast<void*>(reinterpret_cast<const void*>(security_domain_c_str)) }
  };
  SecKeychainAttributeList search_attributes = { arraysize(attributes),
                                                 attributes };
  // For HTML forms, we don't want the security domain to be part of the
  // search, so trim that off of the attribute list.
  if (scheme == PasswordForm::SCHEME_HTML) {
    search_attributes.count -= 1;
  }

  SecKeychainSearchRef keychain_search = NULL;
  OSStatus result = keychain.SearchCreateFromAttributes(
      NULL, kSecInternetPasswordItemClass, &search_attributes,
      &keychain_search);

  if (result != noErr) {
    LOG(ERROR) << "Keychain lookup failed for " << server << " with error "
               << result;
    return;
  }

  SecKeychainItemRef keychain_item;
  while (keychain.SearchCopyNext(keychain_search, &keychain_item) == noErr) {
    // Consumer is responsible for deleting the forms when they are done.
    items->push_back(keychain_item);
  }

  keychain.Free(keychain_search);
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
