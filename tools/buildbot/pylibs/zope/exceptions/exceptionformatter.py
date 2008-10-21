##############################################################################
#
# Copyright (c) 2001, 2002 Zope Corporation and Contributors.
# All Rights Reserved.
#
# This software is subject to the provisions of the Zope Public License,
# Version 2.1 (ZPL).  A copy of the ZPL should accompany this distribution.
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY AND ALL EXPRESS OR IMPLIED
# WARRANTIES ARE DISCLAIMED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF TITLE, MERCHANTABILITY, AGAINST INFRINGEMENT, AND FITNESS
# FOR A PARTICULAR PURPOSE.
#
##############################################################################
"""An exception formatter that shows traceback supplements and traceback info,
optionally in HTML.

$Id: exceptionformatter.py 68429 2006-06-01 10:54:18Z mgedmin $
"""
import sys
import cgi
import linecache
import traceback

DEBUG_EXCEPTION_FORMATTER = 1

class TextExceptionFormatter(object):

    line_sep = '\n'
    show_revisions = 0

    def __init__(self, limit=None, with_filenames=False):
        self.limit = limit
        self.with_filenames = with_filenames

    def escape(self, s):
        return s

    def getPrefix(self):
        return 'Traceback (most recent call last):'

    def getLimit(self):
        limit = self.limit
        if limit is None:
            limit = getattr(sys, 'tracebacklimit', 200)
        return limit

    def formatSupplementLine(self, line):
        return '   - %s' % line

    def formatSourceURL(self, url):
        return [self.formatSupplementLine(url)]

    def formatSupplement(self, supplement, tb):
        result = []
        fmtLine = self.formatSupplementLine

        url = getattr(supplement, 'source_url', None)
        if url is not None:
            result.extend(self.formatSourceURL(url))

        line = getattr(supplement, 'line', 0)
        if line == -1:
            line = tb.tb_lineno
        col = getattr(supplement, 'column', -1)
        if line:
            if col is not None and col >= 0:
                result.append(fmtLine('Line %s, Column %s' % (
                    line, col)))
            else:
                result.append(fmtLine('Line %s' % line))
        elif col is not None and col >= 0:
            result.append(fmtLine('Column %s' % col))

        expr = getattr(supplement, 'expression', None)
        if expr:
            result.append(fmtLine('Expression: %s' % expr))

        warnings = getattr(supplement, 'warnings', None)
        if warnings:
            for warning in warnings:
                result.append(fmtLine('Warning: %s' % warning))

        getInfo = getattr(supplement, 'getInfo', None)
        if getInfo is not None:
            try:
                extra = getInfo()
                if extra:
                    extra = self.escape(extra)
                    if self.line_sep != "\n":
                        extra = extra.replace(" ", "&nbsp;")
                        extra = extra.replace("\n", self.line_sep)
                    result.append(extra)
            except:
                if DEBUG_EXCEPTION_FORMATTER:
                    import traceback
                    traceback.print_exc()
                # else just swallow the exception.
        return result

    def formatTracebackInfo(self, tbi):
        return self.formatSupplementLine('__traceback_info__: %s' % (tbi, ))

    def formatLine(self, tb):
        f = tb.tb_frame
        lineno = tb.tb_lineno
        co = f.f_code
        filename = co.co_filename
        name = co.co_name
        locals = f.f_locals
        globals = f.f_globals

        if self.with_filenames:
            s = '  File "%s", line %d' % (filename, lineno)
        else:
            modname = globals.get('__name__', filename)
            s = '  Module %s, line %d' % (modname, lineno)

        s = s + ', in %s' % name

        result = []
        result.append(self.escape(s))

        # Append the source line, if available
        line = linecache.getline(filename, lineno)
        if line:
            result.append("    " + self.escape(line.strip()))

        # Output a traceback supplement, if any.
        if '__traceback_supplement__' in locals:
            # Use the supplement defined in the function.
            tbs = locals['__traceback_supplement__']
        elif '__traceback_supplement__' in globals:
            # Use the supplement defined in the module.
            # This is used by Scripts (Python).
            tbs = globals['__traceback_supplement__']
        else:
            tbs = None
        if tbs is not None:
            factory = tbs[0]
            args = tbs[1:]
            try:
                supp = factory(*args)
                result.extend(self.formatSupplement(supp, tb))
            except:
                if DEBUG_EXCEPTION_FORMATTER:
                    traceback.print_exc()
                # else just swallow the exception.

        try:
            tbi = locals.get('__traceback_info__', None)
            if tbi is not None:
                result.append(self.formatTracebackInfo(tbi))
        except:
            if DEBUG_EXCEPTION_FORMATTER:
                traceback.print_exc()
            # else just swallow the exception.

        return self.line_sep.join(result)

    def formatExceptionOnly(self, etype, value):
        result = ''.join(traceback.format_exception_only(etype, value))
        return result.replace('\n', self.line_sep)

    def formatLastLine(self, exc_line):
        return self.escape(exc_line)

    def formatException(self, etype, value, tb):
        # The next line provides a way to detect recursion.
        __exception_formatter__ = 1
        result = [self.getPrefix() + '\n']
        limit = self.getLimit()
        n = 0
        while tb is not None and (limit is None or n < limit):
            if tb.tb_frame.f_locals.get('__exception_formatter__'):
                # Stop recursion.
                result.append('(Recursive formatException() stopped)\n')
                break
            line = self.formatLine(tb)
            result.append(line + '\n')
            tb = tb.tb_next
            n = n + 1
        exc_line = self.formatExceptionOnly(etype, value)
        result.append(self.formatLastLine(exc_line))
        return result


class HTMLExceptionFormatter(TextExceptionFormatter):

    line_sep = '<br />\r\n'

    def escape(self, s):
        return cgi.escape(s)

    def getPrefix(self):
        return '<p>Traceback (most recent call last):\r\n<ul>'

    def formatSupplementLine(self, line):
        return '<b>%s</b>' % self.escape(str(line))

    def formatTracebackInfo(self, tbi):
        s = self.escape(str(tbi))
        s = s.replace('\n', self.line_sep)
        return '__traceback_info__: %s' % (s, )

    def formatLine(self, tb):
        line = TextExceptionFormatter.formatLine(self, tb)
        return '<li>%s</li>' % line

    def formatLastLine(self, exc_line):
        return '</ul>%s</p>' % self.escape(exc_line)


def format_exception(t, v, tb, limit=None, as_html=False,
                     with_filenames=False):
    """Format a stack trace and the exception information.

    Similar to 'traceback.format_exception', but adds supplemental
    information to the traceback and accepts two options, 'as_html'
    and 'with_filenames'.
    """
    if as_html:
        fmt = HTMLExceptionFormatter(limit, with_filenames)
    else:
        fmt = TextExceptionFormatter(limit, with_filenames)
    return fmt.formatException(t, v, tb)


def print_exception(t, v, tb, limit=None, file=None, as_html=False,
                    with_filenames=True):
    """Print exception up to 'limit' stack trace entries from 'tb' to 'file'.

    Similar to 'traceback.print_exception', but adds supplemental
    information to the traceback and accepts two options, 'as_html'
    and 'with_filenames'.
    """
    if file is None:
        file = sys.stderr
    lines = format_exception(t, v, tb, limit, as_html, with_filenames)
    for line in lines:
        file.write(line)
