# Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com> 
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(PORTROOT)/bindings/v8 \
    $(WebCore) \
    $(WebCore)/bindings/js \
    $(WebCore)/bindings/v8 \
    $(WebCore)/bindings/objc \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/html \
    $(WebCore)/inspector \
    $(WebCore)/page \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/xml \
    $(WebCore)/workers \
    $(WebCore)/svg \
#

.PHONY : all

ifeq ($(OS),MACOS)
all : \
    CharsetData.cpp
endif

# Not needed because we don't want obj-c bindings generated    
#   DOMAbstractView.h\
    DOMAttr.h \
    DOMCDATASection.h \
    DOMCSSCharsetRule.h \
    DOMCSSFontFaceRule.h \
    DOMCSSImportRule.h \
    DOMCSSMediaRule.h \
    DOMCSSPageRule.h \
    DOMCSSPrimitiveValue.h \
    DOMCSSRule.h \
    DOMCSSRuleList.h \
    DOMCSSStyleDeclaration.h \
    DOMCSSStyleRule.h \
    DOMCSSStyleSheet.h \
    DOMCSSUnknownRule.h \
    DOMCSSValue.h \
    DOMCSSValueList.h \
    DOMCharacterData.h \
    DOMComment.h \
    DOMCounter.h \
    DOMDOMImplementation.h \
    DOMDocument.h \
    DOMDocumentFragment.h \
    DOMDocumentType.h \
    DOMElement.h \
    DOMEntity.h \
    DOMEntityReference.h \
    DOMEvent.h \
    DOMEventListener.h \
    DOMEventTarget.h \
    DOMHTMLAnchorElement.h \
    DOMHTMLAppletElement.h \
    DOMHTMLAreaElement.h \
    DOMHTMLBRElement.h \
    DOMHTMLBaseElement.h \
    DOMHTMLBaseFontElement.h \
    DOMHTMLBodyElement.h \
    DOMHTMLButtonElement.h \
    DOMHTMLCanvasElement.h \
    DOMHTMLCollection.h \
    DOMHTMLDListElement.h \
    DOMHTMLDirectoryElement.h \
    DOMHTMLDivElement.h \
    DOMHTMLDocument.h \
    DOMHTMLElement.h \
    DOMHTMLEmbedElement.h \
    DOMHTMLFieldSetElement.h \
    DOMHTMLFontElement.h \
    DOMHTMLFormElement.h \
    DOMHTMLFrameElement.h \
    DOMHTMLFrameSetElement.h \
    DOMHTMLHRElement.h \
    DOMHTMLHeadElement.h \
    DOMHTMLHeadingElement.h \
    DOMHTMLHtmlElement.h \
    DOMHTMLIFrameElement.h \
    DOMHTMLImageElement.h \
    DOMHTMLInputElement.h \
    DOMHTMLIsIndexElement.h \
    DOMHTMLLIElement.h \
    DOMHTMLLabelElement.h \
    DOMHTMLLegendElement.h \
    DOMHTMLLinkElement.h \
    DOMHTMLMapElement.h \
    DOMHTMLMarqueeElement.h \
    DOMHTMLMenuElement.h \
    DOMHTMLMetaElement.h \
    DOMHTMLModElement.h \
    DOMHTMLOListElement.h \
    DOMHTMLObjectElement.h \
    DOMHTMLOptGroupElement.h \
    DOMHTMLOptionElement.h \
    DOMHTMLOptionsCollection.h \
    DOMHTMLParagraphElement.h \
    DOMHTMLParamElement.h \
    DOMHTMLPreElement.h \
    DOMHTMLQuoteElement.h \
    DOMHTMLScriptElement.h \
    DOMHTMLSelectElement.h \
    DOMHTMLStyleElement.h \
    DOMHTMLTableCaptionElement.h \
    DOMHTMLTableCellElement.h \
    DOMHTMLTableColElement.h \
    DOMHTMLTableElement.h \
    DOMHTMLTableRowElement.h \
    DOMHTMLTableSectionElement.h \
    DOMHTMLTextAreaElement.h \
    DOMHTMLTitleElement.h \
    DOMHTMLUListElement.h \
    DOMKeyboardEvent.h \
    DOMMessageEvent.h \
    DOMMediaList.h \
    DOMMouseEvent.h \
    DOMMutationEvent.h \
    DOMNamedNodeMap.h \
    DOMNode.h \
    DOMNodeFilter.h \
    DOMNodeIterator.h \
    DOMNodeList.h \
    DOMNotation.h \
    DOMOverflowEvent.h \
    DOMProcessingInstruction.h \
    DOMRGBColor.h \
    DOMRange.h \
    DOMRect.h \
    DOMSVGAElement.h \
    DOMSVGAngle.h \
    DOMSVGAnimateColorElement.h \
    DOMSVGAnimateElement.h \
    DOMSVGAnimateTransformElement.h \
    DOMSVGAnimatedAngle.h \
    DOMSVGAnimatedBoolean.h \
    DOMSVGAnimatedEnumeration.h \
    DOMSVGAnimatedInteger.h \
    DOMSVGAnimatedLength.h \
    DOMSVGAnimatedLengthList.h \
    DOMSVGAnimatedNumber.h \
    DOMSVGAnimatedNumberList.h \
    DOMSVGAnimatedPathData.h \
    DOMSVGAnimatedPoints.h \
    DOMSVGAnimatedPreserveAspectRatio.h \
    DOMSVGAnimatedRect.h \
    DOMSVGAnimatedString.h \
    DOMSVGAnimatedTransformList.h \
    DOMSVGAnimationElement.h \
    DOMSVGCircleElement.h \
    DOMSVGClipPathElement.h \
    DOMSVGColor.h \
    DOMSVGComponentTransferFunctionElement.h \
    DOMSVGCursorElement.h \
    DOMSVGDefinitionSrcElement.h \
    DOMSVGDefsElement.h \
    DOMSVGDescElement.h \
    DOMSVGDocument.h \
    DOMSVGElement.h \
    DOMSVGElementInstance.h \
    DOMSVGElementInstanceList.h \
    DOMSVGEllipseElement.h \
    DOMSVGExternalResourcesRequired.h \
    DOMSVGFEBlendElement.h \
    DOMSVGFEColorMatrixElement.h \
    DOMSVGFEComponentTransferElement.h \
    DOMSVGFECompositeElement.h \
    DOMSVGFEDiffuseLightingElement.h \
    DOMSVGFEDisplacementMapElement.h \
    DOMSVGFEDistantLightElement.h \
    DOMSVGFEFloodElement.h \
    DOMSVGFEFuncAElement.h \
    DOMSVGFEFuncBElement.h \
    DOMSVGFEFuncGElement.h \
    DOMSVGFEFuncRElement.h \
    DOMSVGFEGaussianBlurElement.h \
    DOMSVGFEImageElement.h \
    DOMSVGFEMergeElement.h \
    DOMSVGFEMergeNodeElement.h \
    DOMSVGFEOffsetElement.h \
    DOMSVGFEPointLightElement.h \
    DOMSVGFESpecularLightingElement.h \
    DOMSVGFESpotLightElement.h \
    DOMSVGFETileElement.h \
    DOMSVGFETurbulenceElement.h \
    DOMSVGFontElement.h \
    DOMSVGFontFaceElement.h \
    DOMSVGFontFaceFormatElement.h \
    DOMSVGFontFaceNameElement.h \
    DOMSVGFontFaceSrcElement.h \
    DOMSVGFontFaceUriElement.h \
    DOMSVGFilterElement.h \
    DOMSVGFilterPrimitiveStandardAttributes.h \
    DOMSVGFitToViewBox.h \
    DOMSVGForeignObjectElement.h \
    DOMSVGGElement.h \
    DOMSVGGlyphElement.h \
    DOMSVGGradientElement.h \
    DOMSVGImageElement.h \
    DOMSVGLangSpace.h \
    DOMSVGLength.h \
    DOMSVGLengthList.h \
    DOMSVGLineElement.h \
    DOMSVGLinearGradientElement.h \
    DOMSVGLocatable.h \
    DOMSVGMarkerElement.h \
    DOMSVGMaskElement.h \
    DOMSVGMatrix.h \
    DOMSVGMetadataElement.h \
    DOMSVGMissingGlyphElement.h \
    DOMSVGNumber.h \
    DOMSVGNumberList.h \
    DOMSVGPaint.h \
    DOMSVGPathElement.h \
    DOMSVGPathSeg.h \
    DOMSVGPathSegArcAbs.h \
    DOMSVGPathSegArcRel.h \
    DOMSVGPathSegClosePath.h \
    DOMSVGPathSegCurvetoCubicAbs.h \
    DOMSVGPathSegCurvetoCubicRel.h \
    DOMSVGPathSegCurvetoCubicSmoothAbs.h \
    DOMSVGPathSegCurvetoCubicSmoothRel.h \
    DOMSVGPathSegCurvetoQuadraticAbs.h \
    DOMSVGPathSegCurvetoQuadraticRel.h \
    DOMSVGPathSegCurvetoQuadraticSmoothAbs.h \
    DOMSVGPathSegCurvetoQuadraticSmoothRel.h \
    DOMSVGPathSegLinetoAbs.h \
    DOMSVGPathSegLinetoHorizontalAbs.h \
    DOMSVGPathSegLinetoHorizontalRel.h \
    DOMSVGPathSegLinetoRel.h \
    DOMSVGPathSegLinetoVerticalAbs.h \
    DOMSVGPathSegLinetoVerticalRel.h \
    DOMSVGPathSegList.h \
    DOMSVGPathSegMovetoAbs.h \
    DOMSVGPathSegMovetoRel.h \
    DOMSVGPatternElement.h \
    DOMSVGPoint.h \
    DOMSVGPointList.h \
    DOMSVGPolygonElement.h \
    DOMSVGPolylineElement.h \
    DOMSVGPreserveAspectRatio.h \
    DOMSVGRadialGradientElement.h \
    DOMSVGRect.h \
    DOMSVGRectElement.h \
    DOMSVGRenderingIntent.h \
    DOMSVGSVGElement.h \
    DOMSVGScriptElement.h \
    DOMSVGSetElement.h \
    DOMSVGStopElement.h \
    DOMSVGStringList.h \
    DOMSVGStylable.h \
    DOMSVGStyleElement.h \
    DOMSVGSwitchElement.h \
    DOMSVGSymbolElement.h \
    DOMSVGTRefElement.h \
    DOMSVGTSpanElement.h \
    DOMSVGTests.h \
    DOMSVGTextContentElement.h \
    DOMSVGTextElement.h \
    DOMSVGTextPathElement.h \
    DOMSVGTextPositioningElement.h \
    DOMSVGTitleElement.h \
    DOMSVGTransform.h \
    DOMSVGTransformList.h \
    DOMSVGTransformable.h \
    DOMSVGURIReference.h \
    DOMSVGUnitTypes.h \
    DOMSVGUseElement.h \
    DOMSVGViewElement.h \
    DOMSVGZoomAndPan.h \
    DOMSVGZoomEvent.h \
    DOMStyleSheet.h \
    DOMStyleSheetList.h \
    DOMText.h \
    DOMTextEvent.h \
    DOMTreeWalker.h \
    DOMUIEvent.h \
    DOMWheelEvent.h \
    DOMXPathExpression.h \
    DOMXPathNSResolver.h \
    DOMXPathResult.h \
