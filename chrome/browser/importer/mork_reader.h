/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mork Reader.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Source:
// http://mxr.mozilla.org/firefox/source/db/morkreader/nsMorkReader.h

#ifndef CHROME_BROWSER_IMPORTER_MORK_READER_H__
#define CHROME_BROWSER_IMPORTER_MORK_READER_H__

#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/stack_container.h"
#include "chrome/browser/importer/importer.h"

// The nsMorkReader object allows a consumer to read in a mork-format
// file and enumerate the rows that it contains.  It does not provide
// any functionality for modifying mork tables.

// References:
//  http://www.mozilla.org/mailnews/arch/mork/primer.txt
//  http://www.mozilla.org/mailnews/arch/mork/grammar.txt
//  http://www.jwz.org/hacks/mork.pl

class MorkReader {
 public:
  // The IDString type has built-in storage for the hex string representation
  // of a 32-bit row id or atom map key, plus the terminating null.
  // We use STL string here so that is can be operated with STL containers.
  typedef std::string IDString;

  // Lists the contents of a series of columns.
  typedef std::vector<std::string> ColumnDataList;

  // A MorkColumn describes a column of the table.
  struct MorkColumn {
    MorkColumn(IDString i, const std::string& n) : id(i), name(n) { }

    IDString id;
    std::string name;
  };
  typedef std::vector<MorkColumn> MorkColumnList;

  // The key for each row is the identifier for it, and the data is a pointer
  // to an array for each column.
  typedef std::map<IDString, ColumnDataList*> RowMap;

  typedef RowMap::const_iterator iterator;

  MorkReader();
  ~MorkReader();

  // Read in the given mork file. Returns true on success.
  // Note: currently, only single-table mork files are supported
  bool Read(const std::wstring& filename);

  // Returns the list of columns in the current table.
  const MorkColumnList& columns() const { return columns_; }

  // Get the "meta row" for the table.  Each table has at most one meta row,
  // which records information about the table.  Like normal rows, the
  // meta row contains columns in the same order as returned by columns().
  // Returns null if there is no meta row for this table.
  const ColumnDataList& meta_row() const { return meta_row_; }

  // Normalizes the cell value (resolves references to the value map).
  // |value| is modified in-place.
  void NormalizeValue(std::string* value) const;

  // Allow iteration over the table cells using STL iterators. The iterator's
  // |first| will be the row ID, and the iterator's |second| will be a
  // pointer to a ColumnDataList containing the cell data.
  iterator begin() const { return table_.begin(); }
  iterator end() const { return table_.end(); }

 private:
  // A convenience typedef for an ID-to-string mapping.
  typedef std::map<IDString, std::string> StringMap;

  // A convenience typdef for an ID-to-index mapping, used for the column index
  // hashtable.
  typedef std::map<IDString, int> IndexMap;

  // Parses a line of the file which contains key/value pairs (either
  // the column map or the value map). The starting line is parsed starting at
  // the given index. Additional lines are read from stream_ if the line ends
  // mid-entry. The pairs are added to the map.
  bool ParseMap(const std::string& first_line,
                size_t start_index,
                StringMap* map);

  // Parses a line of the file which contains a table or row definition,
  // starting at the given offset within the line.  Additional lines are read
  // from |stream_| of the line ends mid-row.  An entry is added to |table_|
  // using the row ID as the key, which contains a column array for the row.
  // The supplied column hash table maps from column id to an index in
  // |columns_|.
  void ParseTable(const std::string& first_line,
                  size_t start_index,
                  const IndexMap* column_map);

  // Reads a single logical line from mStream into aLine.
  // Any continuation lines are consumed and appended to the line.
  bool ReadLine(std::string* line);

  std::ifstream stream_;

  // Lists the names of the columns for the table.
  MorkColumnList columns_;

  // Maps hex string IDs to the corrsponding names.
  StringMap value_map_;

  // The data of the columns in the meta row.
  ColumnDataList meta_row_;

  // The contents of the mork database. This array pointer is owned by this
  // class and must be deleted.
  RowMap table_;
};

// ImportHistoryFromFirefox2 is the main entry point to the importer.
void ImportHistoryFromFirefox2(std::wstring file, MessageLoop* loop,
                               ProfileWriter* writer);

#endif  // CHROME_BROWSER_IMPORTER_MORK_READER_H__
