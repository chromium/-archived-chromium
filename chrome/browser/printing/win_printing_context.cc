// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/win_printing_context.h"

#include <winspool.h>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/time_format.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/printed_document.h"
#include "skia/ext/platform_device_win.h"

using base::Time;

namespace {

// Retrieves the content of a GetPrinter call.
void GetPrinterHelper(HANDLE printer, int level, scoped_array<uint8>* buffer) {
  DWORD buf_size = 0;
  GetPrinter(printer, level, NULL, 0, &buf_size);
  if (buf_size) {
    buffer->reset(new uint8[buf_size]);
    memset(buffer->get(), 0, buf_size);
    if (!GetPrinter(printer, level, buffer->get(), buf_size, &buf_size)) {
      buffer->reset();
    }
  }
}

}  // namespace

namespace printing {

class PrintingContext::CallbackHandler
    : public IPrintDialogCallback,
      public IObjectWithSite {
 public:
  CallbackHandler(PrintingContext& owner, HWND owner_hwnd)
      : owner_(owner),
        owner_hwnd_(owner_hwnd),
        services_(NULL) {
  }

  ~CallbackHandler() {
    if (services_)
      services_->Release();
  }

  IUnknown* ToIUnknown() {
    return static_cast<IUnknown*>(static_cast<IPrintDialogCallback*>(this));
  }

  // IUnknown
  virtual HRESULT WINAPI QueryInterface(REFIID riid, void**object) {
    if (riid == IID_IUnknown) {
      *object = ToIUnknown();
    } else if (riid == IID_IPrintDialogCallback) {
      *object = static_cast<IPrintDialogCallback*>(this);
    } else if (riid == IID_IObjectWithSite) {
      *object = static_cast<IObjectWithSite*>(this);
    } else {
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  // No real ref counting.
  virtual ULONG WINAPI AddRef() {
    return 1;
  }
  virtual ULONG WINAPI Release() {
    return 1;
  }

  // IPrintDialogCallback methods
  virtual HRESULT WINAPI InitDone() {
    return S_OK;
  }

  virtual HRESULT WINAPI SelectionChange() {
    if (services_) {
      // TODO(maruel): Get the devmode for the new printer with
      // services_->GetCurrentDevMode(&devmode, &size), send that information
      // back to our client and continue. The client needs to recalculate the
      // number of rendered pages and send back this information here.
    }
    return S_OK;
  }

  virtual HRESULT WINAPI HandleMessage(HWND dialog,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       LRESULT* result) {
    // Cheap way to retrieve the window handle.
    if (!owner_.dialog_box_) {
      // The handle we receive is the one of the groupbox in the General tab. We
      // need to get the grand-father to get the dialog box handle.
      owner_.dialog_box_ = GetAncestor(dialog, GA_ROOT);
      // Trick to enable the owner window. This can cause issues with navigation
      // events so it may have to be disabled if we don't fix the side-effects.
      EnableWindow(owner_hwnd_, TRUE);
    }
    return S_FALSE;
  }

  virtual HRESULT WINAPI SetSite(IUnknown* site) {
    if (!site) {
      DCHECK(services_);
      services_->Release();
      services_ = NULL;
      // The dialog box is destroying, PrintJob::Worker don't need the handle
      // anymore.
      owner_.dialog_box_ = NULL;
    } else {
      DCHECK(services_ == NULL);
      HRESULT hr = site->QueryInterface(IID_IPrintDialogServices,
                                        reinterpret_cast<void**>(&services_));
      DCHECK(SUCCEEDED(hr));
    }
    return S_OK;
  }

  virtual HRESULT WINAPI GetSite(REFIID riid, void** site) {
    return E_NOTIMPL;
  }

 private:
  PrintingContext& owner_;
  HWND owner_hwnd_;
  IPrintDialogServices* services_;

  DISALLOW_EVIL_CONSTRUCTORS(CallbackHandler);
};

PrintingContext::PrintingContext()
    : hdc_(NULL),
#ifndef NDEBUG
      page_number_(-1),
#endif
      dialog_box_(NULL),
      dialog_box_dismissed_(false),
      abort_printing_(false),
      in_print_job_(false) {
}

PrintingContext::~PrintingContext() {
  ResetSettings();
}

PrintingContext::Result PrintingContext::AskUserForSettings(
    HWND window,
    int max_pages,
    bool has_selection) {
  DCHECK(window);
  DCHECK(!in_print_job_);
  dialog_box_dismissed_ = false;
  // Show the OS-dependent dialog box.
  // If the user press
  // - OK, the settings are reset and reinitialized with the new settings. OK is
  //   returned.
  // - Apply then Cancel, the settings are reset and reinitialized with the new
  //   settings. CANCEL is returned.
  // - Cancel, the settings are not changed, the previous setting, if it was
  //   initialized before, are kept. CANCEL is returned.
  // On failure, the settings are reset and FAILED is returned.
  PRINTDLGEX dialog_options = { sizeof(PRINTDLGEX) };
  dialog_options.hwndOwner = window;
  // Disable options we don't support currently.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE  |
                         PD_NOCURRENTPAGE | PD_HIDEPRINTTOFILE;
  if (!has_selection)
    dialog_options.Flags |= PD_NOSELECTION;

  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = arraysize(ranges);
    dialog_options.nMinPage = 1;
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  {
    if (PrintDlgEx(&dialog_options) != S_OK) {
      ResetSettings();
      return FAILED;
    }
  }
  // TODO(maruel):  Support PD_PRINTTOFILE.
  return ParseDialogResultEx(dialog_options);
}

PrintingContext::Result PrintingContext::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  PRINTDLG dialog_options = { sizeof(PRINTDLG) };
  dialog_options.Flags = PD_RETURNDC | PD_RETURNDEFAULT;
  if (PrintDlg(&dialog_options) == 0) {
    ResetSettings();
    return FAILED;
  }
  return ParseDialogResult(dialog_options);
}

PrintingContext::Result PrintingContext::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);
  settings_ = settings;
  // TODO(maruel): settings_->ToDEVMODE()
  HANDLE printer;
  if (!OpenPrinter(const_cast<wchar_t*>(settings_.device_name().c_str()),
                   &printer,
                   NULL))
    return FAILED;

