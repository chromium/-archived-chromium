/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollView.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformScrollBar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Range.h"
#include "RenderTheme.h"
#include "ScrollBar.h"
#include "SkiaUtils.h"
#include "WidgetClientChromium.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

#undef LOG
#include "base/gfx/platform_canvas_win.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate : public ScrollbarClient {
public:
    ScrollViewPrivate(ScrollView* view)
        : m_view(view)
        , m_hasStaticBackground(false)
        , m_scrollbarsSuppressed(false)
        , m_inUpdateScrollbars(false)
        , m_scrollbarsAvoidingResizer(0)
        , m_vScrollbarMode(ScrollbarAuto)
        , m_hScrollbarMode(ScrollbarAuto)
        , m_visible(false)
        , m_attachedToWindow(false)
        , m_panScrollIconPoint(0,0)
        , m_drawPanScrollIcon(false)
    {
    }

    ~ScrollViewPrivate()
    {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    void setHasHorizontalScrollbar(bool hasBar);
    void setHasVerticalScrollbar(bool hasBar);

    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;
    virtual bool isActive() const;

    void scrollBackingStore(const IntSize& scrollDelta);

    // Get the vector containing the result from the FindInPage operation.
    const Vector<RefPtr<Range> >* getTickmarks() const;

    // Retrieves the index of the active tickmark for a given frame.  If the
    // frame does not have an active tickmark (for example if the active
    // tickmark resides in another frame) this function returns kNoTickmark.
    size_t ScrollView::ScrollViewPrivate::getActiveTickmarkIndex() const;

    // This is a helper function for accessing the bitmaps that have been cached
    // in the renderer.
    const SkBitmap* GetPreloadedBitmapFromRenderer(int resource_id) const;

    // Highlight the matches found during FindInPage operation.
    void highlightMatches(GraphicsContext* context) const;

    // Highlights the node selected in the DOM inspector.
    void highlightInspectedNode(GraphicsContext* context, Frame* frame) const;
    
    // Highlight a certain Range on the page.
    void highlightRange(HDC hdc, HDC mem_dc, RefPtr<Range> range) const;

    void setAllowsScrolling(bool);
    bool allowsScrolling() const;

    ScrollView* m_view;
    IntSize m_scrollOffset;
    IntSize m_contentsSize;
    bool m_hasStaticBackground;
    bool m_scrollbarsSuppressed;
    bool m_inUpdateScrollbars;
    int m_scrollbarsAvoidingResizer;
    ScrollbarMode m_vScrollbarMode;
    ScrollbarMode m_hScrollbarMode;
    RefPtr<PlatformScrollbar> m_vBar;
    RefPtr<PlatformScrollbar> m_hBar;
    HRGN m_dirtyRegion;
    HashSet<Widget*> m_children;
    bool m_visible;
    bool m_attachedToWindow;
    IntPoint m_panScrollIconPoint;
    bool m_drawPanScrollIcon;
};

const int panIconSizeLength = 20;

void ScrollView::ScrollViewPrivate::setHasHorizontalScrollbar(bool hasBar)
{
    if (Scrollbar::hasPlatformScrollbars()) {
        if (hasBar && !m_hBar) {
            m_hBar = PlatformScrollbar::create(this, HorizontalScrollbar, RegularScrollbar);
            m_view->addChild(m_hBar.get());
        } else if (!hasBar && m_hBar) {
            m_view->removeChild(m_hBar.get());
            m_hBar = 0;
        }
    }
}

void ScrollView::ScrollViewPrivate::setHasVerticalScrollbar(bool hasBar)
{
    if (Scrollbar::hasPlatformScrollbars()) {
        if (hasBar && !m_vBar) {
            m_vBar = PlatformScrollbar::create(this, VerticalScrollbar, RegularScrollbar);
            m_view->addChild(m_vBar.get());
        } else if (!hasBar && m_vBar) {
            m_view->removeChild(m_vBar.get());
            m_vBar = 0;
        }
    }
}

void ScrollView::ScrollViewPrivate::valueChanged(Scrollbar* bar)
{
    // Figure out if we really moved.
    IntSize newOffset = m_scrollOffset;
    if (bar) {
        if (bar == m_hBar)
            newOffset.setWidth(bar->value());
        else if (bar == m_vBar)
            newOffset.setHeight(bar->value());
    }
    IntSize scrollDelta = newOffset - m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    m_scrollOffset = newOffset;

    if (m_scrollbarsSuppressed)
        return;

    scrollBackingStore(scrollDelta);

    if (Frame* frame = static_cast<FrameView*>(m_view)->frame()) {
        frame->sendScrollEvent();

        // Inform the delegate that the scroll position has changed.
        WidgetClientChromium* client =
                static_cast<WidgetClientChromium*>(m_view->client());
        if (client)
            client->onScrollPositionChanged(m_view);
    }
}

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.

    IntRect clipRect = m_view->windowClipRect();
    IntRect scrollViewRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));

    // Negative when our frame is smaller than the min scrollbar width.
    if (scrollViewRect.width() < 0)
        scrollViewRect.setWidth(0);
    if (scrollViewRect.height() < 0)
        scrollViewRect.setHeight(0);

    if (!m_hasStaticBackground) { // The main frame can just blit the WebView window
        // FIXME: Find a way to blit subframes without blitting overlapping content
        m_view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(), scrollViewRect, clipRect);
    } else {
        IntRect updateRect = clipRect;
        updateRect.intersect(scrollViewRect);

         // We need to go ahead and repaint the entire backing store.  Do it now before moving the
         // plugins.
        m_view->addToDirtyRegion(updateRect);
        m_view->updateBackingStore();
    }

    // This call will move child HWNDs (plugins) and invalidate them as well.
    m_view->geometryChanged();
}

