// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/field_trial.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "net/base/base64.h"
#include "net/base/registry_controlled_domain.h"
#include "net/base/sdch_manager.h"
#include "net/url_request/url_request_http_job.h"

using base::Time;
using base::TimeDelta;

//------------------------------------------------------------------------------
// static
const size_t SdchManager::kMaxDictionarySize = 100000;

// static
const size_t SdchManager::kMaxDictionaryCount = 20;

// static
SdchManager* SdchManager::global_;

// static
SdchManager* SdchManager::Global() {
  return global_;
}

// static
void SdchManager::SdchErrorRecovery(ProblemCodes problem) {
  static LinearHistogram histogram("Sdch.ProblemCodes_3", MIN_PROBLEM_CODE,
                                   MAX_PROBLEM_CODE - 1, MAX_PROBLEM_CODE);
  histogram.SetFlags(kUmaTargetedHistogramFlag);
  histogram.Add(problem);
}

// static
void SdchManager::ClearBlacklistings() {
  Global()->blacklisted_domains_.clear();
  Global()->exponential_blacklist_count.clear();
}

// static
void SdchManager::ClearDomainBlacklisting(std::string domain) {
  Global()->blacklisted_domains_.erase(StringToLowerASCII(domain));
}

// static
int SdchManager::BlackListDomainCount(std::string domain) {
  if (Global()->blacklisted_domains_.end() ==
      Global()->blacklisted_domains_.find(domain))
    return 0;
  return Global()->blacklisted_domains_[StringToLowerASCII(domain)];
}

// static
int SdchManager::BlacklistDomainExponential(std::string domain) {
  if (Global()->exponential_blacklist_count.end() ==
      Global()->exponential_blacklist_count.find(domain))
    return 0;
  return Global()->exponential_blacklist_count[StringToLowerASCII(domain)];
}

//------------------------------------------------------------------------------
SdchManager::SdchManager() : sdch_enabled_(false) {
  DCHECK(!global_);
  global_ = this;
}

SdchManager::~SdchManager() {
  DCHECK(global_ == this);
  while (!dictionaries_.empty()) {
    DictionaryMap::iterator it = dictionaries_.begin();
    it->second->Release();
    dictionaries_.erase(it->first);
  }
  global_ = NULL;
}

// static
void SdchManager::BlacklistDomain(const GURL& url) {
  if (!global_ )
    return;

  std::string domain(StringToLowerASCII(url.host()));
  int count = global_->blacklisted_domains_[domain];
  if (count > 0)
    return;  // Domain is already blacklisted.

  count = 1 + 2 * global_->exponential_blacklist_count[domain];
  if (count > 0)
    global_->exponential_blacklist_count[domain] = count;
  else
    count = INT_MAX;

  global_->blacklisted_domains_[domain] = count;
}

// static
void SdchManager::BlacklistDomainForever(const GURL& url) {
  if (!global_ )
    return;

  std::string domain(StringToLowerASCII(url.host()));
  global_->exponential_blacklist_count[domain] = INT_MAX;
  global_->blacklisted_domains_[domain] = INT_MAX;
}

void SdchManager::EnableSdchSupport(const std::string& domain) {
  // We presume that there is a SDCH manager instance.
  global_->supported_domain_ = domain;
  global_->sdch_enabled_ = true;
}

const bool SdchManager::IsInSupportedDomain(const GURL& url) {
  if (!sdch_enabled_ )
    return false;
  if (!supported_domain_.empty() &&
      !url.DomainIs(supported_domain_.data(), supported_domain_.size()))
     return false;  // It is not the singular supported domain.

  if (blacklisted_domains_.empty())
    return true;

  std::string domain(StringToLowerASCII(url.host()));
  DomainCounter::iterator it = blacklisted_domains_.find(domain);
  if (blacklisted_domains_.end() == it)
    return true;

  int count = it->second - 1;
  if (count > 0)
    blacklisted_domains_[domain] = count;
  else
    blacklisted_domains_.erase(domain);
  SdchErrorRecovery(DOMAIN_BLACKLIST_INCLUDES_TARGET);
  return false;
}

