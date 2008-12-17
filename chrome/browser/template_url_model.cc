// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/template_url_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_prepopulate_data.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "net/base/net_util.h"
#include "unicode/rbbi.h"
#include "unicode/uchar.h"

using base::Time;

// String in the URL that is replaced by the search term.
static const wchar_t kSearchTermParameter[] = L"{searchTerms}";

// String in Initializer that is replaced with kSearchTermParameter.
static const wchar_t kTemplateParameter[] = L"%s";

// Term used when generating a search url. Use something obscure so that on
// the rare case the term replaces the URL it's unlikely another keyword would
// have the same url.
static const wchar_t kReplacementTerm[] = L"blah.blah.blah.blah.blah";

class TemplateURLModel::LessWithPrefix {
 public:
  // We want to find the set of keywords that begin with a prefix.  The STL
  // algorithms will return the set of elements that are "equal to" the
  // prefix, where "equal(x, y)" means "!(cmp(x, y) || cmp(y, x))".  When
  // cmp() is the typical std::less<>, this results in lexicographic equality;
  // we need to extend this to mark a prefix as "not less than" a keyword it
  // begins, which will cause the desired elements to be considered "equal to"
  // the prefix.  Note: this is still a strict weak ordering, as required by
  // equal_range() (though I will not prove that here).
  //
  // Unfortunately the calling convention is not "prefix and element" but
  // rather "two elements", so we pass the prefix as a fake "element" which has
  // a NULL KeywordDataElement pointer.
  bool operator()(const KeywordToTemplateMap::value_type& elem1,
                  const KeywordToTemplateMap::value_type& elem2) const {
    return (elem1.second == NULL) ?
        (elem2.first.compare(0, elem1.first.length(), elem1.first) > 0) :
        (elem1.first < elem2.first);
  }
};

TemplateURLModel::TemplateURLModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      next_id_(1) {
  DCHECK(profile_);
  Init(NULL, 0);
}

TemplateURLModel::TemplateURLModel(const Initializer* initializers,
                                   const int count)
    : profile_(NULL),
      loaded_(true),
      load_handle_(0),
      service_(NULL),
      default_search_provider_(NULL),
      next_id_(1) {
  Init(initializers, count);
}

TemplateURLModel::~TemplateURLModel() {
  if (load_handle_) {
    DCHECK(service_.get());
    service_->CancelRequest(load_handle_);
  }

  STLDeleteElements(&template_urls_);

  NotificationService* ns = NotificationService::current();
  if (profile_) {
    ns->RemoveObserver(this, NOTIFY_HISTORY_URL_VISITED,
                       Source<Profile>(profile_->GetOriginalProfile()));
  }
  ns->RemoveObserver(this, NOTIFY_GOOGLE_URL_UPDATED,
                     NotificationService::AllSources());
}

void TemplateURLModel::Init(const Initializer* initializers,
                            int num_initializers) {
  // Register for notifications.
  NotificationService* ns = NotificationService::current();
  if (profile_) {
    // TODO(sky): bug 1166191. The keywords should be moved into the history
    // db, which will mean we no longer need this notification and the history
    // backend can handle automatically adding the search terms as the user
    // navigates.
    ns->AddObserver(this, NOTIFY_HISTORY_URL_VISITED,
                    Source<Profile>(profile_->GetOriginalProfile()));
  }
  ns->AddObserver(this, NOTIFY_GOOGLE_URL_UPDATED,
                  NotificationService::AllSources());

  // Add specific initializers, if any.
  for (int i(0); i < num_initializers; ++i) {
    DCHECK(initializers[i].keyword);
    DCHECK(initializers[i].url);
    DCHECK(initializers[i].content);

    size_t template_position =
        std::wstring(initializers[i].url).find(kTemplateParameter);
    DCHECK(template_position != std::wstring::npos);
    std::wstring osd_url(initializers[i].url);
    osd_url.replace(template_position, arraysize(kTemplateParameter) - 1,
                    kSearchTermParameter);

    // TemplateURLModel ends up owning the TemplateURL, don't try and free it.
    TemplateURL* template_url = new TemplateURL();
    template_url->set_keyword(initializers[i].keyword);
    template_url->set_short_name(initializers[i].content);
    template_url->SetURL(osd_url, 0, 0);
    Add(template_url);
  }

  // Request a server check for the correct Google URL if Google is the default
  // search engine.
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  if (default_provider) {
    const TemplateURLRef* default_provider_ref = default_provider->url();
    if (default_provider_ref && default_provider_ref->HasGoogleBaseURLs())
      GoogleURLTracker::RequestServerCheck();
  }
}