void ScrollView::ScrollViewPrivate::setAllowsScrolling(bool flag)
{
    if (flag && m_vScrollbarMode == ScrollbarAlwaysOff)
        m_vScrollbarMode = ScrollbarAuto;
    else if (!flag)
        m_vScrollbarMode = ScrollbarAlwaysOff;

    if (flag && m_hScrollbarMode == ScrollbarAlwaysOff)
        m_hScrollbarMode = ScrollbarAuto;
    else if (!flag)
        m_hScrollbarMode = ScrollbarAlwaysOff;

    m_view->updateScrollbars(m_scrollOffset);
}

bool ScrollView::ScrollViewPrivate::allowsScrolling() const
{
    // Return YES if either horizontal or vertical scrolling is allowed.
    return m_hScrollbarMode != ScrollbarAlwaysOff || m_vScrollbarMode != ScrollbarAlwaysOff;
}

IntRect ScrollView::ScrollViewPrivate::windowClipRect() const
{
    // FrameView::windowClipRect() will exclude the scrollbars, but here we
    // want to include them, so we are forced to cast to FrameView in order to
    // call the non-virtual version of windowClipRect :-(
    //
    // The non-frame case exists to support FramelessScrollView.

    const FrameView* frameView = static_cast<const FrameView*>(m_view);
    if (frameView->frame())
        return frameView->windowClipRect(false);

    return m_view->windowClipRect();
}

bool ScrollView::ScrollViewPrivate::isActive() const
{
    Page* page = static_cast<const FrameView*>(m_view)->frame()->page();
    return page && page->focusController()->isActive();
}

const Vector<RefPtr<Range> >* ScrollView::ScrollViewPrivate::getTickmarks() const
{
    FrameView* view = static_cast<FrameView*>(m_view);
    ASSERT(view);
    Frame* frame = view->frame();

    if (!frame)
        return NULL;  // NOTE: Frame can be null for dropdown boxes.

    WidgetClientChromium* c =
        static_cast<WidgetClientChromium*>(m_view->client());
    ASSERT(c);
    return c->getTickmarks(view->frame());
}

size_t ScrollView::ScrollViewPrivate::getActiveTickmarkIndex() const
{
  FrameView* view = static_cast<FrameView*>(m_view);
  ASSERT(view);
  Frame* frame = view->frame();

  // NOTE: Frame can be null for dropdown boxes.
  if (!frame)
      return WidgetClientChromium::kNoTickmark;

  WidgetClientChromium* c =
      static_cast<WidgetClientChromium*>(m_view->client());
  ASSERT(c);
  return c->getActiveTickmarkIndex(view->frame());
}

const SkBitmap* ScrollView::ScrollViewPrivate::GetPreloadedBitmapFromRenderer(
        int resource_id) const
{
    WidgetClientChromium* c =
        static_cast<WidgetClientChromium*>(m_view->client());
    if (!c)
        return NULL;

    return c->getPreloadedResourceBitmap(resource_id);
}

