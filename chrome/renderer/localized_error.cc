// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/localized_error.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/l10n_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webkit_glue.h"

namespace {

static const char* kRedirectLoopLearnMoreUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=95626";

enum NAV_SUGGESTIONS {
  SUGGEST_NONE     = 0,
  SUGGEST_RELOAD   = 1 << 0,
  SUGGEST_HOSTNAME = 1 << 1,
  SUGGEST_LEARNMORE = 1 << 2,
};

struct WebErrorNetErrorMap {
  const int error_code;
  const unsigned int title_resource_id;
  const unsigned int heading_resource_id;
  const unsigned int summary_resource_id;
  const unsigned int details_resource_id;
  const int suggestions;  // Bitmap of SUGGEST_* values.
};

WebErrorNetErrorMap net_error_options[] = {
  {net::ERR_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_CONNECT_FAILED,
   SUGGEST_RELOAD,
  },
  {net::ERR_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_NAME_NOT_RESOLVED,
   SUGGEST_RELOAD,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_DISCONNECTED,
   SUGGEST_RELOAD,
  },
  {net::ERR_FILE_NOT_FOUND,
   IDS_ERRORPAGES_TITLE_NOT_FOUND,
   IDS_ERRORPAGES_HEADING_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_NOT_FOUND,
   IDS_ERRORPAGES_DETAILS_FILE_NOT_FOUND,
   SUGGEST_NONE,
  },
  {net::ERR_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_SUMMARY_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_DETAILS_TOO_MANY_REDIRECTS,
   SUGGEST_RELOAD | SUGGEST_LEARNMORE,
  },
};

}  // namespace