bool SdchManager::CanFetchDictionary(const GURL& referring_url,
                                     const GURL& dictionary_url) const {
  /* The user agent may retrieve a dictionary from the dictionary URL if all of 
     the following are true:
       1 The dictionary URL host name matches the referrer URL host name
       2 The dictionary URL host name domain matches the parent domain of the
           referrer URL host name
       3 The parent domain of the referrer URL host name is not a top level 
           domain
       4 The dictionary URL is not an HTTPS URL.
   */
  // Item (1) above implies item (2).  Spec should be updated.
  // I take "host name match" to be "is identical to"
  if (referring_url.host() != dictionary_url.host()) {
    SdchErrorRecovery(DICTIONARY_LOAD_ATTEMPT_FROM_DIFFERENT_HOST);
    return false;
  }
  if (referring_url.SchemeIs("https")) {
    SdchErrorRecovery(DICTIONARY_SELECTED_FOR_SSL);
    return false;
  }

  // TODO(jar): Remove this failsafe conservative hack which is more restrictive
  // than current SDCH spec when needed, and justified by security audit.
  if (!referring_url.SchemeIs("http")) {
    SdchErrorRecovery(DICTIONARY_SELECTED_FROM_NON_HTTP);
    return false;
  }

  return true;
}

void SdchManager::FetchDictionary(const GURL& request_url,
                                  const GURL& dictionary_url) {
  if (SdchManager::Global()->CanFetchDictionary(request_url, dictionary_url) &&
      fetcher_.get())
    fetcher_->Schedule(dictionary_url);
}

bool SdchManager::AddSdchDictionary(const std::string& dictionary_text,
    const GURL& dictionary_url) {
  std::string client_hash;
  std::string server_hash;
  GenerateHash(dictionary_text, &client_hash, &server_hash);
  if (dictionaries_.find(server_hash) != dictionaries_.end()) {
    SdchErrorRecovery(DICTIONARY_ALREADY_LOADED);
    return false;  // Already loaded.
  }

  std::string domain, path;
  std::set<int> ports;
  Time expiration(Time::Now() + TimeDelta::FromDays(30));

  if (dictionary_text.empty()) {
    SdchErrorRecovery(DICTIONARY_HAS_NO_TEXT);
    return false;  // Missing header.
  }

  size_t header_end = dictionary_text.find("\n\n");
  if (std::string::npos == header_end) {
    SdchErrorRecovery(DICTIONARY_HAS_NO_HEADER);
    return false;  // Missing header.
  }
  size_t line_start = 0;  // Start of line being parsed.
  while (1) {
    size_t line_end = dictionary_text.find('\n', line_start);
    DCHECK(std::string::npos != line_end);
    DCHECK(line_end <= header_end);

    size_t colon_index = dictionary_text.find(':', line_start);
    if (std::string::npos == colon_index) {
      SdchErrorRecovery(DICTIONARY_HEADER_LINE_MISSING_COLON);
      return false;  // Illegal line missing a colon.
    }

    if (colon_index > line_end)
      break;

    size_t value_start = dictionary_text.find_first_not_of(" \t",
                                                           colon_index + 1);
    if (std::string::npos != value_start) {
      if (value_start >= line_end)
        break;
      std::string name(dictionary_text, line_start, colon_index - line_start);
      std::string value(dictionary_text, value_start, line_end - value_start);
      name = StringToLowerASCII(name);
      if (name == "domain") {
        domain = value;
      } else if (name == "path") {
        path = value;
      } else if (name == "format-version") {
        if (value != "1.0")
          return false;
      } else if (name == "max-age") {
        expiration = Time::Now() + TimeDelta::FromSeconds(StringToInt64(value));
      } else if (name == "port") {
        int port = StringToInt(value);
        if (port >= 0)
          ports.insert(port);
      }
    }

    if (line_end >= header_end)
      break;
    line_start = line_end + 1;
  }

  if (!Dictionary::CanSet(domain, path, ports, dictionary_url))
    return false;

  // TODO(jar): Remove these hacks to preclude a DOS attack involving piles of
  // useless dictionaries.  We should probably have a cache eviction plan,
  // instead of just blocking additions.  For now, with the spec in flux, it
  // is probably not worth doing eviction handling.
  if (kMaxDictionarySize < dictionary_text.size()) {
    SdchErrorRecovery(DICTIONARY_IS_TOO_LARGE);
    return false;
  }
  if (kMaxDictionaryCount <= dictionaries_.size()) {
    SdchErrorRecovery(DICTIONARY_COUNT_EXCEEDED);
    return false;
  }

  UMA_HISTOGRAM_COUNTS("Sdch.Dictionary size loaded", dictionary_text.size());
  DLOG(INFO) << "Loaded dictionary with client hash " << client_hash <<
      " and server hash " << server_hash;
  Dictionary* dictionary =
      new Dictionary(dictionary_text, header_end + 2, client_hash,
                     dictionary_url, domain, path, expiration, ports);
  dictionary->AddRef();
  dictionaries_[server_hash] = dictionary;
  return true;
}

