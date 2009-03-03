// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/l10n_util.h"
#include "net/base/escape.h"

// The TemplateURLRef has any number of terms that need to be replaced. Each of
// the terms is enclosed in braces. If the character preceeding the final
// brace is a ?, it indicates the term is optional and can be replaced with
// an empty string.
static const wchar_t kStartParameter = '{';
static const wchar_t kEndParameter = '}';
static const wchar_t kOptional = '?';

// Known parameters found in the URL.
static const wchar_t kSearchTermsParameter[] = L"searchTerms";
static const char kSearchTermsParameterFull[] = "{searchTerms}";
static const wchar_t kCountParameter[] = L"count";
static const wchar_t kStartIndexParameter[] = L"startIndex";
static const wchar_t kStartPageParameter[] = L"startPage";
static const wchar_t kLanguageParameter[] = L"language";
static const wchar_t kInputEncodingParameter[] = L"inputEncoding";
static const wchar_t kOutputEncodingParameter[] = L"outputEncoding";

static const wchar_t kGoogleAcceptedSuggestionParameter[] =
    L"google:acceptedSuggestion";
// Host/Domain Google searches are relative to.
static const wchar_t kGoogleBaseURLParameter[] = L"google:baseURL";
static const char kGoogleBaseURLParameterFull[] = "{google:baseURL}";
// Like google:baseURL, but for the Search Suggest capability.
static const char kGoogleBaseSuggestURLParameter[] =
    "google:baseSuggestURL";
static const char kGoogleBaseSuggestURLParameterFull[] =
    "{google:baseSuggestURL}";
static const wchar_t kGoogleOriginalQueryForSuggestionParameter[] =
    L"google:originalQueryForSuggestion";
static const wchar_t kGoogleRLZParameter[] = L"google:RLZ";
// Same as kSearchTermsParameter, with no escaping.
static const wchar_t kGoogleUnescapedSearchTermsParameter[] =
    L"google:unescapedSearchTerms";
static const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

// Display value for kSearchTermsParameter.
static const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
static const char kDisplayUnescapedSearchTerms[] = "%S";

// Used if the count parameter is not optional. Indicates we want 10 search
// results.
static const wchar_t kDefaultCount[] = L"10";

// Used if the parameter kOutputEncodingParameter is required.
static const wchar_t kOutputEncodingType[] = L"UTF-8";

// static
std::wstring* TemplateURLRef::google_base_url_ = NULL;

TemplateURLRef::TemplateURLRef() {
  Set(std::wstring(), 0, 0);
}

void TemplateURLRef::Set(const std::wstring& url,
                         int index_offset,
                         int page_offset) {
  url_ = url;
  index_offset_ = index_offset;
  page_offset_ = page_offset;
  InvalidateCachedValues();
}

bool TemplateURLRef::ParseParameter(size_t start,
                                    size_t end,
                                    std::wstring* url,
                                    Replacements* replacements) const {
  DCHECK(start != std::string::npos &&
         end != std::string::npos && end > start);
  size_t length = end - start - 1;
  bool optional = false;
  if ((*url)[end - 1] == kOptional) {
    optional = true;
    length--;
  }
  std::wstring parameter(url->substr(start + 1, length));
  // Remove the parameter from the string.
  url->erase(start, end - start + 1);
  if (parameter == kSearchTermsParameter) {
    replacements->push_back(Replacement(SEARCH_TERMS, static_cast<int>(start)));
  } else if (parameter == kCountParameter) {
    if (!optional)
      url->insert(start, kDefaultCount);
  } else if (parameter == kStartIndexParameter) {
    if (!optional) {
      url->insert(start, IntToWString(index_offset_));
    }
  } else if (parameter == kStartPageParameter) {
    if (!optional) {
      url->insert(start, IntToWString(page_offset_));
    }
  } else if (parameter == kLanguageParameter) {
    replacements->push_back(Replacement(LANGUAGE, static_cast<int>(start)));
  } else if (parameter == kInputEncodingParameter) {
    replacements->push_back(Replacement(ENCODING, static_cast<int>(start)));
  } else if (parameter == kOutputEncodingParameter) {
    if (!optional)
      url->insert(start, kOutputEncodingType);
  } else if (parameter == kGoogleAcceptedSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ACCEPTED_SUGGESTION,
                                        static_cast<int>(start)));
  } else if (parameter == kGoogleBaseURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_URL,
                                        static_cast<int>(start)));
  } else if (WideToUTF16Hack(parameter) ==
             ASCIIToUTF16(kGoogleBaseSuggestURLParameter)) {
    replacements->push_back(Replacement(GOOGLE_BASE_SUGGEST_URL,
                                        static_cast<int>(start)));
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        static_cast<int>(start)));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, static_cast<int>(start)));
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS,
                                        static_cast<int>(start)));
  } else if (!optional) {
    // Unknown required parameter. No idea what to replace this with,
    // so fail.
    return false;
  }
  return true;
}

