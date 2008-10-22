/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeWin.h"

#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>

#include "CSSValueKeywords.h"
#include "Document.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "PlatformScrollBar.h"
#include "SkiaUtils.h"

#include "base/gfx/native_theme.h"
#include "base/gfx/font_utils.h"
#include "base/win_util.h"
#include "webkit/glue/webkit_glue.h"

// These enums correspond to similarly named values defined by SafariTheme.h
enum ControlSize {
    RegularControlSize,
    SmallControlSize,
    MiniControlSize
};

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

namespace {
    const int kDefaultButtonPadding = 2;

    // These magic numbers come from Apple's version of RenderThemeWin.cpp.
    const int kMenuListPadding[4] = { 1, 2, 1, 2 };

    // The kLayoutTest* constants are metrics used only in layout test mode,
    // so as to match RenderThemeMac.mm and to remain consistent despite any
    //  theme or font changes.
    const int kLayoutTestControlHeight[3] = { 21, 18, 15 };
    const int kLayoutTestButtonPadding[4] = { 0, 8, 0, 8 };
    const int kLayoutTestStyledMenuListInternalPadding[4] = { 1, 0, 2, 8 };
    const int kLayoutTestMenuListInternalPadding[3][4] =
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 }
    };
    const int kLayoutTestMenuListMinimumWidth[3] = { 9, 5, 0 };
    const float kLayoutTestBaseFontSize = 11.0f;
    const float kLayoutTestStatusBarFontSize = 10.0f;
    const float kLayoutTestSystemFontSize = 13.0f;

    const int kLayoutTestSliderThumbWidth = 15;
    const int kLayoutTestSliderThumbHeight = 15;

    const int kLayoutTestMenuListButtonWidth = 15;
    const int kLayoutTestButtonMinHeight = 15;

    const int kLayoutTestSearchFieldHeight[3] = { 22, 19, 17 };
    const int kLayoutTestEmptyResultsOffset = 9;
    const int kLayoutTestResultsArrowWidth = 5;

    const short kLayoutTestSearchFieldBorderWidth = 2;
    const int kLayoutTestSearchFieldPadding = 1;



    // Constants that are used in non-layout-test mode.
    const int kStyledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

    // The default variable-width font size.  We use this as the default font
    // size for the "system font", and as a base size (which we then shrink) for
    // form control fonts.
    float DefaultFontSize = 16.0;

    WebCore::FontDescription SmallSystemFont;
    WebCore::FontDescription MenuFont;
    WebCore::FontDescription LabelFont;
}