void ScrollView::ScrollViewPrivate::highlightMatches(
    GraphicsContext* context) const
{
    if (context->paintingDisabled())
        return;

    const Vector<RefPtr<Range> >* tickmarks = getTickmarks();
    if (!tickmarks || tickmarks->isEmpty())
        return;

    context->save();
    context->translate(m_view->x(), m_view->y());

    // NOTE: We tolerate the platformContext() call here because the scrollbars
    // will not be serialized, i.e. composition is done in the renderer and
    // never in the browser.
    // Prepare for drawing the arrows along the scroll bar.
    gfx::PlatformCanvas* canvas = context->platformContext()->canvas();

    int horz_start = 0;
    int horz_end   = m_view->width();
    int vert_start = 0;
    int vert_end   = m_view->height();

    if (m_vBar) {
        // Account for the amount of scrolling on the vertical scroll bar.
        vert_start += m_scrollOffset.height();
        vert_end   += m_scrollOffset.height();
        // Don't draw atop the vertical scrollbar.
        horz_end   -= PlatformScrollbar::verticalScrollbarWidth() + 1;
    }

    if (m_hBar) {
        // Account for the amount of scrolling on the horizontal scroll bar.
        horz_start += m_scrollOffset.width();
        horz_end   += m_scrollOffset.width();
        // Don't draw atop the horizontal scrollbar.
        vert_end   -= PlatformScrollbar::horizontalScrollbarHeight() + 1;
    }

    HDC hdc = canvas->beginPlatformPaint();

    // We create a memory DC, copy the bits we want to highlight to the DC and
    // then MERGE_COPY pieces of it back with a yellow brush selected (which
    // gives them yellow highlighting).
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, m_view->width(),
                                                  m_view->height());
    HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

    // Now create a brush for hit highlighting. This is needed for the MERGECOPY
    // to paint a yellow highlight onto the matches found. For more details, see
    // the documentation for BitBlt.
    static const COLORREF kFillColor = RGB(255, 250, 150);  // Light yellow.
    HGDIOBJ inactive_brush = CreateSolidBrush(kFillColor);
    static const COLORREF kFillColorActive = RGB(255, 150, 50);  // Orange.
    HGDIOBJ active_brush = CreateSolidBrush(kFillColorActive);
    HGDIOBJ old_brush = SelectObject(hdc, inactive_brush);

    // Keep a copy of what's on screen, so we can MERGECOPY it back later for
    // the purpose of highlighting the text.
    BitBlt(mem_dc, 0, 0, m_view->width(), m_view->height(),
           hdc, 0, 0, SRCCOPY);

    const size_t active_tickmark = getActiveTickmarkIndex();
    for (Vector<RefPtr<Range> >::const_iterator i = tickmarks->begin();
         i != tickmarks->end(); ++i) {
        const RefPtr<Range> range = (*i);
        const IntRect& bounds = range->boundingBox();
        // To highlight the word, we check if the rectangle boundary is within
        // the bounds vertically as well as horizontally.
        if (bounds.bottomRight().y() > vert_start &&
            bounds.topLeft().y() < vert_end &&
            bounds.bottomRight().x() > horz_start &&
            bounds.topLeft().x() < horz_end &&
            WebFrameImpl::RangeShouldBeHighlighted(range.get())) {
            // We highlight the active tick-mark with a green color instead
            // of the normal yellow color.
            SelectObject(hdc, ((i - tickmarks->begin()) == active_tickmark) ?
                active_brush : inactive_brush);
            highlightRange(hdc, mem_dc, range);
        }
    }

    SelectObject(mem_dc, old_brush);
    DeleteObject(active_brush);
    DeleteObject(inactive_brush);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);

    DeleteDC(mem_dc);

    canvas->endPlatformPaint();
    context->restore();
}

// TODO(ojan): http://b/1143983 make this work for inline elements as they can
// wrap (use highlightRange instead?)
void ScrollView::ScrollViewPrivate::highlightInspectedNode(
    GraphicsContext* context, Frame* frame) const
{
    WebViewImpl* c = static_cast<WebViewImpl*>(m_view->client());
    const WebCore::Node* inspected_node = c->getInspectedNode(frame);

    if (!inspected_node)
      return;

    SkPaint paint;
    paint.setARGB(122, 255, 225, 0); // Yellow

    // TODO(ojan): http://b/1143991 Once we sync a Skia version that supports 
    // it, use SkPorterDuff::kScreenMode and remove the transparency.
    // Then port highlightMatches/highlightRanges to use this as well.
    // Although, perhaps the web inspector really should be using
    // an alpha overlay? It's less pretty, but more clear what node
    // is being overlayed. In this case, the TODO is to make
    // highlightMatches/Ranges use Skia and to leave this as is.
    //
    // paint.setPorterDuffXfermode(SkPorterDuff::kScreenMode);

    // TODO(ojan): http://b/1143975 Draw the padding/border/margin boxes in
    // different colors.
    context->platformContext()->paintSkPaint(inspected_node->getRect(), paint);
}

