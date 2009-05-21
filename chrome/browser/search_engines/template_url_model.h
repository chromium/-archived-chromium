// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TEMPLATE_URL_MODEL_H__
#define CHROME_BROWSER_TEMPLATE_URL_MODEL_H__

#include <map>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;
class TemplateURL;
class TemplateURLModelTest;

// TemplateURLModel is the backend for keywords. It's used by
// KeywordAutocomplete.
//
// TemplateURLModel stores a vector of TemplateURLs. The TemplateURLs are
// persisted to the database maintained by WebDataService. *ALL* mutations
// to the TemplateURLs must funnel through TemplateURLModel. This allows
// TemplateURLModel to notify listeners of changes as well as keep the
// database in sync.
//
// There is a TemplateURLModel per Profile.
//
// TemplateURLModel does not load the vector of TemplateURLs in it's
// constructor (except for testing). Use the Load method to trigger a load.
// When TemplateURLModel has completed loading, observers are notified via
// OnTemplateURLModelChanged as well as the TEMPLATE_URL_MODEL_LOADED
// notification message.
//
// TemplateURLModel takes ownership of any TemplateURL passed to it. If there
// is a WebDataService, deletion is handled by WebDataService, otherwise
// TemplateURLModel handles deletion.

// TemplateURLModelObserver is notified whenever the set of TemplateURLs
// are modified.
class TemplateURLModelObserver {
 public:
  // Notification that the template url model has changed in some way.
  virtual void OnTemplateURLModelChanged() = 0;
};