// static
std::wstring TemplateURLModel::GenerateKeyword(const GURL& url,
                                               bool autodetected) {
  // Don't autogenerate keywords for referrers that are the result of a form
  // submission (TODO: right now we approximate this by checking for the URL
  // having a query, but we should replace this with a call to WebCore to see if
  // the originating page was actually a form submission), anything other than
  // http, or referrers with a path.
  //
  // If we relax the path constraint, we need to be sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!url.is_valid() ||
      (autodetected && (url.has_query() || (url.scheme() != "http") ||
                        ((url.path() != "") && (url.path() != "/")))))
    return std::wstring();

  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://b/issue?id=1205573.
  return net::StripWWW(UTF8ToWide(url.host()));
}

// static
std::wstring TemplateURLModel::CleanUserInputKeyword(
    const std::wstring& keyword) {
  // Remove the scheme.
  std::wstring result(l10n_util::ToLower(keyword));
  url_parse::Component scheme_component;
  if (url_parse::ExtractScheme(WideToUTF8(keyword).c_str(),
                               static_cast<int>(keyword.length()),
                               &scheme_component)) {
    // Include trailing ':'.
    result.erase(0, scheme_component.end() + 1);
    // Many schemes usually have "//" after them, so strip it too.
    const std::wstring after_scheme(L"//");
    if (result.compare(0, after_scheme.length(), after_scheme) == 0)
      result.erase(0, after_scheme.length());
  }

  // Remove leading "www.".
  result = net::StripWWW(result);

  // Remove trailing "/".
  return (result.length() > 0 && result[result.length() - 1] == L'/') ?
      result.substr(0, result.length() - 1) : result;
}

// static
GURL TemplateURLModel::GenerateSearchURL(const TemplateURL* t_url) {
  DCHECK(t_url);
  const TemplateURLRef* search_ref = t_url->url();
  if (!search_ref || !search_ref->IsValid())
    return GURL();

  if (!search_ref->SupportsReplacement())
    return GURL(WideToUTF8(search_ref->url()));

  return search_ref->ReplaceSearchTerms(
      *t_url,
      kReplacementTerm,
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring());
}

bool TemplateURLModel::CanReplaceKeyword(
    const std::wstring& keyword,
    const std::wstring& url,
    const TemplateURL** template_url_to_replace) {
  DCHECK(!keyword.empty()); // This should only be called for non-empty
                            // keywords. If we need to support empty kewords
                            // the code needs to change slightly.
  const TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (existing_url) {
    // We already have a TemplateURL for this keyword. Only allow it to be
    // replaced if the TemplateURL can be replaced.
    if (template_url_to_replace)
      *template_url_to_replace = existing_url;
    return CanReplace(existing_url);
  }

  // We don't have a TemplateURL with keyword. Only allow a new one if there
  // isn't a TemplateURL for the specified host, or there is one but it can
  // be replaced. We do this to ensure that if the user assigns a different
  // keyword to a generated TemplateURL, we won't regenerate another keyword for
  // the same host.
  GURL gurl(WideToUTF8(url));
  if (gurl.is_valid() && !gurl.host().empty())
    return CanReplaceKeywordForHost(gurl.host(), template_url_to_replace);
  return true;
}