void ScrollView::ScrollViewPrivate::highlightRange(HDC hdc, HDC mem_dc,
                                                   RefPtr<Range> range) const {
    // We need to figure out whether the match that we want to
    // highlight is on a single line or on multiple lines.
    IntRect start = VisiblePosition(range->startPosition()).caretRect();
    IntRect end = VisiblePosition(range->endPosition()).caretRect();
    IntRect bounds = range->boundingBox();

    // Multi-line bounds have different y pos for start and end.
    if (start.y() == end.y()) {
        int x = bounds.topLeft().x() - m_scrollOffset.width();
        int y = bounds.topLeft().y() - m_scrollOffset.height();
        int w = bounds.bottomRight().x() - bounds.topLeft().x() + 1;
        int h = bounds.bottomRight().y() - bounds.topLeft().y() + 1;

        // MERGECOPY the relevant bits back, creating a highlight.
        BitBlt(hdc, x, y, w, h, mem_dc, x, y, MERGECOPY);
    } else {
        // Multi line bounds, for example, when we need to highlight
        // all the numbers (and only the numbers) in this block of
        // text:
        //
        // xxxxxxxxxxxxxxxx11111111
        // 222222222222222222222222
        // 222222222222222222222222
        // 333333333333333xxxxxxxxx
        //
        // In this case, the bounding box will contain all the text,
        // (including the exes (x)). We highlight in three steps.
        // First we highlight the segment containing the ones (1)
        // above. Then the whole middle section is highlighted, or the
        // twos (2), and finally the remaining segment consisting of
        // the threes (3) is highlighted.

        const int row_height = start.height();
        int x = 0, y = 0, w = 0, h = 0;

        // The start and end caret can be outside the bounding box, for leading
        // and trailing whitespace and we should not highlight those.
        if (start.intersects(bounds)) {
            // Highlight the first segment.
            x = start.x() - m_scrollOffset.width();
            y = start.y() - m_scrollOffset.height();
            w = bounds.topRight().x() - start.x() + 1;
            h = row_height;

            BitBlt(hdc, x, y, w, h, mem_dc, x, y, MERGECOPY);
        }

        // Figure out how large the middle section is.
        int rows_between = (end.y() - start.y()) / row_height - 1;

        if (rows_between > 0) {
            // Highlight the middle segment.
            x = bounds.x() - m_scrollOffset.width();
            y = bounds.y() - m_scrollOffset.height() + row_height;
            w = bounds.width();
            h = rows_between * row_height;

            BitBlt(hdc, x, y, w, h, mem_dc, x, y, MERGECOPY);
        }

        // The end caret might not intersect the bounding box, for example
        // when highlighting the last letter of a line that wraps. In that
        // case the end caret is set to the beginning of the next line, and
        // since it doesn't intersect with the bounding box we don't need to
        // highlight.
        if (end.intersects(bounds)) {
            // Highlight the remaining segment.
            x = bounds.bottomLeft().x() - m_scrollOffset.width();
            y = bounds.bottomLeft().y() - m_scrollOffset.height() -
                row_height + 1;
            w = end.x() - bounds.bottomLeft().x();
            h = row_height;

            BitBlt(hdc, x, y, w, h, mem_dc, x, y, MERGECOPY);
        }
    }
}

ScrollView::ScrollView()
{
    m_data = new ScrollViewPrivate(this);
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& rect, bool now)
{
    if (rect.isEmpty())
        return;

    IntRect containingWindowRect = contentsToWindow(rect);

    if (containingWindowRect.x() < 0)
        containingWindowRect.setX(0);
    if (containingWindowRect.y() < 0)
        containingWindowRect.setY(0);

    updateWindowRect(containingWindowRect, now);
}

void ScrollView::updateWindowRect(const IntRect& rect, bool now)
{
    // TODO(dglazkov): make sure this is actually the right way to do this

    // Cache the dirty spot.
    addToDirtyRegion(rect);

    // since painting always happens asynchronously, we don't have a way to
    // honor the "now" parameter.  it is unclear if it matters.
    if (now) {
      // TODO(iyengar): Should we force a layout to occur here?
      geometryChanged();
    }
}

void ScrollView::update()
{
    // TODO(iyengar): Should we force a layout to occur here?
    geometryChanged();
}

int ScrollView::visibleWidth() const
{
    return width() - (m_data->m_vBar ? m_data->m_vBar->width() : 0);
}

int ScrollView::visibleHeight() const
{
    return height() - (m_data->m_hBar ? m_data->m_hBar->height() : 0);
}

FloatRect ScrollView::visibleContentRect() const
{
    return FloatRect(contentsX(), contentsY(), visibleWidth(), visibleHeight());
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const
{
    // external scrollers not supported for now
    return visibleContentRect();
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w, int h)
{
    IntSize newContentsSize(w, h);
    if (m_data->m_contentsSize != newContentsSize) {
        m_data->m_contentsSize = newContentsSize;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setFrameGeometry(const IntRect& newGeometry)
{
    IntRect normalizedNewGeometry = newGeometry;

    // Webkit sometimes attempts to set negative sizes due to
    // sloppy calculations of width with margins and such.
    // (RenderPart:updateWidgetPosition is one example.)
    // Safeguard against this and prevent negative heights/widths.
    if (normalizedNewGeometry.width() < 0)
      normalizedNewGeometry.setWidth(0);
    if (normalizedNewGeometry.height() < 0)
      normalizedNewGeometry.setHeight(0);

    IntRect oldGeometry = frameGeometry();
    Widget::setFrameGeometry(normalizedNewGeometry);

    if (normalizedNewGeometry == oldGeometry)
        return;

    if (normalizedNewGeometry.width() != oldGeometry.width() ||
        normalizedNewGeometry.height() != oldGeometry.height()) {
        updateScrollbars(m_data->m_scrollOffset);

        // when used to display a popup menu, we do not have a frame
        FrameView* frameView = static_cast<FrameView*>(this);
        if (frameView->frame())
            frameView->setNeedsLayout();
    }

    geometryChanged();
}

int ScrollView::contentsX() const
{
    return scrollOffset().width();
}

int ScrollView::contentsY() const
{
    return scrollOffset().height();
}

int ScrollView::contentsWidth() const
{
    return m_data->m_contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->m_contentsSize.height();
}

IntPoint ScrollView::windowToContents(const IntPoint& windowPoint) const
{
    IntPoint viewPoint = convertFromContainingWindow(windowPoint);
    return viewPoint + scrollOffset();
}

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
    IntPoint viewPoint = contentsPoint - scrollOffset();
    return convertToContainingWindow(viewPoint);
}

IntPoint ScrollView::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    if (child != m_data->m_hBar && child != m_data->m_vBar)
        newPoint = point - scrollOffset();
    return Widget::convertChildToSelf(child, newPoint);
}

IntPoint ScrollView::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    if (child != m_data->m_hBar && child != m_data->m_vBar)
        newPoint = point + scrollOffset();
    return Widget::convertSelfToChild(child, newPoint);
}