  Result status = OK;

  if (!GetPrinterSettings(printer, settings_.device_name()))
    status = FAILED;

  // Close the printer after retrieving the context.
  ClosePrinter(printer);

  if (status != OK)
    ResetSettings();
  return status;
}

void PrintingContext::ResetSettings() {
  if (hdc_ != NULL) {
    DeleteDC(hdc_);
    hdc_ = NULL;
  }
  settings_.Clear();
  in_print_job_ = false;

#ifndef NDEBUG
  page_number_ = -1;
#endif
}

PrintingContext::Result PrintingContext::NewDocument(
    const std::wstring& document_name) {
  DCHECK(!in_print_job_);
  if (!hdc_)
    return OnError();

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(hdc_, &AbortProc))
    return OnError();

  DOCINFO di = { sizeof(DOCINFO) };
  di.lpszDocName = document_name.c_str();

  // Is there a debug dump directory specified? If so, force to print to a file.
  std::wstring debug_dump_path = PrintedDocument::debug_dump_path();
  if (!debug_dump_path.empty()) {
    // Create a filename.
    std::wstring filename;
    Time now(Time::Now());
    filename = base::TimeFormatShortDateNumeric(now);
    filename += L"_";
    filename += base::TimeFormatTimeOfDay(now);
    filename += L"_";
    filename += document_name;
    filename += L"_";
    filename += L"buffer.prn";
    file_util::ReplaceIllegalCharacters(&filename, '_');
    file_util::AppendToPath(&debug_dump_path, filename);
    di.lpszOutput = debug_dump_path.c_str();
  }

  DCHECK_EQ(MessageLoop::current()->NestableTasksAllowed(), false);
  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(hdc_, &di) <= 0)
    return OnError();

#ifndef NDEBUG
  page_number_ = 0;
#endif
  return OK;
}

PrintingContext::Result PrintingContext::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Inform the driver that the application is about to begin sending data.
  if (StartPage(hdc_) <= 0)
    return OnError();

#ifndef NDEBUG
  ++page_number_;
#endif

  return OK;
}

PrintingContext::Result PrintingContext::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  if (EndPage(hdc_) <= 0)
    return OnError();
  return OK;
}

PrintingContext::Result PrintingContext::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Inform the driver that document has ended.
  if (EndDoc(hdc_) <= 0)
    return OnError();

  ResetSettings();
  return OK;
}

void PrintingContext::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  if (hdc_)
    CancelDC(hdc_);
  DismissDialog();
}

void PrintingContext::DismissDialog() {
  if (dialog_box_) {
    DestroyWindow(dialog_box_);
    dialog_box_dismissed_ = true;
  }
}

PrintingContext::Result PrintingContext::OnError() {
  // This will close hdc_ and clear settings_.
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

// static
BOOL PrintingContext::AbortProc(HDC hdc, int nCode) {
  if (nCode) {
    // TODO(maruel):  Need a way to find the right instance to set. Should
    // leverage PrintJobManager here?
    // abort_printing_ = true;
  }
  return true;
}

bool PrintingContext::InitializeSettings(const DEVMODE& dev_mode,
                                         const std::wstring& new_device_name,
                                         const PRINTPAGERANGE* ranges,
                                         int number_ranges,
                                         bool selection_only) {
  skia::PlatformDevice::InitializeDC(hdc_);
  DCHECK(GetDeviceCaps(hdc_, CLIPCAPS));
  DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_STRETCHDIB);
  DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_BITMAP64);
  // Some printers don't advertise these.
  // DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_SCALING);
  // DCHECK(GetDeviceCaps(hdc_, SHADEBLENDCAPS) & SB_CONST_ALPHA);
  // DCHECK(GetDeviceCaps(hdc_, SHADEBLENDCAPS) & SB_PIXEL_ALPHA);

  // StretchDIBits() support is needed for printing.
  if (!(GetDeviceCaps(hdc_, RASTERCAPS) & RC_STRETCHDIB) ||
      !(GetDeviceCaps(hdc_, RASTERCAPS) & RC_BITMAP64)) {
    NOTREACHED();
    ResetSettings();
    return false;
  }

  DCHECK(!in_print_job_);
  DCHECK(hdc_);
  PageRanges ranges_vector;
  if (!selection_only) {
    // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
    ranges_vector.reserve(number_ranges);
    for (int i = 0; i < number_ranges; ++i) {
      PageRange range;
      // Transfer from 1-based to 0-based.
      range.from = ranges[i].nFromPage - 1;
      range.to = ranges[i].nToPage - 1;
      ranges_vector.push_back(range);
    }
  }
  settings_.Init(hdc_,
                 dev_mode,
                 ranges_vector,
                 new_device_name,
                 selection_only);
  return true;
}

