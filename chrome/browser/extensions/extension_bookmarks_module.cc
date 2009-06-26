// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmarks_module.h"

#include "base/json_writer.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace keys = extension_bookmarks_module_constants;

// Helper functions.
class ExtensionBookmarks {
 public:
  // Convert |node| into a JSON value
  static DictionaryValue* GetNodeDictionary(const BookmarkNode* node,
                                            bool recurse) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger(keys::kIdKey, node->id());

    const BookmarkNode* parent = node->GetParent();
    if (parent)
      dict->SetInteger(keys::kParentIdKey, parent->id());

    if (!node->is_folder())
      dict->SetString(keys::kUrlKey, node->GetURL().spec());

    dict->SetString(keys::kTitleKey, node->GetTitle());

    int childCount = node->GetChildCount();
    ListValue* children = new ListValue();
    for (int i = 0; i < childCount; ++i) {
      const BookmarkNode* child = node->GetChild(i);
      if (recurse) {
        DictionaryValue* dict = GetNodeDictionary(child, true);
        children->Append(dict);
      }
    }
    if (recurse)
      dict->Set(keys::kChildrenKey, children);
    return dict;
  }

  // Add a JSON representation of |node| to the JSON |list|.
  static void AddNode(const BookmarkNode* node, ListValue* list, bool recurse) {
    DictionaryValue* dict = GetNodeDictionary(node, recurse);
    list->Append(dict);
  }

  static bool RemoveNode(BookmarkModel* model, int id, bool recursive,
                         std::string* error) {
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node) {
      *error = keys::kNoNodeError;
      return false;
    }
    if (node == model->root_node() ||
        node == model->other_node() ||
        node == model->GetBookmarkBarNode()) {
      *error = keys::kModifySpecialError;
      return false;
    }
    if (node->is_folder() && node->GetChildCount() > 0 && !recursive) {
      *error = keys::kFolderNotEmptyError;
      return false;
    }

    const BookmarkNode* parent = node->GetParent();
    int index = parent->IndexOfChild(node);
    model->Remove(parent, index);
    return true;
  }

 private:
  ExtensionBookmarks();
};

void BookmarksFunction::Run() {
  // TODO(erikkay) temporary hack until adding an event listener can notify the
  // browser.
  BookmarkModel* model = profile()->GetBookmarkModel();
  if (!model->IsLoaded()) {
    // Bookmarks are not ready yet.  We'll wait.
    registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                   NotificationService::AllSources());
    AddRef();  // balanced in Observe()
    return;
  }

  ExtensionBookmarkEventRouter* event_router =
      ExtensionBookmarkEventRouter::GetSingleton();
  event_router->Observe(model);
  SendResponse(RunImpl());
}

void BookmarksFunction::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  DCHECK(type == NotificationType::BOOKMARK_MODEL_LOADED);
  DCHECK(profile()->GetBookmarkModel()->IsLoaded());
  Run();
  Release();  // balanced in Run()
}

// static
ExtensionBookmarkEventRouter* ExtensionBookmarkEventRouter::GetSingleton() {
  return Singleton<ExtensionBookmarkEventRouter>::get();
}

ExtensionBookmarkEventRouter::ExtensionBookmarkEventRouter() {
}

ExtensionBookmarkEventRouter::~ExtensionBookmarkEventRouter() {
}

void ExtensionBookmarkEventRouter::Observe(BookmarkModel* model) {
  if (models_.find(model) == models_.end()) {
    model->AddObserver(this);
    models_.insert(model);
  }
}

void ExtensionBookmarkEventRouter::DispatchEvent(Profile *profile,
                                                 const char* event_name,
                                                 const std::string json_args) {
  ExtensionMessageService::GetInstance(profile->GetRequestContext())->
      DispatchEventToRenderers(event_name, json_args);
}

void ExtensionBookmarkEventRouter::Loaded(BookmarkModel* model) {
  // TODO(erikkay): Perhaps we should send this event down to the extension
  // so they know when it's safe to use the API?
}

