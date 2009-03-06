// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#include <string>

#include "chrome/browser/download/download_util.h"

#include "base/base_drag_source.h"
#include "base/file_util.h"
#include "base/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "SkPath.h"
#include "SkShader.h"

namespace download_util {

// How many times to cycle the complete animation. This should be an odd number
// so that the animation ends faded out.
static const int kCompleteAnimationCycles = 5;

// Download opening ------------------------------------------------------------

bool CanOpenDownload(DownloadItem* download) {
  FilePath file_to_use = download->full_path();
  if (!download->original_name().value().empty())
    file_to_use = download->original_name();

  const FilePath::StringType extension =
      file_util::GetFileExtensionFromPath(file_to_use);
  return !download->manager()->IsExecutable(extension);
}

void OpenDownload(DownloadItem* download) {
  if (download->state() == DownloadItem::IN_PROGRESS)
    download->set_open_when_complete(!download->open_when_complete());
  else if (download->state() == DownloadItem::COMPLETE)
    download->manager()->OpenDownloadInShell(download, NULL);
}

// Download progress painting --------------------------------------------------

// Common bitmaps used for download progress animations. We load them once the
// first time we do a progress paint, then reuse them as they are always the
// same.
SkBitmap* g_foreground_16 = NULL;
SkBitmap* g_background_16 = NULL;
SkBitmap* g_foreground_32 = NULL;
SkBitmap* g_background_32 = NULL;

void PaintDownloadProgress(ChromeCanvas* canvas,
                           views::View* containing_view,
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent_done,
                           PaintDownloadProgressSize size) {
  DCHECK(containing_view);

  // Load up our common bitmaps
  if (!g_background_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_background_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_16);
    g_foreground_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
    g_background_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_32);
  }

  SkBitmap* background = (size == BIG) ? g_background_32 : g_background_16;
  SkBitmap* foreground = (size == BIG) ? g_foreground_32 : g_foreground_16;

  const int kProgressIconSize = (size == BIG) ? kBigProgressIconSize :
                                                kSmallProgressIconSize;

  int height = background->height();

  // We start by storing the bounds of the background and foreground bitmaps
  // so that it is easy to mirror the bounds if the UI layout is RTL.
  gfx::Rect background_bounds(origin_x, origin_y,
                              background->width(), background->height());
  gfx::Rect foreground_bounds(origin_x, origin_y,
                              foreground->width(), foreground->height());

  // Mirror the positions if necessary.
  int mirrored_x = containing_view->MirroredLeftPointForRect(background_bounds);
  background_bounds.set_x(mirrored_x);
  mirrored_x = containing_view->MirroredLeftPointForRect(foreground_bounds);
  foreground_bounds.set_x(mirrored_x);

  // Draw the background progress image.
  SkPaint background_paint;
  canvas->DrawBitmapInt(*background,
                        background_bounds.x(),
                        background_bounds.y(),
                        background_paint);

  // Layer the foreground progress image in an arc proportional to the download
  // progress. The arc grows clockwise, starting in the midnight position, as
  // the download progresses. However, if the download does not have known total
  // size (the server didn't give us one), then we just spin an arc around until
  // we're done.
  float sweep_angle = 0.0;
  float start_pos = static_cast<float>(kStartAngleDegrees);
  if (percent_done < 0) {
    sweep_angle = kUnknownAngleDegrees;
    start_pos = static_cast<float>(start_angle);
  } else if (percent_done > 0) {
    sweep_angle = static_cast<float>(kMaxDegrees / 100.0 * percent_done);
  }

  // Set up an arc clipping region for the foreground image. Don't bother using
  // a clipping region if it would round to 360 (really 0) degrees, since that
  // would eliminate the foreground completely and be quite confusing (it would
  // look like 0% complete when it should be almost 100%).
  SkPaint foreground_paint;
  if (sweep_angle < static_cast<float>(kMaxDegrees - 1)) {
    SkRect oval;
    oval.set(SkIntToScalar(foreground_bounds.x()),
             SkIntToScalar(foreground_bounds.y()),
             SkIntToScalar(foreground_bounds.x() + kProgressIconSize),
             SkIntToScalar(foreground_bounds.y() + kProgressIconSize));
    SkPath path;
    path.arcTo(oval,
               SkFloatToScalar(start_pos),
               SkFloatToScalar(sweep_angle), false);
    path.lineTo(SkIntToScalar(foreground_bounds.x() + kProgressIconSize / 2),
                SkIntToScalar(foreground_bounds.y() + kProgressIconSize / 2));

    SkShader* shader =
        SkShader::CreateBitmapShader(*foreground,
                                     SkShader::kClamp_TileMode,
                                     SkShader::kClamp_TileMode);
    SkMatrix shader_scale;
    shader_scale.setTranslate(SkIntToScalar(foreground_bounds.x()),
                              SkIntToScalar(foreground_bounds.y()));
    shader->setLocalMatrix(shader_scale);
    foreground_paint.setShader(shader);
    foreground_paint.setAntiAlias(true);
    shader->unref();
    canvas->drawPath(path, foreground_paint);
    return;
  }

  canvas->DrawBitmapInt(*foreground,
                        foreground_bounds.x(),
                        foreground_bounds.y(),
                        foreground_paint);
}

