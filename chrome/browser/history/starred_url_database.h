// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H__
#define CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H__

#include <map>
#include <set>

#include "base/basictypes.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/views/tree_node_model.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

struct sqlite3;
class SqliteStatementCache;

namespace history {

// Encapsulates a URL database plus starred information.
//
// WARNING: many of the following methods allow you to update, delete or
// insert starred entries specifying a visual order. These methods do NOT
// adjust the visual order of surrounding entries. You must explicitly do
// it yourself using AdjustStarredVisualOrder as appropriate.
class StarredURLDatabase : public URLDatabase {
 public:
  // Must call InitStarTable() AND any additional init functions provided by
  // URLDatabase before using this class' functions.
  StarredURLDatabase();
  virtual ~StarredURLDatabase();

  // Returns the id of the starred entry. This does NOT return entry.id,
  // rather the database is queried for the id based on entry.url or
  // entry.group_id.
  StarID GetStarIDForEntry(const StarredEntry& entry);

  // Returns the internal star ID for the externally-generated group ID.
  StarID GetStarIDForGroupID(UIStarID group_id);

  // Gets the details for the specified star entry in entry.
  bool GetStarredEntry(StarID star_id, StarredEntry* entry);

  // Creates a starred entry with the requested information. The structure will
  // be updated with the ID of the newly created entry. The URL table will be
  // updated to point to the entry. The URL row will be created if it doesn't
  // exist.
  //
  // We currently only support one entry per URL. This URL should not already be
  // starred when calling this function or it will fail and will return 0.
  StarID CreateStarredEntry(StarredEntry* entry);

  // Returns starred entries.
  // This returns three different result types:
  //   only_on_bookmark_bar == true: only those entries on the bookmark bar are
  //                                 returned.
  //   only_on_bookmark_bar == false and parent_id == 0: all starred
  //                                                     entries/groups.
  //   otherwise only the direct children (groups and entries) of parent_id are
  //   returned.
  bool GetStarredEntries(UIStarID parent_group_id,
                         std::vector<StarredEntry>* entries);

  // Deletes a starred entry. This adjusts the visual order of all siblings
  // after the entry. The deleted entry(s) will be *appended* to
  // |*deleted_entries| (so delete can be called more than once with the same
  // list) and deleted URLs will additionally be added to the set.
  //
  // This function invokes DeleteStarredEntryImpl to do the actual work, which
  // recurses.
  void DeleteStarredEntry(StarID star_id,
                          std::set<GURL>* unstarred_urls,
                          std::vector<StarredEntry>* deleted_entries);

  // Implementation to update the database for UpdateStarredEntry. Returns true
  // on success. If successful, the parent_id, url_id, id and date_added fields
  // of the entry are reset from that of the database.
  bool UpdateStarredEntry(StarredEntry* entry);

  // Gets up to max_count starred entries of type URL adding them to entries.
  // The results are ordered by date added in descending order (most recent
  // first).
  void GetMostRecentStarredEntries(int max_count,
                                   std::vector<StarredEntry>* entries);

  // Gets the URLIDs for all starred entries of type URL whose title matches
  // the specified query.
  void GetURLsForTitlesMatching(const std::wstring& query,
                                std::set<URLID>* ids);

  // Returns true if the starred table is in a sane state. If false, the starred
  // table is likely corrupt and shouldn't be used.
  bool is_starred_valid() const { return is_starred_valid_; }

  // Updates the URL ID for the given starred entry. This is used by the expirer
  // when URL IDs change. It can't use UpdateStarredEntry since that doesn't
  // set the URLID, and it does a bunch of other logic that we don't need.
  bool UpdateURLIDForStar(StarID star_id, URLID new_url_id);

 protected:
  // The unit tests poke our innards.
  friend class HistoryTest;
  FRIEND_TEST(HistoryTest, CreateStarGroup);

  // Returns the database and statement cache for the functions in this
  // interface. The decendent of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

  // Creates the starred tables if necessary.
  bool InitStarTable();

  // Makes sure the starred table is in a sane state. This does the following:
  // . Makes sure there is a bookmark bar and other nodes. If no bookmark bar
  //   node is found, the table is dropped and recreated.
  // . Removes any bookmarks with no URL. This can happen if a URL is removed
  //   from the urls table without updating the starred table correctly.
  // . Makes sure the visual order of all nodes is correct.
  // . Moves all bookmarks and folders that are not descendants of the bookmark
  //   bar or other folders to the bookmark bar.
  // . Makes sure there isn't a cycle in the folders. A cycle means some folder
  //   has as its parent one of its children.
  //
  // This returns false if the starred table is in a bad state and couldn't
  // be fixed, true otherwise.
  //
  // This should be invoked after migration.
  bool EnsureStarredIntegrity();

