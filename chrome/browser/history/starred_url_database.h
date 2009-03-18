// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H_
#define CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/views/controls/tree/tree_node_model.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

struct sqlite3;
class SqliteStatementCache;

namespace history {

// Bookmarks were originally part of the url database, they have since been
// moved to a separate file. This file exists purely for historical reasons and
// contains just enough to allow migration.
class StarredURLDatabase : public URLDatabase {
 public:
  // Must call InitStarTable() AND any additional init functions provided by
  // URLDatabase before using this class' functions.
  StarredURLDatabase();
  virtual ~StarredURLDatabase();

 protected:
  // The unit tests poke our innards.
  friend class HistoryTest;
  friend class StarredURLDatabaseTest;
  FRIEND_TEST(HistoryTest, CreateStarGroup);

  // Writes bookmarks to the specified file.
  bool MigrateBookmarksToFile(const FilePath& path);

  // Returns the database and statement cache for the functions in this
  // interface. The decendent of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

 private:
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

  // Gets all the starred entries.
  bool GetAllStarredEntries(std::vector<StarredEntry>* entries);

  // Sets the title, parent_id, parent_group_id, visual_order and date_modifed
  // of the specified star entry.
  //
  // WARNING: Does not update the visual order.
  bool UpdateStarredEntryRow(StarID star_id,
                             const std::wstring& title,
                             UIStarID parent_group_id,
                             int visual_order,
                             base::Time date_modified);

  // Adjusts the visual order of all children of parent_group_id with a
  // visual_order >= start_visual_order by delta. For example,
  // AdjustStarredVisualOrder(10, 0, 1) increments the visual order all children
  // of group 10 with a visual order >= 0 by 1.
  bool AdjustStarredVisualOrder(UIStarID parent_group_id,
                                int start_visual_order,
                                int delta);

  // Creates a starred entry with the specified parameters in the database.
  // Returns the newly created id, or 0 on failure.
  //
  // WARNING: Does not update the visual order.
  StarID CreateStarredEntryRow(URLID url_id,
                               UIStarID group_id,
                               UIStarID parent_group_id,
                               const std::wstring& title,
                               const base::Time& date_added,
                               int visual_order,
                               StarredEntry::Type type);

  // Deletes the entry from the starred database base on the starred id (NOT
  // the url id).
  //
  // WARNING: Does not update the visual order.
  bool DeleteStarredEntryRow(StarID star_id);

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

  // Used when checking integrity of starred table.
  typedef views::TreeNodeWithValue<history::StarredEntry> StarredNode;

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

  // Does the work of migrating bookmarks to a temporary file that
  // BookmarkStorage will read from.
  bool MigrateBookmarksToFileImpl(const FilePath& path);

  DISALLOW_COPY_AND_ASSIGN(StarredURLDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_STARRED_URL_DATABASE_H_
