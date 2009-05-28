// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/drag_utils.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/os_exchange_data.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "views/controls/button/text_button.h"

namespace drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageMaxWidth = 200;
static const int kLinkDragImageVPadding = 3;

// File dragging pixel measurements
static const int kFileDragImageMaxWidth = 200;
static const SkColor kFileDragImageTextColor = SK_ColorBLACK;

void SetURLAndDragImage(const GURL& url,
                        const std::wstring& title,
                        const SkBitmap& icon,
                        OSExchangeData* data) {
  DCHECK(url.is_valid() && data);

  data->SetURL(url, title);

  // Create a button to render the drag image for us.
  views::TextButton button(NULL,
                           title.empty() ? UTF8ToWide(url.spec()) : title);
  button.set_max_width(kLinkDragImageMaxWidth);
  if (icon.isNull()) {
    button.SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
                   IDR_DEFAULT_FAVICON));
  } else {
    button.SetIcon(icon);
  }
  gfx::Size prefsize = button.GetPreferredSize();
  button.SetBounds(0, 0, prefsize.width(), prefsize.height());

  // Render the image.
  gfx::Canvas canvas(prefsize.width(), prefsize.height(), false);
  button.Paint(&canvas, true);
  SetDragImageOnDataObject(canvas, prefsize.width(), prefsize.height(),
                           prefsize.width() / 2, prefsize.height() / 2,
                           data);
}

void CreateDragImageForFile(const std::wstring& file_name,
                            SkBitmap* icon,
                            OSExchangeData* data_object) {
  DCHECK(icon);
  DCHECK(data_object);

  // Set up our text portion
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ResourceBundle::BaseFont);

  const int width = kFileDragImageMaxWidth;
  // Add +2 here to allow room for the halo.
  const int height = font.height() + icon->height() +
                     kLinkDragImageVPadding + 2;
  gfx::Canvas canvas(width, height, false /* translucent */);

  // Paint the icon.
  canvas.DrawBitmapInt(*icon, (width - icon->width()) / 2, 0);

  // Paint the file name. We inset it one pixel to allow room for the halo.
#if defined(OS_WIN)
  const std::wstring& name = file_util::GetFilenameFromPath(file_name);
  canvas.DrawStringWithHalo(name, font, kFileDragImageTextColor, SK_ColorWHITE,
                            1, icon->height() + kLinkDragImageVPadding + 1,
                            width - 2, font.height(),
                            gfx::Canvas::TEXT_ALIGN_CENTER);
#else
  NOTIMPLEMENTED();
#endif

  SetDragImageOnDataObject(canvas, width, height, width / 2,
                           kLinkDragImageVPadding, data_object);
}

} // namespace drag_utils
