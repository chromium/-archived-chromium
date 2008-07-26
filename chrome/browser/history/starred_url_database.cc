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

#include "chrome/browser/history/starred_url_database.h"

#include "base/logging.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/common/stl_util-inl.h"

// The following table is used to store star (aka bookmark) information. This
// class derives from URLDatabase, which has its own schema.
//
// starred
//   id                 Unique identifier (primary key) for the entry.
//   type               Type of entry, if 0 this corresponds to a URL, 1 for
//                      a system grouping, 2 for a user created group, 3 for
//                      other.
//   url_id             ID of the url, only valid if type == 0
//   group_id           ID of the group, only valid if type != 0. This id comes
//                      from the UI and is NOT the same as id.
//   title              User assigned title.
//   date_added         Creation date.
//   visual_order       Visual order within parent.
//   parent_id          Group ID of the parent this entry is contained in, if 0
//                      entry is not in a group.
//   date_modified      Time the group was last modified. See comments in
//                      StarredEntry::date_group_modified
// NOTE: group_id and parent_id come from the UI, id is assigned by the
// db.

namespace history {

namespace {

// Fields used by FillInStarredEntry.
#define STAR_FIELDS \
    " starred.id, starred.type, starred.title, starred.date_added, " \
    "starred.visual_order, starred.parent_id, urls.url, urls.id, " \
    "starred.group_id, starred.date_modified "
const char kHistoryStarFields[] = STAR_FIELDS;

void FillInStarredEntry(SQLStatement* s, StarredEntry* entry) {
  DCHECK(entry);
  entry->id = s->column_int64(0);
  switch (s->column_int(1)) {
    case 0:
      entry->type = history::StarredEntry::URL;
      entry->url = GURL(s->column_string16(6));
      break;
    case 1:
      entry->type = history::StarredEntry::BOOKMARK_BAR;
      break;
    case 2:
      entry->type = history::StarredEntry::USER_GROUP;
      break;
    case 3:
      entry->type = history::StarredEntry::OTHER;
      break;
    default:
      NOTREACHED();
      break;
  }
  entry->title = s->column_string16(2);
  entry->date_added = Time::FromInternalValue(s->column_int64(3));
  entry->visual_order = s->column_int(4);
  entry->parent_group_id = s->column_int64(5);
  entry->url_id = s->column_int64(7);
  entry->group_id = s->column_int64(8);
  entry->date_group_modified = Time::FromInternalValue(s->column_int64(9));
}

}  // namespace

StarredURLDatabase::StarredURLDatabase()
    : is_starred_valid_(true), check_starred_integrity_on_mutation_(true) {
}

StarredURLDatabase::~StarredURLDatabase() {
}

bool StarredURLDatabase::InitStarTable() {
  if (!DoesSqliteTableExist(GetDB(), "starred")) {
    if (sqlite3_exec(GetDB(), "CREATE TABLE starred ("
                     "id INTEGER PRIMARY KEY,"
                     "type INTEGER NOT NULL DEFAULT 0,"
                     "url_id INTEGER NOT NULL DEFAULT 0,"
                     "group_id INTEGER NOT NULL DEFAULT 0,"
                     "title VARCHAR,"
                     "date_added INTEGER NOT NULL,"
                     "visual_order INTEGER DEFAULT 0,"
                     "parent_id INTEGER DEFAULT 0,"
                     "date_modified INTEGER DEFAULT 0 NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(GetDB(), "CREATE INDEX starred_index "
                           "ON starred(id,url_id)", NULL, NULL, NULL)) {
      NOTREACHED();
      return false;
    }
    // Add an entry that represents the bookmark bar. The title is ignored in
    // the UI.
    StarID bookmark_id = CreateStarredEntryRow(
        0, HistoryService::kBookmarkBarID, 0, L"bookmark-bar", Time::Now(), 0,
        history::StarredEntry::BOOKMARK_BAR);
    if (bookmark_id != HistoryService::kBookmarkBarID) {
      NOTREACHED();
      return false;
    }

    // Add an entry that represents other. The title is ignored in the UI.
    StarID other_id = CreateStarredEntryRow(
        0, HistoryService::kBookmarkBarID + 1, 0, L"other", Time::Now(), 0,
        history::StarredEntry::OTHER);
    if (!other_id) {
      NOTREACHED();
      return false;
    }
  }

  sqlite3_exec(GetDB(), "CREATE INDEX starred_group_index"
               "ON starred(group_id)", NULL, NULL, NULL);
  return true;
}

bool StarredURLDatabase::EnsureStarredIntegrity() {
  if (!is_starred_valid_)
    return false;

  // Assume invalid, we'll set to true if succesful.
  is_starred_valid_ = false;

  std::set<StarredNode*> roots;
  std::set<StarID> groups_with_duplicate_ids;
  std::set<StarredNode*> unparented_urls;
  std::set<StarID> empty_url_ids;

  if (!BuildStarNodes(&roots, &groups_with_duplicate_ids, &unparented_urls,
                      &empty_url_ids)) {
    return false;
  }

  // The table may temporarily get into an invalid state while updating the
  // integrity.
  bool org_check_starred_integrity_on_mutation =
      check_starred_integrity_on_mutation_;
  check_starred_integrity_on_mutation_ = false;
  is_starred_valid_ =
      EnsureStarredIntegrityImpl(&roots, groups_with_duplicate_ids,
                                 &unparented_urls, empty_url_ids);
  check_starred_integrity_on_mutation_ =
      org_check_starred_integrity_on_mutation;

  STLDeleteElements(&roots);
  STLDeleteElements(&unparented_urls);
  return is_starred_valid_;
}

StarID StarredURLDatabase::GetStarIDForEntry(const StarredEntry& entry) {
  if (entry.type == StarredEntry::URL) {
    URLRow url_row;
    URLID url_id = GetRowForURL(entry.url, &url_row);
    if (!url_id)
      return 0;
    return url_row.star_id();
  }
  return GetStarIDForGroupID(entry.group_id);
}

StarID StarredURLDatabase::GetStarIDForGroupID(UIStarID group_id) {
  DCHECK(group_id);

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT id FROM starred WHERE group_id = ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, group_id);
  if (statement->step() == SQLITE_ROW)
    return statement->column_int64(0);
  return 0;
}