class TemplateURLModel : public WebDataServiceConsumer,
                         public NotificationObserver {
 public:
  typedef std::map<std::string, std::string> QueryTerms;

  // Struct used for initializing the data store with fake data.
  // Each initializer is mapped to a TemplateURL.
  struct Initializer {
    const wchar_t* const keyword;
    const wchar_t* const url;
    const wchar_t* const content;
  };

  explicit TemplateURLModel(Profile* profile);
  // The following is for testing.
  TemplateURLModel(const Initializer* initializers, const int count);

  ~TemplateURLModel();

  // Generates a suitable keyword for the specified url.  Returns an empty
  // string if a keyword couldn't be generated.  If |autodetected| is true, we
  // don't generate keywords for a variety of situations where we would probably
  // not want to auto-add keywords, such as keywords for searches on pages that
  // themselves come from form submissions.
  static std::wstring GenerateKeyword(const GURL& url, bool autodetected);

  // Removes any unnecessary characters from a user input keyword.
  // This removes the leading scheme, "www." and any trailing slash.
  static std::wstring CleanUserInputKeyword(const std::wstring& keyword);

  // Returns the search url for t_url.  Returns an empty GURL if t_url has no
  // url().
  static GURL GenerateSearchURL(const TemplateURL* t_url);

  // Returns true if there is no TemplateURL that conflicts with the
  // keyword/url pair, or there is one but it can be replaced. If there is an
  // existing keyword that can be replaced and template_url_to_replace is
  // non-NULL, template_url_to_replace is set to the keyword to replace.
  //
  // url gives the url of the search query. The url is used to avoid generating
  // a TemplateURL for an existing TemplateURL that shares the same host.
  bool CanReplaceKeyword(const std::wstring& keyword,
                         const std::wstring& url,
                         const TemplateURL** template_url_to_replace);

  // Returns (in |matches|) all keywords beginning with |prefix|, sorted
  // shortest-first. If support_replacement_only is true, only keywords that
  // support replacement are returned.
  void FindMatchingKeywords(const std::wstring& prefix,
                            bool support_replacement_only,
                            std::vector<std::wstring>* matches) const;

  // Looks up |keyword| and returns the element it maps to.  Returns NULL if
  // the keyword was not found.
  // The caller should not try to delete the returned pointer; the data store
  // retains ownership of it.
  const TemplateURL* GetTemplateURLForKeyword(
    const std::wstring& keyword) const;

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Adds a new TemplateURL to this model. TemplateURLModel will own the
  // reference, and delete it when the TemplateURL is removed.
  void Add(TemplateURL* template_url);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  void Remove(const TemplateURL* template_url);

  // Removes all auto-generated keywords that were created in the specified
  // range.
  void RemoveAutoGeneratedBetween(base::Time created_after,
                                  base::Time created_before);

  // Replaces existing_turl with new_turl. new_turl is given the same ID as
  // existing_turl. If existing_turl was the default, new_turl is made the
  // default. After this call existing_turl is deleted. As with Add,
  // TemplateURLModel takes ownership of existing_turl.
  void Replace(const TemplateURL* existing_turl,
               TemplateURL* new_turl);

  // Removes all auto-generated keywords that were created on or after the
  // date passed in.
  void RemoveAutoGeneratedSince(base::Time created_after);

  // Returns the set of URLs describing the keywords. The elements are owned
  // by TemplateURLModel and should not be deleted.
  std::vector<const TemplateURL*> GetTemplateURLs() const;

  // Increment the usage count of a keyword.
  // Called when a URL is loaded that was generated from a keyword.
  void IncrementUsageCount(const TemplateURL* url);

  // Resets the title, keyword and search url of the specified TemplateURL.
  // The TemplateURL is marked as not replaceable.
  void ResetTemplateURL(const TemplateURL* url,
                        const std::wstring& title,
                        const std::wstring& keyword,
                        const std::wstring& search_url);

  // The default search provider. This may be null.
  void SetDefaultSearchProvider(const TemplateURL* url);

  // Returns the default search provider. If the TemplateURLModel hasn't been
  // loaded, the default search provider is pulled from preferences.
  //
  // NOTE: At least in unittest mode, this may return NULL.
  const TemplateURL* GetDefaultSearchProvider();

  // Observers used to listen for changes to the model.
  // TemplateURLModel does NOT delete the observers when deleted.
  void AddObserver(TemplateURLModelObserver* observer);
  void RemoveObserver(TemplateURLModelObserver* observer);

  // Loads the keywords. This has no effect if the keywords have already been
  // loaded.
  // Observers are notified when loading completes via the method
  // OnTemplateURLsReset.
  void Load();

  // Whether or not the keywords have been loaded.
  bool loaded() { return loaded_; }

  // Notification that the keywords have been loaded.
  // This is invoked from WebDataService, and should not be directly
  // invoked.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Removes (and deletes) TemplateURLs from |urls| that have duplicate
  // prepopulate ids. Duplicate prepopulate ids are not allowed, but due to a
  // bug it was possible get dups. This step is only called when the version
  // number changes.
  void RemoveDuplicatePrepopulateIDs(std::vector<const TemplateURL*>* urls);

  // NotificationObserver method. TemplateURLModel listens for three
  // notification types:
  // . NOTIFY_HISTORY_URL_VISITED: adds keyword search terms if the visit
  //   corresponds to a keyword.
  // . NOTIFY_GOOGLE_URL_UPDATED: updates mapping for any keywords containing
  //   a google base url replacement term.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile() const { return profile_; }

 protected:
  // Cover method for the method of the same name on the HistoryService.
  // url is the one that was visited with the given search terms.
  //
  // This exists and is virtual for testing.
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const std::wstring& term);

 private:
  FRIEND_TEST(TemplateURLModelTest, BuildQueryTerms);
  FRIEND_TEST(TemplateURLModelTest, UpdateKeywordSearchTermsForURL);
  FRIEND_TEST(TemplateURLModelTest, DontUpdateKeywordSearchForNonReplaceable);
  FRIEND_TEST(TemplateURLModelTest, ChangeGoogleBaseValue);
  friend class TemplateURLModelTest;

  typedef std::map<std::wstring, const TemplateURL*> KeywordToTemplateMap;
  typedef std::vector<const TemplateURL*> TemplateURLVector;
  typedef std::set<const TemplateURL*> TemplateURLSet;
  typedef std::map<std::string, TemplateURLSet> HostToURLsMap;

  // Helper functor for FindMatchingKeywords(), for finding the range of
  // keywords which begin with a prefix.
  class LessWithPrefix;

  void Init(const Initializer* initializers, int num_initializers);

  void RemoveFromMaps(const TemplateURL* template_url);

  // Removes the supplied template_url from the maps. This searches through all
  // entries in the maps and does not generate the host or keyword.
  // This is used when the cached content of the TemplateURL changes.
  void RemoveFromMapsByPointer(const TemplateURL* template_url);

  void AddToMaps(const TemplateURL* template_url);

  // Sets the keywords. This is used once the keywords have been loaded.
  // This does NOT notify the delegate or the database.
  void SetTemplateURLs(const std::vector<const TemplateURL*>& urls);

  void DeleteGeneratedKeywordsMatchingHost(const std::wstring& host);

  // If there is a notification service, sends TEMPLATE_URL_MODEL_LOADED
  // notification.
  void NotifyLoaded();

  // Loads engines from prepopulate data and merges them in with the existing
  // engines.  This is invoked when the version of the prepopulate data changes.
  void MergeEnginesFromPrepopulateData();

  // Saves enough of url to preferences so that it can be loaded from
  // preferences on start up.
  void SaveDefaultSearchProviderToPrefs(const TemplateURL* url);

  // Creates a TemplateURL that was previously saved to prefs via
  // SaveDefaultSearchProviderToPrefs. Returns true if successful, false
  // otherwise. This is used if GetDefaultSearchProvider is invoked before the
  // TemplateURL has loaded. If the user has opted for no default search, this
  // returns true but default_provider is set to NULL.
  bool LoadDefaultSearchProviderFromPrefs(TemplateURL** default_provider);

  // Registers the preferences used to save a TemplateURL to prefs.
  void RegisterPrefs(PrefService* prefs);

  // Returns true if there is no TemplateURL that has a search url with the
  // specified host, or the only TemplateURLs matching the specified host can
  // be replaced.
  bool CanReplaceKeywordForHost(const std::string& host,
                                const TemplateURL** to_replace);

  // Returns true if the TemplateURL is replaceable. This doesn't look at the
  // uniqueness of the keyword or host and is intended to be called after those
  // checks have been done. This returns true if the TemplateURL doesn't appear
  // in the default list and is marked as safe_for_autoreplace.
  bool CanReplace(const TemplateURL* t_url);

  // Returns the preferences we use.
  PrefService* GetPrefs();

  // Iterates through the TemplateURLs to see if one matches the visited url.
  // For each TemplateURL whose url matches the visited url
  // SetKeywordSearchTermsForURL is invoked.
  void UpdateKeywordSearchTermsForURL(
      const history::URLVisitedDetails& details);

  // If necessary, generates a visit for the site http:// + t_url.keyword().
  void AddTabToSearchVisit(const TemplateURL& t_url);

  // Adds each of the query terms in the specified url whose key and value are
  // non-empty to query_terms. If a query key appears multiple times, the value
  // is set to an empty string. Returns true if there is at least one key that
  // does not occur multiple times.
  static bool BuildQueryTerms(
      const GURL& url,
      std::map<std::string,std::string>* query_terms);

  // Invoked when the Google base URL has changed. Updates the mapping for all
  // TemplateURLs that have a replacement term of {google:baseURL} or
  // {google:baseSuggestURL}.
  void GoogleBaseURLChanged();

  NotificationRegistrar registrar_;

  // Mapping from keyword to the TemplateURL.
  KeywordToTemplateMap keyword_to_template_map_;

  TemplateURLVector template_urls_;

  ObserverList<TemplateURLModelObserver> model_observers_;

  // Maps from host to set of TemplateURLs whose search url host is host.
  HostToURLsMap host_to_urls_map_;

  // Used to obtain the WebDataService.
  // When Load is invoked, if we haven't yet loaded, the WebDataService is
  // obtained from the Profile. This allows us to lazily access the database.
  Profile* profile_;

  // Whether the keywords have been loaded.
  bool loaded_;

  // If non-zero, we're waiting on a load.
  WebDataService::Handle load_handle_;

  // Service used to store entries.
  scoped_refptr<WebDataService> service_;

  // List of hosts to feed to DeleteGeneratedKeywordsMatchingHost. When
  // we receive NOTIFY_HOST_DELETED_FROM_HISTORY if we haven't loaded yet,
  // we force a load and add the host to hosts_to_delete_. When done loading
  // we invoke DeleteGeneratedKeywordsMatchingHost with all the elements of
  // the vector.
  std::vector<std::wstring> hosts_to_delete_;

  // All visits that occurred before we finished loading. Once loaded
  // UpdateKeywordSearchTermsForURL is invoked for each element of the vector.
  std::vector<history::URLVisitedDetails> visits_to_add_;

  const TemplateURL* default_search_provider_;

  // The default search provider from preferences. This is only valid if
  // GetDefaultSearchProvider is invoked and we haven't been loaded. Once loaded
  // this is not used.
  scoped_ptr<TemplateURL> prefs_default_search_provider_;

  // ID assigned to next TemplateURL added to this model. This is an ever
  // increasing integer that is initialized from the database.
  TemplateURL::IDType next_id_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLModel);
};

#endif  // CHROME_BROWSER_TEMPLATE_URL_MODEL_H__