namespace WebCore {
  
static void setFixedPadding(RenderStyle* style, const int padding[4])
{
    style->setPaddingLeft(Length(padding[LeftPadding], Fixed));
    style->setPaddingRight(Length(padding[RightPadding], Fixed));
    style->setPaddingTop(Length(padding[TopPadding], Fixed));
    style->setPaddingBottom(Length(padding[BottomPadding], Fixed));
}

// This is logic from RenderThemeMac.mm, and is used by layout test mode.
static ControlSize controlSizeForFont(RenderStyle* style)
{
    if (style->fontSize() >= 16) {
        return RegularControlSize;
    } else if (style->fontSize() >= 11) {
        return SmallControlSize;
    }
    return MiniControlSize;
}

RenderTheme* theme()
{
    static RenderThemeWin winTheme;
    return &winTheme;
}

RenderThemeWin::RenderThemeWin()
{
}

RenderThemeWin::~RenderThemeWin()
{
}

Color RenderThemeWin::platformActiveSelectionBackgroundColor() const
{
    if (webkit_glue::IsLayoutTestMode())
        return Color("#0000FF");  // Royal blue
    COLORREF color = GetSysColor(COLOR_HIGHLIGHT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformInactiveSelectionBackgroundColor() const
{
    if (webkit_glue::IsLayoutTestMode())
        return Color("#999999");  // Medium grey
    COLORREF color = GetSysColor(COLOR_GRAYTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformActiveSelectionForegroundColor() const
{
    if (webkit_glue::IsLayoutTestMode())
        return Color("#FFFFCC");  // Pale yellow
    COLORREF color = GetSysColor(COLOR_HIGHLIGHTTEXT);
    return Color(GetRValue(color), GetGValue(color), GetBValue(color), 255);
}

Color RenderThemeWin::platformInactiveSelectionForegroundColor() const
{
    return Color::white;
}

static float systemFontSizeForControlSize(ControlSize controlSize)
{
    static float sizes[] = { 13.0f, 11.0f, 9.0f };
    
    return sizes[controlSize];
}

// This is basically RenderThemeMac::setFontFromControlSize
static int layoutTestSetFontFromControlSize(CSSStyleSelector* selector, RenderStyle* style)
{
    FontDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::SerifFamily);

    float fontSize = systemFontSizeForControlSize(controlSizeForFont(style));
    fontDescription.firstFamily().setFamily("Lucida Grande");

    fontDescription.setComputedSize(fontSize);
    fontDescription.setSpecifiedSize(fontSize);
    
    // Reset line height
    style->setLineHeight(RenderStyle::initialLineHeight());

    style->setFontDescription(fontDescription);
        style->font().update(0);

    return 0;
}

// Return the height of system font |font| in pixels.  We use this size by
// default for some non-form-control elements.
static float systemFontSize(const LOGFONT& font)
{
    float size = -font.lfHeight;
    if (size < 0) {
        HFONT hFont = CreateFontIndirect(&font);
        if (hFont) {
            HDC hdc = GetDC(0);  // What about printing?  Is this the right DC?
            if (hdc) {
                HGDIOBJ hObject = SelectObject(hdc, hFont);
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                SelectObject(hdc, hObject);
                ReleaseDC(0, hdc);
                size = tm.tmAscent;
            }
            DeleteObject(hFont);
        }
    }

    // The "codepage 936" bit here is from Gecko; apparently this helps make
    // fonts more legible in Simplified Chinese where the default font size is
    // too small.
    // TODO(pkasting): http://b/1119883 Since this is only used for "small
    // caption", "menu", and "status bar" objects, I'm not sure how much this
    // even matters.  Plus the Gecko patch went in back in 2002, and maybe this
    // isn't even relevant anymore.  We should investigate whether this should
    // be removed, or perhaps broadened to be "any CJK locale".
    return ((size < 12.0f) && (GetACP() == 936)) ? 12.0f : size;
}

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// TODO(ojan): Fix this!
// The only case where we know we don't match IE is for ANSI encodings. IE uses
// MS Shell Dlg there, which we render incorrectly at certain pixel sizes
// (e.g. 15px). So, for now we just use Arial.
static wchar_t* defaultGUIFont(Document* document)
{
  UScriptCode dominantScript = document->dominantScript();
  const wchar_t* family = NULL;

  // TODO(jungshik) : Special-casing of Latin/Greeek/Cyrillic should go away
  // once GetFontFamilyForScript is enhanced to support GenericFamilyType for real.
  // For now, we make sure that we use Arial to match IE for those scripts.
  if (dominantScript != USCRIPT_LATIN && dominantScript != USCRIPT_CYRILLIC &&
      dominantScript != USCRIPT_GREEK && dominantScript != USCRIPT_INVALID_CODE) {
      family = gfx::GetFontFamilyForScript(dominantScript,
          gfx::GENERIC_FAMILY_NONE);
      if (family)
          return const_cast<wchar_t*>(family);
  } 
  return L"Arial";
}

// Converts |points| to pixels.  One point is 1/72 of an inch.
static float pointsToPixels(float points)
{
    static float pixelsPerInch = 0.0f;
    if (!pixelsPerInch) {
        HDC hdc = GetDC(0);  // What about printing?  Is this the right DC?
        if (hdc) {  // Can this ever actually be NULL?
            pixelsPerInch = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(0, hdc);
        } else {
            pixelsPerInch = 96.0f;
        }
    }

    static const float POINTS_PER_INCH = 72.0f;
    return points / POINTS_PER_INCH * pixelsPerInch;
}

double RenderThemeWin::caretBlinkFrequency() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (webkit_glue::IsLayoutTestMode())
        return 0;

    // TODO(ericroman): this should be using the platform's blink frequency.
    return RenderTheme::caretBlinkFrequency();
}

void RenderThemeWin::systemFont(int propId, Document* document, FontDescription& fontDescription) const
{
    // This logic owes much to RenderThemeSafari.cpp.
    FontDescription* cachedDesc = NULL;
    wchar_t* faceName = 0;
    float fontSize = 0;
    switch (propId) {
        case CSSValueSmallCaption:
            cachedDesc = &SmallSystemFont;
            if (!SmallSystemFont.isAbsoluteSize()) {
                if (webkit_glue::IsLayoutTestMode()) {
                    fontSize = systemFontSizeForControlSize(SmallControlSize);
                } else {
                    NONCLIENTMETRICS metrics;
                    win_util::GetNonClientMetrics(&metrics);
                    faceName = metrics.lfSmCaptionFont.lfFaceName;
                    fontSize = systemFontSize(metrics.lfSmCaptionFont);
                }
            }
            break;
        case CSSValueMenu:
            cachedDesc = &MenuFont;
            if (!MenuFont.isAbsoluteSize()) {
                if (webkit_glue::IsLayoutTestMode()) {
                    fontSize = systemFontSizeForControlSize(RegularControlSize);
                } else {
                    NONCLIENTMETRICS metrics;
                    win_util::GetNonClientMetrics(&metrics);
                    faceName = metrics.lfMenuFont.lfFaceName;
                    fontSize = systemFontSize(metrics.lfMenuFont);
                }
            }
            break;
        case CSSValueStatusBar:
            cachedDesc = &LabelFont;
            if (!LabelFont.isAbsoluteSize()) {
                if (webkit_glue::IsLayoutTestMode()) {
                    fontSize = kLayoutTestStatusBarFontSize;
                } else {
                    NONCLIENTMETRICS metrics;
                    win_util::GetNonClientMetrics(&metrics);
                    faceName = metrics.lfStatusFont.lfFaceName;
                    fontSize = systemFontSize(metrics.lfStatusFont);
                }
            }
            break;
        case CSSValueWebkitMiniControl:
            if (webkit_glue::IsLayoutTestMode()) {
                fontSize = systemFontSizeForControlSize(MiniControlSize);
            } else {
                faceName = defaultGUIFont(document);
                // Why 2 points smaller?  Because that's what Gecko does.
                // Also see 2 places below.
                fontSize = DefaultFontSize - pointsToPixels(2);
            }
            break;
        case CSSValueWebkitSmallControl:
              if (webkit_glue::IsLayoutTestMode()) {
                  fontSize = systemFontSizeForControlSize(SmallControlSize);
              } else {
                  faceName = defaultGUIFont(document);
                  fontSize = DefaultFontSize - pointsToPixels(2);
              }
            break;
        case CSSValueWebkitControl:
            if (webkit_glue::IsLayoutTestMode()) {
                fontSize = systemFontSizeForControlSize(RegularControlSize);
            } else {
                faceName = defaultGUIFont(document);
                fontSize = DefaultFontSize - pointsToPixels(2);
            }
            break;
        default:
            if (webkit_glue::IsLayoutTestMode()) {
                fontSize = kLayoutTestSystemFontSize;
            } else {
                faceName = defaultGUIFont(document);
                fontSize = DefaultFontSize;
            }
    }

    if (!cachedDesc)
        cachedDesc = &fontDescription;

    if (fontSize) {
        if (webkit_glue::IsLayoutTestMode()) {
          cachedDesc->firstFamily().setFamily("Lucida Grande");
        } else {
          ASSERT(faceName);
          cachedDesc->firstFamily().setFamily(AtomicString(faceName,
                                                           wcslen(faceName)));
        }
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->setSpecifiedSize(fontSize);
        cachedDesc->setWeight(FontWeightNormal);
        cachedDesc->setItalic(false);
    }
    fontDescription = *cachedDesc;
}

bool RenderThemeWin::supportsFocus(EAppearance appearance)
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
            return true;
        default:
            return false;
    }

    return false;
}

bool RenderThemeWin::supportsFocusRing(const RenderStyle* style) const
{
   // Let webkit draw one of its halo rings around any focused element,
   // except push buttons. For buttons we use the windows PBS_DEFAULTED
   // styling to give it a blue border.
   return style->appearance() == ButtonAppearance ||
          style->appearance() == PushButtonAppearance;
}

unsigned RenderThemeWin::determineState(RenderObject* o)
{
    unsigned result = TS_NORMAL;
    if (!isEnabled(o))
        result = TS_DISABLED;
    else if (isReadOnlyControl(o))
        result = ETS_READONLY; // Readonly is supported on textfields.
    else if (isPressed(o)) // Active overrides hover and focused.
        result = TS_PRESSED;
    else if (supportsFocus(o->style()->appearance()) && isFocused(o))
        result = TS_CHECKED;
    else if (isHovered(o))
        result = TS_HOT;
    if (isChecked(o))
        result += 4; // 4 unchecked states, 4 checked states.
    return result;
}

unsigned RenderThemeWin::determineClassicState(RenderObject* o)
{
    unsigned result = 0;
    if (!isEnabled(o) || isReadOnlyControl(o))
        result = DFCS_INACTIVE;
    else if (isPressed(o)) // Active supersedes hover
        result = DFCS_PUSHED;
    else if (isHovered(o))
        result = DFCS_HOT;
    if (isChecked(o))
        result |= DFCS_CHECKED;
    return result;
}

ThemeData RenderThemeWin::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
        case PushButtonAppearance:
        case ButtonAppearance:
            result.m_part = BP_PUSHBUTTON;
            result.m_classicState = DFCS_BUTTONPUSH;
            break;
        case CheckboxAppearance:
            result.m_part = BP_CHECKBOX;
            result.m_classicState = DFCS_BUTTONCHECK;
            break;
        case RadioAppearance:
            result.m_part = BP_RADIOBUTTON;
            result.m_classicState = DFCS_BUTTONRADIO;
            break;
        case ListboxAppearance:
        case MenulistAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
            result.m_part = ETS_NORMAL;
            break;
    }

    result.m_state = determineState(o);
    result.m_classicState |= determineClassicState(o);

    return result;
}

