/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderThemeGtk.h"

#include "AffineTransform.h"
#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "RenderObject.h"
#include "gtkdrawing.h"
#include "GdkSkia.h"

#include <gdk/gdk.h>

namespace WebCore {

static Color makeColor(const GdkColor& c)
{
  return Color(makeRGB(c.red >> 8, c.green >> 8, c.blue >> 8));
}

RenderTheme* theme()
{
    static RenderThemeGtk gtkTheme;
    return &gtkTheme;
}

RenderThemeGtk::RenderThemeGtk()
    : m_gtkWindow(0)
    , m_gtkContainer(0)
    , m_gtkEntry(0)
    , m_gtkTreeView(0)
{
}

static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
        case PushButtonPart:
        case ButtonPart:
        case TextFieldPart:
        case TextAreaPart:
        case SearchFieldPart:
        case MenulistPart:
        case RadioPart:
        case CheckboxPart:
            return true;
        default:
            return false;
    }
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

int RenderThemeGtk::baselinePosition(const RenderObject* o) const
{
    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (o->style()->appearance() == CheckboxPart||
        o->style()->appearance() == RadioPart)
        return o->marginTop() + o->height() - 2;
    return RenderTheme::baselinePosition(o);
}

static GtkTextDirection gtkTextDirection(TextDirection direction)
{
    switch (direction) {
    case RTL:
        return GTK_TEXT_DIR_RTL;
    case LTR:
        return GTK_TEXT_DIR_LTR;
    default:
        return GTK_TEXT_DIR_NONE;
    }
}

static void adjustMozStyle(RenderStyle* style, GtkThemeWidgetType type)
{
    gint left, top, right, bottom;
    GtkTextDirection direction = gtkTextDirection(style->direction());
    gboolean inhtml = true;

    if (moz_gtk_get_widget_border(type, &left, &top, &right, &bottom, direction, inhtml) != MOZ_GTK_SUCCESS)
        return;

    // FIXME: This approach is likely to be incorrect. See other ports and layout tests to see the problem.
    const int xpadding = 1;
    const int ypadding = 1;

    style->setPaddingLeft(Length(xpadding + left, Fixed));
    style->setPaddingTop(Length(ypadding + top, Fixed));
    style->setPaddingRight(Length(xpadding + right, Fixed));
    style->setPaddingBottom(Length(ypadding + bottom, Fixed));
}

static void setMozState(RenderTheme* theme, GtkWidgetState* state, RenderObject* o)
{
    state->active = theme->isPressed(o);
    state->focused = theme->isFocused(o);
    state->inHover = theme->isHovered(o);
    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    state->disabled = !theme->isEnabled(o) || theme->isReadOnlyControl(o);
    state->isDefault = false;
    state->canDefault = false;
    state->depressed = false;
}

static bool paintMozWidget(RenderTheme* theme, GtkThemeWidgetType type, RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // Painting is disabled so just claim to have succeeded
    if (i.context->paintingDisabled())
        return false;

    GtkWidgetState mozState;
    setMozState(theme, &mozState, o);

    int flags;

    // We might want to make setting flags the caller's job at some point rather than doing it here.
    switch (type) {
        case MOZ_GTK_BUTTON:
            flags = GTK_RELIEF_NORMAL;
            break;
        case MOZ_GTK_CHECKBUTTON:
        case MOZ_GTK_RADIOBUTTON:
            flags = theme->isChecked(o);
            break;
        default:
            flags = 0;
            break;
    }

    AffineTransform ctm = i.context->getCTM();

    IntPoint pos = ctm.mapPoint(rect.location());
    GdkRectangle gdkRect;
    gdkRect.x = pos.x();
    gdkRect.y = pos.y();
    gdkRect.width = rect.width();
    gdkRect.height = rect.height();
    GtkTextDirection direction = gtkTextDirection(o->style()->direction());

    PlatformContextSkia* pcs = i.context->platformContext();
    SkCanvas* canvas = pcs->canvas();
    if (!canvas)
        return false;

    const SkIRect clip_region = canvas->getTotalClip().getBounds();

    GdkRectangle gdkClipRect;
    gdkClipRect.x = clip_region.fLeft;
    gdkClipRect.y = clip_region.fTop;
    gdkClipRect.width = clip_region.width();
    gdkClipRect.height = clip_region.height();

    gdk_rectangle_intersect(&gdkRect, &gdkClipRect, &gdkClipRect);

    const gint r =
      moz_gtk_widget_paint(type, pcs->gdk_skia(), &gdkRect, &gdkClipRect, &mozState, flags, direction) != MOZ_GTK_SUCCESS;

    return r;
}

static void setButtonPadding(RenderStyle* style)
{
    // FIXME: This looks incorrect.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(padding / 2, Fixed));
    style->setPaddingBottom(Length(padding / 2, Fixed));
}