IntSize ScrollView::scrollOffset() const
{
    return m_data->m_scrollOffset;
}

IntSize ScrollView::maximumScroll() const
{
    // We should not check whether scrolling is allowed for this view before calculating
    // the maximumScroll. Please refer to http://b/issue?id=1164704, where in scrolling
    // would not work on a scrollview created with scrollbars disabled. The current
    // behavior mirrors Safari's webkit implementation. Firefox also behaves similarly.
    IntSize delta = (m_data->m_contentsSize - IntSize(visibleWidth(), visibleHeight())) - scrollOffset();
    delta.clampNegativeToZero();
    return delta;
}

void ScrollView::scrollBy(int dx, int dy)
{
    IntSize scrollOffset = m_data->m_scrollOffset;
    IntSize newScrollOffset = scrollOffset + IntSize(dx, dy).shrunkTo(maximumScroll());
    newScrollOffset.clampNegativeToZero();

    if (newScrollOffset == scrollOffset)
        return;

    updateScrollbars(newScrollOffset);
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect& r)
{
    IntPoint p(max(0, r.x()), max(0, r.y()));
    ScrollView* view = this;
    while (view) {
        view->setContentsPos(p.x(), p.y());
        p.move(view->x() - view->scrollOffset().width(), view->y() - view->scrollOffset().height());
        view = static_cast<ScrollView*>(view->parent());
    }
}

WebCore::ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->m_hScrollbarMode;
}