endif

# Not needed for V8\
all : \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLEntityNames.c \
    JSAttr.h \
    JSBarInfo.h \
    JSCDATASection.h \
    JSCSSCharsetRule.h \
    JSCSSFontFaceRule.h \
    JSCSSImportRule.h \
    JSCSSMediaRule.h \
    JSCSSPageRule.h \
    JSCSSPrimitiveValue.h \
    JSCSSRule.h \
    JSCSSRuleList.h \
    JSCSSStyleRule.h \
    JSCSSStyleSheet.h \
    JSCSSValue.h \
    JSCSSValueList.h \
    JSCanvasGradient.h \
    JSCanvasPattern.h \
    JSCanvasRenderingContext2D.h \
    JSCharacterData.h \
    JSComment.h \
    JSConsole.h \
    JSCounter.h \
    JSCSSStyleDeclaration.h \
    JSDOMCoreException.h \
    JSDOMImplementation.h \
    JSDOMParser.h \
    JSDOMSelection.h \
    JSDOMWindow.h \
    JSDatabase.h \
    JSDocument.h \
    JSDocumentFragment.h \
    JSDocumentType.h \
    JSElement.h \
    JSEntity.h \
    JSEntityReference.h \
    JSEvent.h \
    JSEventException.h \
    JSEventTargetBase.lut.h \
    JSHTMLAnchorElement.h \
    JSHTMLAppletElement.h \
    JSHTMLAreaElement.h \
    JSHTMLAudioElement.h \
    JSHTMLBaseElement.h \
    JSHTMLBaseFontElement.h \
    JSHTMLBlockquoteElement.h \
    JSHTMLBodyElement.h \
    JSHTMLBRElement.h \
    JSHTMLButtonElement.h \
    JSHTMLCanvasElement.h \
    JSHTMLCollection.h \
    JSHTMLDListElement.h \
    JSHTMLDirectoryElement.h \
    JSHTMLDivElement.h \
    JSHTMLDocument.h \
    JSHTMLElement.h \
    JSHTMLEmbedElement.h \
    JSHTMLFieldSetElement.h \
    JSHTMLFontElement.h \
    JSHTMLFormElement.h \
    JSHTMLFrameElement.h \
    JSHTMLFrameSetElement.h \
    JSHTMLHRElement.h \
    JSHTMLHeadElement.h \
    JSHTMLHeadingElement.h \
    JSHTMLHtmlElement.h \
    JSHTMLIFrameElement.h \
    JSHTMLImageElement.h \
    JSHTMLInputElement.h \
    JSHTMLInputElementBaseTable.cpp \
    JSHTMLIsIndexElement.h \
    JSHTMLLIElement.h \
    JSHTMLLabelElement.h \
    JSHTMLLegendElement.h \
    JSHTMLLinkElement.h \
    JSHTMLMapElement.h \
    JSHTMLMarqueeElement.h \
    JSHTMLMediaElement.h \
    JSHTMLMenuElement.h \
    JSHTMLMetaElement.h \
    JSHTMLModElement.h \
    JSHTMLOListElement.h \
    JSHTMLOptGroupElement.h \
    JSHTMLObjectElement.h \
    JSHTMLOptionElement.h \
    JSHTMLOptionsCollection.h \
    JSHTMLParagraphElement.h \
    JSHTMLParamElement.h \
    JSHTMLPreElement.h \
    JSHTMLQuoteElement.h \
    JSHTMLScriptElement.h \
    JSHTMLSelectElement.h \
    JSHTMLSourceElement.h \
    JSHTMLStyleElement.h \
    JSHTMLTableCaptionElement.h \
    JSHTMLTableCellElement.h \
    JSHTMLTableColElement.h \
    JSHTMLTableElement.h \
    JSHTMLTableRowElement.h \
    JSHTMLTableSectionElement.h \
    JSHTMLTextAreaElement.h \
    JSHTMLTitleElement.h \
    JSHTMLUListElement.h \
    JSHTMLVideoElement.h \
    JSHistory.h \
    JSKeyboardEvent.h \
    JSLocation.lut.h \
    JSMediaError.h \
    JSMediaList.h \
    JSMessageEvent.h \
    JSMouseEvent.h \
    JSMutationEvent.h \
    JSNamedNodeMap.h \
    JSNode.h \
    JSNodeFilter.h \
    JSNodeIterator.h \
    JSNodeList.h \
    JSNotation.h \
    JSOverflowEvent.h \
    JSProcessingInstruction.h \
    JSProgressEvent.h \
    JSRange.h \
    JSRangeException.h \
    JSRect.h \
    JSSQLError.h \
    JSSQLResultSet.h \
    JSSQLResultSetRowList.h \
    JSSQLTransaction.h \
    JSSVGAElement.h \
    JSSVGAngle.h \
    JSSVGAnimatedAngle.h \
    JSSVGAnimateColorElement.h \
    JSSVGAnimateElement.h \
    JSSVGAnimateTransformElement.h \
    JSSVGAnimatedBoolean.h \
    JSSVGAnimatedEnumeration.h \
    JSSVGAnimatedInteger.h \
    JSSVGAnimatedLength.h \
    JSSVGAnimatedLengthList.h \
    JSSVGAnimatedNumber.h \
    JSSVGAnimatedNumberList.h \
    JSSVGAnimatedPreserveAspectRatio.h \
    JSSVGAnimatedRect.h \
    JSSVGAnimatedString.h \
    JSSVGAnimatedTransformList.h \
    JSSVGAnimationElement.h \
    JSSVGColor.h \
    JSSVGCircleElement.h \
    JSSVGClipPathElement.h \
    JSSVGComponentTransferFunctionElement.h \
    JSSVGCursorElement.h \
    JSSVGDefsElement.h \
    JSSVGDefinitionSrcElement.h \
    JSSVGDescElement.h \
    JSSVGDocument.h \
    JSSVGException.h \
    JSSVGLength.h \
    JSSVGMatrix.h \
    JSSVGMetadataElement.h \
    JSSVGPathElement.h \
    JSSVGPathSeg.h \
    JSSVGPathSegArcAbs.h \
    JSSVGPathSegArcRel.h \
    JSSVGPathSegClosePath.h \
    JSSVGPathSegCurvetoCubicAbs.h \
    JSSVGPathSegCurvetoCubicRel.h \
    JSSVGPathSegCurvetoCubicSmoothAbs.h \
    JSSVGPathSegCurvetoCubicSmoothRel.h \
    JSSVGPathSegCurvetoQuadraticAbs.h \
    JSSVGPathSegCurvetoQuadraticRel.h \
    JSSVGPathSegCurvetoQuadraticSmoothAbs.h \
    JSSVGPathSegCurvetoQuadraticSmoothRel.h \
    JSSVGPathSegLinetoAbs.h \
    JSSVGPathSegLinetoHorizontalAbs.h \
    JSSVGPathSegLinetoHorizontalRel.h \
    JSSVGPathSegLinetoRel.h \
    JSSVGPathSegLinetoVerticalAbs.h \
    JSSVGPathSegLinetoVerticalRel.h \
    JSSVGPathSegMovetoAbs.h \
    JSSVGPathSegMovetoRel.h \
    JSSVGNumber.h \
    JSSVGNumberList.h \
    JSSVGPaint.h \
    JSSVGPathSegList.h \
    JSSVGPatternElement.h \
    JSSVGPoint.h \
    JSSVGPointList.h \
    JSSVGPolygonElement.h \
    JSSVGPolylineElement.h \
    JSSVGRadialGradientElement.h \
    JSSVGRect.h \
    JSSVGRectElement.h \
    JSSVGRenderingIntent.h \
    JSSVGSetElement.h \
    JSSVGScriptElement.h \
    JSSVGStyleElement.h \
    JSSVGSwitchElement.h \
    JSSVGStopElement.h \
    JSSVGStringList.h \
    JSSVGSymbolElement.h \
    JSSVGTRefElement.h \
    JSSVGTSpanElement.h \
    JSSVGTextElement.h \
    JSSVGTextContentElement.h \
    JSSVGTextPathElement.h \
    JSSVGTextPositioningElement.h \
    JSSVGTitleElement.h \
    JSSVGTransform.h \
    JSSVGTransformList.h \
    JSSVGUnitTypes.h \
    JSSVGUseElement.h \
    JSSVGViewElement.h \
    JSSVGPreserveAspectRatio.h \
    JSSVGElement.h \
    JSSVGElementInstance.h \
    JSSVGElementInstanceList.h \
    JSSVGSVGElement.h \
    JSSVGEllipseElement.h \
    JSSVGFEBlendElement.h \
    JSSVGFEColorMatrixElement.h \
    JSSVGFEComponentTransferElement.h \
    JSSVGFECompositeElement.h \
    JSSVGFEDiffuseLightingElement.h \
    JSSVGFEDisplacementMapElement.h \
    JSSVGFEDistantLightElement.h \
    JSSVGFEFloodElement.h \
    JSSVGFEFuncAElement.h \
    JSSVGFEFuncBElement.h \
    JSSVGFEFuncGElement.h \
    JSSVGFEFuncRElement.h \
    JSSVGFEGaussianBlurElement.h \
    JSSVGFEImageElement.h \
    JSSVGFEMergeElement.h \
    JSSVGFEMergeNodeElement.h \
    JSSVGFEOffsetElement.h \
    JSSVGFEPointLightElement.h \
    JSSVGFESpecularLightingElement.h \
    JSSVGFESpotLightElement.h \
    JSSVGFETileElement.h \
    JSSVGFETurbulenceElement.h \
    JSSVGFilterElement.h \
    JSSVGFontElement.h \
    JSSVGFontFaceElement.h \
    JSSVGFontFaceFormatElement.h \
    JSSVGFontFaceNameElement.h \
    JSSVGFontFaceSrcElement.h \
    JSSVGFontFaceUriElement.h \
    JSSVGForeignObjectElement.h \
    JSSVGGElement.h \
    JSSVGGlyphElement.h \
    JSSVGGradientElement.h \
    JSSVGImageElement.h \
    JSSVGLength.h \
    JSSVGLengthList.h \
    JSSVGLineElement.h \
    JSSVGLinearGradientElement.h \
    JSSVGMaskElement.h \
    JSSVGMarkerElement.h \
    JSSVGMissingGlyphElement.h \
    JSSVGTransform.h \
    JSSVGZoomEvent.h \
    JSScreen.h \
    JSStyleSheet.h \
    JSStyleSheetList.h \
    JSText.h \
    JSTextEvent.h \
    JSTimeRanges.h \
    JSTreeWalker.h \
    JSUIEvent.h \
    JSVoidCallback.h \
    JSWheelEvent.h \
    JSXMLHttpRequest.lut.h \
    JSXMLHttpRequestException.h \
    JSXMLSerializer.h \
    JSXPathEvaluator.h \
    JSXPathException.h \
    JSXPathExpression.h \
    JSXPathNSResolver.h \
    JSXPathResult.h \
    JSXSLTProcessor.lut.h \
    SVGElementFactory.cpp \
    SVGNames.cpp \
    HTMLNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNames.cpp \
    XPathGrammar.cpp \
    kjs_css.lut.h \
    kjs_events.lut.h \
    kjs_navigator.lut.h \
    kjs_window.lut.h \
    tokenizer.cpp \
    WebCore.exp \
