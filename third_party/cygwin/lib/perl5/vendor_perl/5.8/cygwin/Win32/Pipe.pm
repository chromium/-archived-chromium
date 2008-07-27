package Win32::Pipe;

$VERSION = '0.022';

# Win32::Pipe.pm
#       +==========================================================+
#       |                                                          |
#       |                     PIPE.PM package                      |
#       |                     ---------------                      |
#       |                    Release v96.05.11                     |
#       |                                                          |
#       |    Copyright (c) 1996 Dave Roth. All rights reserved.    |
#       |   This program is free software; you can redistribute    |
#       | it and/or modify it under the same terms as Perl itself. |
#       |                                                          |
#       +==========================================================+
#
#
#	Use under GNU General Public License or Larry Wall's "Artistic License"
#
#	Check the README.TXT file that comes with this package for details about
#	it's history.
#

require Exporter;
require DynaLoader;

@ISA= qw( Exporter DynaLoader );
    # Items to export into callers namespace by default. Note: do not export
    # names by default without a very good reason. Use EXPORT_OK instead.
    # Do not simply export all your public functions/methods/constants.
@EXPORT = qw();

$ErrorNum = 0;
$ErrorText = "";

sub new
{
    my ($self, $Pipe);
    my ($Type, $Name, $Time) = @_;

    if (! $Time){
        $Time = DEFAULT_WAIT_TIME();
    }
    $Pipe = PipeCreate($Name, $Time);
    if ($Pipe){
        $self = bless {};
        $self->{'Pipe'} = $Pipe;
    }else{
        ($ErrorNum, $ErrorText) = PipeError();
        return undef;
    }
    $self;
}

sub Write{
    my($self, $Data) = @_;
    $Data = PipeWrite($self->{'Pipe'}, $Data);
    return $Data;
}

sub Read{
    my($self) = @_;
    my($Data);
    $Data = PipeRead($self->{'Pipe'});
    return $Data;
}

sub Error{
    my($self) = @_;
    my($MyError, $MyErrorText, $Temp);
    if (! ref($self)){
        undef $Temp;
    }else{
        $Temp = $self->{'Pipe'};
    }
    ($MyError, $MyErrorText) = PipeError($Temp);
    return wantarray? ($MyError, $MyErrorText):"[$MyError] \"$MyErrorText\"";
}


sub Close{
    my ($self) = shift;
    PipeClose($self->{'Pipe'});
    $self->{'Pipe'} = 0;
}

sub Connect{
    my ($self) = @_;
    my ($Result);
    $Result = PipeConnect($self->{'Pipe'});
    return $Result;
}

sub Disconnect{
    my ($self, $iPurge) = @_;
    my ($Result);
    if (! $iPurge){
        $iPurge = 1;
    }
    $Result = PipeDisconnect($self->{'Pipe'}, $iPurge);
    return $Result;
}

sub BufferSize{
    my($self) = @_;
    my($Result) =  PipeBufferSize($self->{'Pipe'});
    return $Result;
}

sub ResizeBuffer{
    my($self, $Size) = @_;
    my($Result) = PipeResizeBuffer($self->{'Pipe'}, $Size);
    return $Result;
}


####
#   Auto-Kill an instance of this module
####
sub DESTROY
{
    my ($self) = shift;
    Close($self);
}


sub Credit{
    my($Name, $Version, $Date, $Author, $CompileDate, $CompileTime, $Credits) = Win32::Pipe::Info();
    my($Out, $iWidth);
    $iWidth = 60;
    $Out .=  "\n";
    $Out .=  "  +". "=" x ($iWidth). "+\n";
    $Out .=  "  |". Center("", $iWidth). "|\n";
    $Out .=  "  |" . Center("", $iWidth). "|\n";
    $Out .=  "  |". Center("$Name", $iWidth). "|\n";
    $Out .=  "  |". Center("-" x length("$Name"), $iWidth). "|\n";
    $Out .=  "  |". Center("", $iWidth). "|\n";

    $Out .=  "  |". Center("Version $Version ($Date)", $iWidth). "|\n";
    $Out .=  "  |". Center("by $Author", $iWidth). "|\n";
    $Out .=  "  |". Center("Compiled on $CompileDate at $CompileTime.", $iWidth). "|\n";
    $Out .=  "  |". Center("", $iWidth). "|\n";
    $Out .=  "  |". Center("Credits:", $iWidth). "|\n";
    $Out .=  "  |". Center(("-" x length("Credits:")), $iWidth). "|\n";
    foreach $Temp (split("\n", $Credits)){
        $Out .=  "  |". Center("$Temp", $iWidth). "|\n";
    }
    $Out .=  "  |". Center("", $iWidth). "|\n";
    $Out .=  "  +". "=" x ($iWidth). "+\n";
    return $Out;
}

sub Center{
    local($Temp, $Width) = @_;
    local($Len) = ($Width - length($Temp)) / 2;
    return " " x int($Len) . $Temp . " " x (int($Len) + (($Len != int($Len))? 1:0));
}

