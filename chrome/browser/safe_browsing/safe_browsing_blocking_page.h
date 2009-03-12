// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingService on the UI
// thread when we've determined that a page is malicious. The operation of the
// blocking page occurs on the UI thread, where it waits for the user to make a
// decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingService so that we can cancel the request for the new page, or
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>
#include <vector>

#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class MessageLoop;
class NavigationController;
class WebContents;


class SafeBrowsingBlockingPage : public InterstitialPage {
 public:
  virtual ~SafeBrowsingBlockingPage();

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // You can call this method several times, if an interstitial is already
  // showing, the new one will be queued and displayed if the user decides
  // to proceed on the currently showing interstitial.
  static void ShowBlockingPage(
      SafeBrowsingService* service,
      const SafeBrowsingService::UnsafeResource& resource);

  // InterstitialPage method:
  virtual std::string GetHTMLContents();
  virtual void Proceed();
  virtual void DontProceed();

 protected:
  // InterstitialPage method:
  virtual void CommandReceived(const std::string& command);

 private:
  typedef std::vector<SafeBrowsingService::UnsafeResource> UnsafeResourceList;

  // Don't instanciate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPage(SafeBrowsingService* service,
                           WebContents* web_contents,
                           const UnsafeResourceList& unsafe_resources);

  // Fills the passed dictionary with the strings passed to JS Template when
  // creating the HTML.
  void PopulateMultipleThreatStringDictionary(DictionaryValue* strings);
  void PopulateMalwareStringDictionary(DictionaryValue* strings);
  void PopulatePhishingStringDictionary(DictionaryValue* strings);

  // A helper method used by the Populate methods above used to populate common
  // fields.
  void PopulateStringDictionary(DictionaryValue* strings,
                                const std::wstring& title,
                                const std::wstring& headline,
                                const std::wstring& description1,
                                const std::wstring& description2,
                                const std::wstring& description3);


  // A list of SafeBrowsingService::UnsafeResource for a tab that the user
  // should be warned about.  They are queued when displaying more than one
  // interstitial at a time.
  typedef std::map<WebContents*, UnsafeResourceList> UnsafeResourceMap;
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  // Notifies the SafeBrowsingService on the IO thread whether to proceed or not
  // for the |resources|.
  static void NotifySafeBrowsingService(SafeBrowsingService* sb_service,
                                        const UnsafeResourceList& resources,
                                        bool proceed);

  // Returns true if the passed |unsafe_resources| is for the main page.
  static bool IsMainPage(const UnsafeResourceList& unsafe_resources);

 private:
  // For reporting back user actions.
  SafeBrowsingService* sb_service_;
  MessageLoop* report_loop_;

  // Whether the flagged resource is the main page (or a sub-resource is false).
  bool is_main_frame_;

  // The index of a navigation entry that should be removed when DontProceed()
  // is invoked, -1 if not entry should be removed.
  int navigation_entry_index_to_remove_;

  // The list of unsafe resources this page is warning about.
  UnsafeResourceList unsafe_resources_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
