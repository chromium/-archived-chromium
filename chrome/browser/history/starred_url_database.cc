// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/starred_url_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/json_writer.h"
#include "base/scoped_vector.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

using base::Time;

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
      entry->url = GURL(WideToUTF8(s->column_wstring(6)));
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
  entry->title = s->column_wstring(2);
  entry->date_added = Time::FromInternalValue(s->column_int64(3));
  entry->visual_order = s->column_int(4);
  entry->parent_group_id = s->column_int64(5);
  entry->url_id = s->column_int64(7);
  entry->group_id = s->column_int64(8);
  entry->date_group_modified = Time::FromInternalValue(s->column_int64(9));
}

}  // namespace

StarredURLDatabase::StarredURLDatabase() {
}

StarredURLDatabase::~StarredURLDatabase() {
}

bool StarredURLDatabase::MigrateBookmarksToFile(const FilePath& path) {
  if (!DoesSqliteTableExist(GetDB(), "starred"))
    return true;

  if (EnsureStarredIntegrity() && !MigrateBookmarksToFileImpl(path)) {
    NOTREACHED() << " Bookmarks migration failed";
    return false;
  }

  if (sqlite3_exec(GetDB(), "DROP TABLE starred", NULL, NULL,
                   NULL) != SQLITE_OK) {
    NOTREACHED() << "Unable to drop starred table";
    return false;
  }
  return true;
}

bool StarredURLDatabase::GetAllStarredEntries(
    std::vector<StarredEntry>* entries) {
  DCHECK(entries);
  std::string sql = "SELECT ";
  sql.append(kHistoryStarFields);
  sql.append("FROM starred LEFT JOIN urls ON starred.url_id = urls.id ");
  sql += "ORDER BY parent_id, visual_order";

  SQLStatement s;
  if (s.prepare(GetDB(), sql.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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

bool StarredURLDatabase::EnsureStarredIntegrity() {
  std::set<StarredNode*> roots;
  std::set<StarID> groups_with_duplicate_ids;
  std::set<StarredNode*> unparented_urls;
  std::set<StarID> empty_url_ids;

  if (!BuildStarNodes(&roots, &groups_with_duplicate_ids, &unparented_urls,
                      &empty_url_ids)) {
    return false;
  }

  bool valid = EnsureStarredIntegrityImpl(&roots, groups_with_duplicate_ids,
                                          &unparented_urls, empty_url_ids);

  STLDeleteElements(&roots);
  STLDeleteElements(&unparented_urls);
  return valid;
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
        entry->url_id = url_row.id();  // The caller doesn't have to set this.
      }

      // Create the star entry referring to the URL row.
      entry->id = CreateStarredEntryRow(entry->url_id, entry->group_id,
          entry->parent_group_id, entry->title, entry->date_added,
          entry->visual_order, entry->type);

      // Update the URL row to refer to this new starred entry.
      UpdateURLRow(entry->url_id, url_row);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
  return entry->id;
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
  if (!GetAllStarredEntries(&star_entries)) {
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
    // screwed. Return false, which won't trigger migration and we'll just
    // drop the tables.
    return false;
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
      unparented_urls->erase(i++);
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
        roots->erase(i++);
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

bool StarredURLDatabase::MigrateBookmarksToFileImpl(const FilePath& path) {
  std::vector<history::StarredEntry> entries;
  if (!GetAllStarredEntries(&entries))
    return false;

  // Create the bookmark bar and other folder nodes.
  history::StarredEntry entry;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  BookmarkNode bookmark_bar_node(NULL, GURL());
  bookmark_bar_node.Reset(entry);
  entry.type = history::StarredEntry::OTHER;
  BookmarkNode other_node(NULL, GURL());
  other_node.Reset(entry);

  std::map<history::UIStarID, history::StarID> group_id_to_id_map;
  typedef std::map<history::StarID, BookmarkNode*> IDToNodeMap;
  IDToNodeMap id_to_node_map;

  history::UIStarID other_folder_group_id = 0;
  history::StarID other_folder_id = 0;

  // Iterate through the entries building a mapping between group_id and id.
  for (std::vector<history::StarredEntry>::const_iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (i->type != history::StarredEntry::URL) {
      group_id_to_id_map[i->group_id] = i->id;
      if (i->type == history::StarredEntry::OTHER) {
        other_folder_id = i->id;
        other_folder_group_id = i->group_id;
      }
    }
  }

  // Register the bookmark bar and other folder nodes in the maps.
  id_to_node_map[HistoryService::kBookmarkBarID] = &bookmark_bar_node;
  group_id_to_id_map[HistoryService::kBookmarkBarID] =
      HistoryService::kBookmarkBarID;
  if (other_folder_group_id) {
    id_to_node_map[other_folder_id] = &other_node;
    group_id_to_id_map[other_folder_group_id] = other_folder_id;
  }

  // Iterate through the entries again creating the nodes.
  for (std::vector<history::StarredEntry>::iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (!i->parent_group_id) {
      DCHECK(i->type == history::StarredEntry::BOOKMARK_BAR ||
             i->type == history::StarredEntry::OTHER);
      // Only entries with no parent should be the bookmark bar and other
      // bookmarks folders.
      continue;
    }

    BookmarkNode* node = id_to_node_map[i->id];
    if (!node) {
      // Creating a node results in creating the parent. As such, it is
      // possible for the node representing a group to have been created before
      // encountering the details.

      // The created nodes are owned by the root node.
      node = new BookmarkNode(NULL, i->url);
      id_to_node_map[i->id] = node;
    }
    node->Reset(*i);

    DCHECK(group_id_to_id_map.find(i->parent_group_id) !=
           group_id_to_id_map.end());
    history::StarID parent_id = group_id_to_id_map[i->parent_group_id];
    BookmarkNode* parent = id_to_node_map[parent_id];
    if (!parent) {
      // Haven't encountered the parent yet, create it now.
      parent = new BookmarkNode(NULL, GURL());
      id_to_node_map[parent_id] = parent;
    }

    // Add the node to its parent. |entries| is ordered by parent then
    // visual order so that we know we maintain visual order by always adding
    // to the end.
    parent->Add(parent->GetChildCount(), node);
  }

  // Save to file.
  BookmarkCodec encoder;
  scoped_ptr<Value> encoded_bookmarks(
      encoder.Encode(&bookmark_bar_node, &other_node));
  std::string content;
  JSONWriter::Write(encoded_bookmarks.get(), true, &content);

  return (file_util::WriteFile(path, content.c_str(),
                               static_cast<int>(content.length())) != -1);
}

}  // namespace history
