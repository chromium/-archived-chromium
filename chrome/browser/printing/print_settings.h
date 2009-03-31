// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_SETTINGS_H__
#define CHROME_BROWSER_PRINTING_PRINT_SETTINGS_H__

#include "base/gfx/rect.h"
#include "chrome/browser/printing/page_overlays.h"
#include "chrome/browser/printing/page_range.h"
#include "chrome/browser/printing/page_setup.h"

struct ViewMsg_Print_Params;
typedef struct HDC__* HDC;
typedef struct _devicemodeW DEVMODE;

namespace printing {

// OS-independent print settings.
class PrintSettings {
 public:
  PrintSettings();

  // Reinitialize the settings to the default values.
  void Clear();

#ifdef WIN32
  // Reads the settings from the selected device context. Calculates derived
  // values like printable_area_.
  void Init(HDC hdc,
            const DEVMODE& dev_mode,
            const PageRanges& new_ranges,
            const std::wstring& new_device_name);
#endif

  // Set printer printable area in in pixels.
  void SetPrinterPrintableArea(gfx::Size const& physical_size_pixels,
                               gfx::Rect const& printable_area_pixels);

  // Initializes the print parameters that needs to be sent to the renderer
  // process.
  void RenderParams(ViewMsg_Print_Params* params) const;

  // Equality operator.
  // NOTE: printer_name is NOT tested for equality since it doesn't affect the
  // output.
  bool Equals(const PrintSettings& rhs) const;

  const std::wstring& printer_name() const { return printer_name_; }
  void set_device_name(const std::wstring& device_name) {
    device_name_ = device_name;
  }
  const std::wstring& device_name() const { return device_name_; }
  int dpi() const { return dpi_; }
  const PageSetup& page_setup_pixels() const { return page_setup_pixels_; }

  // Multi-page printing. Each PageRange describes a from-to page combination.
  // This permits printing selected pages only.
  PageRanges ranges;

  // By imaging to a width a little wider than the available pixels, thin pages
  // will be scaled down a little, matching the way they print in IE and Camino.
  // This lets them use fewer sheets than they would otherwise, which is
  // presumably why other browsers do this. Wide pages will be scaled down more
  // than this.
  double min_shrink;

  // This number determines how small we are willing to reduce the page content
  // in order to accommodate the widest line. If the page would have to be
  // reduced smaller to make the widest line fit, we just clip instead (this
  // behavior matches MacIE and Mozilla, at least)
  double max_shrink;

  // Desired visible dots per inch rendering for output. Printing should be
  // scaled to ScreenDpi/dpix*desired_dpi.
  int desired_dpi;

  // The various overlays (headers and footers).
  PageOverlays overlays;

  // Cookie generator. It is used to initialize PrintedDocument with its
  // associated PrintSettings, to be sure that each generated PrintedPage is
  // correctly associated with its corresponding PrintedDocument.
  static int NewCookie();

 private:
  //////////////////////////////////////////////////////////////////////////////
  // Settings that can't be changed without side-effects.

  // Printer name as shown to the user.
  std::wstring printer_name_;

  // Printer device name as opened by the OS.
  std::wstring device_name_;

  // Page setup in pixel units, dpi adjusted.
  PageSetup page_setup_pixels_;

  // Printer's device effective dots per inch in both axis.
  int dpi_;

  // Is the orientation landscape or portrait.
  bool landscape_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_SETTINGS_H__
