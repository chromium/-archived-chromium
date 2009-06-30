// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_html_writer.h"

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history_types.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace bookmark_html_writer {

namespace {

// File header.
const char kHeader[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>\r\n"
    "<!-- This is an automatically generated file.\r\n"
    "     It will be read and overwritten.\r\n"
    "     DO NOT EDIT! -->\r\n"
    "<META HTTP-EQUIV=\"Content-Type\""
    " CONTENT=\"text/html; charset=UTF-8\">\r\n"
    "<TITLE>Bookmarks</TITLE>\r\n"
    "<H1>Bookmarks</H1>\r\n"
    "<DL><p>\r\n";

// Newline separator.
const char kNewline[] = "\r\n";

// The following are used for bookmarks.

// Start of a bookmark.
const char kBookmarkStart[] = "<DT><A HREF=\"";
// After kBookmarkStart.
const char kAddDate[] = "\" ADD_DATE=\"";
// After kAddDate.
const char kBookmarkAttributeEnd[] = "\">";
// End of a bookmark.
const char kBookmarkEnd[] = "</A>";

// The following are used when writing folders.

// Start of a folder.
const char kFolderStart[] = "<DT><H3 ADD_DATE=\"";
// After kFolderStart.
const char kLastModified[] = "\" LAST_MODIFIED=\"";
// After kLastModified when writing the bookmark bar.
const char kBookmarkBar[] = "\" PERSONAL_TOOLBAR_FOLDER=\"true\">";
// After kLastModified when writing a user created folder.
const char kFolderAttributeEnd[] = "\">";
// End of the folder.
const char kFolderEnd[] = "</H3>";
// Start of the children of a folder.
const char kFolderChildren[] = "<DL><p>";
// End of the children for a folder.
const char kFolderChildrenEnd[] = "</DL><p>";

// Number of characters to indent by.
const size_t kIndentSize = 4;

// Class responsible for the actual writing.
class Writer : public Task {
 public:
  Writer(Value* bookmarks, const FilePath& path)
      : bookmarks_(bookmarks),
        path_(path) {
  }

  virtual void Run() {
    if (!OpenFile())
      return;

    Value* roots;
    if (!Write(kHeader) ||
        bookmarks_->GetType() != Value::TYPE_DICTIONARY ||
        !static_cast<DictionaryValue*>(bookmarks_.get())->Get(
            BookmarkCodec::kRootsKey, &roots) ||
        roots->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      return;
    }

    DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
    Value* root_folder_value;
    Value* other_folder_value;
    if (!roots_d_value->Get(BookmarkCodec::kRootFolderNameKey,
                            &root_folder_value) ||
        root_folder_value->GetType() != Value::TYPE_DICTIONARY ||
        !roots_d_value->Get(BookmarkCodec::kOtherBookmarkFolderNameKey,
                            &other_folder_value) ||
        other_folder_value->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      return;  // Invalid type for root folder and/or other folder.
    }

    IncrementIndent();

    if (!WriteNode(*static_cast<DictionaryValue*>(root_folder_value),
                   history::StarredEntry::BOOKMARK_BAR) ||
        !WriteNode(*static_cast<DictionaryValue*>(other_folder_value),
                   history::StarredEntry::OTHER)) {
      return;
    }

    DecrementIndent();

    Write(kFolderChildrenEnd);
    Write(kNewline);
  }

 private:
  // Types of text being written out. The type dictates how the text is
  // escaped.
  enum TextType {
    // The text is the value of an html attribute, eg foo in
    // <a href="foo">.
    ATTRIBUTE_VALUE,

    // Actual content, eg foo in <h1>foo</h2>.
    CONTENT
  };

  // Opens the file, returning true on success.
  bool OpenFile() {
    int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
    return (file_stream_.Open(path_, flags) == net::OK);
  }

  // Increments the indent.
  void IncrementIndent() {
    indent_.resize(indent_.size() + kIndentSize, ' ');
  }

  // Decrements the indent.
  void DecrementIndent() {
    DCHECK(!indent_.empty());
    indent_.resize(indent_.size() - kIndentSize, ' ');
  }

  // Writes raw text out returning true on success. This does not escape
  // the text in anyway.
  bool Write(const std::string& text) {
    size_t wrote = file_stream_.Write(text.c_str(), text.length(), NULL);
    bool result = (wrote == text.length());
    DCHECK(result);
    return result;
  }

  // Writes out the text string (as UTF8). The text is escaped based on
  // type.
  bool Write(const std::wstring& text, TextType type) {
    std::string utf8_string;

    switch (type) {
      case ATTRIBUTE_VALUE:
        // Convert " to &quot;
        if (text.find(L"\"") != std::wstring::npos) {
          string16 replaced_string = WideToUTF16Hack(text);
          ReplaceSubstringsAfterOffset(&replaced_string, 0,
                                       ASCIIToUTF16("\""),
                                       ASCIIToUTF16("&quot;"));
          utf8_string = UTF16ToUTF8(replaced_string);
        } else {
          utf8_string = WideToUTF8(text);
        }
        break;

      case CONTENT:
        utf8_string = WideToUTF8(EscapeForHTML(text));
        break;

      default:
        NOTREACHED();
    }

    return Write(utf8_string);
  }

  // Indents the current line.
  bool WriteIndent() {
    return Write(indent_);
  }

  // Converts a time string written to the JSON codec into a time_t string
  // (used by bookmarks.html) and writes it.
  bool WriteTime(const std::wstring& time_string) {
    base::Time time = base::Time::FromInternalValue(
        StringToInt64(WideToUTF16Hack(time_string)));
    return Write(Int64ToString(time.ToTimeT()));
  }

  // Writes the node and all its children, returning true on success.
  bool WriteNode(const DictionaryValue& value,
                 history::StarredEntry::Type folder_type) {
    std::wstring title, date_added_string, type_string;
    if (!value.GetString(BookmarkCodec::kNameKey, &title) ||
        !value.GetString(BookmarkCodec::kDateAddedKey, &date_added_string) ||
        !value.GetString(BookmarkCodec::kTypeKey, &type_string) ||
        (type_string != BookmarkCodec::kTypeURL &&
         type_string != BookmarkCodec::kTypeFolder))  {
      NOTREACHED();
      return false;
    }

    if (type_string == BookmarkCodec::kTypeURL) {
      std::wstring url_string;
      if (!value.GetString(BookmarkCodec::kURLKey, &url_string)) {
        NOTREACHED();
        return false;
      }
      if (!WriteIndent() ||
          !Write(kBookmarkStart) ||
          !Write(url_string, ATTRIBUTE_VALUE) ||
          !Write(kAddDate) ||
          !WriteTime(date_added_string) ||
          !Write(kBookmarkAttributeEnd) ||
          !Write(title, CONTENT) ||
          !Write(kBookmarkEnd) ||
          !Write(kNewline)) {
        return false;
      }
      return true;
    }

    // Folder.
    std::wstring last_modified_date;
    Value* child_values;
    if (!value.GetString(BookmarkCodec::kDateModifiedKey,
                         &last_modified_date) ||
        !value.Get(BookmarkCodec::kChildrenKey, &child_values) ||
        child_values->GetType() != Value::TYPE_LIST) {
      NOTREACHED();
      return false;
    }
    if (folder_type != history::StarredEntry::OTHER) {
      // The other folder name is not written out. This gives the effect of
      // making the contents of the 'other folder' be a sibling to the bookmark
      // bar folder.
      if (!WriteIndent() ||
          !Write(kFolderStart) ||
          !WriteTime(date_added_string) ||
          !Write(kLastModified) ||
          !WriteTime(last_modified_date)) {
        return false;
      }
      if (folder_type == history::StarredEntry::BOOKMARK_BAR) {
        if (!Write(kBookmarkBar))
          return false;
        title = l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME);
      } else if (!Write(kFolderAttributeEnd)) {
        return false;
      }
      if (!Write(title, CONTENT) ||
          !Write(kFolderEnd) ||
          !Write(kNewline) ||
          !WriteIndent() ||
          !Write(kFolderChildren) ||
          !Write(kNewline)) {
        return false;
      }
      IncrementIndent();
    }

    // Write the children.
    ListValue* children = static_cast<ListValue*>(child_values);
    for (size_t i = 0; i < children->GetSize(); ++i) {
      Value* child_value;
      if (!children->Get(i, &child_value) ||
          child_value->GetType() != Value::TYPE_DICTIONARY) {
        NOTREACHED();
        return false;
      }
      if (!WriteNode(*static_cast<DictionaryValue*>(child_value),
                     history::StarredEntry::USER_GROUP)) {
        return false;
      }
    }
    if (folder_type != history::StarredEntry::OTHER) {
      // Close out the folder.
      DecrementIndent();
      if (!WriteIndent() ||
          !Write(kFolderChildrenEnd) ||
          !Write(kNewline)) {
        return false;
      }
    }
    return true;
  }

  // The BookmarkModel as a Value. This value was generated from the
  // BookmarkCodec.
  scoped_ptr<Value> bookmarks_;

  // Path we're writing to.
  FilePath path_;

  // File we're writing to.
  net::FileStream file_stream_;

  // How much we indent when writing a bookmark/folder. This is modified
  // via IncrementIndent and DecrementIndent.
  std::string indent_;
};

}  // namespace

void WriteBookmarks(MessageLoop* thread,
                    BookmarkModel* model,
                    const std::wstring& path) {
  // BookmarkModel isn't thread safe (nor would we want to lock it down
  // for the duration of the write), as such we make a copy of the
  // BookmarkModel using BookmarkCodec then write from that.
  BookmarkCodec codec;
  scoped_ptr<Writer> writer(new Writer(codec.Encode(model),
                            FilePath::FromWStringHack(path)));
  if (thread)
    thread->PostTask(FROM_HERE, writer.release());
  else
    writer->Run();
}

}  // namespace bookmark_html_writer