void SdchManager::GetVcdiffDictionary(const std::string& server_hash,
    const GURL& referring_url, Dictionary** dictionary) {
  *dictionary = NULL;
  DictionaryMap::iterator it = dictionaries_.find(server_hash);
  if (it == dictionaries_.end()) {
    return;
  }
  Dictionary* matching_dictionary = it->second;
  if (!matching_dictionary->CanUse(referring_url))
    return;
  *dictionary = matching_dictionary;
}

// TODO(jar): If we have evictions from the dictionaries_, then we need to
// change this interface to return a list of reference counted Dictionary
// instances that can be used if/when a server specifies one.
void SdchManager::GetAvailDictionaryList(const GURL& target_url,
                                         std::string* list) {
  int count = 0;
  for (DictionaryMap::iterator it = dictionaries_.begin();
       it != dictionaries_.end(); ++it) {
    if (!it->second->CanAdvertise(target_url))
      continue;
    ++count;
    if (!list->empty())
      list->append(",");
    list->append(it->second->client_hash());
  }
  // Watch to see if we have corrupt or numerous dictionaries.
  if (count > 0)
    UMA_HISTOGRAM_COUNTS("Sdch.Advertisement_Count", count);
}

SdchManager::Dictionary::Dictionary(const std::string& dictionary_text,
    size_t offset, const std::string& client_hash, const GURL& gurl,
    const std::string& domain, const std::string& path, const Time& expiration,
    const std::set<int> ports)
      : text_(dictionary_text, offset),
        client_hash_(client_hash),
        url_(gurl),
        domain_(domain),
        path_(path),
        expiration_(expiration),
        ports_(ports) {
}

// static
void SdchManager::GenerateHash(const std::string& dictionary_text,
    std::string* client_hash, std::string* server_hash) {
  char binary_hash[32];
  base::SHA256HashString(dictionary_text, binary_hash, sizeof(binary_hash));

  std::string first_48_bits(&binary_hash[0], 6);
  std::string second_48_bits(&binary_hash[6], 6);
  UrlSafeBase64Encode(first_48_bits, client_hash);
  UrlSafeBase64Encode(second_48_bits, server_hash);

  DCHECK(server_hash->length() == 8);
  DCHECK(client_hash->length() == 8);
}

// static
void SdchManager::UrlSafeBase64Encode(const std::string& input,
                                      std::string* output) {
  // Since this is only done during a dictionary load, and hashes are only 8
  // characters, we just do the simple fixup, rather than rewriting the encoder.
  net::Base64Encode(input, output);
  for (size_t i = 0; i < output->size(); ++i) {
    switch (output->data()[i]) {
      case '+':
        (*output)[i] = '-';
        continue;
      case '/':
        (*output)[i] = '_';
        continue;
      default:
        continue;
    }
  }
}

//------------------------------------------------------------------------------
// Security functions restricting loads and use of dictionaries.

// static
bool SdchManager::Dictionary::CanSet(const std::string& domain,
                                     const std::string& path,
                                     const std::set<int> ports,
                                     const GURL& dictionary_url) {
  if (!SdchManager::Global()->IsInSupportedDomain(dictionary_url))
    return false;
  /* 
  A dictionary is invalid and must not be stored if any of the following are
  true:
    1. The dictionary has no Domain attribute.
    2. The effective host name that derives from the referer URL host name does
      not domain-match the Domain attribute.
    3. The Domain attribute is a top level domain.
    4. The referer URL host is a host domain name (not IP address) and has the
      form HD, where D is the value of the Domain attribute, and H is a string
      that contains one or more dots.
    5. If the dictionary has a Port attribute and the referer URL's port was not
      in the list.
  */

  // TODO(jar): Redirects in dictionary fetches might plausibly be problematic,
  // and hence the conservative approach is to not allow any redirects (if there
  // were any... then don't allow the dictionary to be set).

  if (domain.empty()) {
    SdchErrorRecovery(DICTIONARY_MISSING_DOMAIN_SPECIFIER);
    return false;  // Domain is required.
  }
  if (net::RegistryControlledDomainService::GetDomainAndRegistry(domain).size()
      == 0) {
    SdchErrorRecovery(DICTIONARY_SPECIFIES_TOP_LEVEL_DOMAIN);
    return false;  // domain was a TLD.
  }
  if (!Dictionary::DomainMatch(dictionary_url, domain)) {
    SdchErrorRecovery(DICTIONARY_DOMAIN_NOT_MATCHING_SOURCE_URL);
    return false;
  }

  std::string referrer_url_host = dictionary_url.host();
  size_t postfix_domain_index = referrer_url_host.rfind(domain);
  // See if it is indeed a postfix, or just an internal string.
  if (referrer_url_host.size() == postfix_domain_index + domain.size()) {
    // It is a postfix... so check to see if there's a dot in the prefix.
    size_t end_of_host_index = referrer_url_host.find_first_of('.');
    if (referrer_url_host.npos != end_of_host_index  &&
        end_of_host_index < postfix_domain_index)
      return false;
  }

  if (!ports.empty()
      && 0 == ports.count(dictionary_url.EffectiveIntPort())) {
    SdchErrorRecovery(DICTIONARY_PORT_NOT_MATCHING_SOURCE_URL);
    return false;
  }
  return true;
}