WebCore::ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->m_vScrollbarMode;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->m_scrollbarsSuppressed = suppressed;
    if (repaintOnSuppress && !suppressed) {
        if (m_data->m_hBar)
            m_data->m_hBar->invalidate();
        if (m_data->m_vBar)
            m_data->m_vBar->invalidate();

        // Invalidate the scroll corner too on unsuppress.
        IntRect hCorner;
        if (m_data->m_hBar && width() - m_data->m_hBar->width() > 0) {
            hCorner = IntRect(m_data->m_hBar->width(),
                              height() - m_data->m_hBar->height(),
                              width() - m_data->m_hBar->width(),
                              m_data->m_hBar->height());
            invalidateRect(hCorner);
        }

        if (m_data->m_vBar && height() - m_data->m_vBar->height() > 0) {
            IntRect vCorner(width() - m_data->m_vBar->width(),
                            m_data->m_vBar->height(),
                            m_data->m_vBar->width(),
                            height() - m_data->m_vBar->height());
            if (vCorner != hCorner)
                invalidateRect(vCorner);
        }
    }
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->m_hScrollbarMode != newMode) {
        m_data->m_hScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->m_vScrollbarMode != newMode) {
        m_data->m_vScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    if (m_data->m_hScrollbarMode != newMode ||
        m_data->m_vScrollbarMode != newMode) {
        m_data->m_hScrollbarMode = m_data->m_vScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->m_hasStaticBackground = flag;
}

void ScrollView::updateScrollbars(const IntSize& desiredOffset)
{
    // Don't allow re-entrancy into this function.
    if (m_data->m_inUpdateScrollbars)
        return;

    m_data->m_inUpdateScrollbars = true;

    bool hasVerticalScrollbar = m_data->m_vBar;
    bool hasHorizontalScrollbar = m_data->m_hBar;
    bool oldHasVertical = hasVerticalScrollbar;
    bool oldHasHorizontal = hasHorizontalScrollbar;
    ScrollbarMode hScroll = m_data->m_hScrollbarMode;
    ScrollbarMode vScroll = m_data->m_vScrollbarMode;

    const int cVerticalWidth = PlatformScrollbar::verticalScrollbarWidth();
    const int cHorizontalHeight = PlatformScrollbar::horizontalScrollbarHeight();

    // we may not be able to support scrollbars due to our frame geometry
    if (width() < cVerticalWidth)
        vScroll = ScrollbarAlwaysOff;
    if (height() < cHorizontalHeight)
        hScroll = ScrollbarAlwaysOff;

    for (int pass = 0; pass < 2; pass++) {
        bool scrollsVertically;
        bool scrollsHorizontally;

        if (!m_data->m_scrollbarsSuppressed && (hScroll == ScrollbarAuto || vScroll == ScrollbarAuto)) {
            // Do a layout if pending before checking if scrollbars are needed.
            if (hasVerticalScrollbar != oldHasVertical || hasHorizontalScrollbar != oldHasHorizontal)
                static_cast<FrameView*>(this)->layout();

            scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() > height());
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() + cVerticalWidth > width());
            else {
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() > width());
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() + cHorizontalHeight > height());
            }
        }
        else {
            scrollsHorizontally = (hScroll == ScrollbarAuto) ? hasHorizontalScrollbar : (hScroll == ScrollbarAlwaysOn);
            scrollsVertically = (vScroll == ScrollbarAuto) ? hasVerticalScrollbar : (vScroll == ScrollbarAlwaysOn);
        }

        if (hasVerticalScrollbar != scrollsVertically) {
            m_data->setHasVerticalScrollbar(scrollsVertically);
            hasVerticalScrollbar = scrollsVertically;
        }

        if (hasHorizontalScrollbar != scrollsHorizontally) {
            m_data->setHasHorizontalScrollbar(scrollsHorizontally);
            hasHorizontalScrollbar = scrollsHorizontally;
        }
    }

    // Set up the range (and page step/line step).
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = desiredOffset.shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    if (m_data->m_hBar) {
        int clientWidth = visibleWidth();
        m_data->m_hBar->setEnabled(contentsWidth() > clientWidth);
        int pageStep = (clientWidth - PAGE_KEEP);
        if (pageStep < 0) pageStep = clientWidth;
        IntRect oldRect(m_data->m_hBar->frameGeometry());
        IntRect hBarRect = IntRect(0,
                                   height() - m_data->m_hBar->height(),
                                   width() - (m_data->m_vBar ? m_data->m_vBar->width() : 0),
                                   m_data->m_hBar->height());
        m_data->m_hBar->setRect(hBarRect);
        if (!m_data->m_scrollbarsSuppressed && oldRect != m_data->m_hBar->frameGeometry())
            m_data->m_hBar->invalidate();

        if (m_data->m_scrollbarsSuppressed)
            m_data->m_hBar->setSuppressInvalidation(true);
        m_data->m_hBar->setSteps(LINE_STEP, pageStep);
        m_data->m_hBar->setProportion(clientWidth, contentsWidth());
        m_data->m_hBar->setValue(scroll.width());
        if (m_data->m_scrollbarsSuppressed)
            m_data->m_hBar->setSuppressInvalidation(false);
    }

    if (m_data->m_vBar) {
        int clientHeight = visibleHeight();
        m_data->m_vBar->setEnabled(contentsHeight() > clientHeight);
        int pageStep = (clientHeight - PAGE_KEEP);
        if (pageStep < 0) pageStep = clientHeight;
        IntRect oldRect(m_data->m_vBar->frameGeometry());
        IntRect vBarRect = IntRect(width() - m_data->m_vBar->width(),
                                   0,
                                   m_data->m_vBar->width(),
                                   height() - (m_data->m_hBar ? m_data->m_hBar->height() : 0));
        m_data->m_vBar->setRect(vBarRect);
        if (!m_data->m_scrollbarsSuppressed && oldRect != m_data->m_vBar->frameGeometry())
            m_data->m_vBar->invalidate();

        if (m_data->m_scrollbarsSuppressed)
            m_data->m_vBar->setSuppressInvalidation(true);
        m_data->m_vBar->setSteps(LINE_STEP, pageStep);
        m_data->m_vBar->setProportion(clientHeight, contentsHeight());
        m_data->m_vBar->setValue(scroll.height());
        if (m_data->m_scrollbarsSuppressed)
            m_data->m_vBar->setSuppressInvalidation(false);
    }

    if (oldHasVertical != (m_data->m_vBar != 0) || oldHasHorizontal != (m_data->m_hBar != 0))
        geometryChanged();

    // See if our offset has changed in a situation where we might not have scrollbars.
    // This can happen when editing a body with overflow:hidden and scrolling to reveal selection.
    // It can also happen when maximizing a window that has scrollbars (but the new maximized result
    // does not).
    IntSize scrollDelta = scroll - m_data->m_scrollOffset;
    if (scrollDelta != IntSize()) {
        m_data->m_scrollOffset = scroll;
        m_data->scrollBackingStore(scrollDelta);

        // Inform the delegate that the scroll position has changed.
        WidgetClientChromium* c = static_cast<WidgetClientChromium*>(client());
        if (c)
            c->onScrollPositionChanged(this);
    }

    m_data->m_inUpdateScrollbars = false;

    ASSERT(visibleWidth() >= 0);
    ASSERT(visibleHeight() >= 0);
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    IntPoint viewPoint = convertFromContainingWindow(mouseEvent.pos());
    if (m_data->m_hBar && m_data->m_hBar->frameGeometry().contains(viewPoint))
        return m_data->m_hBar.get();
    if (m_data->m_vBar && m_data->m_vBar->frameGeometry().contains(viewPoint))
        return m_data->m_vBar.get();
    return 0;
}

