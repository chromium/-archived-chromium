/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "ScrollView.h"

#import "FloatRect.h"
#import "IntRect.h"
#import "BlockExceptions.h"
#import "Logging.h"
#import "WebCoreFrameView.h"

/*
    This class implementation does NOT actually emulate the Qt ScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    ScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see Widget::resize).
*/

namespace WebCore {

ScrollView::ScrollView() {
}

ScrollView::~ScrollView() {
}

int ScrollView::visibleWidth() const {
  return frameGeometry().width();
}

int ScrollView::visibleHeight() const {
  return frameGeometry().height();
}

FloatRect ScrollView::visibleContentRect() const {
  return FloatRect(frameGeometry());
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const {
  return FloatRect(frameGeometry());
}

int ScrollView::contentsWidth() const {
  return frameGeometry().width();
}

int ScrollView::contentsHeight() const {
  return frameGeometry().height();
}

int ScrollView::contentsX() const {
  return frameGeometry().x();
}

int ScrollView::contentsY() const {
  return frameGeometry().y();
}


IntSize ScrollView::scrollOffset() const
{
    return IntSize();
}

void ScrollView::scrollBy(int dx, int dy)
{
    setContentsPos(contentsX() + dx, contentsY() + dy);
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect& r)
{ 
}

void ScrollView::setContentsPos(int x, int y)
{
}

void ScrollView::setVScrollbarMode(ScrollbarMode vMode)
{
}

void ScrollView::setHScrollbarMode(ScrollbarMode hMode)
{
}

void ScrollView::setScrollbarsMode(ScrollbarMode mode)
{
}

ScrollbarMode ScrollView::vScrollbarMode() const
{
    return ScrollbarAuto;
}

ScrollbarMode ScrollView::hScrollbarMode() const
{
    return ScrollbarAuto;
}

void ScrollView::suppressScrollbars(bool suppressed,  bool repaintOnUnsuppress)
{
}

void ScrollView::addChild(Widget* child)
{
}

void ScrollView::removeChild(Widget* child)
{
    child->removeFromSuperview();
}

void ScrollView::resizeContents(int w, int h)
{
#if 0
  // TODO(pinkerton) - figure this out later.
  contentsGeometry.setWidth(w);
  contentsGeometry.setHeight(h);
#endif
  resize (w, h);
}

void ScrollView::updateContents(const IntRect &rect, bool now)
{
}

void ScrollView::update()
{
}

// "Containing Window" means the NSWindow's coord system, which is origin lower left

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
  return IntPoint(contentsPoint.x(),
                  frameGeometry().height() - contentsPoint.y());
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
  return IntPoint(point.x(), frameGeometry().height() - point.y());
}

IntRect ScrollView::contentsToWindow(const IntRect& contentsRect) const
{
  IntRect flipped_rect = contentsRect;
  flipped_rect.setY(frameGeometry().height() - flipped_rect.y());
  return flipped_rect;
}

IntRect ScrollView::windowToContents(const IntRect& rect) const
{
  IntRect flipped_rect = rect;
  flipped_rect.setY(frameGeometry().height() - flipped_rect.y());
  return flipped_rect;
}

void ScrollView::setStaticBackground(bool b)
{
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    // On Mac, the ScrollView is really the "document", so events will never flow into it to get to the scrollers.
    return 0;
}

bool ScrollView::inWindow() const
{
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    // Do nothing.  NSScrollView handles doing the scroll for us.
}

bool ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity) {
  NSLog(@"ScrollViewMacSkia::scroll should do something here");
  return true;
}

NSView* ScrollView::documentView() const {
  // TODO(pinkerton): we really gotta figure out how we're doing scrolling
  return nil;
}

bool ScrollView::isScrollable() {
  // placeholder until we pull new ScrollView implementation from WebKit
  return false;
}

}