std::wstring TemplateURLRef::ParseURL(const std::wstring& url,
                                      Replacements* replacements,
                                      bool* valid) const {
  *valid = false;
  std::wstring parsed_url = url;
  for (size_t last = 0; last != std::string::npos; ) {
    last = parsed_url.find(kStartParameter, last);
    if (last != std::string::npos) {
      size_t endTemplate = parsed_url.find(kEndParameter, last);
      if (endTemplate != std::string::npos) {
        if (!ParseParameter(last, endTemplate, &parsed_url, replacements)) {
          // Not a valid parameter, return.
          return std::wstring();
        }
        // ParseParamter erases from the string, as such we don't need
        // to update last.
      } else {
        // Open brace without a closing brace, return.
        return std::wstring();
      }
    }
  }
  *valid = true;
  return parsed_url;
}

void TemplateURLRef::ParseIfNecessary() const {
  if (!parsed_) {
    parsed_ = true;
    parsed_url_ = ParseURL(url_, &replacements_, &valid_);
    supports_replacements_ = false;
    if (valid_) {
      bool has_only_one_search_term = false;
      for (Replacements::const_iterator i = replacements_.begin();
           i != replacements_.end(); ++i) {
        if ((i->type == SEARCH_TERMS) ||
            (i->type == GOOGLE_UNESCAPED_SEARCH_TERMS)) {
          if (has_only_one_search_term) {
            has_only_one_search_term = false;
            break;
          }
          has_only_one_search_term = true;
          supports_replacements_ = true;
        }
      }
      // Only parse the host/key if there is one search term. Technically there
      // could be more than one term, but it's uncommon; so we punt.
      if (has_only_one_search_term)
        ParseHostAndSearchTermKey();
    }
  }
}

void TemplateURLRef::ParseHostAndSearchTermKey() const {
  string16 url_string = WideToUTF16Hack(url_);
  ReplaceSubstringsAfterOffset(&url_string, 0,
                               ASCIIToUTF16(kGoogleBaseURLParameterFull),
                               WideToUTF16Hack(GoogleBaseURLValue()));
  ReplaceSubstringsAfterOffset(&url_string, 0,
                               ASCIIToUTF16(kGoogleBaseSuggestURLParameterFull),
                               WideToUTF16Hack(GoogleBaseSuggestURLValue()));

  GURL url(UTF16ToUTF8(url_string));
  if (!url.is_valid())
    return;

  std::string query_string = url.query();
  if (query_string.empty())
    return;

  url_parse::Component query, key, value;
  query.len = static_cast<int>(query_string.size());
  while (url_parse::ExtractQueryKeyValue(query_string.c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      std::string value_string = query_string.substr(value.begin, value.len);
      if (value_string.find(kSearchTermsParameterFull, 0) !=
          std::string::npos ||
          value_string.find(kGoogleUnescapedSearchTermsParameterFull, 0) !=
          std::string::npos) {
        search_term_key_ = query_string.substr(key.begin, key.len);
        host_ = url.host();
        path_ = url.path();
        break;
      }
    }
  }
}

