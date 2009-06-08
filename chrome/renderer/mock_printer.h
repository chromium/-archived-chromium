// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_PRINTER_H_
#define CHROME_RENDERER_MOCK_PRINTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

#if defined(OS_WIN)
#include "chrome/renderer/mock_printer_driver_win.h"
#endif

struct ViewMsg_Print_Params;
struct ViewMsg_PrintPages_Params;
struct ViewHostMsg_DidPrintPage_Params;

// A class which represents an output page used in the MockPrinter class.
// The MockPrinter class stores output pages in a vector, so, this class
// inherits the base::RefCounted<> class so that the MockPrinter class can use
// a smart pointer of this object (i.e. scoped_refptr<>).
class MockPrinterPage : public base::RefCounted<MockPrinterPage> {
 public:
  MockPrinterPage()
      : width_(0),
        height_(0),
        source_size_(0),
        bitmap_size_(0) {
  }

  MockPrinterPage(int width,
                  int height,
                  const void* source_data,
                  size_t source_size,
                  const void* bitmap_data,
                  size_t bitmap_size)
      : width_(width),
        height_(height),
        source_size_(source_size),
        bitmap_size_(bitmap_size) {
    // Create copies of the source data and the bitmap data.
    source_data_.reset(new uint8[source_size]);
    if (source_data_.get())
      memcpy(source_data_.get(), source_data, source_size);
    bitmap_data_.reset(new uint8[bitmap_size]);
    if (bitmap_data_.get())
      memcpy(bitmap_data_.get(), bitmap_data, bitmap_size);
  }

  ~MockPrinterPage() {
  }

  int width() { return width_; }
  int height() { return height_; }
  const uint8* source_data() { return source_data_.get(); }
  const size_t source_size() { return source_size_; }
  const uint8* bitmap_data() { return bitmap_data_.get(); }
  const size_t bitmap_size() { return bitmap_size_; }

 private:
  int width_;
  int height_;
  size_t source_size_;
  scoped_array<uint8> source_data_;
  size_t bitmap_size_;
  scoped_array<uint8> bitmap_data_;

  DISALLOW_COPY_AND_ASSIGN(MockPrinterPage);
};

// A class which implements a pseudo-printer object used by the RenderViewTest
// class.
// This class consists of three parts:
// 1. An IPC-message hanlder sent from the RenderView class;
// 2. A renderer that creates a printing job into bitmaps, and;
// 3. A vector which saves the output pages of a printing job.
// A user who writes RenderViewTest cases only use the functions which
// retrieve output pages from this vector to verify them with expected results.
class MockPrinter {
 public:
  enum Status {
    PRINTER_READY,
    PRINTER_PRINTING,
    PRINTER_ERROR,
  };

  MockPrinter();
  ~MockPrinter();

  // Functions that changes settings of a pseudo printer.
  void ResetPrinter();
  void SetDefaultPrintSettings(const ViewMsg_Print_Params& params);

  // Functions that handles IPC events.
  void GetDefaultPrintSettings(ViewMsg_Print_Params* params);
  void ScriptedPrint(int cookie,
                     int expected_pages_count,
                     bool has_selection,
                     ViewMsg_PrintPages_Params* settings);
  void SetPrintedPagesCount(int cookie, int number_pages);
  void PrintPage(const ViewHostMsg_DidPrintPage_Params& params);

  // Functions that retrieve the output pages.
  Status GetPrinterStatus() const { return printer_status_; }
  int GetPrintedPages() const;
  int GetWidth(size_t page) const;
  int GetHeight(size_t page) const;
  bool GetSourceChecksum(size_t page, std::string* checksum) const;
  bool GetBitmapChecksum(size_t page, std::string* checksum) const;
  bool GetSource(size_t page, const void** data, size_t* size) const;
  bool GetBitmap(size_t page, const void** data, size_t* size) const;
  bool SaveSource(size_t page, const std::wstring& filename) const;
  bool SaveBitmap(size_t page, const std::wstring& filename) const;

 protected:
  int CreateDocumentCookie();
  bool GetChecksum(const void* data, size_t size, std::string* checksum) const;

 private:
  // In pixels according to dpi_x and dpi_y.
  int printable_width_;
  int printable_height_;

  // Specifies dots per inch.
  double dpi_;
  double max_shrink_;
  double min_shrink_;

  // Desired apparent dpi on paper.
  int desired_dpi_;

  // Print selection.
  bool selection_only_;

  // Cookie for the document to ensure correctness.
  int document_cookie_;
  int current_document_cookie_;

  // The current status of this printer.
  Status printer_status_;

  // The output of a printing job.
  int number_pages_;
  int page_number_;
  std::vector<scoped_refptr<MockPrinterPage> > pages_;

#if defined(OS_WIN)
  MockPrinterDriverWin driver_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MockPrinter);
};

#endif  // CHROME_RENDERER_MOCK_PRINTER_H_