bool RenderThemeWin::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // Get the correct theme data for a button and paint the button.
    i.context->platformContext()->paintButton(r, getThemeData(o));
    return false;
}

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

int RenderThemeWin::minimumMenuListSize(RenderStyle* style) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        return kLayoutTestMenuListMinimumWidth[controlSizeForFont(style)];
    } else {
        return 0;
    }
}

static IntSize layoutTestCheckboxSize(RenderStyle* style)
{
    static const IntSize sizes[3] = { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) };
    return sizes[controlSizeForFont(style)];
}

static IntSize layoutTestRadioboxSize(RenderStyle* style)
{
    static const IntSize sizes[3] = { IntSize(14, 15), IntSize(12, 13), IntSize(10, 10) };
    return sizes[controlSizeForFont(style)];
}

void RenderThemeWin::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary for now.  It matches Firefox.
    // At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
    // the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
    // metrics.
    const IntSize size = webkit_glue::IsLayoutTestMode() ?
        layoutTestCheckboxSize(style) : IntSize(13, 13);
    setSizeIfAuto(style, size);
}

void RenderThemeWin::setRadioSize(RenderStyle* style) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        setSizeIfAuto(style, layoutTestRadioboxSize(style));
    } else {
        // Use same sizing for radio box as checkbox.
        setCheckboxSize(style);
    }
}

