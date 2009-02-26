// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__
#define CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__

#include "base/gfx/size.h"
#include "chrome/browser/dom_ui/dom_ui_host.h"

// Implement this class to receive notifications.
class HtmlDialogContentsDelegate {
 public:
   // Returns true if the contents needs to be run in a modal dialog.
   virtual bool IsDialogModal() const = 0;
   // Returns the title of the dialog.
   virtual std::wstring GetDialogTitle() const = 0;
   // Get the HTML file path for the content to load in the dialog.
   virtual GURL GetDialogContentURL() const = 0;
   // Get the size of the dialog.
   virtual void GetDialogSize(gfx::Size* size) const = 0;
   // Gets the JSON string input to use when showing the dialog.
   virtual std::string GetDialogArgs() const = 0;
   // A callback to notify the delegate that the dialog closed.
   virtual void OnDialogClosed(const std::string& json_retval) = 0;

  protected:
   ~HtmlDialogContentsDelegate() {}
};

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogContents is a simple wrapper around DOMUIHost and is used to
// display file URL contents inside a modal HTML dialog.
//
////////////////////////////////////////////////////////////////////////////////
class HtmlDialogContents : public DOMUIHost {
 public:
  struct HtmlDialogParams {
    // The URL for the content that will be loaded in the dialog.
    GURL url;
    // Width of the dialog.
    int width;
    // Height of the dialog.
    int height;
    // The JSON input to pass to the dialog when showing it.
    std::string json_input;
  };

  HtmlDialogContents(Profile* profile,
                     SiteInstance* instance,
                     RenderViewHostFactory* rvf);
  virtual ~HtmlDialogContents();

  // Initialize the HtmlDialogContents with the given delegate.  Must be called
  // after the RenderViewHost is created.
  void Init(HtmlDialogContentsDelegate* d);

  // Overridden from DOMUIHost:
  virtual void AttachMessageHandlers();

  // Returns true of this URL should be handled by the HtmlDialogContents.
  static bool IsHtmlDialogUrl(const GURL& url);

 private:
  // JS message handlers:
  void OnDialogClosed(const Value* content);

  // The delegate that knows how to display the dialog and receives the response
  // back from the dialog.
  HtmlDialogContentsDelegate* delegate_;

  DISALLOW_EVIL_CONSTRUCTORS(HtmlDialogContents);
};

#endif  // CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__