static void setToggleSize(RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicator_size and indicator_spacing.
    gint indicator_size, indicator_spacing;

    switch (appearance) {
        case CheckboxPart:
            if (moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing) != MOZ_GTK_SUCCESS)
                return;
            break;
        case RadioPart:
            if (moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing) != MOZ_GTK_SUCCESS)
                return;
            break;
        default:
            return;
    }

    // Other ports hard-code this to 13, but GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    int length = indicator_size + indicator_spacing;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(length, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(length, Fixed));
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(style, RadioPart);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKBUTTON, o, i, rect);
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(style, RadioPart);
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_RADIOBUTTON, o, i, rect);
}

void RenderThemeGtk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // FIXME: Is this condition necessary?
    if (style->appearance() == PushButtonPart) {
        style->resetBorder();
        style->setHeight(Length(Auto));
        style->setWhiteSpace(PRE);
        setButtonPadding(style);
    } else {
        // FIXME: This should not be hard-coded.
        style->setMinHeight(Length(14, Fixed));
        style->resetBorderTop();
        style->resetBorderBottom();
    }
}

bool RenderThemeGtk::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_BUTTON, o, i, rect);
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozStyle(style, MOZ_GTK_DROPDOWN);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_DROPDOWN, o, i, rect);
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozStyle(style, MOZ_GTK_ENTRY);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_ENTRY, o, i, rect);
}

void RenderThemeGtk::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_DROPDOWN_ARROW, o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->base[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->base[GTK_STATE_ACTIVE]);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->text[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->text[GTK_STATE_ACTIVE]);
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->base[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->base[GTK_STATE_ACTIVE]);
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->text[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->text[GTK_STATE_ACTIVE]);
}

double RenderThemeGtk::caretBlinkFrequency() const
{
    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, NULL);

    if (!shouldBlink)
        return 0;

    return time / 2000.;
}

void RenderThemeGtk::systemFont(int propId, Document*, FontDescription& fontDescription) const
{
    switch (propId) {
        case CSSValueMenu:
            notImplemented();
            break;
        case CSSValueStatusBar:
            notImplemented();
            break;
        case CSSValueSmallCaption:
            notImplemented();
            break;
        case CSSValueWebkitSmallControl: {
            // TODO(mmoss) webkit/port/rendering/RenderThemeWin.cpp has
            // special handling for ChromiumBridge::layoutTestMode(). Will
            // Linux need special handling too, or are known styles (fonts,
            // etc.) already enforced elsewhere when in that mode?
            GtkWidget* widget = gtkEntry();
            PangoFontDescription* pangoFontDesc = widget->style->font_desc;
            // TODO(mmoss) - Windows descreases the size by 2pts. Should
            // Linux do the same?
            gint size = pango_font_description_get_size(pangoFontDesc) /
                PANGO_SCALE;
            float pixelscale = 0;
            if (pango_font_description_get_size_is_absolute(pangoFontDesc)) {
                // Already in pixels, no need to scale.
                pixelscale = 1.0;
            } else {
                gdouble dpi = -1;
                GdkScreen* screen = gtk_widget_has_screen(widget) ?
                    gtk_widget_get_screen(widget) : gdk_screen_get_default();
                if (screen)
                    dpi = gdk_screen_get_resolution(screen);
                if (dpi != -1)
                    pixelscale = dpi / 72.0;
            }
            // Only update if we can determine the right size.
            if (pixelscale && size) {
                fontDescription.firstFamily().setFamily(
                    pango_font_description_get_family(pangoFontDesc));
                fontDescription.setSpecifiedSize((float)size * pixelscale);
                fontDescription.setIsAbsoluteSize(true);
            }
            break;
        }
        case CSSValueWebkitMiniControl:
            notImplemented();
            break;
        case CSSValueWebkitControl:
            notImplemented();
            break;
        // TODO(mmoss) These are in WebKit/WebCore/rendering/RenderThemeWin.cpp
        // but webkit/port/rendering/RenderThemeWin.cpp doesn't specifically
        // handle them, so maybe we don't need to either.
        /*
        case CSSValueIcon:
            notImplemented();
            break;
        case CSSValueMessageBox:
            notImplemented();
            break;
        case CSSValueCaption:
            notImplemented();
            break;
        */
        default:
            notImplemented();
    }
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    // FIXME: Make sure this function doesn't get called many times for a single GTK+ style change signal.
    renderTheme->platformColorsDidChange();
}

GtkContainer* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
    m_gtkContainer = GTK_CONTAINER(gtk_fixed_new());
    gtk_container_add(GTK_CONTAINER(m_gtkWindow), GTK_WIDGET(m_gtkContainer));
    gtk_widget_realize(m_gtkWindow);

    return m_gtkContainer;
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;

    m_gtkEntry = gtk_entry_new();
    g_signal_connect(m_gtkEntry, "style-set", G_CALLBACK(gtkStyleSetCallback), theme());
    gtk_container_add(gtkContainer(), m_gtkEntry);
    gtk_widget_realize(m_gtkEntry);

    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;

    m_gtkTreeView = gtk_tree_view_new();
    g_signal_connect(m_gtkTreeView, "style-set", G_CALLBACK(gtkStyleSetCallback), theme());
    gtk_container_add(gtkContainer(), m_gtkTreeView);
    gtk_widget_realize(m_gtkTreeView);

    return m_gtkTreeView;
}

}