void StarredURLDatabase::DeleteStarredEntry(
    StarID star_id,
    std::set<GURL>* unstarred_urls,
    std::vector<StarredEntry>* deleted_entries) {
  DeleteStarredEntryImpl(star_id, unstarred_urls, deleted_entries);
  if (check_starred_integrity_on_mutation_)
    CheckStarredIntegrity();
}

bool StarredURLDatabase::UpdateStarredEntry(StarredEntry* entry) {
  // Determine the ID used by the database.
  const StarID id = GetStarIDForEntry(*entry);
  if (!id) {
    NOTREACHED() << "request to update unknown star entry";
    return false;
  }

  StarredEntry original_entry;
  if (!GetStarredEntry(id, &original_entry)) {
    NOTREACHED() << "Unknown star entry";
    return false;
  }

  if (entry->parent_group_id != original_entry.parent_group_id) {
    // Parent has changed.
    if (original_entry.parent_group_id) {
      AdjustStarredVisualOrder(original_entry.parent_group_id,
                               original_entry.visual_order, -1);
    }
    if (entry->parent_group_id) {
      AdjustStarredVisualOrder(entry->parent_group_id, entry->visual_order, 1);
    }
  } else if (entry->visual_order != original_entry.visual_order &&
             entry->parent_group_id) {
    // Same parent, but visual order changed.
    // Shift everything down from the old location.
    AdjustStarredVisualOrder(original_entry.parent_group_id,
                             original_entry.visual_order, -1);
    // Make room for new location by shifting everything up from the new
    // location.
    AdjustStarredVisualOrder(original_entry.parent_group_id,
                             entry->visual_order, 1);
  }

  // And update title, parent and visual order.
  UpdateStarredEntryRow(id, entry->title, entry->parent_group_id,
                        entry->visual_order, entry->date_group_modified);
  entry->url_id = original_entry.url_id;
  entry->date_added = original_entry.date_added;
  entry->id = original_entry.id;
  if (check_starred_integrity_on_mutation_)
    CheckStarredIntegrity();
  return true;
}

