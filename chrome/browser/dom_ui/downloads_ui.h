// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOWNLOADS_UI_H_
#define CHROME_BROWSER_DOM_UI_DOWNLOADS_UI_H_

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"
#include "chrome/browser/download/download_manager.h"

class GURL;

class DownloadsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  DownloadsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsUIHTMLSource);
};

// The handler for Javascript messages related to the "downloads" view,
// also observes changes to the download manager.
class DownloadsDOMHandler : public DOMMessageHandler,
                            public DownloadManager::Observer,
                            public DownloadItem::Observer {
 public:
  explicit DownloadsDOMHandler(DOMUI* dom_ui, DownloadManager* dlm);
  virtual ~DownloadsDOMHandler();

  void Init();

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download);

  // DownloadManager::Observer interface
  virtual void ModelChanged();
  virtual void SetDownloads(std::vector<DownloadItem*>& downloads);

  // Callback for the "getDownloads" message.
  void HandleGetDownloads(const Value* value);

  // Callback for the "openFile" message - opens the file in the shell.
  void HandleOpenFile(const Value* value);

  // Callback for the "drag" message - initiates a file object drag.
  void HandleDrag(const Value* value);

  // Callback for the "saveDangerous" message - specifies that the user
  // wishes to save a dangerous file.
  void HandleSaveDangerous(const Value* value);

  // Callback for the "discardDangerous" message - specifies that the user
  // wishes to discard (remove) a dangerous file.
  void HandleDiscardDangerous(const Value* value);

  // Callback for the "show" message - shows the file in explorer.
  void HandleShow(const Value* value);

  // Callback for the "pause" message - pauses the file download.
  void HandlePause(const Value* value);

  // Callback for the "cancel" message - cancels the download.
  void HandleCancel(const Value* value);

 private:
  // Send the current list of downloads to the page.
  void SendCurrentDownloads();

  // Creates a representation of a download in a format that the downloads
  // HTML page can understand.
  DictionaryValue* CreateDownloadItemValue(DownloadItem* download, int id);

  // Clear all download items and their observers.
  void ClearDownloadItems();

  // Return the download that corresponds to a given id.
  DownloadItem* GetDownloadById(int id);

  // Return the download that is referred to in a given value.
  DownloadItem* GetDownloadByValue(const Value* value);

  // Current search text.
  std::wstring search_text_;

  // Our model
  DownloadManager* download_manager_;

  // The current set of visible DownloadItems for this view received from the
  // DownloadManager. DownloadManager owns the DownloadItems. The vector is
  // kept in order, sorted by ascending start time.
  typedef std::vector<DownloadItem*> OrderedDownloads;
  OrderedDownloads download_items_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDOMHandler);
};

class DownloadsUI : public DOMUI {
 public:
  explicit DownloadsUI(DOMUIContents* contents);

  // Return the URL for the front page of this UI.
  static GURL GetBaseURL();

  // DOMUI Implementation
  virtual void Init();

 private:

  DISALLOW_COPY_AND_ASSIGN(DownloadsUI);
};

#endif  // CHROME_BROWSER_DOM_UI_DOWNLOADS_UI_H_