void PaintDownloadComplete(ChromeCanvas* canvas,
                           views::View* containing_view,
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size) {
  DCHECK(containing_view);

  // Load up our common bitmaps.
  if (!g_foreground_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  SkBitmap* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  // Mirror the positions if necessary.
  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
  complete_bounds.set_x(
      containing_view->MirroredLeftPointForRect(complete_bounds));

  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  static const double PI = 3.141592653589793;
  double opacity = sin(animation_progress * PI * kCompleteAnimationCycles +
                   PI/2) / 2 + 0.5;

  SkRect bounds;
  bounds.set(SkIntToScalar(complete_bounds.x()),
             SkIntToScalar(complete_bounds.y()),
             SkIntToScalar(complete_bounds.x() + complete_bounds.width()),
             SkIntToScalar(complete_bounds.y() + complete_bounds.height()));
  canvas->saveLayerAlpha(&bounds,
                         static_cast<int>(255.0 * opacity),
                         SkCanvas::kARGB_ClipLayer_SaveFlag);
  canvas->drawARGB(0, 255, 255, 255, SkPorterDuff::kClear_Mode);
  canvas->DrawBitmapInt(*complete, complete_bounds.x(), complete_bounds.y());
  canvas->restore();
}

// Load a language dependent height so that the dangerous download confirmation
// message doesn't overlap with the download link label.
int GetBigProgressIconSize() {
  static int big_progress_icon_size = 0;
  if (big_progress_icon_size == 0) {
    std::wstring locale_size_str =
        l10n_util::GetString(IDS_DOWNLOAD_BIG_PROGRESS_SIZE);
    bool rc = StringToInt(locale_size_str, &big_progress_icon_size);
    if (!rc || big_progress_icon_size < kBigProgressIconSize) {
      NOTREACHED();
      big_progress_icon_size = kBigProgressIconSize;
    }
  }

  return big_progress_icon_size;
}

int GetBigProgressIconOffset() {
  return (GetBigProgressIconSize() - kBigIconSize) / 2;
}

// Download dragging
void DragDownload(const DownloadItem* download, SkBitmap* icon) {
  DCHECK(download);

  // Set up our OLE machinery
  scoped_refptr<OSExchangeData> data(new OSExchangeData);
  if (icon)
    drag_utils::CreateDragImageForFile(download->file_name().ToWStringHack(),
                                       icon, data);
  data->SetFilename(download->full_path().ToWStringHack());
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);

  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(data.get(), drag_source.get(), DROPEFFECT_COPY | DROPEFFECT_LINK,
             &effects);
}

}  // namespace download_util