bool PrintingContext::GetPrinterSettings(HANDLE printer,
                                         const std::wstring& device_name) {
  DCHECK(!in_print_job_);
  scoped_array<uint8> buffer;

  // A PRINTER_INFO_9 structure specifying the per-user default printer
  // settings.
  GetPrinterHelper(printer, 9, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_9* info_9 = reinterpret_cast<PRINTER_INFO_9*>(buffer.get());
    if (info_9->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_9->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_9->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_8 structure specifying the global default printer settings.
  GetPrinterHelper(printer, 8, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_8* info_8 = reinterpret_cast<PRINTER_INFO_8*>(buffer.get());
    if (info_8->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_8->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_8->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_2 structure specifying the driver's default printer
  // settings.
  GetPrinterHelper(printer, 2, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer.get());
    if (info_2->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_2->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_2->pDevMode, device_name, NULL, 0, false);
    }
    buffer.reset();
  }
  // Failed to retrieve the printer settings.
  ResetSettings();
  return false;
}

bool PrintingContext::AllocateContext(const std::wstring& printer_name,
                                      const DEVMODE* dev_mode) {
  hdc_ = CreateDC(L"WINSPOOL", printer_name.c_str(), NULL, dev_mode);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

PrintingContext::Result PrintingContext::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
    // Start fresh.
    ResetSettings();

    DEVMODE* dev_mode = NULL;
    if (dialog_options.hDevMode) {
      dev_mode =
          reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
      DCHECK(dev_mode);
    }

    std::wstring device_name;
    if (dialog_options.hDevNames) {
      DEVNAMES* dev_names =
          reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
      DCHECK(dev_names);
      if (dev_names) {
        device_name =
            reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const wchar_t*>(dev_names) +
                    dev_names->wDeviceOffset);
        GlobalUnlock(dialog_options.hDevNames);
      }
    }

    bool success = false;
    if (dev_mode && !device_name.empty()) {
      hdc_ = dialog_options.hDC;
      PRINTPAGERANGE* page_ranges = NULL;
      DWORD num_page_ranges = 0;
      bool print_selection_only = false;
      if (dialog_options.Flags & PD_PAGENUMS) {
        page_ranges = dialog_options.lpPageRanges;
        num_page_ranges = dialog_options.nPageRanges;
      }
      if (dialog_options.Flags & PD_SELECTION) {
        print_selection_only = true;
      }
      success = InitializeSettings(*dev_mode,
                                   device_name,
                                   dialog_options.lpPageRanges,
                                   dialog_options.nPageRanges,
                                   print_selection_only);
    }

    if (!success && dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
      hdc_ = NULL;
    }

    if (dev_mode) {
      GlobalUnlock(dialog_options.hDevMode);
    }
  } else {
    if (dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
    }
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  switch (dialog_options.dwResultAction) {
    case PD_RESULT_PRINT:
      return hdc_ ? OK : FAILED;
    case PD_RESULT_APPLY:
      return hdc_ ? CANCEL : FAILED;
    case PD_RESULT_CANCEL:
      return CANCEL;
    default:
      return FAILED;
  }
}

PrintingContext::Result PrintingContext::ParseDialogResult(
    const PRINTDLG& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  // Start fresh.
  ResetSettings();

  DEVMODE* dev_mode = NULL;
  if (dialog_options.hDevMode) {
    dev_mode =
        reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
    DCHECK(dev_mode);
  }

  std::wstring device_name;
  if (dialog_options.hDevNames) {
    DEVNAMES* dev_names =
        reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
    DCHECK(dev_names);
    if (dev_names) {
      device_name =
          reinterpret_cast<const wchar_t*>(
              reinterpret_cast<const wchar_t*>(dev_names) +
                  dev_names->wDeviceOffset);
      GlobalUnlock(dialog_options.hDevNames);
    }
  }

  bool success = false;
  if (dev_mode && !device_name.empty()) {
    hdc_ = dialog_options.hDC;
    success = InitializeSettings(*dev_mode, device_name, NULL, 0, false);
  }

  if (!success && dialog_options.hDC) {
    DeleteDC(dialog_options.hDC);
    hdc_ = NULL;
  }

  if (dev_mode) {
    GlobalUnlock(dialog_options.hDevMode);
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  return hdc_ ? OK : FAILED;
}

}  // namespace printing