void ExtensionBookmarkEventRouter::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  ListValue args;
  const BookmarkNode* node = new_parent->GetChild(new_index);
  args.Append(new FundamentalValue(node->id()));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetInteger(keys::kParentIdKey, new_parent->id());
  object_args->SetInteger(keys::kIndexKey, new_index);
  object_args->SetInteger(keys::kOldParentIdKey, old_parent->id());
  object_args->SetInteger(keys::kOldIndexKey, old_index);
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkMoved, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeAdded(BookmarkModel* model,
                                                     const BookmarkNode* parent,
                                                     int index) {
  ListValue args;
  const BookmarkNode* node = parent->GetChild(index);
  args.Append(new FundamentalValue(node->id()));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kTitleKey, node->GetTitle());
  object_args->SetString(keys::kUrlKey, node->GetURL().spec());
  object_args->SetInteger(keys::kParentIdKey, parent->id());
  object_args->SetInteger(keys::kIndexKey, index);
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkAdded, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int index) {
  ListValue args;
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetInteger(keys::kParentIdKey, parent->id());
  object_args->SetInteger(keys::kIndexKey, index);
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkRemoved, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeChanged(
    BookmarkModel* model, const BookmarkNode* node) {
  ListValue args;
  args.Append(new FundamentalValue(node->id()));

  // TODO(erikkay) The only two things that BookmarkModel sends this
  // notification for are title and favicon.  Since we're currently ignoring
  // favicon and since the notification doesn't say which one anyway, for now
  // we only include title.  The ideal thing would be to change BookmarkModel
  // to indicate what changed.
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kTitleKey, node->GetTitle());
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkChanged, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  // TODO(erikkay) anything we should do here?
}

void ExtensionBookmarkEventRouter::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  ListValue args;
  args.Append(new FundamentalValue(node->id()));
  int childCount = node->GetChildCount();
  ListValue* children = new ListValue();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    Value* child_id = new FundamentalValue(child->id());
    children->Append(child_id);
  }
  args.Append(children);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(),
                keys::kOnBookmarkChildrenReordered,
                json_args);
}

bool GetBookmarksFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  scoped_ptr<ListValue> json(new ListValue());
  if (args_->IsType(Value::TYPE_LIST)) {
    ListValue* ids = static_cast<ListValue*>(args_);
    size_t count = ids->GetSize();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      int id = 0;
      EXTENSION_FUNCTION_VALIDATE(ids->GetInteger(i, &id));
      const BookmarkNode* node = model->GetNodeByID(id);
      if (!node) {
        error_ = keys::kNoNodeError;
        return false;
      } else {
        ExtensionBookmarks::AddNode(node, json.get(), false);
      }
    }
  } else {
    int id;
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&id));
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node) {
      error_ = keys::kNoNodeError;
      return false;
    }
    ExtensionBookmarks::AddNode(node, json.get(), false);
  }

  result_.reset(json.release());
  return true;
}

bool GetBookmarkChildrenFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  int id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&id));
  scoped_ptr<ListValue> json(new ListValue());
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  int child_count = node->GetChildCount();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    ExtensionBookmarks::AddNode(child, json.get(), false);
  }

  result_.reset(json.release());
  return true;
}

bool GetBookmarkTreeFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  scoped_ptr<ListValue> json(new ListValue());
  const BookmarkNode* node = model->root_node();
  ExtensionBookmarks::AddNode(node, json.get(), true);
  result_.reset(json.release());
  return true;
}

bool SearchBookmarksFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_STRING));

  std::wstring query;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&query));

  BookmarkModel* model = profile()->GetBookmarkModel();
  ListValue* json = new ListValue();
  std::wstring lang = profile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(model, query, 50, lang, &nodes);
  std::vector<const BookmarkNode*>::iterator i = nodes.begin();
  for (; i != nodes.end(); ++i) {
    const BookmarkNode* node = *i;
    ExtensionBookmarks::AddNode(node, json, false);
  }

  result_.reset(json);
  return true;
}

