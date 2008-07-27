package Win32::Clipboard;
#######################################################################
#
# Win32::Clipboard - Interaction with the Windows clipboard
#
# Version: 0.52
# Author: Aldo Calpini <dada@perl.it>
#
# Modified by: Hideyo Imazu <himazu@gmail.com>
#
#######################################################################

require Exporter;       # to export the constants to the main:: space
require DynaLoader;     # to dynuhlode the module.

@ISA = qw( Exporter DynaLoader );
@EXPORT = qw(
	CF_TEXT
	CF_BITMAP
	CF_METAFILEPICT
	CF_SYLK
	CF_DIF
	CF_TIFF
	CF_OEMTEXT
	CF_DIB
	CF_PALETTE
	CF_PENDATA
	CF_RIFF
	CF_WAVE
	CF_UNICODETEXT
	CF_ENHMETAFILE
	CF_HDROP
	CF_LOCALE
);

#######################################################################
# This AUTOLOAD is used to 'autoload' constants from the constant()
# XS function.  If a constant is not found then control is passed
# to the AUTOLOAD in AutoLoader.
#

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
        if ($! =~ /Invalid/) {
            $AutoLoader::AUTOLOAD = $AUTOLOAD;
            goto &AutoLoader::AUTOLOAD;
        } else {
            my ($pack, $file, $line) = caller;
            die "Win32::Clipboard::$constname is not defined, used at $file line $line.";
        }
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}


#######################################################################
# STATIC OBJECT PROPERTIES
#
$VERSION = "0.5201";

#######################################################################
# FUNCTIONS
#

sub new {
    my($class, $value) = @_;
    my $self = "I'm the Clipboard!";
    Set($value) if defined($value);
    return bless(\$self);
}

sub Version {
    return $VERSION;
}

sub Get {
	if(    IsBitmap() ) { return GetBitmap(); }
	elsif( IsFiles()  ) { return GetFiles();  }
	else                { return GetText();   }
}

sub TIESCALAR {
	my $class = shift;
	my $value = shift;
	Set($value) if defined $value;
	my $self = "I'm the Clipboard!";
	return bless \$self, $class;
}

sub FETCH { Get() }
sub STORE { shift; Set(@_) }

sub DESTROY {
    my($self) = @_;
    undef $self;
    StopClipboardViewer();
}

END {
    StopClipboardViewer();
}

#######################################################################
# dynamically load in the Clipboard.pll module.
#

bootstrap Win32::Clipboard;

#######################################################################
# a little hack to use the module itself as a class.
#

sub main::Win32::Clipboard {
    my($value) = @_;
    my $self={};
    my $result = Win32::Clipboard::Set($value) if defined($value);
    return bless($self, "Win32::Clipboard");
}

1;

__END__

=head1 NAME

Win32::Clipboard - Interaction with the Windows clipboard

=head1 SYNOPSIS

    use Win32::Clipboard;

    $CLIP = Win32::Clipboard();

    print "Clipboard contains: ", $CLIP->Get(), "\n";

    $CLIP->Set("some text to copy into the clipboard");

    $CLIP->Empty();

    $CLIP->WaitForChange();
    print "Clipboard has changed!\n";


=head1 DESCRIPTION

This module lets you interact with the Windows clipboard: you can get its content,
set it, empty it, or let your script sleep until it changes.
This version supports 3 formats for clipboard data:

=over 4

=item *
text (C<CF_TEXT>)

The clipboard contains some text; this is the B<only> format you can use to set 
clipboard data; you get it as a single string.

Example:

    $text = Win32::Clipboard::GetText();
    print $text;

=item *
bitmap (C<CF_DIB>)

The clipboard contains an image, either a bitmap or a picture copied in the
clipboard from a graphic application. The data you get is a binary buffer
ready to be written to a bitmap (BMP format) file.

Example:

    $image = Win32::Clipboard::GetBitmap();
    open    BITMAP, ">some.bmp";
    binmode BITMAP;
    print   BITMAP $image;
    close   BITMAP;

=item *
list of files (C<CF_HDROP>)

The clipboard contains files copied or cutted from an Explorer-like 
application; you get a list of filenames.

Example:

    @files = Win32::Clipboard::GetFiles();
    print join("\n", @files);

=back

=head2 REFERENCE