GURL TemplateURLRef::ReplaceSearchTerms(
    const TemplateURL& host,
    const std::wstring& terms,
    int accepted_suggestion,
    const std::wstring& original_query_for_suggestion) const {
  ParseIfNecessary();
  if (!valid_)
    return GURL();

  if (replacements_.empty())
    return GURL(WideToUTF8(parsed_url_));

  // Encode the search terms so that we know the encoding.
  const std::vector<std::string>& encodings = host.input_encodings();
  std::wstring encoded_terms;
  std::wstring encoded_original_query;
  std::wstring input_encoding;
  for (size_t i = 0; i < encodings.size(); ++i) {
    if (EscapeQueryParamValue(terms, encodings[i].c_str(), &encoded_terms)) {
      if (!original_query_for_suggestion.empty()) {
        EscapeQueryParamValue(original_query_for_suggestion,
                              encodings[i].c_str(), &encoded_original_query);
      }
      input_encoding = ASCIIToWide(encodings[i]);
      break;
    }
  }
  if (input_encoding.empty()) {
    encoded_terms = EscapeQueryParamValueUTF8(terms);
    if (!original_query_for_suggestion.empty()) {
      encoded_original_query =
          EscapeQueryParamValueUTF8(original_query_for_suggestion);
    }
    input_encoding = L"UTF-8";
  }

  std::wstring url = parsed_url_;

  // replacements_ is ordered in ascending order, as such we need to iterate
  // from the back.
  for (Replacements::reverse_iterator i = replacements_.rbegin();
       i != replacements_.rend(); ++i) {
    switch (i->type) {
      case ENCODING:
        url.insert(i->index, input_encoding);
        break;

      case GOOGLE_ACCEPTED_SUGGESTION:
        if (accepted_suggestion == NO_SUGGESTION_CHOSEN)
          url.insert(i->index, L"aq=f&");
        else if (accepted_suggestion != NO_SUGGESTIONS_AVAILABLE)
          url.insert(i->index, StringPrintf(L"aq=%d&", accepted_suggestion));
        break;

      case GOOGLE_BASE_URL:
        url.insert(i->index, GoogleBaseURLValue());
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        url.insert(i->index, GoogleBaseSuggestURLValue());
        break;

      case GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION:
        if (accepted_suggestion >= 0)
          url.insert(i->index, L"oq=" + encoded_original_query + L"&");
        break;

      case GOOGLE_RLZ: {
        std::wstring rlz_string;
        RLZTracker::GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, &rlz_string);
        if (!rlz_string.empty()) {
          rlz_string = L"rlz=" + rlz_string + L"&";
          url.insert(i->index, rlz_string);
        }
        break;
      }

      case GOOGLE_UNESCAPED_SEARCH_TERMS: {
        std::string unescaped_terms;
        WideToCodepage(terms, WideToASCII(input_encoding).c_str(),
                       OnStringUtilConversionError::SKIP, &unescaped_terms);
        url.insert(i->index, std::wstring(unescaped_terms.begin(),
                                          unescaped_terms.end()));
        break;
      }

      case LANGUAGE:
        url.insert(i->index, g_browser_process->GetApplicationLocale());
        break;

      case SEARCH_TERMS:
        url.insert(i->index, encoded_terms);
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  return GURL(WideToUTF8(url));
}

bool TemplateURLRef::SupportsReplacement() const {
  ParseIfNecessary();
  return valid_ && supports_replacements_;
}

bool TemplateURLRef::IsValid() const {
  ParseIfNecessary();
  return valid_;
}

std::wstring TemplateURLRef::DisplayURL() const {
  ParseIfNecessary();
  if (!valid_)
    return url_;  // If we're not valid, don't escape anything.

  if (replacements_.empty())
    return url_;  // Nothing to replace, return the url.

  string16 result = WideToUTF16Hack(url_);
  ReplaceSubstringsAfterOffset(&result, 0,
                               ASCIIToUTF16(kSearchTermsParameterFull),
                               ASCIIToUTF16(kDisplaySearchTerms));

  ReplaceSubstringsAfterOffset(
      &result, 0,
      ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull),
      ASCIIToUTF16(kDisplayUnescapedSearchTerms));

  return UTF16ToWideHack(result);
}

// static
std::wstring TemplateURLRef::DisplayURLToURLRef(
    const std::wstring& display_url) {
  string16 result = WideToUTF16Hack(display_url);
  ReplaceSubstringsAfterOffset(&result, 0, ASCIIToUTF16(kDisplaySearchTerms),
                               ASCIIToUTF16(kSearchTermsParameterFull));
  ReplaceSubstringsAfterOffset(
      &result, 0,
      ASCIIToUTF16(kDisplayUnescapedSearchTerms),
      ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull));
  return UTF16ToWideHack(result);
}

const std::string& TemplateURLRef::GetHost() const {
  ParseIfNecessary();
  return host_;
}

const std::string& TemplateURLRef::GetPath() const {
  ParseIfNecessary();
  return path_;
}

const std::string& TemplateURLRef::GetSearchTermKey() const {
  ParseIfNecessary();
  return search_term_key_;
}