#

all : \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLElementFactory.cpp \
    HTMLEntityNames.c \
    V8Attr.h \
    V8BarInfo.h \
    V8CanvasPixelArray.h \
    V8ClientRect.h \
    V8ClientRectList.h \
    V8CDATASection.h \
    V8CSSCharsetRule.h \
    V8CSSFontFaceRule.h \
    V8CSSImportRule.h \
    V8CSSMediaRule.h \
    V8CSSPageRule.h \
    V8CSSPrimitiveValue.h \
    V8CSSRule.h \
    V8CSSRuleList.h \
    V8CSSStyleRule.h \
    V8CSSStyleSheet.h \
    V8CSSValue.h \
    V8CSSValueList.h \
    V8CanvasGradient.h \
    V8CanvasPattern.h \
    V8CanvasRenderingContext2D.h \
    V8CharacterData.h \
    V8Comment.h \
    V8Console.h \
    V8Counter.h \
    V8CSSStyleDeclaration.h \
    V8CSSVariablesDeclaration.h \
    V8CSSVariablesRule.h \
    V8DOMCoreException.h \
    V8DOMImplementation.h \
    V8DOMParser.h \
    V8DOMSelection.h \
    V8DOMStringList.h \
    V8DOMWindow.h \
    V8Database.h \
    V8Document.h \
    V8DocumentFragment.h \
    V8DocumentType.h \
    V8Element.h \
    V8Entity.h \
    V8EntityReference.h \
    V8Event.h \
    V8EventException.h \
    V8File.h \
    V8FileList.h \
    V8HTMLAnchorElement.h \
    V8HTMLAppletElement.h \
    V8HTMLAreaElement.h \
    V8HTMLAudioElement.h \
    V8HTMLBaseElement.h \
    V8HTMLBaseFontElement.h \
    V8HTMLBlockquoteElement.h \
    V8HTMLBodyElement.h \
    V8HTMLBRElement.h \
    V8HTMLButtonElement.h \
    V8HTMLCanvasElement.h \
    V8HTMLCollection.h \
    V8HTMLDListElement.h \
    V8HTMLDirectoryElement.h \
    V8HTMLDivElement.h \
    V8HTMLDocument.h \
    V8HTMLElement.h \
    V8HTMLEmbedElement.h \
    V8HTMLFieldSetElement.h \
    V8HTMLFontElement.h \
    V8HTMLFormElement.h \
    V8HTMLFrameElement.h \
    V8HTMLFrameSetElement.h \
    V8HTMLHRElement.h \
    V8HTMLHeadElement.h \
    V8HTMLHeadingElement.h \
    V8HTMLHtmlElement.h \
    V8HTMLIFrameElement.h \
    V8HTMLImageElement.h \
    V8HTMLInputElement.h \
    V8HTMLIsIndexElement.h \
    V8HTMLLIElement.h \
    V8HTMLLabelElement.h \
    V8HTMLLegendElement.h \
    V8HTMLLinkElement.h \
    V8HTMLMapElement.h \
    V8HTMLMarqueeElement.h \
    V8HTMLMediaElement.h \
    V8HTMLMenuElement.h \
    V8HTMLMetaElement.h \
    V8HTMLModElement.h \
    V8HTMLOListElement.h \
    V8HTMLOptGroupElement.h \
    V8HTMLObjectElement.h \
    V8HTMLOptionElement.h \
    V8HTMLOptionsCollection.h \
    V8HTMLParagraphElement.h \
    V8HTMLParamElement.h \
    V8HTMLPreElement.h \
    V8HTMLQuoteElement.h \
    V8HTMLScriptElement.h \
    V8HTMLSelectElement.h \
    V8HTMLSourceElement.h \
    V8HTMLStyleElement.h \
    V8HTMLTableCaptionElement.h \
    V8HTMLTableCellElement.h \
    V8HTMLTableColElement.h \
    V8HTMLTableElement.h \
    V8HTMLTableRowElement.h \
    V8HTMLTableSectionElement.h \
    V8HTMLTextAreaElement.h \
    V8HTMLTitleElement.h \
    V8HTMLUListElement.h \
    V8HTMLVideoElement.h \
    V8History.h \
    V8ImageData.h \
    V8KeyboardEvent.h \
    V8MediaError.h \
    V8MediaList.h \
    V8MessageChannel.h \
    V8MessageEvent.h \
    V8MessagePort.h \
    V8MouseEvent.h \
    V8MutationEvent.h \
    V8NamedNodeMap.h \
    V8Node.h \
    V8NodeFilter.h \
    V8NodeIterator.h \
    V8NodeList.h \
    V8Notation.h \
    V8OverflowEvent.h \
    V8ProcessingInstruction.h \
    V8ProgressEvent.h \
    V8Range.h \
    V8RangeException.h \
    V8Rect.h \
    V8SQLError.h \
    V8SQLResultSet.h \
    V8SQLResultSetRowList.h \
    V8SQLTransaction.h \
    V8SVGAElement.h \
    V8SVGAltGlyphElement.h \
    V8SVGAngle.h \
    V8SVGAnimatedAngle.h \
    V8SVGAnimateColorElement.h \
    V8SVGAnimateElement.h \
    V8SVGAnimateTransformElement.h \
    V8SVGAnimatedBoolean.h \
    V8SVGAnimatedEnumeration.h \
    V8SVGAnimatedInteger.h \
    V8SVGAnimatedLength.h \
    V8SVGAnimatedLengthList.h \
    V8SVGAnimatedNumber.h \
    V8SVGAnimatedNumberList.h \
    V8SVGAnimatedPreserveAspectRatio.h \
    V8SVGAnimatedRect.h \
    V8SVGAnimatedString.h \
    V8SVGAnimatedTransformList.h \
    V8SVGAnimationElement.h \
    V8SVGColor.h \
    V8SVGCircleElement.h \
    V8SVGClipPathElement.h \
    V8SVGComponentTransferFunctionElement.h \
    V8SVGCursorElement.h \
    V8SVGDefsElement.h \
    V8SVGDefinitionSrcElement.h \
    V8SVGDescElement.h \
    V8SVGDocument.h \
    V8SVGException.h \
    V8SVGLength.h \
    V8SVGMatrix.h \
    V8SVGMetadataElement.h \
    V8SVGPathElement.h \
    V8SVGPathSeg.h \
    V8SVGPathSegArcAbs.h \
    V8SVGPathSegArcRel.h \
    V8SVGPathSegClosePath.h \
    V8SVGPathSegCurvetoCubicAbs.h \
    V8SVGPathSegCurvetoCubicRel.h \
    V8SVGPathSegCurvetoCubicSmoothAbs.h \
    V8SVGPathSegCurvetoCubicSmoothRel.h \
    V8SVGPathSegCurvetoQuadraticAbs.h \
    V8SVGPathSegCurvetoQuadraticRel.h \
    V8SVGPathSegCurvetoQuadraticSmoothAbs.h \
    V8SVGPathSegCurvetoQuadraticSmoothRel.h \
    V8SVGPathSegLinetoAbs.h \
    V8SVGPathSegLinetoHorizontalAbs.h \
    V8SVGPathSegLinetoHorizontalRel.h \
    V8SVGPathSegLinetoRel.h \
    V8SVGPathSegLinetoVerticalAbs.h \
    V8SVGPathSegLinetoVerticalRel.h \
    V8SVGPathSegMovetoAbs.h \
    V8SVGPathSegMovetoRel.h \
    V8SVGNumber.h \
    V8SVGNumberList.h \
    V8SVGPaint.h \
    V8SVGPathSegList.h \
    V8SVGPatternElement.h \
    V8SVGPoint.h \
    V8SVGPointList.h \
    V8SVGPolygonElement.h \
    V8SVGPolylineElement.h \
    V8SVGRadialGradientElement.h \
    V8SVGRect.h \
    V8SVGRectElement.h \
    V8SVGRenderingIntent.h \
    V8SVGSetElement.h \
    V8SVGScriptElement.h \
    V8SVGStyleElement.h \
    V8SVGSwitchElement.h \
    V8SVGStopElement.h \
    V8SVGStringList.h \
    V8SVGSymbolElement.h \
    V8SVGTRefElement.h \
    V8SVGTSpanElement.h \
    V8SVGTextElement.h \
    V8SVGTextContentElement.h \
    V8SVGTextPathElement.h \
    V8SVGTextPositioningElement.h \
    V8SVGTitleElement.h \
    V8SVGTransform.h \
    V8SVGTransformList.h \
    V8SVGUnitTypes.h \
    V8SVGUseElement.h \
    V8SVGViewElement.h \
    V8SVGPreserveAspectRatio.h \
    V8SVGElement.h \
    V8SVGElementInstance.h \
    V8SVGElementInstanceList.h \
    V8SVGSVGElement.h \
    V8SVGEllipseElement.h \
    V8SVGFEBlendElement.h \
    V8SVGFEColorMatrixElement.h \
    V8SVGFEComponentTransferElement.h \
    V8SVGFECompositeElement.h \
    V8SVGFEDiffuseLightingElement.h \
    V8SVGFEDisplacementMapElement.h \
    V8SVGFEDistantLightElement.h \
    V8SVGFEFloodElement.h \
    V8SVGFEFuncAElement.h \
    V8SVGFEFuncBElement.h \
    V8SVGFEFuncGElement.h \
    V8SVGFEFuncRElement.h \
    V8SVGFEGaussianBlurElement.h \
    V8SVGFEImageElement.h \
    V8SVGFEMergeElement.h \
    V8SVGFEMergeNodeElement.h \
    V8SVGFEOffsetElement.h \
    V8SVGFEPointLightElement.h \
    V8SVGFESpecularLightingElement.h \
    V8SVGFESpotLightElement.h \
    V8SVGFETileElement.h \
    V8SVGFETurbulenceElement.h \
    V8SVGFilterElement.h \
    V8SVGFontElement.h \
    V8SVGFontFaceElement.h \
    V8SVGFontFaceFormatElement.h \
    V8SVGFontFaceNameElement.h \
    V8SVGFontFaceSrcElement.h \
    V8SVGFontFaceUriElement.h \
    V8SVGForeignObjectElement.h \
    V8SVGGElement.h \
    V8SVGGlyphElement.h \
    V8SVGGradientElement.h \
    V8SVGImageElement.h \
    V8SVGLength.h \
    V8SVGLengthList.h \
    V8SVGLineElement.h \
    V8SVGLinearGradientElement.h \
    V8SVGMaskElement.h \
    V8SVGMarkerElement.h \
    V8SVGMissingGlyphElement.h \
    V8SVGTransform.h \
    V8SVGZoomEvent.h \
    V8Screen.h \
    V8StyleSheet.h \
    V8StyleSheetList.h \
    V8Text.h \
    V8TextMetrics.h \
    V8TextEvent.h \
    V8TimeRanges.h \
    V8TreeWalker.h \
    V8UIEvent.h \
    V8VoidCallback.h \
    V8WebKitAnimationEvent.h \
    V8WebKitCSSKeyframeRule.h \
    V8WebKitCSSKeyframesRule.h \
    V8WebKitCSSMatrix.h \
    V8WebKitCSSTransformValue.h \
    V8WebKitPoint.h \
    V8WebKitTransitionEvent.h \
    V8WheelEvent.h \
    V8Worker.h \
    V8WorkerContext.h \
    V8WorkerLocation.h \
    V8WorkerNavigator.h \
    V8XMLHttpRequest.h \
    V8XMLHttpRequestException.h \
    V8XMLHttpRequestProgressEvent.h \
    V8XMLHttpRequestUpload.h \
    V8XMLSerializer.h \
    V8XPathEvaluator.h \
    V8XPathException.h \
    V8XPathExpression.h \
    V8XPathNSResolver.h \
    V8XPathResult.h \
    V8XSLTProcessor.h \
    SVGElementFactory.cpp \
    SVGNames.cpp \
    HTMLNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNames.cpp \
    XPathGrammar.cpp \
    tokenizer.cpp \
    V8Clipboard.h \
    V8InspectorController.h \
    V8Location.h \
    V8Navigator.h \
    V8MimeType.h \
    V8MimeTypeArray.h \
    V8Plugin.h \
    V8PluginArray.h \
    V8RGBColor.h \
    V8SVGAnimatedPoints.h \
    V8SVGURIReference.h \
    V8UndetectableHTMLCollection.h \
