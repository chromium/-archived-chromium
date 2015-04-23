// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_VISITSEGMENT_DATABASE_H__
#define CHROME_BROWSER_HISTORY_VISITSEGMENT_DATABASE_H__

#include "base/basictypes.h"
#include "chrome/browser/history/history_types.h"

class PageUsageData;
struct sqlite3;
class SqliteStatementCache;

namespace history {

// Tracks pages used for the most visited view.
class VisitSegmentDatabase {
 public:
  // Must call InitSegmentTables before using any other part of this class.
  VisitSegmentDatabase();
  virtual ~VisitSegmentDatabase();

  // Compute a segment name given a URL. The segment name is currently the
  // source url spec less some information such as query strings.
  static std::string ComputeSegmentName(const GURL& url);

  // Returns the ID of the segment with the corresponding name, or 0 if there
  // is no segment with that name.
  SegmentID GetSegmentNamed(const std::string& segment_name);

  // Update the segment identified by |out_segment_id| with the provided URL ID.
  // The URL identifies the page that will now represent the segment. If url_id
  // is non zero, it is assumed to be the row id of |url|.
  bool UpdateSegmentRepresentationURL(SegmentID segment_id,
                                      URLID url_id);

  // Return the ID of the URL currently used to represent this segment or 0 if
  // an error occured.
  URLID GetSegmentRepresentationURL(SegmentID segment_id);

  // Create a segment for the provided URL ID with the given name. Returns the
  // ID of the newly created segment, or 0 on failure.
  SegmentID CreateSegment(URLID url_id, const std::string& segment_name);

  // Increase the segment visit count by the provided amount. Return true on
  // success.
  bool IncreaseSegmentVisitCount(SegmentID segment_id, const base::Time& ts,
                                 int amount);

  // Compute the segment usage since |from_time| using the provided aggregator.
  // A PageUsageData is added in |result| for the highest-scored segments up to
  // |max_result_count|.
  void QuerySegmentUsage(const base::Time& from_time,
                         int max_result_count,
                         std::vector<PageUsageData*>* result);

  // Delete all the segment usage data which is older than the provided time
  // stamp.
  void DeleteSegmentData(const base::Time& older_than);

  // Change the presentation id for the segment identified by |segment_id|
  void SetSegmentPresentationIndex(SegmentID segment_id, int index);

  // Delete the segment currently using the provided url for representation.
  // This will also delete any associated segment usage data.
  bool DeleteSegmentForURL(URLID url_id);

 protected:
  // Returns the database and statement cache for the functions in this
  // interface. The decendent of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitSegmentTables();

  // Deletes all the segment tables, returning true on success.
  bool DropSegmentTables();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(VisitSegmentDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_VISITSEGMENT_DATABASE_H__
