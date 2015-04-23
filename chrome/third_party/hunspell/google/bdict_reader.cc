// Copyright 2008 Google Inc. All Rights Reserved.

#include "chrome/third_party/hunspell/google/bdict_reader.h"

#include "base/logging.h"
#include "chrome/third_party/hunspell/google/bdict.h"

namespace hunspell {

// Like the "Visitor" design pattern, this lightweight object provides an
// interface around a serialized trie node at the given address in the memory.
class NodeReader {
 public:
  // Return values for GetChildAt.
  enum FindResult {
    // A node is found.
    FIND_NODE,

    // There are no more children for this node, no child node is returned.
    FIND_DONE,  

    // There is no node at this location, but there are more if you continue
    // iterating. This happens when there is a lookup node with empty entries.
    FIND_NOTHING
  };

  // The default constructor makes an invalid reader.
  NodeReader();
  NodeReader(const unsigned char* bdict_data, size_t bdict_length,
             size_t node_offset, int node_depth);

  // Returns true if the reader is valid. False means you shouldn't use it.
  bool is_valid() const { return !!bdict_data_; }

  // Recursively finds the given NULL terminated word.
  // See BDictReader::FindWord.
  int FindWord(const unsigned char* word,
               int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const;

  // Allows iterating over the children of this node. When it returns
  // FIND_NODE, |*result| will be populated with the reader for the found node.
  // The first index is 0. The single character for this node will be placed
  // into |*found_char|.
  FindResult GetChildAt(int index, char* found_char, NodeReader* result) const;

  // Leaf ----------------------------------------------------------------------

  inline bool is_leaf() const {
    return (id_byte() & BDict::LEAF_NODE_TYPE_MASK) ==
        BDict::LEAF_NODE_TYPE_VALUE;
  }

  // If this is a leaf node with an additional string, this function will return
  // a pointer to the beginning of the additional string. It will be NULL
  // terminated. If it is not a leaf or has no additional string, it will return
  // NULL.
  inline const unsigned char* additional_string_for_leaf() const {
    // Leaf nodes with additional strings start with bits "01" in the ID byte.
    if ((id_byte() & BDict::LEAF_NODE_ADDITIONAL_MASK) ==
        BDict::LEAF_NODE_ADDITIONAL_VALUE)
      return &bdict_data_[node_offset_ + 2];  // Starts after the 2 byte ID.
    return NULL;
  }

  // Returns the first affix ID corresponding to the given leaf node. The
  // current node must be a leaf or this will do the wrong thing. There may be
  // additional affix IDs following the node when leaf_has_following is set,
  // but this will not handle those.
  inline int affix_id_for_leaf() const {
    // Take the lowest 6 bits of the first byte, and all 8 bits of the second.
    return ((bdict_data_[node_offset_ + 0] &
             BDict::LEAF_NODE_FIRST_BYTE_AFFIX_MASK) << 8) +
           bdict_data_[node_offset_ + 1];
  }

  // Returns true if there is a list of additional affix matches following this
  // leaf node.
  inline bool leaf_has_following() const {
    return ((id_byte() & BDict::LEAF_NODE_FOLLOWING_MASK) ==
        BDict::LEAF_NODE_FOLLOWING_VALUE);
  }

  // Fills the affix indices into the output array given a matching leaf node.
  // |additional_bytes| is the number of bytes of the additional string,
  // including the NULL terminator, following this leaf node. This will be 0 if
  // there is no additional string.
  int FillAffixesForLeafMatch(
      size_t additional_bytes,
      int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const;

  // Lookup --------------------------------------------------------------------

  inline bool is_lookup() const {
    return (id_byte() & BDict::LOOKUP_NODE_TYPE_MASK) ==
        BDict::LOOKUP_NODE_TYPE_VALUE;
  }

  inline bool is_lookup_32() const {
    return (id_byte() & BDict::LOOKUP_NODE_32BIT_MASK) ==
        BDict::LOOKUP_NODE_32BIT_VALUE;
  }

  inline bool lookup_has_0th() const {
    return (id_byte() & BDict::LOOKUP_NODE_0TH_MASK) ==
        BDict::LOOKUP_NODE_0TH_VALUE;
  }

  // Returns the first entry after the lookup table header. When there is a
  // magic 0th entry, it will be that address.
  inline size_t zeroth_entry_offset() const {
    return node_offset_ + 3;
  }

  // Returns the index of the first element in the lookup table. This skips any
  // magic 0th entry.
  size_t lookup_table_offset() const {
    size_t table_offset = zeroth_entry_offset();
    if (lookup_has_0th())
      return table_offset + (is_lookup_32() ? 4 : 2);
    return table_offset;
  }

  inline unsigned char lookup_first_char() const {
    return bdict_data_[node_offset_ + 1];
  }

  inline int lookup_num_chars() const {
    return bdict_data_[node_offset_ + 2];
  }

  // Computes a node reader for the magic 0th entry of the table. This assumes
  // it has a 0th entry. This will always return FOUND_NODE (for compatilibility
  // with GetChildAt).
  FindResult ReaderForLookup0th(NodeReader* result) const;

  // Gets a node reader for the |offset|th element in the table, not counting
  // the magic 0th element, if any (so passing 0 here will give you the first
  // element in the regular lookup table). The offset is assumed to be valid.
  //
  // |child_node_char| is the character value that the child node will
  // represent. The single character for this node will be placed into
  // |*found_char|.
  FindResult ReaderForLookupAt(size_t index, char* found_char,
                               NodeReader* result) const;

  // List ----------------------------------------------------------------------

  inline bool is_list() const {
    return (id_byte() & BDict::LIST_NODE_TYPE_MASK) ==
        BDict::LIST_NODE_TYPE_VALUE;
  }

  inline int is_list_16() const {
    // 16 bit lst nodes have the high 4 bits of 1.
    return (id_byte() & BDict::LIST_NODE_16BIT_MASK) ==
        BDict::LIST_NODE_16BIT_VALUE;
  }

  inline size_t list_item_count() const {
    // The list count is stored in the low 4 bits of the ID.
    return id_byte() & BDict::LIST_NODE_COUNT_MASK;
  }

  // Returns a NodeReader for the list item with the given index. The single
  // character for this node will be placed into |*found_char|.
  FindResult ReaderForListAt(size_t index, char* found_char,
                             NodeReader* result) const;

 private:
  inline unsigned char id_byte() const {
    DCHECK(node_offset_ < bdict_length_);
    return bdict_data_[node_offset_];
  }

  // Checks the given leaf node to see if it's a match for the given word.
  // The parameters and return values are the same as BDictReader::FindWord.
  int CompareLeafNode(const unsigned char* word,
                      int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const;

  // Recursive calls used by FindWord to look up child nodes of different types.
  int FindInLookup(const unsigned char* word,
                   int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const;
  int FindInList(const unsigned char* word,
                 int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const;

  // The entire bdict file. This will be NULL if it is invalid.
  const unsigned char* bdict_data_;
  size_t bdict_length_;

  // Absolute offset within |bdict_data_| of the beginning of this node.
  size_t node_offset_;

  // The character index into the word that this node represents.
  int node_depth_;
};

NodeReader::NodeReader()
    : bdict_data_(NULL),
      bdict_length_(0),
      node_offset_(0),
      node_depth_(0) {
}

NodeReader::NodeReader(const unsigned char* bdict_data, size_t bdict_length,
                       size_t node_offset, int node_depth)
    : bdict_data_(bdict_data),
      bdict_length_(bdict_length),
      node_offset_(node_offset),
      node_depth_(node_depth) {
}

int NodeReader::FindWord(const unsigned char* word,
                         int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  if (is_leaf())
    return CompareLeafNode(word, affix_indices);

  if (is_lookup())
    return FindInLookup(word, affix_indices);
  if (is_list())
    return FindInList(word, affix_indices);
  return 0;  // Corrupt file.
}

NodeReader::FindResult NodeReader::GetChildAt(int index, char* found_char,
                                              NodeReader* result) const {
  if (is_lookup()) {
    if (lookup_has_0th()) {
      if (index == 0) {
        *found_char = 0;
        return ReaderForLookup0th(result);
      }
      index--;  // Make index relative to the non-0th-element table.
    }
    return ReaderForLookupAt(index, found_char, result);
  }
  if (is_list()) {
    return ReaderForListAt(index, found_char, result);
  }
  return FIND_DONE;
}

int NodeReader::CompareLeafNode(
    const unsigned char* word,
    int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  // See if there is an additional string.
  const unsigned char* additional = additional_string_for_leaf();
  if (!additional) {
    // No additional string. This means we should have reached the end of the
    // word to get a match.
    if (word[node_depth_] != 0)
      return 0;
    return FillAffixesForLeafMatch(0, affix_indices);
  }

  // Check the additional string.
  int cur = 0;
  while (additional[cur]) {
    if (word[node_depth_ + cur] != additional[cur])
      return 0;  // Not a match.
    cur++;
  }

  // Got to the end of the additional string, the word should also be over for
  // a match (the same as above).
  if (word[node_depth_ + cur] != 0)
    return 0;
  return FillAffixesForLeafMatch(cur + 1, affix_indices);
}

int NodeReader::FillAffixesForLeafMatch(
    size_t additional_bytes,
    int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  // The first match is easy, it always comes from the affix_id included in the
  // leaf node.
  affix_indices[0] = affix_id_for_leaf();

  if (!leaf_has_following() && affix_indices[0] != BDict::FIRST_AFFIX_IS_UNUSED)
    return 1;  // Common case: no additional affix group IDs.

  // We may or may not need to ignore that first value we just read, since it
  // could be a dummy placeholder value. The |list_offset| is the starting
  // position in the output list to write the rest of the values, which may
  // overwrite the first value.
  int list_offset = 1;
  if (affix_indices[0] == BDict::FIRST_AFFIX_IS_UNUSED)
    list_offset = 0;

  // Process all remaining matches.
  const unsigned short* following_array =
      reinterpret_cast<const unsigned short*>(
          &bdict_data_[node_offset_ + additional_bytes + 2]);
  for (int i = 0; i < BDict::MAX_AFFIXES_PER_WORD - list_offset; i++) {
    if (following_array[i] == BDict::LEAF_NODE_FOLLOWING_LIST_TERMINATOR)
      return i + list_offset;  // Found the end of the list.
    affix_indices[i + list_offset] = following_array[i];
  }
  return BDict::MAX_AFFIXES_PER_WORD;
}

int NodeReader::FindInLookup(
    const unsigned char* word,
    int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  unsigned char next_char = word[node_depth_];

  NodeReader child_reader;
  if (next_char == 0 && lookup_has_0th()) {
    if (ReaderForLookup0th(&child_reader) != FIND_NODE)
      return 0;
  } else {
    // Look up in the regular part of the table.
    int offset_in_table = static_cast<int>(next_char) - lookup_first_char();
    if (offset_in_table < 0 || offset_in_table > lookup_num_chars())
      return 0;  // Table can not include this value.

    char dummy_char;
    if (ReaderForLookupAt(offset_in_table, &dummy_char, &child_reader) !=
        FIND_NODE)
      return 0;
    DCHECK(dummy_char == static_cast<char>(next_char));
  }

  if (!child_reader.is_valid())
    return 0;  // Something is messed up.

  // Now recurse into that child node.
  return child_reader.FindWord(word, affix_indices);
}

NodeReader::FindResult NodeReader::ReaderForLookup0th(
    NodeReader* result) const {
  size_t child_offset;
  if (is_lookup_32()) {
    child_offset = *reinterpret_cast<const unsigned int*>(
        &bdict_data_[zeroth_entry_offset()]);
  } else {
    child_offset = *reinterpret_cast<const unsigned short*>(
        &bdict_data_[zeroth_entry_offset()]);
    child_offset += node_offset_;
  }

  // Range check the offset;
  if (child_offset > bdict_length_)
    return FIND_DONE;

  // Now recurse into that child node. We don't advance to the next character
  // here since the 0th element will be a leaf (see ReaderForLookupAt).
  *result = NodeReader(bdict_data_, bdict_length_, child_offset, node_depth_);
  return FIND_NODE;
}

NodeReader::FindResult NodeReader::ReaderForLookupAt(
    size_t index,
    char* found_char,
    NodeReader* result) const {
  const unsigned char* table_begin = &bdict_data_[lookup_table_offset()];

  if (index >= static_cast<size_t>(lookup_num_chars()))
    return FIND_DONE;
  
  size_t child_offset;
  if (is_lookup_32()) {
    // Table contains 32-bit absolute offsets.
    child_offset =
        reinterpret_cast<const unsigned int*>(table_begin)[index];
    if (!child_offset)
      return FIND_NOTHING;  // This entry in the table is empty.
  } else {
    // Table contains 16-bit offsets relative to the current node.
    child_offset =
        reinterpret_cast<const unsigned short*>(table_begin)[index];
    if (!child_offset)
      return FIND_NOTHING;  // This entry in the table is empty.
    child_offset += node_offset_;
  }

  // Range check the offset;
  if (child_offset > bdict_length_)
    return FIND_DONE;  // Error.

  // This is a bit tricky. When we've just reached the end of a word, the word
  // itself will be stored in a leaf "node" off of this node. That node, of
  // course, will want to know that it's the end of the word and so we have to
  // have it use the same index into the word as we're using at this level.
  //
  // This happens when there is a word in the dictionary that is a strict
  // prefix of other words in the dictionary, and so we'll have a non-leaf
  // node representing the entire word before the ending leaf node.
  //
  // In all other cases, we want to advance to the next character. Even if the
  // child node is a leaf, it will have an additional character that it will
  // want to check.
  *found_char = static_cast<char>(index + lookup_first_char());
  int char_advance = *found_char == 0 ? 0 : 1;

  *result = NodeReader(bdict_data_, bdict_length_,
                       child_offset, node_depth_ + char_advance);
  return FIND_NODE;
}

int NodeReader::FindInList(
    const unsigned char* word,
    int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  unsigned char next_char = word[node_depth_];

  // TODO(brettw) replace with binary search.
  size_t list_count = list_item_count();
  const unsigned char* list_begin = &bdict_data_[node_offset_ + 1];

  int bytes_per_index = (is_list_16() ? 3 : 2);

  for (size_t i = 0; i < list_count; i++) {
    if (list_begin[i * bytes_per_index] == next_char) {
      // Found a match.
      char dummy_char;
      NodeReader child_reader;
      if (ReaderForListAt(i, &dummy_char, &child_reader) != FIND_NODE)
        return 0;
      DCHECK(dummy_char == static_cast<char>(next_char));
      return child_reader.FindWord(word, affix_indices);
    }
  }
  return 0;
}

NodeReader::FindResult NodeReader::ReaderForListAt(
    size_t index,
    char* found_char,
    NodeReader* result) const {
  size_t list_begin = node_offset_ + 1;

  if (index >= list_item_count())
    return FIND_DONE;

  size_t offset;
  if (is_list_16()) {
    const unsigned char* list_item_begin = bdict_data_ + list_begin + index * 3;
    *found_char = static_cast<char>(list_item_begin[0]);

    // The children begin right after the list.
    size_t children_begin = list_begin + list_item_count() * 3;
    offset = children_begin + *reinterpret_cast<const unsigned short*>(
        &list_item_begin[1]);
  } else {
    const unsigned char* list_item_begin = bdict_data_ + list_begin + index * 2;
    *found_char = list_item_begin[0];

    size_t children_begin = list_begin + list_item_count() * 2;
    offset = children_begin + list_item_begin[1];
  }

  if (offset == 0 || node_offset_ > bdict_length_)
    return FIND_DONE;  // Error, should not happen except for corruption.

  int char_advance = *found_char == 0 ? 0 : 1;  // See ReaderForLookupAt.
  *result = NodeReader(bdict_data_, bdict_length_,
                       offset, node_depth_ + char_advance);
  return FIND_NODE;
}

// WordIterator ----------------------------------------------------------------

struct WordIterator::NodeInfo {
  // The current offset is set to -1 so we start iterating at 0 when Advance
  // is called.
  NodeInfo(const NodeReader& rdr, char add)
      : reader(rdr),
        addition(add),
        cur_offset(-1) {
  }

  // The reader for this node level.
  NodeReader reader;

  // The character that this level represents. For the 0th level, this will
  // be 0 (since it is the root that represents no characters).
  char addition;

  // The current index into the reader that we're reading. Combined with the
  // |stack_|, this allows us to iterate over the tree in depth-first order.
  int cur_offset;
};

WordIterator::WordIterator(const NodeReader& reader) {
  NodeInfo info(reader, 0);
  stack_.push_back(info);
}

WordIterator::WordIterator(const WordIterator& other) {
  operator=(other);
}

WordIterator::~WordIterator() {
  // Can't be in the header for the NodeReader destructor.
}

WordIterator& WordIterator::operator=(const WordIterator& other) {
  stack_ = other.stack_;
  return *this;
}

int WordIterator::Advance(char* output_buffer, size_t output_len,
                          int affix_ids[BDict::MAX_AFFIXES_PER_WORD]) {
  // In-order tree walker. This uses a loop for fake tail recursion.
  while (!stack_.empty()) {
    NodeInfo& cur = stack_.back();
    cur.cur_offset++;
    char cur_char;
    NodeReader child_reader;

    /*if (cur.reader.is_leaf()) {
      child_reader = cur.reader;
      cur_char = cur.addition;
      stack_.pop_back();
      return FoundLeaf(child_reader, cur_char, output_buffer, output_len,
                       affix_ids);
    }*/

    switch (cur.reader.GetChildAt(cur.cur_offset, &cur_char, &child_reader)) {
      case NodeReader::FIND_NODE:
        // Got a valid child node.
        if (child_reader.is_leaf()) {
          return FoundLeaf(child_reader, cur_char, output_buffer, output_len,
                           affix_ids);
        }

        // Not a leaf. Add the new node to our stack and try again.
        stack_.push_back(NodeInfo(child_reader, cur_char));
        break;
    
      case NodeReader::FIND_NOTHING:
        // This one is empty, but we're not done. Continue on.
        break;

      case NodeReader::FIND_DONE:
        // No more children at this level, pop the stack and go back one.
        stack_.pop_back();
    }
  }

  return false;
}

int WordIterator::FoundLeaf(const NodeReader& reader, char cur_char,
                            char* output_buffer, size_t output_len,
                            int affix_ids[BDict::MAX_AFFIXES_PER_WORD]) {
  // Remember that the first item in the stack is the root and so doesn't count.
  int i;
  for (i = 0; i < static_cast<int>(stack_.size()) - 1 && i < static_cast<int>(output_len) - 1; i++)
    output_buffer[i] = stack_[i + 1].addition;
  output_buffer[i++] = cur_char;  // The one we just found.
  
  // Possibly add any extra parts.
  size_t additional_string_length = 0;
  const char* additional = reinterpret_cast<const char*>(
      reader.additional_string_for_leaf());
  for (; i < static_cast<int>(output_len) - 1 && additional &&
           additional[additional_string_length] != 0;
       i++, additional_string_length++)
    output_buffer[i] = additional[additional_string_length];
  if (additional_string_length)
    additional_string_length++;  // Account for the null terminator.
  output_buffer[i] = 0;

  return reader.FillAffixesForLeafMatch(additional_string_length,
                                        affix_ids);
}

// LineIterator ----------------------------------------------------------------

LineIterator::LineIterator(
    const unsigned char* bdict_data,
    size_t bdict_length,
    size_t first_offset)
    : bdict_data_(bdict_data),
      bdict_length_(bdict_length),
      cur_offset_(first_offset) {
}

// Returns true when all data has been read. We're done when we reach a
// double-NULL or a the end of the input (shouldn't happen).
bool LineIterator::IsDone() const {
  return cur_offset_ >= bdict_length_ || bdict_data_[cur_offset_] == 0;
}

const char* LineIterator::Advance() {
  if (IsDone())
    return NULL;

  const char* begin = reinterpret_cast<const char*>(&bdict_data_[cur_offset_]);

  // Advance over this word to find the end.
  while (cur_offset_ < bdict_length_ && bdict_data_[cur_offset_])
    cur_offset_++;
  cur_offset_++;  // Advance over the NULL terminator.

  return begin;
}

bool LineIterator::AdvanceAndCopy(char* buf, size_t buf_len) {
  if (IsDone())
    return false;

  const char* begin = reinterpret_cast<const char*>(&bdict_data_[cur_offset_]);

  // Advance over this word to find the end.
  size_t i;
  for (i = 0;
       i < buf_len && cur_offset_ < bdict_length_ && bdict_data_[cur_offset_];
       i++, cur_offset_++) {
    buf[i] = bdict_data_[cur_offset_];
  }
  // Handle the NULL terminator.
  cur_offset_++;  // Consume in the input
  if (i < buf_len)
    buf[i] = 0;  // Save in the output.
  else
    buf[buf_len - 1] = 0;  // Overflow, make sure it's terminated.

  return !!buf[0];
}

// ReplacementIterator ---------------------------------------------------------

// Fills pointers to NULL terminated strings into the given output params.
// Returns false if there are no more pairs and nothing was filled in.
bool ReplacementIterator::GetNext(const char** first, const char** second) {
  if (IsDone())
    return false;
  *first = Advance();
  *second = Advance();
  return *first && *second;
}

// BDictReader -----------------------------------------------------------------

BDictReader::BDictReader()
    : bdict_data_(NULL),
      bdict_length_(0),
      header_(NULL) {
}

bool BDictReader::Init(const unsigned char* bdict_data, size_t bdict_length) {
  if (bdict_length < sizeof(BDict::Header))
    return false;

  // Check header.
  header_ = reinterpret_cast<const BDict::Header*>(bdict_data);
  if (header_->signature != BDict::SIGNATURE ||
      header_->major_version != 1 ||
      header_->aff_offset > bdict_length ||
      header_->dic_offset > bdict_length)
    return false;

  // Get the affix header, make sure there is enough room for it.
  if (header_->aff_offset + sizeof(BDict::AffHeader) > bdict_length)
    return false;
  aff_header_ = reinterpret_cast<const BDict::AffHeader*>(
      &bdict_data[header_->aff_offset]);

  // Make sure there is enough room for the affix group count dword.
  if (aff_header_->affix_group_offset + sizeof(uint32) > bdict_length)
    return false;

  // Don't set these until the end. This way, NULL bdict_data_ will indicate
  // failure.
  bdict_data_ = bdict_data;
  bdict_length_ = bdict_length;
  return true;
}

int BDictReader::FindWord(
    const char* word,
    int affix_indices[BDict::MAX_AFFIXES_PER_WORD]) const {
  if (!bdict_data_ ||
      header_->dic_offset > bdict_length_) {
    // When the dictionary is corrupt, we return 0 which means the word is valid
    // and has no rules. This means when there is some problem, we'll default
    // to no spellchecking rather than marking everything as misspelled.
    return 0;
  }
  NodeReader reader(bdict_data_, bdict_length_, header_->dic_offset, 0);
  return reader.FindWord(reinterpret_cast<const unsigned char*>(word),
                         affix_indices);
}

LineIterator BDictReader::GetAfLineIterator() const {
  if (!bdict_data_ ||
      aff_header_->affix_group_offset == 0 ||
      aff_header_->affix_group_offset >= bdict_length_)
    return LineIterator(bdict_data_, 0, 0);  // Item is empty or invalid.
  return LineIterator(bdict_data_, bdict_length_,
                      aff_header_->affix_group_offset);
}

LineIterator BDictReader::GetAffixLineIterator() const {
  if (!bdict_data_ ||
      aff_header_->affix_rule_offset == 0 ||
      aff_header_->affix_rule_offset >= bdict_length_)
    return LineIterator(bdict_data_, 0, 0);  // Item is empty or invalid.
  return LineIterator(bdict_data_, bdict_length_,
                      aff_header_->affix_rule_offset);
}

LineIterator BDictReader::GetOtherLineIterator() const {
  if (!bdict_data_ ||
      aff_header_->other_offset == 0 ||
      aff_header_->other_offset >= bdict_length_)
    return LineIterator(bdict_data_, 0, 0);  // Item is empty or invalid.
  return LineIterator(bdict_data_, bdict_length_,
                      aff_header_->other_offset);
}

ReplacementIterator BDictReader::GetReplacementIterator() const {
  return ReplacementIterator(bdict_data_, bdict_length_,
                             aff_header_->rep_offset);
}


WordIterator BDictReader::GetAllWordIterator() const {
  NodeReader reader(bdict_data_, bdict_length_, header_->dic_offset, 0);
  return WordIterator(reader);
}

}  // namespace hunspell