#

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSPropertyNames.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/SVGCSSPropertyNames.in
    WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in
endif

# Chromium does not support this.
#ifeq ($(ENABLE_DASHBOARD_SUPPORT), 1)
#    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/DashboardSupportCSSPropertyNames.in
#endif

CSSPropertyNames.h : $(WEBCORE_CSS_PROPERTY_NAMES) css/makeprop.pl
	if dos2unix $(WEBCORE_CSS_PROPERTY_NAMES) | sort | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $(WEBCORE_CSS_PROPERTY_NAMES) > CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : $(WEBCORE_CSS_VALUE_KEYWORDS) css/makevalues.pl
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	if dos2unix CSSValueKeywords.in | sort | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	perl "$(WebCore)/css/makevalues.pl"

# DOCTYPE strings

DocTypeStrings.cpp : html/DocTypeStrings.gperf
	gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

# HTML entity names

HTMLEntityNames.c : html/HTMLEntityNames.gperf
	gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@

# color names

ColorData.c : platform/ColorData.gperf
	gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@

# CSS tokenizer

tokenizer.cpp : css/tokenizer.flex css/maketokenizer
	flex -t $< | perl $(WebCore)/css/maketokenizer > $@

# CSS grammar
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	echo '#ifndef CSSGrammar_h' > CSSGrammar.h
	echo '#define CSSGrammar_h' >> CSSGrammar.h
	cat CSSGrammar.cpp.h CSSGrammar.hpp >> CSSGrammar.h
	echo '#endif' >> CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# XPath grammar
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