// static
bool SdchManager::Dictionary::CanUse(const GURL referring_url) {
  if (!SdchManager::Global()->IsInSupportedDomain(referring_url))
    return false;
  /*
    1. The request URL's host name domain-matches the Domain attribute of the
      dictionary.
    2. If the dictionary has a Port attribute, the request port is one of the
      ports listed in the Port attribute.
    3. The request URL path-matches the path attribute of the dictionary.
    4. The request is not an HTTPS request.
*/
  if (!DomainMatch(referring_url, domain_)) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_DOMAIN);
    return false;
  }
  if (!ports_.empty()
      && 0 == ports_.count(referring_url.EffectiveIntPort())) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_PORT_LIST);
    return false;
  }
  if (path_.size() && !PathMatch(referring_url.path(), path_)) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_PATH);
    return false;
  }
  if (referring_url.SchemeIsSecure()) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_SCHEME);
    return false;
  }

  // TODO(jar): Remove overly restrictive failsafe test (added per security
  // review) when we have a need to be more general.
  if (!referring_url.SchemeIs("http")) {
    SdchErrorRecovery(ATTEMPT_TO_DECODE_NON_HTTP_DATA);
    return false;
  }

  return true;
}

bool SdchManager::Dictionary::CanAdvertise(const GURL& target_url) {
  if (!SdchManager::Global()->IsInSupportedDomain(target_url))
    return false;
  /* The specific rules of when a dictionary should be advertised in an 
     Avail-Dictionary header are modeled after the rules for cookie scoping. The
     terms "domain-match" and "pathmatch" are defined in RFC 2965 [6]. A
     dictionary may be advertised in the Avail-Dictionaries header exactly when
     all of the following are true:
      1. The server's effective host name domain-matches the Domain attribute of
         the dictionary.
      2. If the dictionary has a Port attribute, the request port is one of the
         ports listed in the Port attribute.
      3. The request URI path-matches the path header of the dictionary.
      4. The request is not an HTTPS request.
    */
  if (!DomainMatch(target_url, domain_))
    return false;
  if (!ports_.empty() && 0 == ports_.count(target_url.EffectiveIntPort()))
    return false;
  if (path_.size() && !PathMatch(target_url.path(), path_))
    return false;
  if (target_url.SchemeIsSecure())
    return false;
  if (Time::Now() > expiration_)
    return false;
  return true;
}

bool SdchManager::Dictionary::PathMatch(const std::string& path,
                                        const std::string& restriction) {
  /*  Must be either:
  1. P2 is equal to P1
  2. P2 is a prefix of P1 and either the final character in P2 is "/" or the
      character following P2 in P1 is "/". 
      */
  if (path == restriction)
    return true;
  size_t prefix_length = restriction.size();
  if (prefix_length > path.size())
    return false;  // Can't be a prefix.
  if (0 != restriction.compare(0, prefix_length, path))
    return false;
  return restriction[prefix_length - 1] == '/' || path[prefix_length] == '/';
}

// static
bool SdchManager::Dictionary::DomainMatch(const GURL& gurl,
                                          const std::string& restriction) {
  // TODO(jar): This is not precisely a domain match definition.
  return gurl.DomainIs(restriction.data(), restriction.size());
}