bool RemoveBookmarkFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  bool recursive = false;
  EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(1, &recursive));

  BookmarkModel* model = profile()->GetBookmarkModel();
  int id;
  if (args->GetInteger(0, &id)) {
    return ExtensionBookmarks::RemoveNode(model, id, recursive, &error_);
  } else {
    ListValue* ids;
    EXTENSION_FUNCTION_VALIDATE(args->GetList(0, &ids));
    size_t count = ids->GetSize();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      EXTENSION_FUNCTION_VALIDATE(ids->GetInteger(i, &id));
      if (!ExtensionBookmarks::RemoveNode(model, id, recursive, &error_))
        return false;
    }
    return true;
  }
}

bool CreateBookmarkFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* json = static_cast<DictionaryValue*>(args_);

  BookmarkModel* model = profile()->GetBookmarkModel();
  int parentId;
  if (!json->HasKey(keys::kParentIdKey)) {
    // optional, default to "other bookmarks"
    parentId = model->other_node()->id();
  } else {
    EXTENSION_FUNCTION_VALIDATE(json->GetInteger(keys::kParentIdKey,
                                                 &parentId));
  }
  const BookmarkNode* parent = model->GetNodeByID(parentId);
  if (!parent) {
    error_ = keys::kNoParentError;
    return false;
  }
  if (parent->GetParent() == NULL) {  // can't create children of the root
    error_ = keys::kNoParentError;
    return false;
  }

  int index;
  if (!json->HasKey(keys::kIndexKey)) {  // optional (defaults to end)
    index = parent->GetChildCount();
  } else {
    EXTENSION_FUNCTION_VALIDATE(json->GetInteger(keys::kIndexKey, &index));
    if (index > parent->GetChildCount() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  }

  std::wstring title;
  json->GetString(keys::kTitleKey, &title);  // optional
  std::string url_string;
  json->GetString(keys::kUrlKey, &url_string);  // optional
  GURL url(url_string);
  if (!url.is_empty() && !url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return false;
  }

  const BookmarkNode* node;
  if (url_string.length())
    node = model->AddURL(parent, index, title, url);
  else
    node = model->AddGroup(parent, index, title);
  DCHECK(node);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }

  DictionaryValue* ret = ExtensionBookmarks::GetNodeDictionary(node, false);
  result_.reset(ret);

  return true;
}

bool MoveBookmarkFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  int id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &id));
  DictionaryValue* destination;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &destination));

  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (node == model->root_node() ||
      node == model->other_node() ||
      node == model->GetBookmarkBarNode()) {
    error_ = keys::kModifySpecialError;
    return false;
  }

  const BookmarkNode* parent;
  if (!destination->HasKey(keys::kParentIdKey)) {
    // optional, defaults to current parent
    parent = node->GetParent();
  } else {
    int parentId;
    EXTENSION_FUNCTION_VALIDATE(destination->GetInteger(keys::kParentIdKey,
                                                        &parentId));
    parent = model->GetNodeByID(parentId);
  }
  if (!parent) {
    error_ = keys::kNoParentError;
    // TODO(erikkay) return an error message
    return false;
  }
  if (parent == model->root_node()) {
    error_ = keys::kModifySpecialError;
    return false;
  }

  int index;
  if (destination->HasKey(keys::kIndexKey)) {  // optional (defaults to end)
    EXTENSION_FUNCTION_VALIDATE(destination->GetInteger(keys::kIndexKey,
                                                        &index));
    if (index > parent->GetChildCount() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  } else {
    index = parent->GetChildCount();
  }

  model->Move(node, parent, index);
  return true;
}

bool SetBookmarkTitleFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* json = static_cast<DictionaryValue*>(args_);

  std::wstring title;
  json->GetString(keys::kTitleKey, &title);  // optional (empty is clear)

  BookmarkModel* model = profile()->GetBookmarkModel();
  int id = 0;
  EXTENSION_FUNCTION_VALIDATE(json->GetInteger(keys::kIdKey, &id));
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (node == model->root_node() ||
      node == model->other_node() ||
      node == model->GetBookmarkBarNode()) {
    error_ = keys::kModifySpecialError;
    return false;
  }
  model->SetTitle(node, title);
  return true;
}