void GetLocalizedErrorValues(const WebError& error,
                             DictionaryValue* error_strings) {
  // Grab strings that are applicable to all error pages
  error_strings->SetString(
      LIT16("detailsLink"),
      WideToUTF16Hack(l10n_util::GetString(IDS_ERRORPAGES_DETAILS_LINK)));
  error_strings->SetString(
      LIT16("detailsHeading"),
      WideToUTF16Hack(l10n_util::GetString(IDS_ERRORPAGES_DETAILS_HEADING)));

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  WebErrorNetErrorMap options = {
    0,
    IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    IDS_ERRORPAGES_DETAILS_UNKNOWN,
    SUGGEST_NONE,
  };
  int error_code = error.GetErrorCode();
  for (size_t i = 0; i < arraysize(net_error_options); ++i) {
    if (net_error_options[i].error_code == error_code) {
      memcpy(&options, &net_error_options[i], sizeof(WebErrorNetErrorMap));
      break;
    }
  }

  std::wstring suggestions_heading;
  if (options.suggestions != SUGGEST_NONE) {
    suggestions_heading =
        l10n_util::GetString(IDS_ERRORPAGES_SUGGESTION_HEADING);
  }
  error_strings->SetString(LIT16("suggestionsHeading"),
                           WideToUTF16Hack(suggestions_heading));

  std::wstring failed_url(ASCIIToWide(error.GetFailedURL().spec()));
  // URLs are always LTR.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetString(
      LIT16("title"),
      WideToUTF16Hack(l10n_util::GetStringF(options.title_resource_id,
                                            failed_url)));
  error_strings->SetString(
      LIT16("heading"),
      WideToUTF16Hack(l10n_util::GetString(options.heading_resource_id)));

  DictionaryValue* summary = new DictionaryValue;
  summary->SetString(
      LIT16("msg"),
      WideToUTF16Hack(l10n_util::GetString(options.summary_resource_id)));
  // TODO(tc): we want the unicode url here since it's being displayed
  summary->SetString(LIT16("failedUrl"), WideToUTF16Hack(failed_url));
  error_strings->Set(LIT16("summary"), summary);

  // Error codes are expected to be negative
  DCHECK(error_code < 0);
  std::wstring details = l10n_util::GetString(options.details_resource_id);
  error_strings->SetString(
      LIT16("details"),
      WideToUTF16Hack(l10n_util::GetStringF(
                          IDS_ERRORPAGES_DETAILS_TEMPLATE,
                          IntToWString(-error_code),
                          ASCIIToWide(net::ErrorToString(error_code)),
                          details)));

  if (options.suggestions & SUGGEST_RELOAD) {
    DictionaryValue* suggest_reload = new DictionaryValue;
    suggest_reload->SetString(
        LIT16("msg"),
        WideToUTF16Hack(l10n_util::GetString(
                            IDS_ERRORPAGES_SUGGESTION_RELOAD)));
    suggest_reload->SetString(LIT16("reloadUrl"),
                              WideToUTF16Hack(failed_url));
    error_strings->Set(LIT16("suggestionsReload"), suggest_reload);
  }

  if (options.suggestions & SUGGEST_HOSTNAME) {
    // Only show the "Go to hostname" suggestion if the failed_url has a path.
    const GURL& failed_url = error.GetFailedURL();
    if (std::string() == failed_url.path()) {
      DictionaryValue* suggest_home_page = new DictionaryValue;
      suggest_home_page->SetString(
          LIT16("suggestionsHomepageMsg"),
          WideToUTF16Hack(l10n_util::GetString(
                              IDS_ERRORPAGES_SUGGESTION_HOMEPAGE)));
      std::wstring homepage(ASCIIToWide(failed_url.GetWithEmptyPath().spec()));
      // URLs are always LTR.
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
        l10n_util::WrapStringWithLTRFormatting(&homepage);
      suggest_home_page->SetString(LIT16("homePage"),
                                   WideToUTF16Hack(homepage));
      // TODO(tc): we actually want the unicode hostname
      suggest_home_page->SetString(LIT16("hostName"),
                                   UTF8ToUTF16(failed_url.host()));
      error_strings->Set(LIT16("suggestionsHomepage"), suggest_home_page);
    }
  }

  if (options.suggestions & SUGGEST_LEARNMORE) {
    GURL learn_more_url;
    switch (options.error_code) {
      case net::ERR_TOO_MANY_REDIRECTS:
        learn_more_url = GURL(kRedirectLoopLearnMoreUrl);
        break;
      default:
        break;
    }

    if (learn_more_url.is_valid()) {
      // Add the language parameter to the URL.
      std::string query = learn_more_url.query() + "&hl=" +
          WideToASCII(webkit_glue::GetWebKitLocale());
      GURL::Replacements repl;
      repl.SetQueryStr(query);
      learn_more_url = learn_more_url.ReplaceComponents(repl);

      DictionaryValue* suggest_learn_more = new DictionaryValue;
      suggest_learn_more->SetString(
          LIT16("msg"),
          WideToUTF16Hack(l10n_util::GetString(
                              IDS_ERRORPAGES_SUGGESTION_LEARNMORE)));
      suggest_learn_more->SetString(LIT16("learnMoreUrl"),
                                    UTF8ToUTF16(learn_more_url.spec()));
      error_strings->Set(LIT16("suggestionsLearnMore"), suggest_learn_more);
    }
  }
}

void GetFormRepostErrorValues(const GURL& display_url,
                              DictionaryValue* error_strings) {
  std::wstring failed_url(ASCIIToWide(display_url.spec()));
  // URLs are always LTR.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetString(
      LIT16("title"),
      WideToUTF16Hack(l10n_util::GetStringF(
                          IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
                          failed_url.c_str())));
  error_strings->SetString(
      LIT16("heading"),
      WideToUTF16Hack(l10n_util::GetString(IDS_HTTP_POST_WARNING_TITLE)));
  error_strings->SetString(LIT16("suggestionsHeading"), LIT16(""));
  DictionaryValue* summary = new DictionaryValue;
  summary->SetString(
      LIT16("msg"),
      WideToUTF16Hack(l10n_util::GetString(IDS_ERRORPAGES_HTTP_POST_WARNING)));
  error_strings->Set(LIT16("summary"), summary);
}
