// Copyright (c) 008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_publisher.h"

#if defined(OS_WIN)
#include <atlsafe.h>
#include <objbase.h>
#include <oleauto.h>
#include <wtypes.h>
#endif

#include "base/registry.h"

namespace history {

const wchar_t* HistoryPublisher::kRegKeyRegisteredIndexersInfo =
    L"Software\\Google\\Google Chrome\\IndexerPlugins";
const char* HistoryPublisher::kThumbnailImageFormat = "image/jpeg";

// static
double HistoryPublisher::TimeToUTCVariantTime(const base::Time& time) {
  double var_time = 0;
#if defined(OS_WIN)
  if (!time.is_null()) {
    base::Time::Exploded exploded;
    time.UTCExplode(&exploded);

    // Create the system time struct representing our exploded time.
    SYSTEMTIME system_time;
    system_time.wYear = exploded.year;
    system_time.wMonth = exploded.month;
    system_time.wDayOfWeek = exploded.day_of_week;
    system_time.wDay = exploded.day_of_month;
    system_time.wHour = exploded.hour;
    system_time.wMinute = exploded.minute;
    system_time.wSecond = exploded.second;
    system_time.wMilliseconds = exploded.millisecond;
    SystemTimeToVariantTime(&system_time, &var_time);
  }
#endif

  return var_time;
}

HistoryPublisher::HistoryPublisher() {
#if defined(OS_WIN)
  CoInitialize(NULL);
#endif
}

HistoryPublisher::~HistoryPublisher() {
#if defined(OS_WIN)
  CoUninitialize();
#endif
}

bool HistoryPublisher::Init() {
  return ReadRegisteredIndexersFromRegistry();
}

bool HistoryPublisher::ReadRegisteredIndexersFromRegistry() {
#if defined(OS_WIN)
  RegistryKeyIterator iter(HKEY_CURRENT_USER, kRegKeyRegisteredIndexersInfo);
  while (iter.Valid()) {
    // The subkey name is the GUID of the Indexer COM object which implements
    // the IChromeHistoryIndexer interface. We shall store that and use it to
    // send historical data to the indexer.
    CLSID clsid;
    CLSIDFromString(static_cast<LPOLESTR>(
        const_cast<TCHAR*>(iter.Name())), &clsid);
    CComPtr<IChromeHistoryIndexer> indexer;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC,
                                  __uuidof(IChromeHistoryIndexer),
                                  reinterpret_cast<void**>(&indexer));
    if (SUCCEEDED(hr) && indexer != NULL)
      indexers_.push_back(indexer);
    ++iter;
  }
  return indexers_.size() > 0;
#else
  // The indexing plublisher is implemented only for Windows platform as of
  // now. Hence returning false for other platforms.
  return false;
#endif
}

void HistoryPublisher::PublishPageThumbnail(
    const std::vector<unsigned char>& thumbnail, const GURL& url,
    const base::Time& time) const {
  PageData page_data = {
    time,
    url,
    NULL,
    NULL,
    kThumbnailImageFormat,
    &thumbnail,
  };

  PublishDataToIndexers(page_data);
}

void HistoryPublisher::PublishPageContent(const base::Time& time,
                                          const GURL& url,
                                          const std::wstring& title,
                                          const std::wstring& contents) const {
  PageData page_data = {
    time,
    url,
    contents.c_str(),
    title.c_str(),
    NULL,
    NULL,
  };

  PublishDataToIndexers(page_data);
}

void HistoryPublisher::PublishDataToIndexers(const PageData& page_data)
    const {
#if defined(OS_WIN)
  double var_time = TimeToUTCVariantTime(page_data.time);

  CComSafeArray<unsigned char> thumbnail_arr;
  if (page_data.thumbnail) {
    for(size_t i = 0; i < page_data.thumbnail->size(); ++i)
      thumbnail_arr.Add((*page_data.thumbnail)[i]);
  }

  // Send data to registered indexers.
  for(size_t i = 0; i < indexers_.size(); ++i) {
    indexers_[i]->SendPageData(
        CComVariant(var_time, VT_DATE),
        CComBSTR(page_data.url.spec().c_str()),
        CComBSTR(page_data.html),
        CComBSTR(page_data.title),
        CComBSTR(page_data.thumbnail_format),
        CComVariant(thumbnail_arr.m_psa));
  }
#endif
}

void HistoryPublisher::DeleteUserHistoryBetween(const base::Time& begin_time,
                                                const base::Time& end_time) const {
#if defined(OS_WIN)
  double var_begin_time = TimeToUTCVariantTime(begin_time);
  double var_end_time = TimeToUTCVariantTime(end_time);
  for(size_t i = 0; i < indexers_.size(); ++i) {
    indexers_[i]->DeleteUserHistoryBetween(CComVariant(var_begin_time, VT_DATE),
                                           CComVariant(var_end_time, VT_DATE));
  }
#endif
}

}  // namespace history