void TemplateURLModel::FindMatchingKeywords(
    const std::wstring& prefix,
    bool support_replacement_only,
    std::vector<std::wstring>* matches) const {
  // Sanity check args.
  if (prefix.empty())
    return;
  DCHECK(matches != NULL);
  DCHECK(matches->empty());  // The code for exact matches assumes this.

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const std::pair<KeywordToTemplateMap::const_iterator,
                  KeywordToTemplateMap::const_iterator> match_range(
      std::equal_range(
          keyword_to_template_map_.begin(), keyword_to_template_map_.end(),
          KeywordToTemplateMap::value_type(prefix, NULL), LessWithPrefix()));

  // Return vector of matching keywords.
  for (KeywordToTemplateMap::const_iterator i(match_range.first);
       i != match_range.second; ++i) {
    DCHECK(i->second->url());
    if (!support_replacement_only || i->second->url()->SupportsReplacement())
      matches->push_back(i->first);
  }
}

const TemplateURL* TemplateURLModel::GetTemplateURLForKeyword(
                                     const std::wstring& keyword) const {
  KeywordToTemplateMap::const_iterator elem(
      keyword_to_template_map_.find(keyword));
  return (elem == keyword_to_template_map_.end()) ? NULL : elem->second;
}

const TemplateURL* TemplateURLModel::GetTemplateURLForHost(
    const std::string& host) const {
  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return NULL;
  return *(iter->second.begin());  // Return the 1st element.
}

void TemplateURLModel::Add(TemplateURL* template_url) {
  DCHECK(template_url);
  DCHECK(template_url->id() == 0);
  DCHECK(find(template_urls_.begin(), template_urls_.end(), template_url) ==
         template_urls_.end());
  template_url->set_id(++next_id_);
  template_urls_.push_back(template_url);
  AddToMaps(template_url);

  if (service_.get())
    service_->AddKeyword(*template_url);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}

void TemplateURLModel::AddToMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty())
    keyword_to_template_map_[template_url->keyword()] = template_url;

  const GURL url(GenerateSearchURL(template_url));
  if (url.is_valid() && url.has_host())
    host_to_urls_map_[url.host()].insert(template_url);
}

void TemplateURLModel::Remove(const TemplateURL* template_url) {
  TemplateURLVector::iterator i = find(template_urls_.begin(),
                                       template_urls_.end(),
                                       template_url);
  if (i == template_urls_.end())
    return;

  if (template_url == default_search_provider_) {
    // Should never delete the default search provider.
    NOTREACHED();
    return;
  }

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  template_urls_.erase(i);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }

  if (service_.get())
    service_->RemoveKeyword(*template_url);

  if (profile_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->DeleteAllSearchTermsForKeyword(template_url->id());
  }

  // We own the TemplateURL and need to delete it.
  delete template_url;
}

void TemplateURLModel::Replace(const TemplateURL* existing_turl,
                               TemplateURL* new_turl) {
  DCHECK(existing_turl && new_turl);

  TemplateURLVector::iterator i = find(template_urls_.begin(),
                                       template_urls_.end(),
                                       existing_turl);
  DCHECK(i != template_urls_.end());
  RemoveFromMaps(existing_turl);
  template_urls_.erase(i);

  new_turl->set_id(existing_turl->id());

  template_urls_.push_back(new_turl);
  AddToMaps(new_turl);

  if (service_.get())
    service_->UpdateKeyword(*new_turl);

  if (default_search_provider_ == existing_turl)
    SetDefaultSearchProvider(new_turl);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }

  delete existing_turl;
}

void TemplateURLModel::RemoveAutoGeneratedBetween(Time created_after,
                                                  Time created_before) {
  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i])) {
      Remove(template_urls_[i]);
    } else {
      ++i;
    }
  }
}