  // Creates a starred entry with the specified parameters in the database.
  // Returns the newly created id, or 0 on failure.
  //
  // WARNING: Does not update the visual order.
  StarID CreateStarredEntryRow(URLID url_id,
                               UIStarID group_id,
                               UIStarID parent_group_id,
                               const std::wstring& title,
                               const Time& date_added,
                               int visual_order,
                               StarredEntry::Type type);

  // Sets the title, parent_id, parent_group_id, visual_order and date_modifed
  // of the specified star entry.
  //
  // WARNING: Does not update the visual order.
  bool UpdateStarredEntryRow(StarID star_id,
                             const std::wstring& title,
                             UIStarID parent_group_id,
                             int visual_order,
                             Time date_modified);

  // Adjusts the visual order of all children of parent_group_id with a
  // visual_order >= start_visual_order by delta. For example,
  // AdjustStarredVisualOrder(10, 0, 1) increments the visual order all children
  // of group 10 with a visual order >= 0 by 1.
  bool AdjustStarredVisualOrder(UIStarID parent_group_id,
                                int start_visual_order,
                                int delta);

  // Deletes the entry from the starred database base on the starred id (NOT
  // the url id).
  //
  // WARNING: Does not update the visual order.
  bool DeleteStarredEntryRow(StarID star_id);

  // Should the integrity of the starred table be checked on every mutation?
  // Default is true. Set to false when running tests that need to allow the
  // table to be in an invalid state while testing.
  bool check_starred_integrity_on_mutation_;

 private:
  // Used when checking integrity of starred table.
  typedef ChromeViews::TreeNodeWithValue<history::StarredEntry> StarredNode;

  // Implementation of DeleteStarredEntryImpl.
  void DeleteStarredEntryImpl(StarID star_id,
                              std::set<GURL>* unstarred_urls,
                              std::vector<StarredEntry>* deleted_entries);

  // Returns the max group id, or 0 if there is an error.
  UIStarID GetMaxGroupID();

  // Gets all the bookmarks and folders creating a StarredNode for each
  // bookmark and folder. On success all the root nodes (bookmark bar node,
  // other folder node, folders with no parent or folders with a parent that
  // would make a cycle) are added to roots.
  //
  // If a group_id occurs more than once, all but the first ones id is added to
  // groups_with_duplicate_ids.
  //
  // All bookmarks not on the bookmark bar/other folder are added to
  // unparented_urls.
  //
  // It's up to the caller to delete the nodes returned in roots and
  // unparented_urls.
  //
  // This is used during integrity enforcing/checking of the starred table.
  bool BuildStarNodes(
      std::set<StarredNode*>* roots,
      std::set<StarID>* groups_with_duplicate_ids,
      std::set<StarredNode*>* unparented_urls,
      std::set<StarID>* empty_url_ids);

  // Sets the visual order of all of node's children match the order in |node|.
  // If the order differs, the database is updated. Returns false if the order
  // differed and the db couldn't be updated.
  bool EnsureVisualOrder(StarredNode* node);

  // Returns the first node in nodes with the specified type, or null if there
  // is not a node with the specified type.
  StarredNode* GetNodeByType(
      const std::set<StarredNode*>& nodes,
      StarredEntry::Type type);

  // Implementation for setting starred integrity. See description of
  // EnsureStarredIntegrity for the details of what this does.
  //
  // All entries in roots that are not the bookmark bar and other node are
  // moved to be children of the bookmark bar node. Similarly all nodes
  // in unparented_urls are moved to be children of the bookmark bar.
  //
  // Returns true on success, false if the starred table is in a bad state and
  // couldn't be repaired.
  bool EnsureStarredIntegrityImpl(
      std::set<StarredNode*>* roots,
      const std::set<StarID>& groups_with_duplicate_ids,
      std::set<StarredNode*>* unparented_urls,
      const std::set<StarID>& empty_url_ids);

  // Resets the visual order and parent_group_id of source's StarredEntry
  // and adds it to the end of new_parent's children.
  //
  // This is used if the starred table is an unexpected state and an entry
  // needs to be moved.
  bool Move(StarredNode* source, StarredNode* new_parent);

  // Drops and recreates the starred table as well as marking all URLs as
  // unstarred. This is used as a last resort if the bookmarks table is
  // totally corrupt.
  bool RecreateStarredEntries();

  // Iterates through the children of node make sure the visual order matches
  // the order in node's children. DCHECKs if the order differs. This recurses.
  void CheckVisualOrder(StarredNode* node);

  // Makes sure the starred table is in a sane state, if not this DCHECKs.
  // This is invoked internally after any mutation to the starred table.
  //
  // This is a no-op in release builds.
  void CheckStarredIntegrity();

  // True if the starred table is valid. This is initialized in
  // EnsureStarredIntegrityImpl.
  bool is_starred_valid_;

  DISALLOW_EVIL_CONSTRUCTORS(StarredURLDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H__