std::wstring TemplateURLRef::SearchTermToWide(const TemplateURL& host,
                                              const std::string& term) const {
  const std::vector<std::string>& encodings = host.input_encodings();
  std::wstring result;

  std::string unescaped =
      UnescapeURLComponent(term, UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  for (size_t i = 0; i < encodings.size(); ++i) {
    if (CodepageToWide(unescaped, encodings[i].c_str(),
                       OnStringUtilConversionError::FAIL, &result))
      return result;
  }

  // Always fall back on UTF-8 if it works.
  if (CodepageToWide(unescaped, "UTF-8",
                     OnStringUtilConversionError::FAIL, &result))
    return result;

  // When nothing worked, just use the escaped text. We have no idea what the
  // encoding is. We need to substitute spaces for pluses ourselves since we're
  // not sending it through an unescaper.
  result = UTF8ToWide(term);
  std::replace(result.begin(), result.end(), '+', ' ');
  return result;
}

bool TemplateURLRef::HasGoogleBaseURLs() const {
  ParseIfNecessary();
  for (size_t i = 0; i < replacements_.size(); ++i) {
    if ((replacements_[i].type == GOOGLE_BASE_URL) ||
        (replacements_[i].type == GOOGLE_BASE_SUGGEST_URL))
      return true;
  }
  return false;
}

void TemplateURLRef::InvalidateCachedValues() const {
  supports_replacements_ = valid_ = parsed_ = false;
  host_.clear();
  path_.clear();
  search_term_key_.clear();
  replacements_.clear();
}

// Returns the value to use for replacements of type GOOGLE_BASE_URL.
// static
std::wstring TemplateURLRef::GoogleBaseURLValue() {
  return google_base_url_ ?
    (*google_base_url_) : UTF8ToWide(GoogleURLTracker::GoogleURL().spec());
}

// Returns the value to use for replacements of type GOOGLE_BASE_SUGGEST_URL.
// static
std::wstring TemplateURLRef::GoogleBaseSuggestURLValue() {
  // The suggest base URL we want at the end is something like
  // "http://clients1.google.TLD/complete/".  The key bit we want from the
  // original Google base URL is the TLD.

  // Start with the Google base URL.
  const GURL base_url(google_base_url_ ?
      GURL(WideToUTF8(*google_base_url_)) : GoogleURLTracker::GoogleURL());
  DCHECK(base_url.is_valid());

  // Change "www." to "clients1." in the hostname.  If no "www." was found, just
  // prepend "clients1.".
  const std::string base_host(base_url.host());
  GURL::Replacements repl;
  const std::string suggest_host("clients1." +
      (base_host.compare(0, 4, "www.") ? base_host : base_host.substr(4)));
  repl.SetHostStr(suggest_host);

  // Replace any existing path with "/complete/".
  static const std::string suggest_path("/complete/");
  repl.SetPathStr(suggest_path);

  // Clear the query and ref.
  repl.ClearQuery();
  repl.ClearRef();
  return UTF8ToWide(base_url.ReplaceComponents(repl).spec());
}

// TemplateURL ----------------------------------------------------------------

// static
GURL TemplateURL::GenerateFaviconURL(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements rep;

  const char favicon_path[] = "/favicon.ico";
  int favicon_path_len = arraysize(favicon_path) - 1;

  rep.SetPath(favicon_path, url_parse::Component(0, favicon_path_len));
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return url.ReplaceComponents(rep);
}

void TemplateURL::SetSuggestionsURL(const std::wstring& suggestions_url,
                                    int index_offset,
                                    int page_offset) {
  suggestions_url_.Set(suggestions_url, index_offset, page_offset);
}

void TemplateURL::SetURL(const std::wstring& url,
                         int index_offset,
                         int page_offset) {
  url_.Set(url, index_offset, page_offset);
}

void TemplateURL::set_keyword(const std::wstring& keyword) {
  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = l10n_util::ToLower(keyword);
  autogenerate_keyword_ = false;
}

const std::wstring& TemplateURL::keyword() const {
  if (autogenerate_keyword_ && keyword_.empty()) {
    // Generate a keyword and cache it.
    keyword_ = TemplateURLModel::GenerateKeyword(
        TemplateURLModel::GenerateSearchURL(this).GetWithEmptyPath(), true);
  }
  return keyword_;
}

bool TemplateURL::ShowInDefaultList() const {
  return show_in_default_list() && url() && url()->SupportsReplacement();
}

void TemplateURL::SetFavIconURL(const GURL& url) {
  for (std::vector<ImageRef>::iterator i = image_refs_.begin();
       i != image_refs_.end(); ++i) {
    if (i->type == L"image/x-icon" &&
        i->width == kFavIconSize && i->height == kFavIconSize) {
      if (!url.is_valid())
        image_refs_.erase(i);
      else
        i->url = url;
      return;
    }
  }
  // Don't have one yet, add it.
  if (url.is_valid()) {
    add_image_ref(
        TemplateURL::ImageRef(L"image/x-icon", kFavIconSize, kFavIconSize,
                              url));
  }
}

GURL TemplateURL::GetFavIconURL() const {
  for (std::vector<ImageRef>::const_iterator i = image_refs_.begin();
       i != image_refs_.end(); ++i) {
    if ((i->type == L"image/x-icon" || i->type == L"image/vnd.microsoft.icon")
        && i->width == kFavIconSize && i->height == kFavIconSize) {
      return i->url;
    }
  }
  return GURL();
}

void TemplateURL::InvalidateCachedValues() const {
  url_.InvalidateCachedValues();
  suggestions_url_.InvalidateCachedValues();
  if (autogenerate_keyword_)
    keyword_.clear();
}
