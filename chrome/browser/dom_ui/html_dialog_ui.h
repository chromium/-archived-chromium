// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_HTML_DIALOG_UI_H_
#define CHROME_BROWSER_DOM_UI_HTML_DIALOG_UI_H_

#include "base/gfx/size.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/common/property_bag.h"
#include "googleurl/src/gurl.h"

// Implement this class to receive notifications.
class HtmlDialogUIDelegate {
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
   ~HtmlDialogUIDelegate() {}
};

// Displays file URL contents inside a modal HTML dialog.
//
// This application really should not use WebContents + DOMUI. It should instead
// just embed a RenderView in a dialog and be done with it.
//
// Before loading a URL corresponding to this DOMUI, the caller should set its
// delegate as a property on the WebContents. This DOMUI will pick it up from
// there and call it back. This is a bit of a hack to allow the dialog to pass
// its delegate to the DOM UI without having nasty accessors on the WebContents.
// The correct design using RVH directly would avoid all of this.
class HtmlDialogUI : public DOMUI {
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

  // When created, the property should already be set on the WebContents.
  HtmlDialogUI(WebContents* web_contents);
  virtual ~HtmlDialogUI();

  // Returns the PropertyBag accessor object used to write the delegate pointer
  // into the WebContents (see class-level comment above).
  static PropertyAccessor<HtmlDialogUIDelegate*>& GetPropertyAccessor();

 private:
  // DOMUI overrides.
  virtual void RenderViewCreated(RenderViewHost* render_view_host);

  // JS message handler.
  void OnDialogClosed(const Value* content);

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogUI);
};

#endif  // CHROME_BROWSER_DOM_UI_HTML_DIALOG_UI_H_
