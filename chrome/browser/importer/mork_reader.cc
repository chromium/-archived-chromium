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
// http://mxr.mozilla.org/firefox/source/db/morkreader/nsMorkReader.cpp
// This file has been converted to google style.

#include "chrome/browser/importer/mork_reader.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/firefox_importer_utils.h"

using base::Time;

namespace {

// Convert a hex character (0-9, A-F) to its corresponding byte value.
// Returns -1 if the character is invalid.
inline int HexCharToInt(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

// Unescape a Mork value.  Mork uses $xx escaping to encode non-ASCII
// characters.  Additionally, '$' and '\' are backslash-escaped.
// The result of the unescape is in returned.
std::string MorkUnescape(const std::string& input) {
  // We optimize for speed over space here -- size the result buffer to
  // the size of the source, which is an upper bound on the size of the
  // unescaped string.
  std::string result;
  size_t input_length = input.size();
  result.reserve(input_length);

  for (size_t i = 0; i < input_length; i++) {
    char c = input[i];
    if (c == '\\') {
      // Escaped literal, slip the backslash, append the next character.
      i++;
      if (i < input_length)
        result.push_back(input[i]);
    } else if (c == '$') {
      // Dollar sign denotes a hex character.
      if (i < input_length - 2) {
        // Would be nice to use ToInteger() here, but it currently
        // requires a null-terminated string.
        int first = HexCharToInt(input[++i]);
        int second = HexCharToInt(input[++i]);
        if (first >= 0 && second >= 0)
          result.push_back((first << 4) | second);
      }
    } else {
      // Regular character, just append.
      result.push_back(input[i]);
    }
  }
  return result;
}

}  // namespace

MorkReader::MorkReader() {
}

MorkReader::~MorkReader() {
  // Need to delete all the pointers to vectors we have in the table.
  for (RowMap::iterator i = table_.begin(); i != table_.end(); ++i)
    delete i->second;
}

bool MorkReader::Read(const std::wstring& filename) {
  FilePath path = FilePath::FromWStringHack(filename);
  stream_.open(path.value().c_str());
  if (!stream_.is_open())
    return false;

  std::string line;
  if (!ReadLine(&line) ||
      line.compare("// <!-- <mdb:mork:z v=\"1.4\"/> -->") != 0)
    return false;  // Unexpected file format.

  IndexMap column_map;
  while (ReadLine(&line)) {
    // Trim off leading spaces
    size_t idx = 0;
    size_t len = line.size();
    while (idx < len && line[idx] == ' ')
      ++idx;
    if (idx >= len)
      continue;

    // Look at the line to figure out what section type this is
    if (StartsWithASCII(&line[idx], "< <(a=c)>", true)) {
      // Column map.  We begin by creating a hash of column id to column name.
      StringMap column_name_map;
      ParseMap(line, idx, &column_name_map);

      // Now that we have the list of columns, we put them into a flat array.
      // Rows will have value arrays of the same size, with indexes that
      // correspond to the columns array.  As we insert each column into the
      // array, we also make an entry in columnMap so that we can look up the
      // index given the column id.
      columns_.reserve(column_name_map.size());

      for (StringMap::const_iterator i = column_name_map.begin();
           i != column_name_map.end(); ++i) {
        column_map[i->first] = static_cast<int>(columns_.size());
        MorkColumn col(i->first, i->second);
        columns_.push_back(col);
      }
    } else if (StartsWithASCII(&line[idx], "<(", true)) {
      // Value map.
      ParseMap(line, idx, &value_map_);
    } else if (line[idx] == '{' || line[idx] == '[') {
      // Table / table row.
      ParseTable(line, idx, &column_map);
    } else {
      // Don't know, hopefully don't care.
    }
  }
  return true;
}

// Parses a key/value map of the form
// <(k1=v1)(k2=v2)...>
bool MorkReader::ParseMap(const std::string& first_line,
                          size_t start_index,
                          StringMap* map) {
  // If the first line is the a=c line (column map), just skip over it.
  std::string line(first_line);
  if (StartsWithASCII(line, "< <(a=c)>", true))
    ReadLine(&line);

  std::string key;
  do {
    size_t idx = start_index;
    size_t len = line.size();
    size_t token_start;

    while (idx < len) {
      switch (line[idx++]) {
        case '(':
          // Beginning of a key/value pair.
          if (!key.empty()) {
            DLOG(WARNING) << "unterminated key/value pair?";
            key.clear();
          }

          token_start = idx;
          while (idx < len && line[idx] != '=')
            ++idx;
          key.assign(&line[token_start], idx - token_start);
          break;

        case '=': {
          // Beginning of the value.
          if (key.empty()) {
            DLOG(WARNING) << "stray value";
            break;
          }

          token_start = idx;
          while (idx < len && line[idx] != ')') {
            if (line[idx] == '\\')
              ++idx;  // Skip escaped ')' characters.
            ++idx;
          }
          size_t token_end = std::min(idx, len);
          ++idx;

          std::string value = MorkUnescape(
              std::string(&line[token_start], token_end - token_start));
          (*map)[key] = value;
          key.clear();
          break;
        }
        case '>':
          // End of the map.
          DLOG_IF(WARNING, key.empty()) <<
              "map terminates inside of key/value pair";
          return true;
      }
    }

    // We should start reading the next line at the beginning.
    start_index = 0;
  } while (ReadLine(&line));

  // We ran out of lines and the map never terminated.  This probably indicates
  // a parsing error.
  DLOG(WARNING) << "didn't find end of key/value map";
  return false;
}

// Parses a table row of the form [123(^45^67)..]
// (row id 123 has the value with id 67 for the column with id 45).
// A '^' prefix for a column or value references an entry in the column or
// value map.  '=' is used as the separator when the value is a literal.
void MorkReader::ParseTable(const std::string& first_line,
                            size_t start_index,
                            const IndexMap* column_map) {
  std::string line(first_line);

  // Column index of the cell we're parsing, minus one if invalid.
  int column_index = -1;

  // Points to the current row we're parsing inside of the |table_|, will be
  // NULL if we're not inside a row.
  ColumnDataList* current_row = NULL;

  bool in_meta_row = false;

  do {
    size_t idx = start_index;
    size_t len = line.size();

    while (idx < len) {
      switch (line[idx++]) {
        case '{':
          // This marks the beginning of a table section.  There's a lot of
          // junk before the first row that looks like cell values but isn't.
          // Skip to the first '['.
          while (idx < len && line[idx] != '[') {
            if (line[idx] == '{') {
              in_meta_row = true;  // The meta row is enclosed in { }
            } else if (line[idx] == '}') {
              in_meta_row = false;
            }
            ++idx;
          }
          break;

        case '[': {
          // Start of a new row.  Consume the row id, up to the first '('.
          // Row edits also have a table namespace, separated from the row id
          // by a colon.  We don't make use of the namespace, but we need to
          // make sure not to consider it part of the row id.
          if (current_row) {
            DLOG(WARNING) << "unterminated row?";
            current_row = NULL;
          }

          // Check for a '-' at the start of the id.  This signifies that
          // if the row already exists, we should delete all columns from it
          // before adding the new values.
          bool cut_columns;
          if (idx < len && line[idx] == '-') {
            cut_columns = true;
            ++idx;
          } else {
            cut_columns = false;
          }

          // Locate the range of the ID.
          size_t token_start = idx;  // Index of the first char of the token.
          while (idx < len &&
                 line[idx] != '(' &&
                 line[idx] != ']' &&
                 line[idx] != ':') {
            ++idx;
          }
          size_t token_end = idx;  // Index of the char following the token.
          while (idx < len && line[idx] != '(' && line[idx] != ']') {
            ++idx;
          }

          if (in_meta_row) {
            // Need to create the meta row.
            meta_row_.resize(columns_.size());
            current_row = &meta_row_;
          } else {
            // Find or create the regular row for this.
            IDString row_id(&line[token_start], token_end - token_start);
            RowMap::iterator found_row = table_.find(row_id);
            if (found_row == table_.end()) {
              // We don't already have this row, create a new one for it.
              current_row = new ColumnDataList(columns_.size());
              table_[row_id] = current_row;
            } else {
              // The row already exists and we're adding/replacing things.
              current_row = found_row->second;
            }
          }
          if (cut_columns) {
            for (size_t i = 0; i < current_row->size(); ++i)
              (*current_row)[i].clear();
          }
          break;
        }

        case ']':
          // We're done with the row.
          current_row = NULL;
          in_meta_row = false;
          break;

        case '(': {
          if (!current_row) {
            DLOG(WARNING) << "cell value outside of row";
            break;
          }

          bool column_is_atom;
          if (line[idx] == '^') {
            column_is_atom = true;
            ++idx;  // This is not part of the column id, advance past it.
          } else {
            column_is_atom = false;
          }
          size_t token_start = idx;
          while (idx < len && line[idx] != '^' && line[idx] != '=') {
            if (line[idx] == '\\')
              ++idx;  // Skip escaped characters.
            ++idx;
          }

          size_t token_end = std::min(idx, len);

          IDString column;
          if (column_is_atom)
            column.assign(&line[token_start], token_end - token_start);
          else
            column = MorkUnescape(line.substr(token_start,
                                              token_end - token_start));

          IndexMap::const_iterator found_column = column_map->find(column);
          if (found_column == column_map->end()) {
            DLOG(WARNING) << "Column not in column map, discarding it";
            column_index = -1;
          } else {
            column_index = found_column->second;
          }
          break;
        }

        case '=':
        case '^': {
          if (column_index == -1) {
            DLOG(WARNING) << "stray ^ or = marker";
            break;
          }

          bool value_is_atom = (line[idx - 1] == '^');
          size_t token_start = idx - 1;  // Include the '=' or '^' marker.
          while (idx < len && line[idx] != ')') {
            if (line[idx] == '\\')
              ++idx;  // Skip escaped characters.
            ++idx;
          }
          size_t token_end = std::min(idx, len);
          ++idx;

          if (value_is_atom) {
            (*current_row)[column_index].assign(&line[token_start],
                                                token_end - token_start);
          } else {
            (*current_row)[column_index] =
                MorkUnescape(line.substr(token_start, token_end - token_start));
          }
          column_index = -1;
        }
        break;
      }
    }

    // Start parsing the next line at the beginning.
    start_index = 0;
  } while (current_row && ReadLine(&line));
}

bool MorkReader::ReadLine(std::string* line) {
  line->resize(256);
  std::getline(stream_, *line);
  if (stream_.eof() || stream_.bad())
    return false;

  while (!line->empty() &&  (*line)[line->size() - 1] == '\\') {
    // There is a continuation for this line.  Read it and append.
    std::string new_line;
    std::getline(stream_, new_line);
    if (stream_.eof())
      return false;
    line->erase(line->size() - 1);
    line->append(new_line);
  }

  return true;
}

void MorkReader::NormalizeValue(std::string* value) const {
  if (value->empty())
    return;
  MorkReader::StringMap::const_iterator i;
  switch (value->at(0)) {
    case '^':
      // Hex ID, lookup the name for it in the |value_map_|.
      i = value_map_.find(value->substr(1));
      if (i == value_map_.end())
        value->clear();
      else
        *value = i->second;
      break;
    case '=':
      // Just use the literal after the equals sign.
      value->erase(value->begin());
      break;
    default:
      // Anything else is invalid.
      value->clear();
      break;
  }
}

// Source:
// http://mxr.mozilla.org/firefox/source/toolkit/components/places/src/nsMorkHistoryImporter.cpp

// Columns for entry (non-meta) history rows
enum {
  kURLColumn,
  kNameColumn,
  kVisitCountColumn,
  kHiddenColumn,
  kTypedColumn,
  kLastVisitColumn,
  kColumnCount  // Keep me last.
};

static const char * const gColumnNames[] = {
  "URL", "Name", "VisitCount", "Hidden", "Typed", "LastVisitDate"
};

struct TableReadClosure {
  explicit TableReadClosure(const MorkReader& r)
      : reader(r),
        swap_bytes(false),
        byte_order_column(-1) {
    for (int i = 0; i < kColumnCount; ++i)
      column_indexes[i] = -1;
  }