void TemplateURLModel::RemoveAutoGeneratedSince(Time created_after) {
  RemoveAutoGeneratedBetween(created_after, Time());
}

void TemplateURLModel::SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                                   const GURL& url,
                                                   const std::wstring& term) {
  HistoryService* history = profile_  ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : NULL;
  if (!history)
    return;
  history->SetKeywordSearchTermsForURL(url, t_url->id(), term);
}

void TemplateURLModel::RemoveFromMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty()) {
    keyword_to_template_map_.erase(template_url->keyword());
  }

  const GURL url(GenerateSearchURL(template_url));
  if (url.is_valid() && url.has_host()) {
    const std::string host(url.host());
    DCHECK(host_to_urls_map_.find(host) != host_to_urls_map_.end());
    TemplateURLSet& urls = host_to_urls_map_[host];
    DCHECK(urls.find(template_url) != urls.end());
    urls.erase(urls.find(template_url));
    if (urls.empty())
      host_to_urls_map_.erase(host_to_urls_map_.find(host));
  }
}

void TemplateURLModel::RemoveFromMapsByPointer(
    const TemplateURL* template_url) {
  DCHECK(template_url);
  for (KeywordToTemplateMap::iterator i = keyword_to_template_map_.begin();
       i != keyword_to_template_map_.end(); ++i) {
    if (i->second == template_url) {
      keyword_to_template_map_.erase(i);
      // A given TemplateURL only occurs once in the map. As soon as we find the
      // entry, stop.
      break;
    }
  }

  for (HostToURLsMap::iterator i = host_to_urls_map_.begin();
       i != host_to_urls_map_.end(); ++i) {
    TemplateURLSet::iterator url_set_iterator = i->second.find(template_url);
    if (url_set_iterator != i->second.end()) {
      i->second.erase(url_set_iterator);
      if (i->second.empty())
        host_to_urls_map_.erase(i);
      // A given TemplateURL only occurs once in the map. As soon as we find the
      // entry, stop.
      return;
    }
  }
}

void TemplateURLModel::SetTemplateURLs(
      const std::vector<const TemplateURL*>& urls) {
  DCHECK(template_urls_.empty()); // This should only be called on load,
                                  // when we have no TemplateURLs.

  // Add mappings for the new items.
  for (TemplateURLVector::const_iterator i = urls.begin(); i != urls.end();
       ++i) {
    next_id_ = std::max(next_id_, (*i)->id());
    AddToMaps(*i);
  }

  template_urls_ = urls;
}

std::vector<const TemplateURL*> TemplateURLModel::GetTemplateURLs() const {
  return template_urls_;
}