void ScrollView::addChild(Widget* child)
{
    child->setParent(this);

    // There is only one global widget client (which should be the WebViewImpl).
    // It is responsible for things like capturing the mouse.
    child->setClient(client());

    m_data->m_children.add(child);
}

void ScrollView::removeChild(Widget* child)
{
    child->setParent(0);
    m_data->m_children.remove(child);
}

void ScrollView::paint(GraphicsContext* context, const IntRect& rect)
{
    // FIXME: This code is here so we don't have to fork FrameView.h/.cpp.
    // In the end, FrameView should just merge with ScrollView.
    ASSERT(isFrameView());

    if (context->paintingDisabled())
        return;

    if (Frame* frame = static_cast<FrameView*>(this)->frame()) {
        IntRect documentDirtyRect = rect;
        documentDirtyRect.intersect(frameGeometry());

        context->save();

        context->translate(x(), y());
        documentDirtyRect.move(-x(), -y());

        context->translate(-contentsX(), -contentsY());
        documentDirtyRect.move(contentsX(), contentsY());

        // do not allow painting outside of the dirty rect
        context->clip(documentDirtyRect);

        frame->paint(context, documentDirtyRect);

        // Highlights the node selected in the DOM inspector.
        m_data->highlightInspectedNode(context, frame);

        context->restore();
    }

    // Highlight the matches found on the page, during a FindInPage operation.
    m_data->highlightMatches(context);

    // Now paint the scrollbars.
    if (!m_data->m_scrollbarsSuppressed && (m_data->m_hBar || m_data->m_vBar)) {
        context->save();
        IntRect scrollViewDirtyRect = rect;
        scrollViewDirtyRect.intersect(frameGeometry());
        context->translate(x(), y());
        scrollViewDirtyRect.move(-x(), -y());
        if (m_data->m_hBar)
            m_data->m_hBar->paint(context, scrollViewDirtyRect);
        if (m_data->m_vBar)
            m_data->m_vBar->paint(context, scrollViewDirtyRect);

        // Fill the scroll corner with white.
        IntRect hCorner;
        if (m_data->m_hBar && width() - m_data->m_hBar->width() > 0) {
            hCorner = IntRect(m_data->m_hBar->width(),
                              height() - m_data->m_hBar->height(),
                              width() - m_data->m_hBar->width(),
                              m_data->m_hBar->height());
            if (hCorner.intersects(scrollViewDirtyRect))
                context->fillRect(hCorner, Color::white);
        }

        if (m_data->m_vBar && height() - m_data->m_vBar->height() > 0) {
            IntRect vCorner(width() - m_data->m_vBar->width(),
                            m_data->m_vBar->height(),
                            m_data->m_vBar->width(),
                            height() - m_data->m_vBar->height());
            if (vCorner != hCorner && vCorner.intersects(scrollViewDirtyRect))
                context->fillRect(vCorner, Color::white);
        }

        context->restore();
    }
}

void ScrollView::themeChanged()
{
    PlatformScrollbar::themeChanged();
    theme()->themeChanged();
    invalidate();
}

void ScrollView::wheelEvent(PlatformWheelEvent& e)
{
    if (!m_data->allowsScrolling())
        return;

    // Determine how much we want to scroll.  If we can move at all, we will accept the event.
    IntSize maxScrollDelta = maximumScroll();
    if ((e.deltaX() < 0 && maxScrollDelta.width() > 0) ||
        (e.deltaX() > 0 && scrollOffset().width() > 0) ||
        (e.deltaY() < 0 && maxScrollDelta.height() > 0) ||
        (e.deltaY() > 0 && scrollOffset().height() > 0)) {
        e.accept();
        scrollBy(-e.deltaX() * LINE_STEP, -e.deltaY() * LINE_STEP);
    }
}

HashSet<Widget*>* ScrollView::children()
{
    return &(m_data->m_children);
}

void ScrollView::geometryChanged() const
{
    HashSet<Widget*>::const_iterator end = m_data->m_children.end();
    for (HashSet<Widget*>::const_iterator current = m_data->m_children.begin(); current != end; ++current)
        (*current)->geometryChanged();
}

bool ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if  (direction == ScrollUp || direction == ScrollDown) {
        if (m_data->m_vBar)
            return m_data->m_vBar->scroll(direction, granularity);
    } else {
        if (m_data->m_hBar)
            return m_data->m_hBar->scroll(direction, granularity);
    }
    return false;
}

