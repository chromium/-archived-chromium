// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_VISIT_DATABASE_H__
#define CHROME_BROWSER_HISTORY_VISIT_DATABASE_H__

#include "chrome/browser/history/history_types.h"

struct sqlite3;
class SqliteStatementCache;
class SQLStatement;

namespace history {

// A visit database is one which stores visits for URLs, that is, times and
// linking information. A visit database must also be a URLDatabase, as this
// modifies tables used by URLs directly and could be thought of as inheriting
// from URLDatabase. However, this inheritance is not explicit as things would
// get too complicated and have multiple inheritance.
class VisitDatabase {
 public:
  // Must call InitVisitTable() before using to make sure the database is
  // initialized.
  VisitDatabase();
  virtual ~VisitDatabase();

  // Deletes the visit table. Used for rapidly clearing all visits. In this
  // case, InitVisitTable would be called immediately afterward to re-create it.
  // Returns true on success.
  bool DropVisitTable();

  // Adds a line to the visit database with the given information, returning
  // the added row ID on success, 0 on failure. The given visit is updated with
  // the new row ID on success.
  VisitID AddVisit(VisitRow* visit);

  // Deletes the given visit from the database. If a visit with the given ID
  // doesn't exist, it will not do anything.
  void DeleteVisit(const VisitRow& visit);

  // Query a VisitInfo giving an visit id, filling the given VisitRow.
  // Returns true on success.
  bool GetRowForVisit(VisitID visit_id, VisitRow* out_visit);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The visit must exist. Returns true on success.
  bool UpdateVisitRow(const VisitRow& visit);

  // Fills in the given vector with all of the visits for the given page ID,
  // sorted in ascending order of date. Returns true on success (although there
  // may still be no matches).
  bool GetVisitsForURL(URLID url_id, VisitVector* visits);

  // Fills all visits in the time range [begin, end) to the given vector. Either
  // time can be is_null(), in which case the times in that direction are
  // unbounded.
  //
  // If |max_results| is non-zero, up to that many results will be returned. If
  // there are more results than that, the oldest ones will be returned. (This
  // is used for history expiration.)
  //
  // The results will be in increasing order of date.
  void GetAllVisitsInRange(base::Time begin_time, base::Time end_time,
                           int max_results, VisitVector* visits);

  // Fills all visits in the given time range into the given vector that should
  // be user-visible, which excludes things like redirects and subframes. The
  // begin time is inclusive, the end time is exclusive. Either time can be
  // is_null(), in which case the times in that direction are unbounded.
  //
  // Up to |max_count| visits will be returned. If there are more visits than
  // that, the most recent |max_count| will be returned. If 0, all visits in the
  // range will be computed.
  //
  // When |most_recent_visit_only| is set, only one visit for each URL will be
  // returned, and it will be the most recent one in the time range.
  void GetVisibleVisitsInRange(base::Time begin_time, base::Time end_time,
                               bool most_recent_visit_only,
                               int max_count,
                               VisitVector* visits);

  // Returns the visit ID for the most recent visit of the given URL ID, or 0
  // if there is no visit for the URL.
  //
  // If non-NULL, the given visit row will be filled with the information of
  // the found visit. When no visit is found, the row will be unchanged.
  VisitID GetMostRecentVisitForURL(URLID url_id,
                                   VisitRow* visit_row);

  // Returns the |max_results| most recent visit sessions for |url_id|.
  //
  // Returns false if there's a failure preparing the statement. True
  // otherwise. (No results are indicated with an empty |visits|
  // vector.)
  bool GetMostRecentVisitsForURL(URLID url_id,
                                 int max_results,
                                 VisitVector* visits);

  // Finds a redirect coming from the given |from_visit|. If a redirect is
  // found, it fills the visit ID and URL into the out variables and returns
  // true. If there is no redirect from the given visit, returns false.
  //
  // If there is more than one redirect, this will compute a random one. But
  // duplicates should be very rare, and we don't actually care which one we
  // get in most cases. These will occur when the user goes back and gets
  // redirected again.
  //
  // to_visit and to_url can be NULL in which case they are ignored.
  bool GetRedirectFromVisit(VisitID from_visit,
                            VisitID* to_visit,
                            GURL* to_url);

  // Returns the number of visits to all urls on the scheme/host/post
  // identified by url. This is only valid for http and https urls (all other
  // schemes are ignored and false is returned).
  // count is set to the number of visits, first_visit is set to the first time
  // the host was visited. Returns true on success.
  bool GetVisitCountToHost(const GURL& url, int* count,
                           base::Time* first_visit);

  // Get the time of the first item in our database.
  bool GetStartDate(base::Time* first_visit);

 protected:
  // Returns the database and statement cache for the functions in this
  // interface. The decendent of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

  // Called by the derived classes on initialization to make sure the tables
  // and indices are properly set up. Must be called before anything else.
  bool InitVisitTable();

  // Convenience to fill a VisitRow. Assumes the visit values are bound starting
  // at index 0.
  static void FillVisitRow(SQLStatement& statement, VisitRow* visit);

  // Convenience to fill a VisitVector. Assumes that statement.step()
  // hasn't happened yet.
  static void FillVisitVector(SQLStatement& statement, VisitVector* visits);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(VisitDatabase);
};

}  // history

#endif  // CHROME_BROWSER_HISTORY_VISIT_DATABASE_H__