void TemplateURLModel::IncrementUsageCount(const TemplateURL* url) {
  DCHECK(url && find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  const_cast<TemplateURL*>(url)->set_usage_count(url->usage_count() + 1);
  if (service_.get())
    service_.get()->UpdateKeyword(*url);
}

void TemplateURLModel::ResetTemplateURL(const TemplateURL* url,
                                        const std::wstring& title,
                                        const std::wstring& keyword,
                                        const std::wstring& search_url) {
  DCHECK(url && find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  RemoveFromMaps(url);
  TemplateURL* modifiable_url = const_cast<TemplateURL*>(url);
  modifiable_url->set_short_name(title);
  modifiable_url->set_keyword(keyword);
  if ((modifiable_url->url() && search_url.empty()) ||
      (!modifiable_url->url() && !search_url.empty()) ||
      (modifiable_url->url() && modifiable_url->url()->url() != search_url)) {
    // The urls have changed, reset the favicon url.
    modifiable_url->SetFavIconURL(GURL());
    modifiable_url->SetURL(search_url, 0, 0);
  }
  modifiable_url->set_safe_for_autoreplace(false);
  AddToMaps(url);
  if (service_.get())
    service_.get()->UpdateKeyword(*url);

  FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                    OnTemplateURLModelChanged());
}

void TemplateURLModel::SetDefaultSearchProvider(const TemplateURL* url) {
  if (default_search_provider_ == url)
    return;

  DCHECK(!url || find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  default_search_provider_ = url;

  if (url) {
    TemplateURL* modifiable_url = const_cast<TemplateURL*>(url);
    // Don't mark the url as edited, otherwise we won't be able to rev the
    // templateurls we ship with.
    modifiable_url->set_show_in_default_list(true);
    if (service_.get())
      service_.get()->UpdateKeyword(*url);

    const TemplateURLRef* url_ref = url->url();
    if (url_ref && url_ref->HasGoogleBaseURLs()) {
      GoogleURLTracker::RequestServerCheck();
      RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                     RLZTracker::CHROME_OMNIBOX,
                                     RLZTracker::SET_TO_GOOGLE);
    }
  }

  SaveDefaultSearchProviderToPrefs(url);

  if (service_.get())
    service_->SetDefaultSearchProvider(url);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}

const TemplateURL* TemplateURLModel::GetDefaultSearchProvider() {
  if (loaded_)
    return default_search_provider_;

  if (!prefs_default_search_provider_.get()) {
    TemplateURL* default_from_prefs;
    if (LoadDefaultSearchProviderFromPrefs(&default_from_prefs)) {
      prefs_default_search_provider_.reset(default_from_prefs);
    } else {
      std::vector<TemplateURL*> loaded_urls;
      size_t default_search_index;
      TemplateURLPrepopulateData::GetPrepopulatedEngines(GetPrefs(),
                                                         &loaded_urls,
                                                         &default_search_index);
      if (default_search_index < loaded_urls.size()) {
        prefs_default_search_provider_.reset(loaded_urls[default_search_index]);
        loaded_urls.erase(loaded_urls.begin() + default_search_index);
      }
      STLDeleteElements(&loaded_urls);
    }
  }

  return prefs_default_search_provider_.get();
}

void TemplateURLModel::AddObserver(TemplateURLModelObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TemplateURLModel::RemoveObserver(TemplateURLModelObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TemplateURLModel::Load() {
  if (loaded_ || load_handle_)
    return;

  if (!service_.get())
    service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);

  if (service_.get()) {
    load_handle_ = service_->GetKeywords(this);
  } else {
    loaded_ = true;
    NotifyLoaded();
  }
}

void TemplateURLModel::OnWebDataServiceRequestDone(
                       WebDataService::Handle h,
                       const WDTypedResult* result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away.
    loaded_ = true;
    NotifyLoaded();
    return;
  }

  DCHECK(result->GetType() == KEYWORDS_RESULT);

  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(result)->GetValue();

  // prefs_default_search_provider_ is only needed before we've finished
  // loading. Now that we've loaded we can nuke it.
  prefs_default_search_provider_.reset();

  // Compiler won't implicitly convert std::vector<TemplateURL*> to
  // std::vector<const TemplateURL*>, and reinterpret_cast is unsafe,
  // so we just copy it.
  std::vector<const TemplateURL*> template_urls(keyword_result.keywords.begin(),
                                                keyword_result.keywords.end());

  const int resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion();
  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    // There should never be duplicate TemplateURLs. We had a bug such that
    // duplicate TemplateURLs existed for one locale. As such we invoke
    // RemoveDuplicatePrepopulateIDs to nuke the duplicates.
    RemoveDuplicatePrepopulateIDs(&template_urls);
  }
  SetTemplateURLs(template_urls);

  if (keyword_result.default_search_provider_id) {
    // See if we can find the default search provider.
    for (TemplateURLVector::iterator i = template_urls_.begin();
         i != template_urls_.end(); ++i) {
      if ((*i)->id() == keyword_result.default_search_provider_id) {
        default_search_provider_ = *i;
        break;
      }
    }
  }

  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    MergeEnginesFromPrepopulateData();
    service_->SetBuiltinKeywordVersion(resource_keyword_version);
  }

  // Always save the default search provider to prefs. That way we don't have to
  // worry about it being out of sync.
  if (default_search_provider_)
    SaveDefaultSearchProviderToPrefs(default_search_provider_);

  // Delete any hosts that were deleted before we finished loading.
  for (std::vector<std::wstring>::iterator i = hosts_to_delete_.begin();
       i != hosts_to_delete_.end(); ++i) {
    DeleteGeneratedKeywordsMatchingHost(*i);
  }
  hosts_to_delete_.clear();

  // Index any visits that occurred before we finished loading.
  for (size_t i = 0; i < visits_to_add_.size(); ++i)
    UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
  visits_to_add_.clear();

  loaded_ = true;

  FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                    OnTemplateURLModelChanged());

  NotifyLoaded();
}