XPathGrammar.cpp : xml/XPathGrammar.y $(PROJECT_FILE)
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	echo '#ifndef XPathGrammar_h' > XPathGrammar.h
	echo '#define XPathGrammar_h' >> XPathGrammar.h
	cat XPathGrammar.cpp.h XPathGrammar.hpp >> XPathGrammar.h
	echo '#endif' >> XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html4.css $(WebCore)/css/quirks.css $(WebCore)/css/view-source.css $(WebCore)/css/themeWin.css $(WebCore)/css/themeWinQuirks.css

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/svg.css 
endif

ifeq ($(findstring ENABLE_WML,$(FEATURE_DEFINES)), ENABLE_WML)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/wml.css
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mediaControls.css
endif

UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS)
	perl $< $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# character set name table

CharsetData.cpp : platform/text/mac/make-charset-table.pl platform/text/mac/character-sets.txt $(ENCODINGS_FILE)
	perl $^ $(ENCODINGS_PREFIX) > $@

# lookup tables for old-style JavaScript bindings

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@
%Table.cpp: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@

# --------

# HTML tag and attribute names

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_VIDEO=1
endif

ifdef HTML_FLAGS

HTMLElementFactory.cpp HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory --extraDefines "$(HTML_FLAGS)"

else

HTMLElementFactory.cpp HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory

