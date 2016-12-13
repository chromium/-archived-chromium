#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.gather.tr_html'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import types
import unittest

from grit.gather import tr_html
from grit import clique
from grit import util


class ParserUnittest(unittest.TestCase):
  def testChunking(self):
    p = tr_html.HtmlChunks()
    chunks = p.Parse('<p>Hello <b>dear</b> how <i>are</i>you?<p>Fine!')
    self.failUnless(chunks == [
      (False, '<p>', ''), (True, 'Hello <b>dear</b> how <i>are</i>you?', ''),
      (False, '<p>', ''), (True, 'Fine!', '')])

    chunks = p.Parse('<p> Hello <b>dear</b> how <i>are</i>you? <p>Fine!')
    self.failUnless(chunks == [
      (False, '<p> ', ''), (True, 'Hello <b>dear</b> how <i>are</i>you?', ''),
      (False, ' <p>', ''), (True, 'Fine!', '')])

    chunks = p.Parse('<p> Hello <b>dear how <i>are you? <p> Fine!')
    self.failUnless(chunks == [
      (False, '<p> ', ''), (True, 'Hello <b>dear how <i>are you?', ''),
      (False, ' <p> ', ''), (True, 'Fine!', '')])

    # Ensure translateable sections that start with inline tags contain
    # the starting inline tag.
    chunks = p.Parse('<b>Hello!</b> how are you?<p><i>I am fine.</i>')
    self.failUnless(chunks == [
      (True, '<b>Hello!</b> how are you?', ''), (False, '<p>', ''),
      (True, '<i>I am fine.</i>', '')])

    # Ensure translateable sections that end with inline tags contain
    # the ending inline tag.
    chunks = p.Parse("Hello! How are <b>you?</b><p><i>I'm fine!</i>")
    self.failUnless(chunks == [
      (True, 'Hello! How are <b>you?</b>', ''), (False, '<p>', ''),
      (True, "<i>I'm fine!</i>", '')])

    # Check capitals and explicit descriptions
    chunks = p.Parse('<!-- desc=bingo! --><B>Hello!</B> how are you?<P><I>I am fine.</I>')
    self.failUnless(chunks == [
      (True, '<B>Hello!</B> how are you?', 'bingo!'), (False, '<P>', ''),
      (True, '<I>I am fine.</I>', '')])
    chunks = p.Parse('<B><!-- desc=bingo! -->Hello!</B> how are you?<P><I>I am fine.</I>')
    self.failUnless(chunks == [
      (True, '<B>Hello!</B> how are you?', 'bingo!'), (False, '<P>', ''),
      (True, '<I>I am fine.</I>', '')])
    # Linebreaks get changed to spaces just like any other HTML content
    chunks = p.Parse('<B>Hello!</B> <!-- desc=bi\nngo\n! -->how are you?<P><I>I am fine.</I>')
    self.failUnless(chunks == [
      (True, '<B>Hello!</B> how are you?', 'bi ngo !'), (False, '<P>', ''),
      (True, '<I>I am fine.</I>', '')])

    # In this case, because the explicit description appears after the first
    # translateable, it will actually apply to the second translateable.
    chunks = p.Parse('<B>Hello!</B> how are you?<!-- desc=bingo! --><P><I>I am fine.</I>')
    self.failUnless(chunks == [
      (True, '<B>Hello!</B> how are you?', ''), (False, '<P>', ''),
      (True, '<I>I am fine.</I>', 'bingo!')])

    # Check that replaceables within block tags (where attributes would go) are
    # handled correctly.
    chunks = p.Parse('<b>Hello!</b> how are you?<p [BINGO] [$~BONGO~$]>'
                     '<i>I am fine.</i>')
    self.failUnless(chunks == [
      (True, '<b>Hello!</b> how are you?', ''),
      (False, '<p [BINGO] [$~BONGO~$]>', ''),
      (True, '<i>I am fine.</i>', '')])

    # Check that the contents of preformatted tags preserve line breaks.
    chunks = p.Parse('<textarea>Hello\nthere\nhow\nare\nyou?</textarea>')
    self.failUnless(chunks == [(False, '<textarea>', ''),
      (True, 'Hello\nthere\nhow\nare\nyou?', ''), (False, '</textarea>', '')])

    # ...and that other tags' line breaks are converted to spaces
    chunks = p.Parse('<p>Hello\nthere\nhow\nare\nyou?</p>')
    self.failUnless(chunks == [(False, '<p>', ''),
      (True, 'Hello there how are you?', ''), (False, '</p>', '')])

  def testTranslateableAttributes(self):
    p = tr_html.HtmlChunks()

    # Check that the translateable attributes in <img>, <submit>, <button> and
    # <text> elements buttons are handled correctly.
    chunks = p.Parse('<img src=bingo.jpg alt="hello there">'
                     '<input type=submit value="hello">'
                     '<input type="button" value="hello">'
                     '<input type=\'text\' value=\'Howdie\'>')
    self.failUnless(chunks == [
      (False, '<img src=bingo.jpg alt="', ''), (True, 'hello there', ''),
      (False, '"><input type=submit value="', ''), (True, 'hello', ''),
      (False, '"><input type="button" value="', ''), (True, 'hello', ''),
      (False, '"><input type=\'text\' value=\'', ''), (True, 'Howdie', ''),
      (False, '\'>', '')])


  def testTranslateableHtmlToMessage(self):
    msg = tr_html.HtmlToMessage(
      'Hello <b>[USERNAME]</b>, &lt;how&gt;&nbsp;<i>are</i> you?')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    'Hello BEGIN_BOLDX_USERNAME_XEND_BOLD, '
                    '<how>&nbsp;BEGIN_ITALICareEND_ITALIC you?')

    msg = tr_html.HtmlToMessage('<b>Hello</b><I>Hello</I><b>Hello</b>')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    'BEGIN_BOLD_1HelloEND_BOLD_1BEGIN_ITALICHelloEND_ITALIC'
                    'BEGIN_BOLD_2HelloEND_BOLD_2')

    # Check that nesting (of the <font> tags) is handled correctly - i.e. that
    # the closing placeholder numbers match the opening placeholders.
    msg = tr_html.HtmlToMessage(
      '''<font size=-1><font color=#FF0000>Update!</font> '''
      '''<a href='http://desktop.google.com/whatsnew.html?hl=[$~LANG~$]'>'''
      '''New Features</a>: Now search PDFs, MP3s, Firefox web history, and '''
      '''more</font>''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    'BEGIN_FONT_1BEGIN_FONT_2Update!END_FONT_2 BEGIN_LINK'
                    'New FeaturesEND_LINK: Now search PDFs, MP3s, Firefox '
                    'web history, and moreEND_FONT_1')

    msg = tr_html.HtmlToMessage('''<a href='[$~URL~$]'><b>[NUM][CAT]</b></a>''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres == 'BEGIN_LINKBEGIN_BOLDX_NUM_XX_CAT_XEND_BOLDEND_LINK')

    msg = tr_html.HtmlToMessage(
      '''<font size=-1><a class=q onClick='return window.qs?qs(this):1' '''
      '''href='http://[WEBSERVER][SEARCH_URI]'>Desktop</a></font>&nbsp;&nbsp;'''
      '''&nbsp;&nbsp;''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    '''BEGIN_FONTBEGIN_LINKDesktopEND_LINKEND_FONTSPACE''')

    msg = tr_html.HtmlToMessage(
      '''<br><br><center><font size=-2>&copy;2005 Google </font></center>''', 1)
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    u'BEGIN_BREAK_1BEGIN_BREAK_2BEGIN_CENTERBEGIN_FONT\xa92005'
                    u' Google END_FONTEND_CENTER')

    msg = tr_html.HtmlToMessage(
      '''&nbsp;-&nbsp;<a class=c href=[$~CACHE~$]>Cached</a>''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    '&nbsp;-&nbsp;BEGIN_LINKCachedEND_LINK')

    # Check that upper-case tags are handled correctly.
    msg = tr_html.HtmlToMessage(
      '''You can read the <A HREF='http://desktop.google.com/privacypolicy.'''
      '''html?hl=[LANG_CODE]'>Privacy Policy</A> and <A HREF='http://desktop'''
      '''.google.com/privacyfaq.html?hl=[LANG_CODE]'>Privacy FAQ</A> online.''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres ==
                    'You can read the BEGIN_LINK_1Privacy PolicyEND_LINK_1 and '
                    'BEGIN_LINK_2Privacy FAQEND_LINK_2 online.')

    # Check that tags with linebreaks immediately preceding them are handled
    # correctly.
    msg = tr_html.HtmlToMessage(
      '''You can read the
<A HREF='http://desktop.google.com/privacypolicy.html?hl=[LANG_CODE]'>Privacy Policy</A>
and <A HREF='http://desktop.google.com/privacyfaq.html?hl=[LANG_CODE]'>Privacy FAQ</A> online.''')
    pres = msg.GetPresentableContent()
    self.failUnless(pres == '''You can read the
BEGIN_LINK_1Privacy PolicyEND_LINK_1
and BEGIN_LINK_2Privacy FAQEND_LINK_2 online.''')



class TrHtmlUnittest(unittest.TestCase):
  def testTable(self):
    html = tr_html.TrHtml('''<table class="shaded-header"><tr>
<td class="header-element b expand">Preferences</td>
<td class="header-element s">
<a href="http://desktop.google.com/preferences.html">Preferences&nbsp;Help</a>
</td>
</tr></table>''')
    html.Parse()
    self.failUnless(html.skeleton_[3].GetMessage().GetPresentableContent() ==
                    'BEGIN_LINKPreferences&nbsp;HelpEND_LINK')

  def testSubmitAttribute(self):
    html = tr_html.TrHtml('''</td>
<td class="header-element"><input type=submit value="Save Preferences"
name=submit2></td>
</tr></table>''')
    html.Parse()
    self.failUnless(html.skeleton_[1].GetMessage().GetPresentableContent() ==
                    'Save Preferences')

  def testWhitespaceAfterInlineTag(self):
    '''Test that even if there is whitespace after an inline tag at the start
    of a translateable section the inline tag will be included.
    '''
    html = tr_html.TrHtml('''<label for=DISPLAYNONE><font size=-1> Hello</font>''')
    html.Parse()
    self.failUnless(html.skeleton_[1].GetMessage().GetRealContent() ==
                    '<font size=-1> Hello</font>')

  def testSillyHeader(self):
    html = tr_html.TrHtml('''[!]
title\tHello
bingo
bongo
bla

<p>Other stuff</p>''')
    html.Parse()
    content = html.skeleton_[1].GetMessage().GetRealContent()
    self.failUnless(content == 'Hello')
    self.failUnless(html.skeleton_[-1] == '</p>')
    # Right after the translateable the nontranslateable should start with
    # a linebreak (this catches a bug we had).
    self.failUnless(html.skeleton_[2][0] == '\n')


  def testExplicitDescriptions(self):
    html = tr_html.TrHtml('Hello [USER]<br/><!-- desc=explicit --><input type="button">Go!</input>')
    html.Parse()
    msg = html.GetCliques()[1].GetMessage()
    self.failUnless(msg.GetDescription() == 'explicit')
    self.failUnless(msg.GetRealContent() == 'Go!')


  def testRegressionInToolbarAbout(self):
    html = tr_html.TrHtml.FromFile(
      util.PathFromRoot(r'grit/test/data/toolbar_about.html'))
    html.Parse()
    cliques = html.GetCliques()
    for cl in cliques:
      content = cl.GetMessage().GetRealContent()
      if content.count('De parvis grandis acervus erit'):
        self.failIf(content.count('$/translate'))


  def HtmlFromFileWithManualCheck(self, f):
    html = tr_html.TrHtml.FromFile(f)
    html.Parse()

    # For manual results inspection only...
    list = []
    for item in html.skeleton_:
      if isinstance(item, types.StringTypes):
        list.append(item)
      else:
        list.append(item.GetMessage().GetPresentableContent())

    return html


  def testPrivacyHtml(self):
    html = self.HtmlFromFileWithManualCheck(
      util.PathFromRoot(r'grit/test/data/privacy.html'))

    self.failUnless(html.skeleton_[1].GetMessage().GetRealContent() ==
                    'Privacy and Google Desktop Search')
    self.failUnless(html.skeleton_[3].startswith('<'))
    self.failUnless(len(html.skeleton_) > 10)


  def testPreferencesHtml(self):
    html = self.HtmlFromFileWithManualCheck(
      util.PathFromRoot(r'grit/test/data/preferences.html'))

    # Verify that we don't get '[STATUS-MESSAGE]' as the original content of
    # one of the MessageClique objects (it would be a placeholder-only message
    # and we're supposed to have stripped those).

    for item in filter(lambda x: isinstance(x, clique.MessageClique),
                       html.skeleton_):
      if (item.GetMessage().GetRealContent() == '[STATUS-MESSAGE]' or
          item.GetMessage().GetRealContent() == '[ADDIN-DO] [ADDIN-OPTIONS]'):
        self.fail()

    self.failUnless(len(html.skeleton_) > 100)

  def AssertNumberOfTranslateables(self, files, num):
    '''Fails if any of the files in files don't have exactly
    num translateable sections.

    Args:
      files: ['file1', 'file2']
      num: 3
    '''
    for f in files:
      f = util.PathFromRoot(r'grit/test/data/%s' % f)
      html = self.HtmlFromFileWithManualCheck(f)
      self.failUnless(len(html.GetCliques()) == num)

  def testFewTranslateables(self):
    self.AssertNumberOfTranslateables(['browser.html', 'email_thread.html',
                                       'header.html', 'mini.html',
                                       'oneclick.html', 'script.html',
                                       'time_related.html', 'versions.html'], 0)
    self.AssertNumberOfTranslateables(['footer.html', 'hover.html'], 1)

  def testOtherHtmlFilesForManualInspection(self):
    files = [
      'about.html', 'bad_browser.html', 'cache_prefix.html',
      'cache_prefix_file.html', 'chat_result.html', 'del_footer.html',
      'del_header.html', 'deleted.html', 'details.html', 'email_result.html',
      'error.html', 'explicit_web.html', 'footer.html',
      'homepage.html', 'indexing_speed.html',
      'install_prefs.html', 'install_prefs2.html',
      'oem_enable.html', 'oem_non_admin.html', 'onebox.html',
      'password.html', 'quit_apps.html', 'recrawl.html',
      'searchbox.html', 'sidebar_h.html', 'sidebar_v.html', 'status.html',
    ]
    for f in files:
      self.HtmlFromFileWithManualCheck(
        util.PathFromRoot(r'grit/test/data/%s' % f))

  def testTranslate(self):
    # Note that the English translation of documents that use character
    # literals (e.g. &copy;) will not be the same as the original document
    # because the character literal will be transformed into the Unicode
    # character itself.  So for this test we choose some relatively complex
    # HTML without character entities (but with &nbsp; because that's handled
    # specially).
    html = tr_html.TrHtml('''  <script>
      <!--
      function checkOffice() { var w = document.getElementById("h7");
      var e = document.getElementById("h8"); var o = document.getElementById("h10");
      if (!(w.checked || e.checked)) { o.checked=0;o.disabled=1;} else {o.disabled=0;} }
      // -->
        </script>
        <input type=checkbox [CHECK-DOC] name=DOC id=h7 onclick='checkOffice()'>
        <label for=h7> Word</label><br>
        <input type=checkbox [CHECK-XLS] name=XLS id=h8 onclick='checkOffice()'>
        <label for=h8> Excel</label><br>
        <input type=checkbox [CHECK-PPT] name=PPT id=h9>
        <label for=h9> PowerPoint</label><br>
        </span></td><td nowrap valign=top><span class="s">
        <input type=checkbox [CHECK-PDF] name=PDF id=hpdf>
        <label for=hpdf> PDF</label><br>
        <input type=checkbox [CHECK-TXT] name=TXT id=h6>
        <label for=h6> Text, media, and other files</label><br>
       </tr>&nbsp;&nbsp;
       <tr><td nowrap valign=top colspan=3><span class="s"><br />
       <input type=checkbox [CHECK-SECUREOFFICE] name=SECUREOFFICE id=h10>
        <label for=h10> Password-protected Office documents (Word, Excel)</label><br />
        <input type=checkbox [DISABLED-HTTPS] [CHECK-HTTPS] name=HTTPS id=h12><label
        for=h12> Secure pages (HTTPS) in web history</label></span></td></tr>
      </table>''')
    html.Parse()
    trans = html.Translate('en')
    if (html.GetText() != trans):
      self.fail()


  def testHtmlToMessageWithBlockTags(self):
    msg = tr_html.HtmlToMessage(
      'Hello<p>Howdie<img alt="bingo" src="image.gif">', True)
    result = msg.GetPresentableContent()
    self.failUnless(
      result == 'HelloBEGIN_PARAGRAPHHowdieBEGIN_BLOCKbingoEND_BLOCK')

    msg = tr_html.HtmlToMessage(
      'Hello<p>Howdie<input type="button" value="bingo">', True)
    result = msg.GetPresentableContent()
    self.failUnless(
      result == 'HelloBEGIN_PARAGRAPHHowdieBEGIN_BLOCKbingoEND_BLOCK')


  def testHtmlToMessageRegressions(self):
    msg = tr_html.HtmlToMessage(' - ', True)
    result = msg.GetPresentableContent()
    self.failUnless(result == ' - ')


  def testEscapeUnescaped(self):
    text = '&copy;&nbsp; & &quot;&lt;hello&gt;&quot;'
    unescaped = util.UnescapeHtml(text)
    self.failUnless(unescaped == u'\u00a9\u00a0 & "<hello>"')
    escaped_unescaped = util.EscapeHtml(unescaped, True)
    self.failUnless(escaped_unescaped ==
                    u'\u00a9\u00a0 &amp; &quot;&lt;hello&gt;&quot;')

  def testRegressionCjkHtmlFile(self):
    # TODO(joi) Fix this problem where unquoted attributes that
    # have a value that is CJK characters causes the regular expression
    # match never to return.  (culprit is the _ELEMENT regexp(
    if False:
      html = self.HtmlFromFileWithManualCheck(util.PathFromRoot(
        r'grit/test/data/ko_oem_enable_bug.html'))
      self.failUnless(True)

  def testRegressionCpuHang(self):
    # If this regression occurs, the unit test will never return
    html = tr_html.TrHtml(
      '''<input type=text size=12 id=advFileTypeEntry [~SHOW-FILETYPE-BOX~] value="[EXT]" name=ext>''')
    html.Parse()

if __name__ == '__main__':
  unittest.main()