bool StarredURLDatabase::GetStarredEntries(
    UIStarID parent_group_id,
    std::vector<StarredEntry>* entries) {
  DCHECK(entries);
  std::string sql = "SELECT ";
  sql.append(kHistoryStarFields);
  sql.append("FROM starred LEFT JOIN urls ON starred.url_id = urls.id ");
  if (parent_group_id)
    sql += "WHERE starred.parent_id = ? ";
  sql += "ORDER BY parent_id, visual_order";

  SQLStatement s;
  if (s.prepare(GetDB(), sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  if (parent_group_id)
    s.bind_int64(0, parent_group_id);

  history::StarredEntry entry;
  while (s.step() == SQLITE_ROW) {
    FillInStarredEntry(&s, &entry);
    // Reset the url for non-url types. This is needed as we're reusing the
    // same entry for the loop.
    if (entry.type != history::StarredEntry::URL)
      entry.url = GURL();
    entries->push_back(entry);
  }
  return true;
}

bool StarredURLDatabase::UpdateStarredEntryRow(StarID star_id,
                                               const std::wstring& title,
                                               UIStarID parent_group_id,
                                               int visual_order,
                                               Time date_modified) {
  DCHECK(star_id && visual_order >= 0);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE starred SET title=?, parent_id=?, visual_order=?, "
      "date_modified=? WHERE id=?");
  if (!statement.is_valid())
    return 0;

  statement->bind_wstring(0, title);
  statement->bind_int64(1, parent_group_id);
  statement->bind_int(2, visual_order);
  statement->bind_int64(3, date_modified.ToInternalValue());
  statement->bind_int64(4, star_id);
  return statement->step() == SQLITE_DONE;
}

bool StarredURLDatabase::AdjustStarredVisualOrder(UIStarID parent_group_id,
                                                  int start_visual_order,
                                                  int delta) {
  DCHECK(parent_group_id && start_visual_order >= 0);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE starred SET visual_order=visual_order+? "
      "WHERE parent_id=? AND visual_order >= ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int(0, delta);
  statement->bind_int64(1, parent_group_id);
  statement->bind_int(2, start_visual_order);
  return statement->step() == SQLITE_DONE;
}

StarID StarredURLDatabase::CreateStarredEntryRow(URLID url_id,
                                                 UIStarID group_id,
                                                 UIStarID parent_group_id,
                                                 const std::wstring& title,
                                                 const Time& date_added,
                                                 int visual_order,
                                                 StarredEntry::Type type) {
  DCHECK(visual_order >= 0 &&
         (type != history::StarredEntry::URL || url_id));
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "INSERT INTO starred "
      "(type, url_id, group_id, title, date_added, visual_order, parent_id, "
      "date_modified) VALUES (?,?,?,?,?,?,?,?)");
  if (!statement.is_valid())
    return 0;

  switch (type) {
    case history::StarredEntry::URL:
      statement->bind_int(0, 0);
      break;
    case history::StarredEntry::BOOKMARK_BAR:
      statement->bind_int(0, 1);
      break;
    case history::StarredEntry::USER_GROUP:
      statement->bind_int(0, 2);
      break;
    case history::StarredEntry::OTHER:
      statement->bind_int(0, 3);
      break;
    default:
      NOTREACHED();
  }
  statement->bind_int64(1, url_id);
  statement->bind_int64(2, group_id);
  statement->bind_wstring(3, title);
  statement->bind_int64(4, date_added.ToInternalValue());
  statement->bind_int(5, visual_order);
  statement->bind_int64(6, parent_group_id);
  statement->bind_int64(7, Time().ToInternalValue());
  if (statement->step() == SQLITE_DONE)
    return sqlite3_last_insert_rowid(GetDB());
  return 0;
}

bool StarredURLDatabase::DeleteStarredEntryRow(StarID star_id) {
  if (check_starred_integrity_on_mutation_)
    DCHECK(star_id && star_id != HistoryService::kBookmarkBarID);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
                          "DELETE FROM starred WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, star_id);
  return statement->step() == SQLITE_DONE;
}

bool StarredURLDatabase::GetStarredEntry(StarID star_id, StarredEntry* entry) {
  DCHECK(entry && star_id);
  SQLITE_UNIQUE_STATEMENT(s, GetStatementCache(),
      "SELECT" STAR_FIELDS "FROM starred LEFT JOIN urls ON "
      "starred.url_id = urls.id WHERE starred.id=?");
  if (!s.is_valid())
    return false;

  s->bind_int64(0, star_id);

  if (s->step() == SQLITE_ROW) {
    FillInStarredEntry(s.statement(), entry);
    return true;
  }
  return false;
}