void TemplateURLModel::RemoveDuplicatePrepopulateIDs(
    std::vector<const TemplateURL*>* urls) {
  std::set<int> ids;
  for (std::vector<const TemplateURL*>::iterator i = urls->begin();
       i != urls->end(); ) {
    int prepopulate_id = (*i)->prepopulate_id();
    if (prepopulate_id) {
      if (ids.find(prepopulate_id) != ids.end()) {
        if (service_.get())
          service_->RemoveKeyword(**i);
        delete *i;
        i = urls->erase(i);
      } else {
        ids.insert(prepopulate_id);
        ++i;
      }
    } else {
      ++i;
    }
  }
}

void TemplateURLModel::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (type == NOTIFY_HISTORY_URL_VISITED) {
    Details<history::URLVisitedDetails> visit_details(details);

    if (!loaded())
      visits_to_add_.push_back(visit_details->row);
    else
      UpdateKeywordSearchTermsForURL(visit_details->row);
  } else if (type == NOTIFY_GOOGLE_URL_UPDATED) {
    if (loaded_)
      GoogleBaseURLChanged();
  } else {
    NOTREACHED();
  }
}

void TemplateURLModel::DeleteGeneratedKeywordsMatchingHost(
    const std::wstring& host) {
  const std::wstring host_slash = host + L"/";
  // Iterate backwards as we may end up removing multiple entries.
  for (int i = static_cast<int>(template_urls_.size()) - 1; i >= 0; --i) {
    if (CanReplace(template_urls_[i]) &&
        (template_urls_[i]->keyword() == host ||
         template_urls_[i]->keyword().compare(0, host_slash.length(),
                                              host_slash) == 0)) {
      Remove(template_urls_[i]);
    }
  }
}

void TemplateURLModel::NotifyLoaded() {
  NotificationService::current()->
      Notify(TEMPLATE_URL_MODEL_LOADED, Source<TemplateURLModel>(this),
             NotificationService::NoDetails());
}

void TemplateURLModel::MergeEnginesFromPrepopulateData() {
  // Build a map from prepopulate id to TemplateURL of existing urls.
  std::map<int, const TemplateURL*> id_to_turl;
  for (size_t i = 0; i < template_urls_.size(); ++i) {
    if (template_urls_[i]->prepopulate_id() > 0)
      id_to_turl[template_urls_[i]->prepopulate_id()] = template_urls_[i];
  }

  std::vector<TemplateURL*> loaded_urls;
  size_t default_search_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(GetPrefs(),
                                                     &loaded_urls,
                                                     &default_search_index);

  for (size_t i = 0; i < loaded_urls.size(); ++i) {
    scoped_ptr<TemplateURL> t_url(loaded_urls[i]);

    if (!t_url->prepopulate_id()) {
      // Prepopulate engines need an id.
      NOTREACHED();
      continue;
    }

    const TemplateURL* existing_url = id_to_turl[t_url->prepopulate_id()];
    if (existing_url) {
      if (!existing_url->safe_for_autoreplace()) {
        // User edited the entry, preserve the keyword and description.
        loaded_urls[i]->set_safe_for_autoreplace(false);
        loaded_urls[i]->set_keyword(existing_url->keyword());
        loaded_urls[i]->set_autogenerate_keyword(
            existing_url->autogenerate_keyword());
        loaded_urls[i]->set_short_name(existing_url->short_name());
      }
      Replace(existing_url, loaded_urls[i]);
      id_to_turl[t_url->prepopulate_id()] = loaded_urls[i];
    } else {
      Add(loaded_urls[i]);
    }
    if (i == default_search_index && !default_search_provider_)
      SetDefaultSearchProvider(loaded_urls[i]);

    t_url.release();
  }
}