bool RenderThemeWin::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextFieldInternal(o, i, r, true);
}

bool RenderThemeWin::paintTextFieldInternal(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r, bool drawEdges)
{
    // Nasty hack to make us not paint the border on text fields with a border-radius.
    // Webkit paints elements with border-radius for us.
    // TODO(ojan): Get rid of this if-check once we can properly clip rounded borders
    // http://b/1112604 and http://b/1108635
    // TODO(ojan): make sure we do the right thing if css background-clip is set.
    if (o->style()->hasBorderRadius())
      return false;

    // Get the correct theme data for a textfield and paint the text field.
    i.context->platformContext()->paintTextField(r, getThemeData(o), o->style()->backgroundColor().rgb(), drawEdges);
    return false;
}

bool RenderThemeWin::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeWin::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());

    if (webkit_glue::IsLayoutTestMode()) {
        style->resetBorder();
        style->setHeight(Length(Auto));
        // Select one of the 3 fixed heights for controls
        style->resetPadding();
        if (style->height().isAuto()) {
            // RenderThemeMac locks the size to 3 distinct values (NSControlSize).
            // We on the other hand, base the height off the font.
            int fixedHeight = kLayoutTestControlHeight[controlSizeForFont(style)];
            style->setHeight(Length(fixedHeight, Fixed));
        }
        layoutTestSetFontFromControlSize(selector, style);
    }
}

void RenderThemeWin::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