All the functions can be used either with their full name (eg. B<Win32::Clipboard::Get>)
or as methods of a C<Win32::Clipboard> object.
For the syntax, refer to L</SYNOPSIS> above. Note also that you can create a clipboard
object and set its content at the same time with:

    $CLIP = Win32::Clipboard("blah blah blah");

or with the more common form:

    $CLIP = new Win32::Clipboard("blah blah blah");

If you prefer, you can even tie the Clipboard to a variable like this:

	tie $CLIP, 'Win32::Clipboard';
	
	print "Clipboard content: $CLIP\n";
	
	$CLIP = "some text to copy to the clipboard...";

In this case, you can still access other methods using the tied() function:

	tied($CLIP)->Empty;
	print "got the picture" if tied($CLIP)->IsBitmap;

=over 4

=item Empty()

Empty the clipboard.

=for html <P>

=item EnumFormats()

Returns an array of identifiers describing the format for the data currently in the
clipboard. Formats can be standard ones (described in the L</CONSTANTS> section) or 
application-defined custom ones. See also IsFormatAvailable().

=for html <P>

=item Get()

Returns the clipboard content; note that the result depends on the nature of
clipboard data; to ensure that you get only the desired format, you should use
GetText(), GetBitmap() or GetFiles() instead. Get() is in fact implemented as:

	if(    IsBitmap() ) { return GetBitmap(); }
	elsif( IsFiles()  ) { return GetFiles();  }
	else                { return GetText();   }

See also IsBitmap(), IsFiles(), IsText(), EnumFormats() and IsFormatAvailable()
to check the clipboard format before getting data.

=for html <P>

=item GetAs(FORMAT)

Returns the clipboard content in the desired FORMAT (can be one of the constants
defined in the L</CONSTANTS> section or a custom format). Note that the only
meaningful identifiers are C<CF_TEXT>, C<CF_DIB> and C<CF_HDROP>; any other
format is treated as a string.

=for html <P>

=item GetBitmap()

Returns the clipboard content as an image, or C<undef> on errors.

=for html <P>

=item GetFiles()

Returns the clipboard content as a list of filenames, or C<undef> on errors.

=for html <P>

=item GetFormatName(FORMAT)

Returns the name of the specified custom clipboard format, or C<undef> on errors;
note that you cannot get the name of the standard formats (described in the
L</CONSTANTS> section).

=for html <P>

=item GetText()

Returns the clipboard content as a string, or C<undef> on errors.

=for html <P>

=item IsBitmap()

Returns a boolean value indicating if the clipboard contains an image.
See also GetBitmap().

=for html <P>

=item IsFiles()

Returns a boolean value indicating if the clipboard contains a list of
files. See also GetFiles().

=for html <P>

=item IsFormatAvailable(FORMAT)

Checks if the clipboard data matches the specified FORMAT (one of the constants 
described in the L</CONSTANTS> section); returns zero if the data does not match,
a nonzero value if it matches.

=for html <P>

=item IsText()

Returns a boolean value indicating if the clipboard contains text.
See also GetText().

=for html <P>

=item Set(VALUE)

Set the clipboard content to the specified string.

=for html <P>

=item WaitForChange([TIMEOUT])

This function halts the script until the clipboard content changes. If you specify
a C<TIMEOUT> value (in milliseconds), the function will return when this timeout
expires, even if the clipboard hasn't changed. If no value is given, it will wait
indefinitely. Returns 1 if the clipboard has changed, C<undef> on errors.

=back

=head2 CONSTANTS

These constants are the standard clipboard formats recognized by Win32::Clipboard:

	CF_TEXT             1
	CF_DIB              8
	CF_HDROP            15

The following formats are B<not recognized> by Win32::Clipboard; they are,
however, exported constants and can eventually be used with the EnumFormats(), 
IsFormatAvailable() and GetAs() functions:

	CF_BITMAP           2
	CF_METAFILEPICT     3
	CF_SYLK             4
	CF_DIF              5
	CF_TIFF             6
	CF_OEMTEXT          7
	CF_PALETTE          9
	CF_PENDATA          10
	CF_RIFF             11
	CF_WAVE             12
	CF_UNICODETEXT      13
	CF_ENHMETAFILE      14
	CF_LOCALE           16

=head1 AUTHOR

This version was released by Hideyo Imazu <F<himazu@gmail.com>>.

Aldo Calpini <F<dada@perl.it>> was the former maintainer.

Original XS porting by Gurusamy Sarathy <F<gsar@activestate.com>>.

=cut