void TemplateURLModel::SaveDefaultSearchProviderToPrefs(
    const TemplateURL* t_url) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  RegisterPrefs(prefs);

  const std::wstring search_url =
      (t_url && t_url->url()) ? t_url->url()->url() : std::wstring();
  prefs->SetString(prefs::kDefaultSearchProviderSearchURL, search_url);

  const std::wstring suggest_url =
      (t_url && t_url->suggestions_url()) ? t_url->suggestions_url()->url() :
                                            std::wstring();
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, suggest_url);

  const std::wstring name =
      t_url ? t_url->short_name() : std::wstring();
  prefs->SetString(prefs::kDefaultSearchProviderName, name);

  const std::wstring id_string =
      t_url ? Int64ToWString(t_url->id()) : std::wstring();
  prefs->SetString(prefs::kDefaultSearchProviderID, id_string);

  prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
}

bool TemplateURLModel::LoadDefaultSearchProviderFromPrefs(
    TemplateURL** default_provider) {
  PrefService* prefs = GetPrefs();
  if (!prefs || !prefs->HasPrefPath(prefs::kDefaultSearchProviderSearchURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderSuggestURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderName) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderID)) {
    return false;
  }
  RegisterPrefs(prefs);

  std::wstring suggest_url =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURL);
  std::wstring search_url =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURL);

  if (suggest_url.empty() && search_url.empty()) {
    // The user doesn't want a default search provider.
    *default_provider = NULL;
    return true;
  }

  std::wstring name = prefs->GetString(prefs::kDefaultSearchProviderName);

  std::wstring id_string = prefs->GetString(prefs::kDefaultSearchProviderID);

  *default_provider = new TemplateURL();
  (*default_provider)->set_short_name(name);
  (*default_provider)->SetURL(search_url, 0, 0);
  (*default_provider)->SetSuggestionsURL(suggest_url, 0, 0);
  if (!id_string.empty())
    (*default_provider)->set_id(StringToInt64(id_string));
  return true;
}

void TemplateURLModel::RegisterPrefs(PrefService* prefs) {
  if (prefs->IsPrefRegistered(prefs::kDefaultSearchProviderName))
    return;
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderName, std::wstring());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderID, std::wstring());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderSuggestURL, std::wstring());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchURL, std::wstring());
}

bool TemplateURLModel::CanReplaceKeywordForHost(
    const std::string& host,
    const TemplateURL** to_replace) {
  const HostToURLsMap::iterator matching_urls = host_to_urls_map_.find(host);
  const bool have_matching_urls = (matching_urls != host_to_urls_map_.end());
  if (have_matching_urls) {
    TemplateURLSet& urls = matching_urls->second;
    for (TemplateURLSet::iterator i = urls.begin(); i != urls.end(); ++i) {
      const TemplateURL* url = *i;
      if (CanReplace(url)) {
        if (to_replace)
          *to_replace = url;
        return true;
      }
    }
  }

  if (to_replace)
    *to_replace = NULL;
  return !have_matching_urls;
}

bool TemplateURLModel::CanReplace(const TemplateURL* t_url) {
  return (t_url != default_search_provider_ && !t_url->show_in_default_list() &&
          t_url->safe_for_autoreplace());
}