  // Backpointers to the reader and history we're operating on.
  const MorkReader& reader;

  // Whether we need to swap bytes (file format is other-endian).
  bool swap_bytes;

  // Indexes of the columns that we care about.
  int column_indexes[kColumnCount];
  int byte_order_column;
};

void AddToHistory(MorkReader::ColumnDataList* column_values,
                  const TableReadClosure& data,
                  std::vector<history::URLRow>* rows) {
  std::string values[kColumnCount];

  for (size_t i = 0; i < kColumnCount; ++i) {
    if (data.column_indexes[i] != -1) {
      values[i] = column_values->at(data.column_indexes[i]);
      data.reader.NormalizeValue(&values[i]);
      // Do not import hidden records.
      if (i == kHiddenColumn && values[i] == "1")
        return;
    }
  }

  GURL url(values[kURLColumn]);

  if (CanImportURL(url)) {
    history::URLRow row(url);

    // title is really a UTF-16 string at this point
    std::wstring title;
    if (data.swap_bytes) {
      CodepageToWide(values[kNameColumn], "UTF-16BE",
                     OnStringUtilConversionError::SKIP, &title);
    } else {
      CodepageToWide(values[kNameColumn], "UTF-16LE",
                     OnStringUtilConversionError::SKIP, &title);
    }
    row.set_title(title);

    int count = atoi(values[kVisitCountColumn].c_str());
    if (count == 0)
      count = 1;
    row.set_visit_count(count);

    time_t date = StringToInt64(values[kLastVisitColumn]);
    if (date != 0)
      row.set_last_visit(Time::FromTimeT(date/1000000));

    bool is_typed = (values[kTypedColumn] == "1");
    if (is_typed)
      row.set_typed_count(1);

    rows->push_back(row);
  }
}

// It sets up the file stream and loops over the lines in the file to
// parse them, then adds the resulting row set to history.
void ImportHistoryFromFirefox2(std::wstring file, MessageLoop* loop,
                               ProfileWriter* writer) {
  MorkReader reader;
  reader.Read(file);

  // Gather up the column ids so we don't need to find them on each row
  TableReadClosure data(reader);
  const MorkReader::MorkColumnList& columns = reader.columns();
  for (size_t i = 0; i < columns.size(); ++i) {
    for (int j = 0; j < kColumnCount; ++j)
      if (columns[i].name == gColumnNames[j]) {
        data.column_indexes[j] = static_cast<int>(i);
        break;
      }
    if (columns[i].name == "ByteOrder")
      data.byte_order_column = static_cast<int>(i);
  }

  // Determine the byte order from the table's meta-row.
  const MorkReader::ColumnDataList& meta_row = reader.meta_row();
  if (!meta_row.empty() && data.byte_order_column != -1) {
    std::string byte_order = meta_row[data.byte_order_column];
    if (!byte_order.empty()) {
      // Note whether the file uses a non-native byte ordering.
      // If it does, we'll have to swap bytes for PRUnichar values.
      // "BE" and "LE" are the only recognized values, anything
      // else is garbage and the file will be treated as native-endian
      // (no swapping).
      std::string byte_order_value(byte_order);
      reader.NormalizeValue(&byte_order_value);
      data.swap_bytes = (byte_order_value == "BE");
    }
  }

  std::vector<history::URLRow> rows;
  for (MorkReader::iterator i = reader.begin(); i != reader.end(); ++i)
    AddToHistory(i->second, data, &rows);
  if (!rows.empty())
    loop->PostTask(FROM_HERE, NewRunnableMethod(writer,
                   &ProfileWriter::AddHistoryPage, rows));
}
