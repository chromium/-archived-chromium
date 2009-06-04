// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/gfx/size.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/units.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/weburlrequest.h"

#if defined(OS_WIN)
#include "chrome/common/gfx/emf.h"
#include "skia/ext/vector_canvas.h"
#endif


void PrintWebViewHelper::SyncPrint(WebFrame* frame) {
#if defined(OS_WIN)
  // Retrieve the default print settings to calculate the expected number of
  // pages.
  ViewMsg_Print_Params default_settings;

  IPC::SyncMessage* msg =
      new ViewHostMsg_GetDefaultPrintSettings(routing_id(), &default_settings);
  if (Send(msg)) {
    msg = NULL;
    // Check if the printer returned any settings, if the settings is empty, we
    // can safely assume there are no printer drivers configured. So we safely
    // terminate.
    if (default_settings.IsEmpty()) {
      RunJavaScriptAlert(frame,
          l10n_util::GetString(IDS_DEFAULT_PRINTER_NOT_FOUND_WARNING_TITLE));
      return;
    }

    // Continue only if the settings are valid.
    if (default_settings.dpi && default_settings.document_cookie) {
      int expected_pages_count = 0;
      gfx::Size canvas_size;
      canvas_size.set_width(
          printing::ConvertUnit(default_settings.printable_size.width(),
          static_cast<int>(default_settings.dpi),
          default_settings.desired_dpi));
      canvas_size.set_height(
          printing::ConvertUnit(default_settings.printable_size.height(),
          static_cast<int>(default_settings.dpi),
          default_settings.desired_dpi));
      frame->BeginPrint(canvas_size, &expected_pages_count);
      DCHECK(expected_pages_count);
      frame->EndPrint();

      // Ask the browser to show UI to retrieve the final print settings.
      ViewMsg_PrintPages_Params print_settings;
      // host_window_ may be NULL at this point if the current window is a popup
      // and the print() command has been issued from the parent. The receiver
      // of this message has to deal with this.
      msg = new ViewHostMsg_ScriptedPrint(routing_id(),
                                          render_view_->host_window(),
                                          default_settings.document_cookie,
                                          expected_pages_count,
                                          &print_settings);
      if (Send(msg)) {
        msg = NULL;

        // If the settings are invalid, early quit.
        if (print_settings.params.dpi &&
            print_settings.params.document_cookie) {
          // Render the printed pages. It will implicitly revert the document to
          // display CSS media type.
          PrintPages(print_settings, frame);
          // All went well.
          return;
        } else {
          // The user cancelled.
        }
      } else {
        // Send() failed.
        NOTREACHED();
      }
    } else {
      // The user cancelled.
    }
  } else {
    // Send() failed.
    NOTREACHED();
  }
  // TODO(maruel):  bug 1123882 Alert the user that printing failed.
#else  // defined(OS_WIN)
  // TODO(port): print not implemented
  NOTIMPLEMENTED();
#endif
}

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                 WebFrame* frame) {
  int page_count = 0;
  gfx::Size canvas_size;
  canvas_size.set_width(
      printing::ConvertUnit(params.params.printable_size.width(),
                            static_cast<int>(params.params.dpi),
                            params.params.desired_dpi));
  canvas_size.set_height(
      printing::ConvertUnit(params.params.printable_size.height(),
                            static_cast<int>(params.params.dpi),
                            params.params.desired_dpi));
  frame->BeginPrint(canvas_size, &page_count);
  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                               params.params.document_cookie,
                                               page_count));
  if (page_count) {
    ViewMsg_PrintPage_Params page_params;
    page_params.params = params.params;
    if (params.pages.empty()) {
      for (int i = 0; i < page_count; ++i) {
        page_params.page_number = i;
        PrintPage(page_params, canvas_size, frame);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        PrintPage(page_params, canvas_size, frame);
      }
    }
  }
  frame->EndPrint();
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                const gfx::Size& canvas_size,
                                WebFrame* frame) {
#if defined(OS_WIN)
  // Generate a memory-based EMF file. The EMF will use the current screen's
  // DPI.
  gfx::Emf emf;

  emf.CreateDc(NULL, NULL);
  HDC hdc = emf.hdc();
  DCHECK(hdc);
  skia::PlatformDeviceWin::InitializeDC(hdc);
  // Since WebKit extends the page width depending on the magical shrink
  // factor we make sure the canvas covers the worst case scenario
  // (x2.0 currently).  PrintContext will then set the correct clipping region.
  int size_x = static_cast<int>(canvas_size.width() * params.params.max_shrink);
  int size_y = static_cast<int>(canvas_size.height() *
      params.params.max_shrink);
  // Calculate the dpi adjustment.
  float shrink = static_cast<float>(canvas_size.width()) /
      params.params.printable_size.width();
#if 0
  // TODO(maruel): This code is kept for testing until the 100% GDI drawing
  // code is stable. maruels use this code's output as a reference when the
  // GDI drawing code fails.

  // Mix of Skia and GDI based.
  skia::PlatformCanvasWin canvas(size_x, size_y, true);
  canvas.drawARGB(255, 255, 255, 255, SkPorterDuff::kSrc_Mode);
  float webkit_shrink = frame->PrintPage(params.page_number, &canvas);
  if (shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(size_x, size_y, &bitmap_header);
  const SkBitmap& src_bmp = canvas.getDevice()->accessBitmap(true);
  SkAutoLockPixels src_lock(src_bmp);
  int retval = StretchDIBits(hdc,
                             0,
                             0,
                             size_x, size_y,
                             0, 0,
                             size_x, size_y,
                             src_bmp.getPixels(),
                             reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                             DIB_RGB_COLORS,
                             SRCCOPY);
  DCHECK(retval != GDI_ERROR);
#else
  // 100% GDI based.
  skia::VectorCanvas canvas(hdc, size_x, size_y);
  float webkit_shrink = frame->PrintPage(params.page_number, &canvas);
  if (shrink <= 0) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
  } else {
    // Update the dpi adjustment with the "page shrink" calculated in webkit.
    shrink /= webkit_shrink;
  }
#endif

  // Done printing. Close the device context to retrieve the compiled EMF.
  if (!emf.CloseDc()) {
    NOTREACHED() << "EMF failed";
  }

  // Get the size of the compiled EMF.
  unsigned buf_size = emf.GetDataSize();
  DCHECK_GT(buf_size, 128u);
  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = 0;
  page_params.emf_data_handle = NULL;
  page_params.page_number = params.page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = shrink;
  base::SharedMemory shared_buf;

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (buf_size < 350*1024*1024) {
    // Allocate a shared memory buffer to hold the generated EMF data.
    if (shared_buf.Create(L"", false, false, buf_size) &&
        shared_buf.Map(buf_size)) {
      // Copy the bits into shared memory.
      if (emf.GetData(shared_buf.memory(), buf_size)) {
        page_params.emf_data_handle = shared_buf.handle();
        page_params.data_size = buf_size;
      } else {
        NOTREACHED() << "GetData() failed";
      }
      shared_buf.Unmap();
    } else {
      NOTREACHED() << "Buffer allocation failed";
    }
  } else {
    NOTREACHED() << "Buffer too large: " << buf_size;
  }
  emf.CloseEmf();
  if (Send(new ViewHostMsg_DuplicateSection(routing_id(),
                                            page_params.emf_data_handle,
                                            &page_params.emf_data_handle))) {
    Send(new ViewHostMsg_DidPrintPage(routing_id(), page_params));
  }
#else  // defined(OS_WIN)
  // TODO(port) implement printing
  NOTIMPLEMENTED();
#endif
}

bool PrintWebViewHelper::Send(IPC::Message* msg) {
  return render_view_->Send(msg);
}

int32 PrintWebViewHelper::routing_id() {
  return render_view_->routing_id();
}

void PrintWebViewHelper::GetWindowRect(WebWidget* webwidget,
                                    WebKit::WebRect* rect) {
  NOTREACHED();
}

void PrintWebViewHelper::DidStopLoading(WebView* webview) {
  NOTREACHED();
}

WebKit::WebScreenInfo PrintWebViewHelper::GetScreenInfo(WebWidget* webwidget) {
  WebKit::WebScreenInfo info;
  NOTREACHED();
  return info;
}

gfx::NativeViewId PrintWebViewHelper::GetContainingView(WebWidget* webwidget) {
  NOTREACHED();
  return gfx::NativeViewId();
}

bool PrintWebViewHelper::IsHidden(WebWidget* webwidget) {
  NOTREACHED();
  return true;
}