PrefService* TemplateURLModel::GetPrefs() {
  return profile_ ? profile_->GetPrefs() : NULL;
}

void TemplateURLModel::UpdateKeywordSearchTermsForURL(
    const history::URLRow& row) {
  if (!row.url().is_valid() ||
      !row.url().parsed_for_possibly_invalid_spec().query.is_nonempty()) {
    return;
  }

  HostToURLsMap::const_iterator t_urls_for_host_iterator =
      host_to_urls_map_.find(row.url().host());
  if (t_urls_for_host_iterator == host_to_urls_map_.end() ||
      t_urls_for_host_iterator->second.empty()) {
    return;
  }

  const TemplateURLSet& urls_for_host = t_urls_for_host_iterator->second;
  QueryTerms query_terms;
  bool built_terms = false;  // Most URLs won't match a TemplateURLs host;
                             // so we lazily build the query_terms.
  const std::string path = row.url().path();

  for (TemplateURLSet::const_iterator i = urls_for_host.begin();
       i != urls_for_host.end(); ++i) {
    const TemplateURLRef* search_ref = (*i)->url();

    // Count the URL against a TemplateURL if the host and path of the
    // visited URL match that of the TemplateURL as well as the search term's
    // key of the TemplateURL occurring in the visited url.
    //
    // NOTE: Even though we're iterating over TemplateURLs indexed by the host
    // of the URL we still need to call GetHost on the search_ref. In
    // particular, GetHost returns an empty string if search_ref doesn't support
    // replacement or isn't valid for use in keyword search terms.

    if (search_ref && search_ref->GetHost() == row.url().host() &&
        search_ref->GetPath() == path) {
      if (!built_terms && !BuildQueryTerms(row.url(), &query_terms)) {
        // No query terms. No need to continue with the rest of the
        // TemplateURLs.
        return;
      }
      built_terms = true;

      QueryTerms::iterator terms_iterator =
          query_terms.find(search_ref->GetSearchTermKey());
      if (terms_iterator != query_terms.end() &&
          !terms_iterator->second.empty()) {
        SetKeywordSearchTermsForURL(
            *i, row.url(), search_ref->SearchTermToWide(*(*i),
            terms_iterator->second));
      }
    }
  }
}

// static
bool TemplateURLModel::BuildQueryTerms(const GURL& url,
                                       QueryTerms* query_terms) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  size_t valid_term_count = 0;
  while (url_parse::ExtractQueryKeyValue(url.spec().c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      std::string key_string = url.spec().substr(key.begin, key.len);
      std::string value_string = url.spec().substr(value.begin, value.len);
      QueryTerms::iterator query_terms_iterator =
          query_terms->find(key_string);
      if (query_terms_iterator != query_terms->end()) {
        if (!query_terms_iterator->second.empty() &&
            query_terms_iterator->second != value_string) {
          // The term occurs in multiple places with different values. Treat
          // this as if the term doesn't occur by setting the value to an empty
          // string.
          (*query_terms)[key_string] = std::string();
          DCHECK (valid_term_count > 0);
          valid_term_count--;
        }
      } else {
        valid_term_count++;
        (*query_terms)[key_string] = value_string;
      }
    }
  }
  return (valid_term_count > 0);
}

void TemplateURLModel::GoogleBaseURLChanged() {
  bool something_changed = false;
  for (size_t i = 0; i < template_urls_.size(); ++i) {
    const TemplateURL* t_url = template_urls_[i];
    if ((t_url->url() && t_url->url()->HasGoogleBaseURLs()) ||
        (t_url->suggestions_url() &&
         t_url->suggestions_url()->HasGoogleBaseURLs())) {
      RemoveFromMapsByPointer(t_url);
      t_url->InvalidateCachedValues();
      AddToMaps(t_url);
      something_changed = true;
    }
  }

  if (something_changed && loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}
