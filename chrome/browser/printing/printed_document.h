// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTED_DOCUMENT_H__
#define CHROME_BROWSER_PRINTING_PRINTED_DOCUMENT_H__

#include <map>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/printing/print_settings.h"
#include "googleurl/src/gurl.h"

class ChromeFont;
class MessageLoop;

namespace gfx {
class Emf;
}

namespace printing {

class PrintedPage;
class PrintedPagesSource;

// A collection of rendered pages. The settings are immuable. If the print
// settings are changed, a new PrintedDocument must be created.
// Warning: May be accessed from many threads at the same time. Only one thread
// will have write access. Sensible functions are protected by a lock.
// Warning: Once a page is loaded, it cannot be replaced. Pages may be discarded
// under low memory conditions.
class PrintedDocument : public base::RefCountedThreadSafe<PrintedDocument> {
 public:
  // The cookie shall be unique and has a specific relationship with its
  // originating source and settings.
  PrintedDocument(const PrintSettings& settings,
                  PrintedPagesSource* source,
                  int cookie);
  ~PrintedDocument();

  // Sets a page's data. 0-based. Takes emf ownership.
  // Note: locks for a short amount of time.
  void SetPage(int page_number, gfx::Emf* emf, double shrink);

  // Retrieves a page. If the page is not available right now, it
  // requests to have this page be rendered and returns false.
  // Note: locks for a short amount of time.
  bool GetPage(int page_number, scoped_refptr<PrintedPage>* page);

  // Draws the page in the context.
  // Note: locks for a short amount of time in debug only.
  void RenderPrintedPage(const PrintedPage& page, HDC context) const;

  // Draws the page in the context. If the page is not available right now, it
  // requests to have this page be rendered and returns false.
  // Note: locks for a short amount of time.
  bool RenderPrintedPageNumber(int page_number, HDC context);

  // Returns true if all the necessary pages for the settings are already
  // rendered.
  // Note: locks while parsing the whole tree.
  bool IsComplete() const;

  // Requests all the missing pages. Returns true if at least one page has been
  // requested. Returns false if there was not enough information to request the
  // missing pages, i.e. document_page_count_ is not initialized or no page has
  // been requested.
  // Note: locks while parsing the whole tree.
  bool RequestMissingPages();

  // Disconnects the PrintedPage source (PrintedPagesSource). It is done when
  // the source is being destroyed.
  void DisconnectSource();

  // Retrieves the current memory usage of the renderer pages.
  // Note: locks for a short amount of time.
  size_t MemoryUsage() const;

  // Sets the number of pages in the document to be rendered. Can only be set
  // once.
  // Note: locks for a short amount of time.
  void set_page_count(int max_page);

  // Number of pages in the document. Used for headers/footers.
  // Note: locks for a short amount of time.
  int page_count() const;

  // Returns the number of expected pages to be rendered. It is a non-linear
  // series if settings().ranges is not empty. It is the same value as
  // document_page_count() otherwise.
  // Note: locks for a short amount of time.
  int expected_page_count() const;

  // Getters. All these items are immuable hence thread-safe.
  const PrintSettings& settings() const { return immutable_.settings_; }
  const std::wstring& name() const {
    return immutable_.name_;
  }
  const GURL& url() const { return immutable_.url_; }
  const std::wstring& date() const { return immutable_.date_; }
  const std::wstring& time() const { return immutable_.time_; }
  const int cookie() const { return immutable_.cookie_; }

 private:
  // Array of EMF data for each print previewed page.
  typedef std::map<int, scoped_refptr<PrintedPage>> PrintedPages;

  // Contains all the mutable stuff. All this stuff MUST be accessed with the
  // lock held.
  struct Mutable {
    Mutable(PrintedPagesSource* source);

    // Source that generates the PrintedPage's (i.e. a WebContents). It will be
    // set back to NULL if the source is deleted before this object.
    PrintedPagesSource* source_;

    // Contains the pages' representation. This is a collection of PrintedPage.
    // Warning: Lock must be held when accessing this member.
    PrintedPages pages_;

    // Number of expected pages to be rendered.
    // Warning: Lock must be held when accessing this member.
    int expected_page_count_;

    // The total number of pages in the document.
    int page_count_;

    // Shrink done in comparison to desired_dpi.
    double shrink_factor;
  };

  // Contains all the immutable stuff. All this stuff can be accessed without
  // any lock held. This is because it can't be changed after the object's
  // construction.
  struct Immutable {
    Immutable(const PrintSettings& settings, PrintedPagesSource* source,
              int cookie);

    // Print settings used to generate this document. Immuable.
    PrintSettings settings_;

    // Native thread for the render source.
    MessageLoop* source_message_loop_;

    // Document name. Immuable.
    std::wstring name_;

    // URL that generated this document. Immuable.
    GURL url_;

    // The date on which this job started. Immuable.
    std::wstring date_;

    // The time at which this job started. Immuable.
    std::wstring time_;

    // Cookie to uniquely identify this document. It is used to make sure that a
    // PrintedPage is correctly belonging to the PrintedDocument. Since
    // PrintedPage generation is completely asynchronous, it could be easy to
    // mess up and send the page to the wrong document. It can be viewed as a
    // simpler hash of PrintSettings since a new document is made each time the
    // print settings change.
    int cookie_;
  };

  // Prints the headers and footers for one page in the specified context
  // according to the current settings.
  void PrintHeaderFooter(HDC context,
                         const PrintedPage& page,
                         PageOverlays::HorizontalPosition x,
                         PageOverlays::VerticalPosition y,
                         const ChromeFont& font) const;

  // Calls the render source to render a page. Makes sure to execute the call in
  // the right thread context.
  void PrintPage_ThreadJump(int page_number);

  // All writable data member access must be guarded by this lock. Needs to be
  // mutable since it can be acquired from const member functions.
  mutable Lock lock_;

  // All the mutable members.
  Mutable mutable_;

  // All the immutable members.
  const Immutable immutable_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintedDocument);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTED_DOCUMENT_H__