StarID StarredURLDatabase::CreateStarredEntry(StarredEntry* entry) {
  entry->id = 0;  // Ensure 0 for failure case.

  // Adjust the visual order when we are inserting it somewhere.
  if (entry->parent_group_id)
    AdjustStarredVisualOrder(entry->parent_group_id, entry->visual_order, 1);

  // Insert the new entry.
  switch (entry->type) {
    case StarredEntry::USER_GROUP:
      entry->id = CreateStarredEntryRow(0, entry->group_id,
          entry->parent_group_id, entry->title, entry->date_added,
          entry->visual_order, entry->type);
      break;

    case StarredEntry::URL: {
      // Get the row for this URL.
      URLRow url_row;
      if (!GetRowForURL(entry->url, &url_row)) {
        // Create a new URL row for this entry.
        url_row = URLRow(entry->url);
        url_row.set_title(entry->title);
        url_row.set_hidden(false);
        entry->url_id = this->AddURL(url_row);
      } else {
        // Update the existing row for this URL.
        if (url_row.starred()) {
          DCHECK(false) << "We are starring an already-starred URL";
          return 0;
        }
        entry->url_id = url_row.id();  // The caller doesn't have to set this.
      }

      // Create the star entry referring to the URL row.
      entry->id = CreateStarredEntryRow(entry->url_id, entry->group_id,
          entry->parent_group_id, entry->title, entry->date_added,
          entry->visual_order, entry->type);

      // Update the URL row to refer to this new starred entry.
      url_row.set_star_id(entry->id);
      UpdateURLRow(entry->url_id, url_row);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
  if (check_starred_integrity_on_mutation_)
    CheckStarredIntegrity();
  return entry->id;
}

void StarredURLDatabase::GetMostRecentStarredEntries(
    int max_count,
    std::vector<StarredEntry>* entries) {
  DCHECK(max_count && entries);
  SQLITE_UNIQUE_STATEMENT(s, GetStatementCache(),
      "SELECT" STAR_FIELDS "FROM starred "
      "LEFT JOIN urls ON starred.url_id = urls.id "
      "WHERE starred.type=0 ORDER BY starred.date_added DESC, "
      "starred.id DESC " // This is for testing, when the dates are
                       // typically the same.
      "LIMIT ?");
  if (!s.is_valid())
    return;
  s->bind_int(0, max_count);

  while (s->step() == SQLITE_ROW) {
    history::StarredEntry entry;
    FillInStarredEntry(s.statement(), &entry);
    entries->push_back(entry);
  }
}

void StarredURLDatabase::GetURLsForTitlesMatching(const std::wstring& query,
                                                  std::set<URLID>* ids) {
  QueryParser parser;
  ScopedVector<QueryNode> nodes;
  parser.ParseQuery(query, &nodes.get());
  if (nodes.empty())
    return;

  SQLITE_UNIQUE_STATEMENT(s, GetStatementCache(),
      "SELECT url_id, title FROM starred WHERE type=?");
  if (!s.is_valid())
    return;

  s->bind_int(0, static_cast<int>(StarredEntry::URL));
  while (s->step() == SQLITE_ROW) {
    if (parser.DoesQueryMatch(s->column_string16(1), nodes.get()))
      ids->insert(s->column_int64(0));
  }
}

bool StarredURLDatabase::UpdateURLIDForStar(StarID star_id, URLID new_url_id) {
  SQLITE_UNIQUE_STATEMENT(s, GetStatementCache(),
      "UPDATE starred SET url_id=? WHERE id=?");
  if (!s.is_valid())
    return false;
  s->bind_int64(0, new_url_id);
  s->bind_int64(1, star_id);
  return s->step() == SQLITE_DONE;
}

void StarredURLDatabase::DeleteStarredEntryImpl(
    StarID star_id,
    std::set<GURL>* unstarred_urls,
    std::vector<StarredEntry>* deleted_entries) {

  StarredEntry deleted_entry;
  if (!GetStarredEntry(star_id, &deleted_entry))
    return;  // Couldn't find information on the entry.

  if (deleted_entry.type == StarredEntry::BOOKMARK_BAR ||
      deleted_entry.type == StarredEntry::OTHER) {
    return;
  }

  if (deleted_entry.parent_group_id != 0) {
    // This entry has a parent. All siblings after this entry need to be shifted
    // down by 1.
    AdjustStarredVisualOrder(deleted_entry.parent_group_id,
                             deleted_entry.visual_order, -1);
  }

  deleted_entries->push_back(deleted_entry);

  switch (deleted_entry.type) {
    case StarredEntry::URL: {
      unstarred_urls->insert(deleted_entry.url);

      // Need to unmark the starred flag in the URL database.
      URLRow url_row;
      if (GetURLRow(deleted_entry.url_id, &url_row)) {
        url_row.set_star_id(0);
        UpdateURLRow(deleted_entry.url_id, url_row);
      }
      break;
    }

    case StarredEntry::USER_GROUP: {
      // Need to delete all the children of the group. Use the db version which
      // avoids shifting the visual order.
      std::vector<StarredEntry> descendants;
      GetStarredEntries(deleted_entry.group_id, &descendants);
      for (std::vector<StarredEntry>::iterator i =
           descendants.begin(); i != descendants.end(); ++i) {
        // Recurse down into the child.
        DeleteStarredEntryImpl(i->id, unstarred_urls, deleted_entries);
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }
  DeleteStarredEntryRow(star_id);
}

UIStarID StarredURLDatabase::GetMaxGroupID() {
  SQLStatement max_group_id_statement;
  if (max_group_id_statement.prepare(GetDB(),
      "SELECT MAX(group_id) FROM starred") != SQLITE_OK) {
    NOTREACHED();
    return 0;
  }
  if (max_group_id_statement.step() != SQLITE_ROW) {
    NOTREACHED();
    return 0;
  }
  return max_group_id_statement.column_int64(0);
}

bool StarredURLDatabase::BuildStarNodes(
    std::set<StarredURLDatabase::StarredNode*>* roots,
    std::set<StarID>* groups_with_duplicate_ids,
    std::set<StarredNode*>* unparented_urls,
    std::set<StarID>* empty_url_ids) {
  std::vector<StarredEntry> star_entries;
  if (!GetStarredEntries(0, &star_entries)) {
    NOTREACHED() << "Unable to get bookmarks from database";
    return false;
  }

  // Create the group/bookmark-bar/other nodes.
  std::map<UIStarID, StarredNode*> group_id_to_node_map;
  for (size_t i = 0; i < star_entries.size(); ++i) {
    if (star_entries[i].type != StarredEntry::URL) {
      if (group_id_to_node_map.find(star_entries[i].group_id) !=
          group_id_to_node_map.end()) {
        // There's already a group with this ID.
        groups_with_duplicate_ids->insert(star_entries[i].id);
      } else {
        // Create the node and update the mapping.
        StarredNode* node = new StarredNode(star_entries[i]);
        group_id_to_node_map[star_entries[i].group_id] = node;
      }
    }
  }

  // Iterate again, creating nodes for URL bookmarks and parenting all
  // bookmarks/folders. In addition populate the empty_url_ids with all entries
  // of type URL that have an empty URL.
  std::map<StarID, StarredNode*> id_to_node_map;
  for (size_t i = 0; i < star_entries.size(); ++i) {
    if (star_entries[i].type == StarredEntry::URL) {
      if (star_entries[i].url.is_empty()) {
        empty_url_ids->insert(star_entries[i].id);
      } else if (!star_entries[i].parent_group_id ||
          group_id_to_node_map.find(star_entries[i].parent_group_id) ==
          group_id_to_node_map.end()) {
        // This entry has no parent, or we couldn't find the parent.
        StarredNode* node = new StarredNode(star_entries[i]);
        unparented_urls->insert(node);
      } else {
        // Add the node to its parent.
        StarredNode* parent =
            group_id_to_node_map[star_entries[i].parent_group_id];
        StarredNode* node = new StarredNode(star_entries[i]);
        parent->Add(parent->GetChildCount(), node);
      }
    } else if (groups_with_duplicate_ids->find(star_entries[i].id) ==
               groups_with_duplicate_ids->end()) {
      // The entry is a group (or bookmark bar/other node) that isn't
      // marked as a duplicate.
      if (!star_entries[i].parent_group_id ||
          group_id_to_node_map.find(star_entries[i].parent_group_id) ==
          group_id_to_node_map.end()) {
        // Entry has no parent, or the parent wasn't found.
        roots->insert(group_id_to_node_map[star_entries[i].group_id]);
      } else {
        // Parent the group node.
        StarredNode* parent =
            group_id_to_node_map[star_entries[i].parent_group_id];
        StarredNode* node = group_id_to_node_map[star_entries[i].group_id];
        if (!node->HasAncestor(parent) && !parent->HasAncestor(node)) {
          parent->Add(parent->GetChildCount(), node);
        } else{
          // The node has a cycle. Add it to the list of roots so the cycle is
          // broken.
          roots->insert(node);
        }
      }
    }
  }
  return true;
}

StarredURLDatabase::StarredNode* StarredURLDatabase::GetNodeByType(
    const std::set<StarredURLDatabase::StarredNode*>& nodes,
    StarredEntry::Type type) {
  for (std::set<StarredNode*>::const_iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    if ((*i)->value.type == type)
      return *i;
  }
  return NULL;
}

bool StarredURLDatabase::EnsureVisualOrder(
    StarredURLDatabase::StarredNode* node) {
  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (node->GetChild(i)->value.visual_order != i) {
      StarredEntry& entry = node->GetChild(i)->value;
      entry.visual_order = i;
      LOG(WARNING) << "Bookmark visual order is wrong";
      if (!UpdateStarredEntryRow(entry.id, entry.title, entry.parent_group_id,
                                 i, entry.date_group_modified)) {
        NOTREACHED() << "Unable to update visual order";
        return false;
      }
    }
    if (!EnsureVisualOrder(node->GetChild(i)))
      return false;
  }
  return true;
}

bool StarredURLDatabase::EnsureStarredIntegrityImpl(
    std::set<StarredURLDatabase::StarredNode*>* roots,
    const std::set<StarID>& groups_with_duplicate_ids,
    std::set<StarredNode*>* unparented_urls,
    const std::set<StarID>& empty_url_ids) {
  // Make sure the bookmark bar entry exists.
  StarredNode* bookmark_node =
      GetNodeByType(*roots, StarredEntry::BOOKMARK_BAR);
  if (!bookmark_node) {
    LOG(WARNING) << "No bookmark bar folder in database";
    // If there is no bookmark bar entry in the db things are really
    // screwed. The UI assumes the bookmark bar entry has a particular
    // ID which is hard to enforce when creating a new entry. As this
    // shouldn't happen, we take the drastic route of recreating the
    // entire table.
    return RecreateStarredEntries();
  }

  // Make sure the other node exists.
  StarredNode* other_node = GetNodeByType(*roots, StarredEntry::OTHER);
  if (!other_node) {
    LOG(WARNING) << "No bookmark other folder in database";
    StarredEntry entry;
    entry.group_id = GetMaxGroupID() + 1;
    if (entry.group_id == 1) {
      NOTREACHED() << "Unable to get new id for other bookmarks folder";
      return false;
    }
    entry.id = CreateStarredEntryRow(
        0, entry.group_id, 0, L"other", Time::Now(), 0,
        history::StarredEntry::OTHER);
    if (!entry.id) {
      NOTREACHED() << "Unable to create other bookmarks folder";
      return false;
    }
    entry.type = StarredEntry::OTHER;
    StarredNode* other_node = new StarredNode(entry);
    roots->insert(other_node);
  }

  // We could potentially make sure only one group with type
  // BOOKMARK_BAR/OTHER, but history backend enforces this.

  // Nuke any entries with no url.
  for (std::set<StarID>::const_iterator i = empty_url_ids.begin();
       i != empty_url_ids.end(); ++i) {
    LOG(WARNING) << "Bookmark exists with no URL";
    if (!DeleteStarredEntryRow(*i)) {
      NOTREACHED() << "Unable to delete bookmark";
      return false;
    }
  }

  // Make sure the visual order of the nodes is correct.
  for (std::set<StarredNode*>::const_iterator i = roots->begin();
       i != roots->end(); ++i) {
    if (!EnsureVisualOrder(*i))
      return false;
  }

  // Move any unparented bookmarks to the bookmark bar.
  {
    std::set<StarredNode*>::iterator i = unparented_urls->begin();
    while (i != unparented_urls->end()) {
      LOG(WARNING) << "Bookmark not in a bookmark folder found";
      if (!Move(*i, bookmark_node))
        return false;
      i = unparented_urls->erase(i);
    }
  }

  // Nuke any groups with duplicate ids. A duplicate id means there are two
  // folders in the starred table with the same group_id. We only keep the
  // first folder, all other groups are removed.
  for (std::set<StarID>::const_iterator i = groups_with_duplicate_ids.begin();
       i != groups_with_duplicate_ids.end(); ++i) {
    LOG(WARNING) << "Duplicate group id in bookmark database";
    if (!DeleteStarredEntryRow(*i)) {
      NOTREACHED() << "Unable to delete folder";
      return false;
    }
  }

  // Move unparented user groups back to the bookmark bar.
  {
    std::set<StarredNode*>::iterator i = roots->begin();
    while (i != roots->end()) {
      if ((*i)->value.type == StarredEntry::USER_GROUP) {
        LOG(WARNING) << "Bookmark folder not on bookmark bar found";
        if (!Move(*i, bookmark_node))
          return false;
        i = roots->erase(i);
      } else {
        ++i;
      }
    }
  }

  return true;
}

bool StarredURLDatabase::Move(StarredNode* source, StarredNode* new_parent) {
  history::StarredEntry& entry = source->value;
  entry.visual_order = new_parent->GetChildCount();
  entry.parent_group_id = new_parent->value.group_id;
  if (!UpdateStarredEntryRow(entry.id, entry.title,
                             entry.parent_group_id, entry.visual_order,
                             entry.date_group_modified)) {
    NOTREACHED() << "Unable to move folder";
    return false;
  }
  new_parent->Add(new_parent->GetChildCount(), source);
  return true;
}

bool StarredURLDatabase::RecreateStarredEntries() {
  if (sqlite3_exec(GetDB(), "DROP TABLE starred", NULL, NULL,
                   NULL) != SQLITE_OK) {
    NOTREACHED() << "Unable to drop starred table";
    return false;
  }

  if (!InitStarTable()) {
    NOTREACHED() << "Unable to create starred table";
    return false;
  }

  if (sqlite3_exec(GetDB(), "UPDATE urls SET starred_id=0", NULL, NULL,
                   NULL) != SQLITE_OK) {
    NOTREACHED() << "Unable to mark all URLs as unstarred";
    return false;
  }
  return true;
}

void StarredURLDatabase::CheckVisualOrder(StarredNode* node) {
  for (int i = 0; i < node->GetChildCount(); ++i) {
    DCHECK(i == node->GetChild(i)->value.visual_order);
    CheckVisualOrder(node->GetChild(i));
  }
}

void StarredURLDatabase::CheckStarredIntegrity() {
#ifndef NDEBUG
  std::set<StarredNode*> roots;
  std::set<StarID> groups_with_duplicate_ids;
  std::set<StarredNode*> unparented_urls;
  std::set<StarID> empty_url_ids;

  if (!BuildStarNodes(&roots, &groups_with_duplicate_ids, &unparented_urls,
                      &empty_url_ids)) {
    return;
  }

  // There must be root and bookmark bar nodes.
  if (!GetNodeByType(roots, StarredEntry::BOOKMARK_BAR))
    NOTREACHED() << "No bookmark bar folder in starred";
  else
    CheckVisualOrder(GetNodeByType(roots, StarredEntry::BOOKMARK_BAR));

  if (!GetNodeByType(roots, StarredEntry::OTHER))
    NOTREACHED() << "No other bookmarks folder in starred";
  else
    CheckVisualOrder(GetNodeByType(roots, StarredEntry::OTHER));

  // The only roots should be the bookmark bar and other nodes. Also count how
  // many bookmark bar and other folders exists.
  int bookmark_bar_count = 0;
  int other_folder_count = 0;
  for (std::set<StarredNode*>::const_iterator i = roots.begin();
       i != roots.end(); ++i) {
    if ((*i)->value.type == StarredEntry::USER_GROUP)
      NOTREACHED() << "Bookmark folder not in bookmark bar or other folders";
    else if ((*i)->value.type == StarredEntry::BOOKMARK_BAR)
      bookmark_bar_count++;
    else if ((*i)->value.type == StarredEntry::OTHER)
      other_folder_count++;
  }
  DCHECK(bookmark_bar_count == 1) << "Should be only one bookmark bar entry";
  DCHECK(other_folder_count == 1) << "Should be only one other folder entry";

  // There shouldn't be any folders with the same group id.
  if (!groups_with_duplicate_ids.empty())
    NOTREACHED() << "Bookmark folder with duplicate ids exist";

  // And all URLs should be parented.
  if (!unparented_urls.empty())
    NOTREACHED() << "Bookmarks not on the bookmark/'other folder' exist";

  if (!empty_url_ids.empty())
    NOTREACHED() << "Bookmarks with no corresponding URL exist";

  STLDeleteElements(&roots);
  STLDeleteElements(&unparented_urls);
#endif NDEBUG
}

}  // namespace history