endif

XMLNames.cpp : dom/make_names.pl xml/xmlattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.exp

ifeq ($(findstring ENABLE_SVG_USE,$(FEATURE_DEFINES)), ENABLE_SVG_USE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_USE=1
endif

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

ifeq ($(findstring ENABLE_SVG_FILTERS,$(FEATURE_DEFINES)), ENABLE_SVG_FILTERS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FILTERS=1
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Filters.exp
endif

ifeq ($(findstring ENABLE_SVG_AS_IMAGE,$(FEATURE_DEFINES)), ENABLE_SVG_AS_IMAGE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_AS_IMAGE=1
endif

ifeq ($(findstring ENABLE_SVG_ANIMATION,$(FEATURE_DEFINES)), ENABLE_SVG_ANIMATION)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_ANIMATION=1
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Animation.exp
endif

ifeq ($(findstring ENABLE_SVG_FOREIGN_OBJECT,$(FEATURE_DEFINES)), ENABLE_SVG_FOREIGN_OBJECT)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FOREIGN_OBJECT=1
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.ForeignObject.exp
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)

ifdef SVG_FLAGS

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory
else

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory

endif

JSSVGElementWrapperFactory.cpp : SVGNames.cpp

XLinkNames.cpp : dom/make_names.pl svg/xlinkattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/svg/xlinkattrs.in

else

SVGElementFactory.cpp :
	echo > $@

SVGNames.cpp :
	echo > $@

XLinkNames.cpp :
	echo > $@

# This file is autogenerated by make_names.pl when SVG is enabled.

JSSVGElementWrapperFactory.cpp :
	echo > $@

endif

# new-style Objective-C bindings

OBJC_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorObjC.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

DOM%.h : %.idl $(OBJC_BINDINGS_SCRIPTS) $(PUBLICDOMINTERFACES)
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_OBJECTIVE_C" --generator ObjC --include dom --include html --include css --include page --include xml --include svg --include bindings/js  --include plugins --outputdir . $<

# new-style JavaScript bindings

JS_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorJS.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(JS_BINDINGS_SCRIPTS)
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --include css --include page --include xml --include svg --include bindings/js --include plugins --outputdir . $<

# new-style V8 bindings

V8_SCRIPTS = \
    $(PORTROOT)/bindings/scripts/CodeGenerator.pm \
    $(PORTROOT)/bindings/scripts/CodeGeneratorV8.pm \
    $(PORTROOT)/bindings/scripts/IDLParser.pm \
    $(WebCore)/bindings/scripts/IDLStructure.pm \
    $(PORTROOT)/bindings/scripts/generate-bindings.pl \
#

# Sometimes script silently fails (Cygwin problem?), 
# use a bounded loop to retry if so, but not do so forever.
V8%.h : %.idl $(V8_SCRIPTS)
	rm -f $@; \
	for i in 1 2 3 4 5 6 7 8 9 10; do \
	  if test -e $@; then break; fi; \
	  perl -w -I $(PORTROOT)/bindings/scripts -I $(WebCore)/bindings/scripts $(PORTROOT)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT V8_BINDING" --generator V8 --include svg --include dom --include html --include css --include page --include xml --include plugins --outputdir . $< ; \
	done