IntRect ScrollView::windowResizerRect()
{
    return IntRect();
}

bool ScrollView::resizerOverlapsContent() const
{
    return !m_data->m_scrollbarsAvoidingResizer;
}

void ScrollView::adjustOverlappingScrollbarCount(int overlapDelta)
{
    int oldCount = m_data->m_scrollbarsAvoidingResizer;
    m_data->m_scrollbarsAvoidingResizer += overlapDelta;
    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(overlapDelta);
    else if (!m_data->m_scrollbarsSuppressed) {
        // If we went from n to 0 or from 0 to n and we're the outermost view,
        // we need to invalidate the windowResizerRect(), since it will now need to paint
        // differently.
        if (oldCount > 0 && m_data->m_scrollbarsAvoidingResizer == 0 ||
            oldCount == 0 && m_data->m_scrollbarsAvoidingResizer > 0)
            invalidateRect(windowResizerRect());
    }
}

void ScrollView::setParent(ScrollView* parentView)
{
    if (!parentView && m_data->m_scrollbarsAvoidingResizer && parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(false);
    Widget::setParent(parentView);
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    WidgetClientChromium* c = static_cast<WidgetClientChromium*>(client());
    if (c)
        c->invalidateRect(containingWindowRect);
}

void ScrollView::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    // We don't know how to scroll in two directions at once.
    if (dx && dy) {
        IntRect updateRect = clipRect;
        updateRect.intersect(scrollViewRect);
        addToDirtyRegion(updateRect);
        return;
    }

    WidgetClientChromium* c = static_cast<WidgetClientChromium*>(client());
    if (c) {
        // TODO(ericroman): would be better to pass both the scroll rect
        // and clip rect up to the client and let them decide how best to
        // scroll the backing store.
        IntRect clippedScrollRect = scrollViewRect;
        clippedScrollRect.intersect(clipRect);
        c->scrollRect(dx, dy, clippedScrollRect);
    }
}

void ScrollView::updateBackingStore()
{
    // nothing to do.  painting happens asynchronously.
}

bool ScrollView::inWindow() const
{
    WidgetClientChromium* c = static_cast<WidgetClientChromium*>(client());
    if (!c)
        return false;

    return !c->isHidden();
}

void ScrollView::attachToWindow()
{
    if (m_data->m_attachedToWindow)
        return;

    m_data->m_attachedToWindow = true;

    if (m_data->m_visible) {
        HashSet<Widget*>::iterator end = m_data->m_children.end();
        for (HashSet<Widget*>::iterator it = m_data->m_children.begin(); it != end; ++it)
            (*it)->attachToWindow();
    }
}

void ScrollView::detachFromWindow()
{
    if (!m_data->m_attachedToWindow)
        return;

    if (m_data->m_visible) {
        HashSet<Widget*>::iterator end = m_data->m_children.end();
        for (HashSet<Widget*>::iterator it = m_data->m_children.begin(); it != end; ++it)
            (*it)->detachFromWindow();
    }

    m_data->m_attachedToWindow = false;
}

void ScrollView::show()
{
    if (!m_data->m_visible) {
        m_data->m_visible = true;
        if (isAttachedToWindow()) {
            HashSet<Widget*>::iterator end = m_data->m_children.end();
            for (HashSet<Widget*>::iterator it = m_data->m_children.begin(); it != end; ++it)
                (*it)->attachToWindow();
        }
    }

    Widget::show();
}

void ScrollView::hide()
{
    if (m_data->m_visible) {
        if (isAttachedToWindow()) {
            HashSet<Widget*>::iterator end = m_data->m_children.end();
            for (HashSet<Widget*>::iterator it = m_data->m_children.begin(); it != end; ++it)
                (*it)->detachFromWindow();
        }
        m_data->m_visible = false;
    }

    Widget::hide();
}

bool ScrollView::isAttachedToWindow() const
{
    return m_data->m_attachedToWindow;
}

void ScrollView::setAllowsScrolling(bool flag)
{
  m_data->setAllowsScrolling(flag);
}

bool ScrollView::allowsScrolling() const
{
  return m_data->allowsScrolling();
}

void ScrollView::printPanScrollIcon(const IntPoint& iconPosition)
{
    m_data->m_drawPanScrollIcon = true;    
    m_data->m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2) ;

    updateWindowRect(IntRect(m_data->m_panScrollIconPoint, IntSize(panIconSizeLength,panIconSizeLength)), true);    
}

void ScrollView::removePanScrollIcon()
{
    m_data->m_drawPanScrollIcon = false; 

    updateWindowRect(IntRect(m_data->m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)), true);
}

bool ScrollView::isScrollable() 
{ 
    return m_data->m_vBar != 0 || m_data->m_hBar != 0;
}

} // namespace WebCore
