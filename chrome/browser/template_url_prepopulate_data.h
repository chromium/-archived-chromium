// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TEMPLATE_URL_PREPOPULATE_DATA_H__
#define CHROME_BROWSER_TEMPLATE_URL_PREPOPULATE_DATA_H__

#include <vector>

class PrefService;
class TemplateURL;

namespace TemplateURLPrepopulateData {

void RegisterUserPrefs(PrefService* prefs);

// Returns the current version of the prepopulate data, so callers can know when
// they need to re-merge.
int GetDataVersion();

// Loads the set of TemplateURLs from the prepopulate data.  Ownership of the
// TemplateURLs is passed to the caller.  On return,
// |default_search_provider_index| is set to the index of the default search
// provider.
void GetPrepopulatedEngines(PrefService* prefs,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index);

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_TEMPLATE_URL_PREPOPULATE_DATA_H__