// Used to paint unstyled menulists (i.e. with the default border)
bool RenderThemeWin::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    int borderRight = o->borderRight();
    int borderLeft = o->borderLeft();
    int borderTop = o->borderTop();
    int borderBottom = o->borderBottom();

    // If all the borders are 0, then tell skia not to paint the border on the textfield.
    // TODO(ojan): http://b/1210017 Figure out how to get Windows to not draw individual 
    // borders and then pass that to skia so we can avoid drawing any borders that are
    // set to 0. For non-zero borders, we draw the border, but webkit just draws
    // over it.
    // TODO(ojan): layout-test-mode removes borders, so we end up never drawing
    // edges in layout-test-mode. See adjustMenuListStyle, style->resetBorder().
    // We really need to remove the layout-test-mode only hacks.
    bool drawEdges = !(borderRight == 0 && borderLeft == 0 && borderTop == 0 && borderBottom == 0);

    paintTextFieldInternal(o, i, r, drawEdges);

    // Take padding and border into account.
    // If the MenuList is smaller than the size of a button, make sure to
    // shrink it appropriately and not put its x position to the left of 
    // the menulist.
    const int buttonWidth = webkit_glue::IsLayoutTestMode() ?
        kLayoutTestMenuListButtonWidth : GetSystemMetrics(SM_CXVSCROLL);
    int spacingLeft = borderLeft + o->paddingLeft();
    int spacingRight = borderRight + o->paddingRight();
    int spacingTop = borderTop + o->paddingTop();
    int spacingBottom = borderBottom + o->paddingBottom();

    int buttonX;
    if (r.right() - r.x() < buttonWidth) {
      buttonX = r.x();
    } else {
      buttonX = o->style()->direction() == LTR ? r.right() - spacingRight - buttonWidth : r.x() + spacingLeft;
    }
    IntRect buttonRect(buttonX,
                       r.y() + spacingTop,
                       std::min(buttonWidth, r.right() - r.x()),
                       r.height() - (spacingTop + spacingBottom));

    // Get the correct theme data for a textfield and paint the menu.
    i.context->platformContext()->paintMenuListArrowButton(buttonRect, determineState(o), determineClassicState(o));
    return false;
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeWin::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintMenuList(o, i, r);
}

int RenderThemeWin::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeWin::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeWin::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeWin::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

// Hacks for using Mac menu list metrics when in layout test mode.
static int layoutTestMenuListInternalPadding(RenderStyle* style, int paddingType)
{
    if (style->appearance() == MenulistAppearance) {
        return kLayoutTestMenuListInternalPadding[controlSizeForFont(style)][paddingType];
    }
    if (style->appearance() == MenulistButtonAppearance) {
        if (paddingType == RightPadding) {
            const float baseArrowWidth = 5.0f;
            float fontScale = style->fontSize() / kLayoutTestBaseFontSize;
            float arrowWidth = ceilf(baseArrowWidth * fontScale);

            int arrowPaddingLeft = 6;
            int arrowPaddingRight = 6;
            int paddingBeforeSeparator = 4;
            // Add 2 for separator space, seems to match RenderThemeMac::paintMenuListButton.
            return static_cast<int>(arrowWidth + arrowPaddingLeft + arrowPaddingRight +
                                    paddingBeforeSeparator);
        } else {
            return kLayoutTestStyledMenuListInternalPadding[paddingType];
        }
    }
    return 0;
}

int RenderThemeWin::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        return layoutTestMenuListInternalPadding(style, paddingType);
    }

    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = kStyledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is 
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // RenderMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int bar_type = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == bar_type && style->appearance() != NoAppearance)
        padding += PlatformScrollbar::verticalScrollbarWidth();

    return padding;
}

void RenderThemeWin::adjustButtonInnerStyle(RenderStyle* style) const 
{ 
    // This inner padding matches Firefox.
    style->setPaddingTop(Length(1, Fixed));
    style->setPaddingRight(Length(3, Fixed));
    style->setPaddingBottom(Length(1, Fixed));
    style->setPaddingLeft(Length(3, Fixed));
}

void RenderThemeWin::setButtonPadding(RenderStyle* style) const
{  
    if (webkit_glue::IsLayoutTestMode()) {
        setFixedPadding(style, kLayoutTestButtonPadding);

    } else if (!style->width().isAuto()) {
        // We need to set the minimum padding for the buttons, or else they
        // render too small and clip the button face text. The right way to do
        // this is to ask window's theme manager to give us the minimum 
        // (TS_MIN) size for the part. As a failsafe we set at least 
        // kDefaultButtonPadding because zero just looks bad.
        Length minXPadding(kDefaultButtonPadding, Fixed);
        Length minYPadding(kDefaultButtonPadding, Fixed);
        // Find minimum padding.
        getMinimalButtonPadding(&minXPadding);

        // Set the minimum padding.
        if (style->paddingLeft().value() < minXPadding.value()) {
            style->setPaddingLeft(minXPadding);
        }
        if (style->paddingRight().value() < minXPadding.value()) {
            style->setPaddingRight(minXPadding);
        }
        if (style->paddingBottom().value() < minYPadding.value()) { 
            style->setPaddingBottom(minYPadding);
        }
        if (style->paddingTop().value() < minYPadding.value()) { 
            style->setPaddingTop(minYPadding);
        }
    }
}