# ------------------ A U T O L O A D   F U N C T I O N ---------------------

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    $val = constant($constname, @_ ? $_[0] : 0);

    if ($! != 0) {
    if ($! =~ /Invalid/) {
        $AutoLoader::AUTOLOAD = $AUTOLOAD;
        goto &AutoLoader::AUTOLOAD;
    }
    else {

            # Added by JOC 06-APR-96
            # $pack = 0;
        $pack = 0;
        ($pack,$file,$line) = caller;
            print "Your vendor has not defined Win32::Pipe macro $constname, used in $file at line $line.";
    }
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Win32::Pipe;

1;
__END__

=head1 NAME

Win32::Pipe - Win32 Named Pipe

=head1 SYNOPSIS

To use this extension, follow these basic steps. First, you need to
'use' the pipe extension:

    use Win32::Pipe;

Then you need to create a server side of a named pipe:

    $Pipe = new Win32::Pipe("My Pipe Name");

or if you are going to connect to pipe that has already been created:

    $Pipe = new Win32::Pipe("\\\\server\\pipe\\My Pipe Name");

    NOTE: The "\\\\server\\pipe\\" is necessary when connecting
          to an existing pipe! If you are accessing the same
          machine you could use "\\\\.\\pipe\\" but either way
          works fine.

You should check to see if C<$Pipe> is indeed defined otherwise there
has been an error.

Whichever end is the server, it must now wait for a connection...

    $Result = $Pipe->Connect();

    NOTE: The client end does not do this! When the client creates
          the pipe it has already connected!

Now you can read and write data from either end of the pipe:

    $Data = $Pipe->Read();

    $Result = $Pipe->Write("Howdy! This is cool!");

When the server is finished it must disconnect:

    $Pipe->Disconnect();

Now the server could C<Connect> again (and wait for another client) or
it could destroy the named pipe...

    $Data->Close();

The client should C<Close> in order to properly end the session.

=head1 DESCRIPTION

=head2 General Use

This extension gives Win32 Perl the ability to use Named Pipes. Why?
Well considering that Win32 Perl does not (yet) have the ability to
C<fork> I could not see what good the C<pipe(X,Y)> was. Besides, where
I am as an admin I must have several perl daemons running on several
NT Servers. It dawned on me one day that if I could pipe all these
daemons' output to my workstation (across the net) then it would be
much easier to monitor. This was the impetus for an extension using
Named Pipes. I think that it is kinda cool. :)

=head2 Benefits

And what are the benefits of this module?

=over

=item *

You may create as many named pipes as you want (uh, well, as many as
your resources will allow).

=item *

Currently there is a limit of 256 instances of a named pipe (once a
pipe is created you can have 256 client/server connections to that
name).

=item *

The default buffer size is 512 bytes; this can be altered by the
C<ResizeBuffer> method.

=item *

All named pipes are byte streams. There is currently no way to alter a
pipe to be message based.

=item *

Other things that I cannot think of right now... :)

=back

=head1 CONSTRUCTOR

=over

=item new ( NAME )

Creates a named pipe if used in server context or a connection to the
specified named pipe if used in client context. Client context is
determined by prepending $Name with "\\\\".

Returns I<true> on success, I<false> on failure.

=back

=head1 METHODS

=over

=item BufferSize ()

Returns the size of the instance of the buffer of the named pipe.

=item Connect ()

Tells the named pipe to create an instance of the named pipe and wait
until a client connects. Returns I<true> on success, I<false> on
failure.

=item Close ()

Closes the named pipe.

=item Disconnect ()

Disconnects (and destroys) the instance of the named pipe from the
client. Returns I<true> on success, I<false> on failure.

=item Error ()

Returns the last error messages pertaining to the named pipe. If used
in context to the package. Returns a list containing C<ERROR_NUMBER>
and C<ERROR_TEXT>.

=item Read ()

Reads from the named pipe. Returns data read from the pipe on success,
undef on failure.

=item ResizeBuffer ( SIZE )

Sets the size of the buffer of the instance of the named pipe to
C<SIZE>. Returns the size of the buffer on success, I<false> on
failure.

=item Write ( DATA )

Writes C<DATA> to the named pipe. Returns I<true> on success, I<false>
on failure.

=back

=head1 LIMITATIONS

What known problems does this thing have?

=over

=item *

If someone is waiting on a C<Read> and the other end terminates then
you will wait for one B<REALLY> long time! (If anyone has an idea on
how I can detect the termination of the other end let me know!)

=item *

All pipes are blocking. I am considering using threads and callbacks
into Perl to perform async IO but this may be too much for my time
stress. ;)

=item *

There is no security placed on these pipes.

=item *

This module has neither been optimized for speed nor optimized for
memory consumption. This may run into memory bloat.

=back

=head1 INSTALLATION NOTES

If you wish to use this module with a build of Perl other than
ActivePerl, you may wish to fetch the source distribution for this
module. The source is included as part of the C<libwin32> bundle,
which you can find in any CPAN mirror here:

  modules/by-authors/Gurusamy_Sarathy/libwin32-0.151.tar.gz

The source distribution also contains a pair of sample client/server
test scripts. For the latest information on this module, consult the
following web site:

  http://www.roth.net/perl

=head1 AUTHOR

Dave Roth <rothd@roth.net>

=head1 DISCLAIMER

I do not guarantee B<ANYTHING> with this package. If you use it you
are doing so B<AT YOUR OWN RISK>! I may or may not support this
depending on my time schedule.

=head1 COPYRIGHT

Copyright (c) 1996 Dave Roth. All rights reserved.
This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

=cut