void RenderThemeWin::adjustSliderThumbSize(RenderObject* o) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        if (o->style()->appearance() == SliderThumbHorizontalAppearance ||
            o->style()->appearance() == SliderThumbVerticalAppearance) {
            o->style()->setWidth(Length(kLayoutTestSliderThumbWidth, Fixed));
            o->style()->setHeight(Length(kLayoutTestSliderThumbHeight, Fixed));
        }
    }
}

void RenderThemeWin::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        // Override border.
        style->resetBorder();
        style->setBorderLeftWidth(kLayoutTestSearchFieldBorderWidth);
        style->setBorderLeftStyle(INSET);
        style->setBorderRightWidth(kLayoutTestSearchFieldBorderWidth);
        style->setBorderRightStyle(INSET);
        style->setBorderBottomWidth(kLayoutTestSearchFieldBorderWidth);
        style->setBorderBottomStyle(INSET);
        style->setBorderTopWidth(kLayoutTestSearchFieldBorderWidth);
        style->setBorderTopStyle(INSET);

        // Override height.
        style->setHeight(Length(
            kLayoutTestSearchFieldHeight[controlSizeForFont(style)],
            Fixed));

        // Override padding size to match AppKit text positioning.
        style->setPaddingLeft(Length(kLayoutTestSearchFieldPadding, Fixed));
        style->setPaddingRight(Length(kLayoutTestSearchFieldPadding, Fixed));
        style->setPaddingTop(Length(kLayoutTestSearchFieldPadding, Fixed));
        style->setPaddingBottom(Length(kLayoutTestSearchFieldPadding, Fixed));

        style->setBoxShadow(0);
    }
}

static const IntSize* layoutTestCancelButtonSizes()
{
    static const IntSize sizes[3] = { IntSize(16, 13), IntSize(13, 11), IntSize(13, 9) };
    return sizes;
}

static const IntSize* layoutTestResultsButtonSizes()
{
    static const IntSize sizes[3] = { IntSize(19, 13), IntSize(17, 11), IntSize(17, 9) };
    return sizes;
}

void RenderThemeWin::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        IntSize size = layoutTestCancelButtonSizes()[controlSizeForFont(style)];
        style->setWidth(Length(size.width(), Fixed));
        style->setHeight(Length(size.height(), Fixed));
        style->setBoxShadow(0);
    }
}

void RenderThemeWin::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        IntSize size = layoutTestResultsButtonSizes()[controlSizeForFont(style)];
        style->setWidth(Length(size.width() - kLayoutTestEmptyResultsOffset, Fixed));
        style->setHeight(Length(size.height(), Fixed));
        style->setBoxShadow(0);
    }
}

void RenderThemeWin::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        IntSize size = layoutTestResultsButtonSizes()[controlSizeForFont(style)];
        style->setWidth(Length(size.width(), Fixed));
        style->setHeight(Length(size.height(), Fixed));
        style->setBoxShadow(0);
    }
}

void RenderThemeWin::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (webkit_glue::IsLayoutTestMode()) {
        IntSize size = layoutTestResultsButtonSizes()[controlSizeForFont(style)];
        style->setWidth(Length(size.width() + kLayoutTestResultsArrowWidth, Fixed));
        style->setHeight(Length(size.height(), Fixed));
        style->setBoxShadow(0);
    }
}

void RenderThemeWin::getMinimalButtonPadding(Length* minXPadding) const {
    // TODO(maruel): This get messy if 1. the theme change; 2. we are serializing.
    SIZE size;
    if (SUCCEEDED(gfx::NativeTheme::instance()->GetThemePartSize(
            gfx::NativeTheme::BUTTON, 0, BP_PUSHBUTTON, PBS_NORMAL, 0, TS_MIN,
            &size))) {
        *minXPadding = Length(size.cx, Fixed);
    }
}

// static
void RenderThemeWin::setDefaultFontSize(int fontSize) {
    DefaultFontSize = static_cast<float>(fontSize);

    // Reset cached fonts.
    SmallSystemFont = MenuFont = LabelFont = FontDescription();
}

}
