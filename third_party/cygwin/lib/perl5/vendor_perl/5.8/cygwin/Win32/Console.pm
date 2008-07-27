#######################################################################
#
# Win32::Console - Win32 Console and Character Mode Functions
#
#######################################################################

package Win32::Console;

require Exporter;
require DynaLoader;

$VERSION = "0.07";

@ISA= qw( Exporter DynaLoader );
@EXPORT = qw(
    BACKGROUND_BLUE
    BACKGROUND_GREEN
    BACKGROUND_INTENSITY
    BACKGROUND_RED
    CAPSLOCK_ON
    CONSOLE_TEXTMODE_BUFFER
    CTRL_BREAK_EVENT
    CTRL_C_EVENT
    ENABLE_ECHO_INPUT
    ENABLE_LINE_INPUT
    ENABLE_MOUSE_INPUT
    ENABLE_PROCESSED_INPUT
    ENABLE_PROCESSED_OUTPUT
    ENABLE_WINDOW_INPUT
    ENABLE_WRAP_AT_EOL_OUTPUT
    ENHANCED_KEY
    FILE_SHARE_READ
    FILE_SHARE_WRITE
    FOREGROUND_BLUE
    FOREGROUND_GREEN
    FOREGROUND_INTENSITY
    FOREGROUND_RED
    LEFT_ALT_PRESSED
    LEFT_CTRL_PRESSED
    NUMLOCK_ON
    GENERIC_READ
    GENERIC_WRITE
    RIGHT_ALT_PRESSED
    RIGHT_CTRL_PRESSED
    SCROLLLOCK_ON
    SHIFT_PRESSED
    STD_INPUT_HANDLE
    STD_OUTPUT_HANDLE
    STD_ERROR_HANDLE
    $FG_BLACK
    $FG_GRAY
    $FG_BLUE
    $FG_LIGHTBLUE
    $FG_RED
    $FG_LIGHTRED
    $FG_GREEN
    $FG_LIGHTGREEN
    $FG_MAGENTA
    $FG_LIGHTMAGENTA
    $FG_CYAN
    $FG_LIGHTCYAN
    $FG_BROWN
    $FG_YELLOW
    $FG_LIGHTGRAY
    $FG_WHITE
    $BG_BLACK
    $BG_GRAY
    $BG_BLUE
    $BG_LIGHTBLUE
    $BG_RED
    $BG_LIGHTRED
    $BG_GREEN
    $BG_LIGHTGREEN
    $BG_MAGENTA
    $BG_LIGHTMAGENTA
    $BG_CYAN
    $BG_LIGHTCYAN
    $BG_BROWN
    $BG_YELLOW
    $BG_LIGHTGRAY
    $BG_WHITE
    $ATTR_NORMAL
    $ATTR_INVERSE
    @CONSOLE_COLORS
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
#    if ($! =~ /Invalid/) {
#        $AutoLoader::AUTOLOAD = $AUTOLOAD;
#        goto &AutoLoader::AUTOLOAD;
#    } else {
        ($pack, $file, $line) = caller; undef $pack;
        die "Symbol Win32::Console::$constname not defined, used at $file line $line.";
#    }
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}


#######################################################################
# STATIC OBJECT PROPERTIES
#

# %HandlerRoutineStack = ();
# $HandlerRoutineRegistered = 0;

#######################################################################
# PUBLIC METHODS
#

#========
sub new {
#========
    my($class, $param1, $param2) = @_;

    my $self = {};

    if (defined($param1)
    and ($param1 == constant("STD_INPUT_HANDLE",  0)
    or   $param1 == constant("STD_OUTPUT_HANDLE", 0)
    or   $param1 == constant("STD_ERROR_HANDLE",  0)))
    {
        $self->{'handle'} = _GetStdHandle($param1);
    }
    else {
        $param1 = constant("GENERIC_READ", 0)    | constant("GENERIC_WRITE", 0) unless $param1;
        $param2 = constant("FILE_SHARE_READ", 0) | constant("FILE_SHARE_WRITE", 0) unless $param2;
        $self->{'handle'} = _CreateConsoleScreenBuffer($param1, $param2,
                                                       constant("CONSOLE_TEXTMODE_BUFFER", 0));
    }
    bless $self, $class;
    return $self;
}

#============
sub Display {
#============
    my($self) = @_;
    return undef unless ref($self);
    return _SetConsoleActiveScreenBuffer($self->{'handle'});
}

#===========
sub Select {
#===========
    my($self, $type) = @_;
    return undef unless ref($self);
    return _SetStdHandle($type, $self->{'handle'});
}

#===========
sub SetIcon {
#===========
    my($self, $icon) = @_;
    $icon = $self unless ref($self);
    return _SetConsoleIcon($icon);
}

#==========
sub Title {
#==========
    my($self, $title) = @_;
    $title = $self unless ref($self);

    if (defined($title)) {
	return _SetConsoleTitle($title);
    }
    else {
	return _GetConsoleTitle();
    }
}

#==============
sub WriteChar {
#==============
    my($self, $text, $col, $row) = @_;
    return undef unless ref($self);
    return _WriteConsoleOutputCharacter($self->{'handle'},$text,$col,$row);
}

#=============
sub ReadChar {
#=============
    my($self, $size, $col, $row) = @_;
    return undef unless ref($self);

    my $buffer = (" " x $size);
    if (_ReadConsoleOutputCharacter($self->{'handle'}, $buffer, $size, $col, $row)) {
        return $buffer;
    }
    else {
        return undef;
    }
}

#==============
sub WriteAttr {
#==============
    my($self, $attr, $col, $row) = @_;
    return undef unless ref($self);
    return _WriteConsoleOutputAttribute($self->{'handle'}, $attr, $col, $row);
}

#=============
sub ReadAttr {
#=============
    my($self, $size, $col, $row) = @_;
    return undef unless ref($self);
    return _ReadConsoleOutputAttribute($self->{'handle'}, $size, $col, $row);
}

#==========
sub Write {
#==========
    my($self,$string) = @_;
    return undef unless ref($self);
    return _WriteConsole($self->{'handle'}, $string);
}

#=============
sub ReadRect {
#=============
    my($self, $left, $top, $right, $bottom) = @_;
    return undef unless ref($self);

    my $col = $right  - $left + 1;
    my $row = $bottom - $top  + 1;

    my $buffer = (" " x ($col*$row*4));
    if (_ReadConsoleOutput($self->{'handle'},   $buffer,
                          $col,  $row, 0,      0,
                          $left, $top, $right, $bottom))
    {
        return $buffer;
    }
    else {
        return undef;
    }
}

#==============
sub WriteRect {
#==============
    my($self, $buffer, $left, $top, $right, $bottom) = @_;
    return undef unless ref($self);

    my $col = $right  - $left + 1;
    my $row = $bottom - $top  + 1;

    return _WriteConsoleOutput($self->{'handle'},   $buffer,
                               $col,  $row, 0,  0,
                               $left, $top, $right, $bottom);
}

#===========
sub Scroll {
#===========
    my($self, $left1, $top1, $right1, $bottom1,
              $col,   $row,  $char,   $attr,
              $left2, $top2, $right2, $bottom2) = @_;
    return undef unless ref($self);

    return _ScrollConsoleScreenBuffer($self->{'handle'},
                                      $left1, $top1, $right1, $bottom1,
                                      $col,   $row,  $char,   $attr,
                                      $left2, $top2, $right2, $bottom2);
}

#==============
sub MaxWindow {
#==============
    my($self, $flag) = @_;
    return undef unless ref($self);

    if (not defined($flag)) {
        my @info = _GetConsoleScreenBufferInfo($self->{'handle'});
        return $info[9], $info[10];
    }
    else {
        return _GetLargestConsoleWindowSize($self->{'handle'});
    }
}

#=========
sub Info {
#=========
    my($self) = @_;
    return undef unless ref($self);
    return _GetConsoleScreenBufferInfo($self->{'handle'});
}

#===========
sub Window {
#===========
    my($self, $flag, $left, $top, $right, $bottom) = @_;
    return undef unless ref($self);

    if (not defined($flag)) {
        my @info = _GetConsoleScreenBufferInfo($self->{'handle'});
        return $info[5], $info[6], $info[7], $info[8];
    }
    else {
        return _SetConsoleWindowInfo($self->{'handle'}, $flag, $left, $top, $right, $bottom);
    }
}

#==============
sub GetEvents {
#==============
    my($self) = @_;
    return undef unless ref($self);
    return _GetNumberOfConsoleInputEvents($self->{'handle'});
}

#==========
sub Flush {
#==========
    my($self) = @_;
    return undef unless ref($self);
    return _FlushConsoleInputBuffer($self->{'handle'});
}

#==============
sub InputChar {
#==============
    my($self, $number) = @_;
    return undef unless ref($self);

    $number = 1 unless defined($number);

    my $buffer = (" " x $number);
    if (_ReadConsole($self->{'handle'}, $buffer, $number) == $number) {
        return $buffer;
    }
    else {
        return undef;
    }
}

#==========
sub Input {
#==========
    my($self) = @_;
    return undef unless ref($self);
    return _ReadConsoleInput($self->{'handle'});
}

#==============
sub PeekInput {
#==============
    my($self) = @_;
    return undef unless ref($self);
    return _PeekConsoleInput($self->{'handle'});
}

#===============
sub WriteInput {
#===============
    my($self) = shift;
    return undef unless ref($self);
    return _WriteConsoleInput($self->{'handle'}, @_);
}

#=========
sub Mode {
#=========
    my($self, $mode) = @_;
    return undef unless ref($self);
    if (defined($mode)) {
        return _SetConsoleMode($self->{'handle'}, $mode);
    }
    else {
        return _GetConsoleMode($self->{'handle'});
    }
}

#========
sub Cls {
#========
    my($self, $attr) = @_;
    return undef unless ref($self);

    $attr = $ATTR_NORMAL unless defined($attr);

    my ($x, $y) = $self->Size();
    my($left, $top, $right ,$bottom) = $self->Window();
    my $vx = $right  - $left;
    my $vy = $bottom - $top;
    $self->FillChar(" ", $x*$y, 0, 0);
    $self->FillAttr($attr, $x*$y, 0, 0);
    $self->Cursor(0, 0);
    $self->Window(1, 0, 0, $vx, $vy);
}

#=========
sub Attr {
#=========
    my($self, $attr) = @_;
    return undef unless ref($self);

    if (not defined($attr)) {
        return (_GetConsoleScreenBufferInfo($self->{'handle'}))[4];
    }
    else {
        return _SetConsoleTextAttribute($self->{'handle'}, $attr);
    }
}

#===========
sub Cursor {
#===========
    my($self, $col, $row, $size, $visi) = @_;
    return undef unless ref($self);

    my $curr_row  = 0;
    my $curr_col  = 0;
    my $curr_size = 0;
    my $curr_visi = 0;
    my $return    = 0;
    my $discard   = 0;


    if (defined($col)) {
        $row = -1 if not defined($row);
        if ($col == -1 or $row == -1) {
            ($discard, $discard, $curr_col, $curr_row) = _GetConsoleScreenBufferInfo($self->{'handle'});
            $col=$curr_col if $col==-1;
            $row=$curr_row if $row==-1;
        }
        $return += _SetConsoleCursorPosition($self->{'handle'}, $col, $row);
        if (defined($size) and defined($visi)) {
            if ($size == -1 or $visi == -1) {
                ($curr_size, $curr_visi) = _GetConsoleCursorInfo($self->{'handle'});
                $size = $curr_size if $size == -1;
                $visi = $curr_visi if $visi == -1;
            }
            $size = 1 if $size < 1;
            $size = 99 if $size > 99;
            $return += _SetConsoleCursorInfo($self->{'handle'}, $size, $visi);
        }
        return $return;
    }
    else {
        ($discard, $discard, $curr_col, $curr_row) = _GetConsoleScreenBufferInfo($self->{'handle'});
        ($curr_size, $curr_visi) = _GetConsoleCursorInfo($self->{'handle'});
        return ($curr_col, $curr_row, $curr_size, $curr_visi);
    }
}

#=========
sub Size {
#=========
    my($self, $col, $row) = @_;
    return undef unless ref($self);

    if (not defined($col)) {
        ($col, $row) = _GetConsoleScreenBufferInfo($self->{'handle'});
        return ($col, $row);
    }
    else {
        $row = -1 if not defined($row);
        if ($col == -1 or $row == -1) {
            ($curr_col, $curr_row) = _GetConsoleScreenBufferInfo($self->{'handle'});
            $col=$curr_col if $col==-1;
            $row=$curr_row if $row==-1;
        }
        return _SetConsoleScreenBufferSize($self->{'handle'}, $col, $row);
    }
}

#=============
sub FillAttr {
#=============
    my($self, $attr, $number, $col, $row) = @_;
    return undef unless ref($self);

    $number = 1 unless $number;

    if (!defined($col) or !defined($row) or $col == -1 or $row == -1) {
        ($discard,  $discard,
         $curr_col, $curr_row) = _GetConsoleScreenBufferInfo($self->{'handle'});
        $col = $curr_col if !defined($col) or $col == -1;
        $row = $curr_row if !defined($row) or $row == -1;
    }
    return _FillConsoleOutputAttribute($self->{'handle'}, $attr, $number, $col, $row);
}

#=============
sub FillChar {
#=============
    my($self, $char, $number, $col, $row) = @_;
    return undef unless ref($self);

    if (!defined($col) or !defined($row) or $col == -1 or $row == -1) {
        ($discard,  $discard,
         $curr_col, $curr_row) = _GetConsoleScreenBufferInfo($self->{'handle'});
        $col = $curr_col if !defined($col) or $col == -1;
        $row = $curr_row if !defined($row) or $row == -1;
    }
    return _FillConsoleOutputCharacter($self->{'handle'}, $char, $number, $col, $row);
}

#============
sub InputCP {
#============
    my($self, $codepage) = @_;
    $codepage = $self if (defined($self) and ref($self) ne "Win32::Console");
    if (defined($codepage)) {
        return _SetConsoleCP($codepage);
    }
    else {
        return _GetConsoleCP();
    }
}

#=============
sub OutputCP {
#=============
    my($self, $codepage) = @_;
    $codepage = $self if (defined($self) and ref($self) ne "Win32::Console");
    if (defined($codepage)) {
        return _SetConsoleOutputCP($codepage);
    }
    else {
        return _GetConsoleOutputCP();
    }
}

#======================
sub GenerateCtrlEvent {
#======================
    my($self, $type, $pid) = @_;
    $type = constant("CTRL_C_EVENT", 0) unless defined($type);
    $pid = 0 unless defined($pid);
    return _GenerateConsoleCtrlEvent($type, $pid);
}

#===================
#sub SetCtrlHandler {
#===================
#    my($name, $add) = @_;
#    $add = 1 unless defined($add);
#    my @nor = keys(%HandlerRoutineStack);
#    if ($add == 0) {
#        foreach $key (@nor) {
#            delete $HandlerRoutineStack{$key}, last if $HandlerRoutineStack{$key}==$name;
#        }
#        $HandlerRoutineRegistered--;
#    } else {
#        if ($#nor == -1) {
#            my $r = _SetConsoleCtrlHandler();
#            if (!$r) {
#                print "WARNING: SetConsoleCtrlHandler failed...\n";
#            }
#        }
#        $HandlerRoutineRegistered++;
#        $HandlerRoutineStack{$HandlerRoutineRegistered} = $name;
#    }
#}

#===================
sub get_Win32_IPC_HANDLE { # So Win32::IPC can wait on a console handle
#===================
    $_[0]->{'handle'};
}

########################################################################
# PRIVATE METHODS
#

#================
#sub CtrlHandler {
#================
#    my($ctrltype) = @_;
#    my $routine;
#    my $result = 0;
#    CALLEM: foreach $routine (sort { $b <=> $a } keys %HandlerRoutineStack) {
#        #print "CtrlHandler: calling $HandlerRoutineStack{$routine}($ctrltype)\n";
#        $result = &{"main::".$HandlerRoutineStack{$routine}}($ctrltype);
#        last CALLEM if $result;
#    }
#    return $result;
#}

#============
sub DESTROY {
#============
    my($self) = @_;
    _CloseHandle($self->{'handle'});
}

#######################################################################
# dynamically load in the Console.pll module.
#

bootstrap Win32::Console;

#######################################################################
# ADDITIONAL CONSTANTS EXPORTED IN THE MAIN NAMESPACE
#

$FG_BLACK        = 0;
$FG_GRAY         = constant("FOREGROUND_INTENSITY",0);
$FG_BLUE         = constant("FOREGROUND_BLUE",0);
$FG_LIGHTBLUE    = constant("FOREGROUND_BLUE",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_RED          = constant("FOREGROUND_RED",0);
$FG_LIGHTRED     = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_GREEN        = constant("FOREGROUND_GREEN",0);
$FG_LIGHTGREEN   = constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_MAGENTA      = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_BLUE",0);
$FG_LIGHTMAGENTA = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_BLUE",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_CYAN         = constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_BLUE",0);
$FG_LIGHTCYAN    = constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_BLUE",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_BROWN        = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_GREEN",0);
$FG_YELLOW       = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_INTENSITY",0);
$FG_LIGHTGRAY    = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_BLUE",0);
$FG_WHITE        = constant("FOREGROUND_RED",0)|
                   constant("FOREGROUND_GREEN",0)|
                   constant("FOREGROUND_BLUE",0)|
                   constant("FOREGROUND_INTENSITY",0);

$BG_BLACK        = 0;
$BG_GRAY         = constant("BACKGROUND_INTENSITY",0);
$BG_BLUE         = constant("BACKGROUND_BLUE",0);
$BG_LIGHTBLUE    = constant("BACKGROUND_BLUE",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_RED          = constant("BACKGROUND_RED",0);
$BG_LIGHTRED     = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_GREEN        = constant("BACKGROUND_GREEN",0);
$BG_LIGHTGREEN   = constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_MAGENTA      = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_BLUE",0);
$BG_LIGHTMAGENTA = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_BLUE",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_CYAN         = constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_BLUE",0);
$BG_LIGHTCYAN    = constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_BLUE",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_BROWN        = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_GREEN",0);
$BG_YELLOW       = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_INTENSITY",0);
$BG_LIGHTGRAY    = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_BLUE",0);
$BG_WHITE        = constant("BACKGROUND_RED",0)|
                   constant("BACKGROUND_GREEN",0)|
                   constant("BACKGROUND_BLUE",0)|
                   constant("BACKGROUND_INTENSITY",0);

$ATTR_NORMAL  = $FG_LIGHTGRAY|$BG_BLACK;
$ATTR_INVERSE = $FG_BLACK|$BG_LIGHTGRAY;

for my $fg ($FG_BLACK, $FG_GRAY, $FG_BLUE, $FG_GREEN,
	    $FG_CYAN, $FG_RED, $FG_MAGENTA, $FG_BROWN,
	    $FG_LIGHTBLUE, $FG_LIGHTGREEN, $FG_LIGHTCYAN,
	    $FG_LIGHTRED, $FG_LIGHTMAGENTA, $FG_YELLOW,
	    $FG_LIGHTGRAY, $FG_WHITE)
{
    for my $bg ($BG_BLACK, $BG_GRAY, $BG_BLUE, $BG_GREEN,
		$BG_CYAN, $BG_RED, $BG_MAGENTA, $BG_BROWN,
		$BG_LIGHTBLUE, $BG_LIGHTGREEN, $BG_LIGHTCYAN,
		$BG_LIGHTRED, $BG_LIGHTMAGENTA, $BG_YELLOW,
		$BG_LIGHTGRAY, $BG_WHITE)
    {
        push(@CONSOLE_COLORS, $fg|$bg);
    }
}

# Preloaded methods go here.

#Currently Autoloading is not implemented in Perl for win32
# Autoload methods go after __END__, and are processed by the autosplit program.

1;

__END__

=head1 NAME

Win32::Console - Win32 Console and Character Mode Functions


=head1 DESCRIPTION

This module implements the Win32 console and character mode
functions.  They give you full control on the console input and output,
including: support of off-screen console buffers (eg. multiple screen
pages)

=over

=item *

reading and writing of characters, attributes and whole portions of
the screen

=item *

complete processing of keyboard and mouse events

=item *

some very funny additional features :)

=back

Those functions should also make possible a port of the Unix's curses
library; if there is anyone interested (and/or willing to contribute)
to this project, e-mail me.  Thank you.


=head1 REFERENCE


=head2 Methods

=over

=item Alloc

Allocates a new console for the process.  Returns C<undef> on errors, a
nonzero value on success.  A process cannot be associated with more
than one console, so this method will fail if there is already an
allocated console.  Use Free to detach the process from the console,
and then call Alloc to create a new console.  See also: C<Free>

Example:

    $CONSOLE->Alloc();

=item Attr [attr]

Gets or sets the current console attribute.  This attribute is used by
the Write method.

Example:

    $attr = $CONSOLE->Attr();
    $CONSOLE->Attr($FG_YELLOW | $BG_BLUE);

=item Close

Closes a shortcut object.  Note that it is not "strictly" required to
close the objects you created, since the Win32::Shortcut objects are
automatically closed when the program ends (or when you elsehow
destroy such an object).

Example:

    $LINK->Close();

=item Cls [attr]

Clear the console, with the specified I<attr> if given, or using
ATTR_NORMAL otherwise.

Example:

    $CONSOLE->Cls();
    $CONSOLE->Cls($FG_WHITE | $BG_GREEN);

=item Cursor [x, y, size, visible]

Gets or sets cursor position and appearance.  Returns C<undef> on
errors, or a 4-element list containing: I<x>, I<y>, I<size>,
I<visible>.  I<x> and I<y> are the current cursor position; ...

Example:

    ($x, $y, $size, $visible) = $CONSOLE->Cursor();

    # Get position only
    ($x, $y) = $CONSOLE->Cursor();

    $CONSOLE->Cursor(40, 13, 50, 1);

    # Set position only
    $CONSOLE->Cursor(40, 13);

    # Set size and visibility without affecting position
    $CONSOLE->Cursor(-1, -1, 50, 1);

=item Display

Displays the specified console on the screen.  Returns C<undef> on errors,
a nonzero value on success.

Example:

    $CONSOLE->Display();

=item FillAttr [attribute, number, col, row]

Fills the specified number of consecutive attributes, beginning at
I<col>, I<row>, with the value specified in I<attribute>.  Returns the
number of attributes filled, or C<undef> on errors.  See also:
C<FillChar>.

Example:

    $CONSOLE->FillAttr($FG_BLACK | $BG_BLACK, 80*25, 0, 0);

=item FillChar char, number, col, row

Fills the specified number of consecutive characters, beginning at
I<col>, I<row>, with the character specified in I<char>.  Returns the
number of characters filled, or C<undef> on errors.  See also:
C<FillAttr>.

Example:

    $CONSOLE->FillChar("X", 80*25, 0, 0);

=item Flush

Flushes the console input buffer.  All the events in the buffer are
discarded.  Returns C<undef> on errors, a nonzero value on success.

Example:

    $CONSOLE->Flush();

=item Free

Detaches the process from the console.  Returns C<undef> on errors, a
nonzero value on success.  See also: C<Alloc>.

Example:

    $CONSOLE->Free();

=item GenerateCtrlEvent [type, processgroup]

Sends a break signal of the specified I<type> to the specified
I<processgroup>.  I<type> can be one of the following constants:

    CTRL_BREAK_EVENT
    CTRL_C_EVENT

they signal, respectively, the pressing of Control + Break and of
Control + C; if not specified, it defaults to CTRL_C_EVENT.
I<processgroup> is the pid of a process sharing the same console.  If
omitted, it defaults to 0 (the current process), which is also the
only meaningful value that you can pass to this function.  Returns
C<undef> on errors, a nonzero value on success.

Example:

    # break this script now
    $CONSOLE->GenerateCtrlEvent();

=item GetEvents

Returns the number of unread input events in the console's input
buffer, or C<undef> on errors.  See also: C<Input>, C<InputChar>,
C<PeekInput>, C<WriteInput>.

Example:

    $events = $CONSOLE->GetEvents();

=item Info

Returns an array of informations about the console (or C<undef> on
errors), which contains:

=over

=item *

columns (X size) of the console buffer.

=item *

rows (Y size) of the console buffer.

=item *

current column (X position) of the cursor.

=item *

current row (Y position) of the cursor.

=item *

current attribute used for C<Write>.

=item *

left column (X of the starting point) of the current console window.

=item *

top row (Y of the starting point) of the current console window.

=item *

right column (X of the final point) of the current console window.

=item *

bottom row (Y of the final point) of the current console window.

=item *

maximum number of columns for the console window, given the current
buffer size, font and the screen size.

=item *

maximum number of rows for the console window, given the current
buffer size, font and the screen size.

=back

See also: C<Attr>, C<Cursor>, C<Size>, C<Window>, C<MaxWindow>.

Example:

    @info = $CONSOLE->Info();
    print "Cursor at $info[3], $info[4].\n";

=item Input

Reads an event from the input buffer.  Returns a list of values, which
depending on the event's nature are:

=over

=item keyboard event

The list will contain:

=over

=item *

event type: 1 for keyboard

=item *

key down: TRUE if the key is being pressed, FALSE if the key is being released

=item *

repeat count: the number of times the key is being held down

=item *

virtual keycode: the virtual key code of the key

=item *

virtual scancode: the virtual scan code of the key

=item *

char: the ASCII code of the character (if the key is a character key, 0 otherwise)

=item *

control key state: the state of the control keys (SHIFTs, CTRLs, ALTs, etc.)

=back

=item mouse event

The list will contain:

=over

=item *

event type: 2 for mouse

=item *

mouse pos. X: X coordinate (column) of the mouse location

=item *

mouse pos. Y: Y coordinate (row) of the mouse location

=item *

button state: the mouse button(s) which are pressed

=item *

control key state: the state of the control keys (SHIFTs, CTRLs, ALTs, etc.)

=item *

event flags: the type of the mouse event

=back

=back

This method will return C<undef> on errors.  Note that the events
returned are depending on the input C<Mode> of the console; for example,
mouse events are not intercepted unless ENABLE_MOUSE_INPUT is
specified.  See also: C<GetEvents>, C<InputChar>, C<Mode>,
C<PeekInput>, C<WriteInput>.

Example:

    @event = $CONSOLE->Input();

=item InputChar number

Reads and returns I<number> characters from the console input buffer,
or C<undef> on errors.  See also: C<Input>, C<Mode>.

Example:

    $key = $CONSOLE->InputChar(1);

=item InputCP [codepage]

Gets or sets the input code page used by the console.  Note that this
doesn't apply to a console object, but to the standard input
console.  This attribute is used by the Write method.  See also:
C<OutputCP>.

Example:

    $codepage = $CONSOLE->InputCP();
    $CONSOLE->InputCP(437);

    # you may want to use the non-instanciated form to avoid confuzion :)
    $codepage = Win32::Console::InputCP();
    Win32::Console::InputCP(437);

=item MaxWindow

Returns the size of the largest possible console window, based on the
current font and the size of the display.  The result is C<undef> on
errors, otherwise a 2-element list containing col, row.

Example:

    ($maxCol, $maxRow) = $CONSOLE->MaxWindow();

=item Mode [flags]

Gets or sets the input or output mode of a console.  I<flags> can be a
combination of the following constants:

    ENABLE_LINE_INPUT
    ENABLE_ECHO_INPUT
    ENABLE_PROCESSED_INPUT
    ENABLE_WINDOW_INPUT
    ENABLE_MOUSE_INPUT
    ENABLE_PROCESSED_OUTPUT
    ENABLE_WRAP_AT_EOL_OUTPUT

For more informations on the meaning of those flags, please refer to
the L<"Microsoft's Documentation">.

Example:

    $mode = $CONSOLE->Mode();
    $CONSOLE->Mode(ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT);

=item MouseButtons

Returns the number of the buttons on your mouse, or C<undef> on errors.

Example:

    print "Your mouse has ", $CONSOLE->MouseButtons(), " buttons.\n";

=item new Win32::Console standard_handle

=item new Win32::Console [accessmode, sharemode]

Creates a new console object.  The first form creates a handle to a
standard channel, I<standard_handle> can be one of the following:

    STD_OUTPUT_HANDLE
    STD_ERROR_HANDLE
    STD_INPUT_HANDLE

The second form, instead, creates a console screen buffer in memory,
which you can access for reading and writing as a normal console, and
then redirect on the standard output (the screen) with C<Display>.  In
this case, you can specify one or both of the following values for
I<accessmode>:

    GENERIC_READ
    GENERIC_WRITE

which are the permissions you will have on the created buffer, and one
or both of the following values for I<sharemode>:

    FILE_SHARE_READ
    FILE_SHARE_WRITE

which affect the way the console can be shared.  If you don't specify
any of those parameters, all 4 flags will be used.

Example:

    $STDOUT = new Win32::Console(STD_OUTPUT_HANDLE);
    $STDERR = new Win32::Console(STD_ERROR_HANDLE);
    $STDIN  = new Win32::Console(STD_INPUT_HANDLE);

    $BUFFER = new Win32::Console();
    $BUFFER = new Win32::Console(GENERIC_READ | GENERIC_WRITE);

=item OutputCP [codepage]

Gets or sets the output code page used by the console.  Note that this
doesn't apply to a console object, but to the standard output console.
See also: C<InputCP>.

Example:

    $codepage = $CONSOLE->OutputCP();
    $CONSOLE->OutputCP(437);

    # you may want to use the non-instanciated form to avoid confuzion :)
    $codepage = Win32::Console::OutputCP();
    Win32::Console::OutputCP(437);

=item PeekInput

Does exactly the same as C<Input>, except that the event read is not
removed from the input buffer.  See also: C<GetEvents>, C<Input>,
C<InputChar>, C<Mode>, C<WriteInput>.

Example:

    @event = $CONSOLE->PeekInput();

=item ReadAttr [number, col, row]

Reads the specified I<number> of consecutive attributes, beginning at
I<col>, I<row>, from the console.  Returns the attributes read (a
variable containing one character for each attribute), or C<undef> on
errors.  You can then pass the returned variable to C<WriteAttr> to
restore the saved attributes on screen.  See also: C<ReadChar>,
C<ReadRect>.

Example:

    $colors = $CONSOLE->ReadAttr(80*25, 0, 0);

=item ReadChar [number, col, row]

Reads the specified I<number> of consecutive characters, beginning at
I<col>, I<row>, from the console.  Returns a string containing the
characters read, or C<undef> on errors.  You can then pass the
returned variable to C<WriteChar> to restore the saved characters on
screen.  See also: C<ReadAttr>, C<ReadRect>.

Example:

    $chars = $CONSOLE->ReadChar(80*25, 0, 0);

=item ReadRect left, top, right, bottom

Reads the content (characters and attributes) of the rectangle
specified by I<left>, I<top>, I<right>, I<bottom> from the console.
Returns a string containing the rectangle read, or C<undef> on errors.
You can then pass the returned variable to C<WriteRect> to restore the
saved rectangle on screen (or on another console).  See also:
C<ReadAttr>, C<ReadChar>.

Example:

     $rect = $CONSOLE->ReadRect(0, 0, 80, 25);

=item Scroll left, top, right, bottom, col, row, char, attr,
             [cleft, ctop, cright, cbottom]

Moves a block of data in a console buffer; the block is identified by
I<left>, I<top>, I<right>, I<bottom>, while I<row>, I<col> identify
the new location of the block.  The cells left empty as a result of
the move are filled with the character I<char> and attribute I<attr>.
Optionally you can specify a clipping region with I<cleft>, I<ctop>,
I<cright>, I<cbottom>, so that the content of the console outside this
rectangle are unchanged.  Returns C<undef> on errors, a nonzero value
on success.

Example:

    # scrolls the screen 10 lines down, filling with black spaces
    $CONSOLE->Scroll(0, 0, 80, 25, 0, 10, " ", $FG_BLACK | $BG_BLACK);

=item Select standard_handle

Redirects a standard handle to the specified console.
I<standard_handle> can have one of the following values:

    STD_INPUT_HANDLE
    STD_OUTPUT_HANDLE
    STD_ERROR_HANDLE

Returns C<undef> on errors, a nonzero value on success.

Example:

    $CONSOLE->Select(STD_OUTPUT_HANDLE);

=item SetIcon icon_file

Sets the icon in the title bar of the current console window.

Example:

    $CONSOLE->SetIcon("C:/My/Path/To/Custom.ico");

=item Size [col, row]

Gets or sets the console buffer size.

Example:

    ($x, $y) = $CONSOLE->Size();
    $CONSOLE->Size(80, 25);

=item Title [title]

Gets or sets the title of the current console window.

Example:

    $title = $CONSOLE->Title();
    $CONSOLE->Title("This is a title");

=item Window [flag, left, top, right, bottom]

Gets or sets the current console window size.  If called without
arguments, returns a 4-element list containing the current window
coordinates in the form of I<left>, I<top>, I<right>, I<bottom>.  To
set the window size, you have to specify an additional I<flag>
parameter: if it is 0 (zero), coordinates are considered relative to
the current coordinates; if it is non-zero, coordinates are absolute.

Example:

    ($left, $top, $right, $bottom) = $CONSOLE->Window();
    $CONSOLE->Window(1, 0, 0, 80, 50);

=item Write string

Writes I<string> on the console, using the current attribute, that you
can set with C<Attr>, and advancing the cursor as needed.  This isn't
so different from Perl's "print" statement.  Returns the number of
characters written or C<undef> on errors.  See also: C<WriteAttr>,
C<WriteChar>, C<WriteRect>.

Example:

    $CONSOLE->Write("Hello, world!");

=item WriteAttr attrs, col, row

Writes the attributes in the string I<attrs>, beginning at I<col>,
I<row>, without affecting the characters that are on screen.  The
string attrs can be the result of a C<ReadAttr> function, or you can
build your own attribute string; in this case, keep in mind that every
attribute is treated as a character, not a number (see example).
Returns the number of attributes written or C<undef> on errors.  See
also: C<Write>, C<WriteChar>, C<WriteRect>.

Example:

    $CONSOLE->WriteAttr($attrs, 0, 0);

    # note the use of chr()...
    $attrs = chr($FG_BLACK | $BG_WHITE) x 80;
    $CONSOLE->WriteAttr($attrs, 0, 0);

=item WriteChar chars, col, row

Writes the characters in the string I<attr>, beginning at I<col>, I<row>,
without affecting the attributes that are on screen.  The string I<chars>
can be the result of a C<ReadChar> function, or a normal string.  Returns
the number of characters written or C<undef> on errors.  See also:
C<Write>, C<WriteAttr>, C<WriteRect>.

Example:

    $CONSOLE->WriteChar("Hello, worlds!", 0, 0);

=item WriteInput (event)

Pushes data in the console input buffer.  I<(event)> is a list of values,
for more information see C<Input>.  The string chars can be the result of
a C<ReadChar> function, or a normal string.  Returns the number of
characters written or C<undef> on errors.  See also: C<Write>,
C<WriteAttr>, C<WriteRect>.

Example:

    $CONSOLE->WriteInput(@event);

=item WriteRect rect, left, top, right, bottom

Writes a rectangle of characters and attributes (contained in I<rect>)
on the console at the coordinates specified by I<left>, I<top>,
I<right>, I<bottom>.  I<rect> can be the result of a C<ReadRect>
function.  Returns C<undef> on errors, otherwise a 4-element list
containing the coordinates of the affected rectangle, in the format
I<left>, I<top>, I<right>, I<bottom>.  See also: C<Write>,
C<WriteAttr>, C<WriteChar>.

Example:

    $CONSOLE->WriteRect($rect, 0, 0, 80, 25);

=back


=head2 Constants

The following constants are exported in the main namespace of your
script using Win32::Console:

    BACKGROUND_BLUE
    BACKGROUND_GREEN
    BACKGROUND_INTENSITY
    BACKGROUND_RED
    CAPSLOCK_ON
    CONSOLE_TEXTMODE_BUFFER
    ENABLE_ECHO_INPUT
    ENABLE_LINE_INPUT
    ENABLE_MOUSE_INPUT
    ENABLE_PROCESSED_INPUT
    ENABLE_PROCESSED_OUTPUT
    ENABLE_WINDOW_INPUT
    ENABLE_WRAP_AT_EOL_OUTPUT
    ENHANCED_KEY
    FILE_SHARE_READ
    FILE_SHARE_WRITE
    FOREGROUND_BLUE
    FOREGROUND_GREEN
    FOREGROUND_INTENSITY
    FOREGROUND_RED
    LEFT_ALT_PRESSED
    LEFT_CTRL_PRESSED
    NUMLOCK_ON
    GENERIC_READ
    GENERIC_WRITE
    RIGHT_ALT_PRESSED
    RIGHT_CTRL_PRESSED
    SCROLLLOCK_ON
    SHIFT_PRESSED
    STD_INPUT_HANDLE
    STD_OUTPUT_HANDLE
    STD_ERROR_HANDLE

Additionally, the following variables can be used:

    $FG_BLACK
    $FG_GRAY
    $FG_BLUE
    $FG_LIGHTBLUE
    $FG_RED
    $FG_LIGHTRED
    $FG_GREEN
    $FG_LIGHTGREEN
    $FG_MAGENTA
    $FG_LIGHTMAGENTA
    $FG_CYAN
    $FG_LIGHTCYAN
    $FG_BROWN
    $FG_YELLOW
    $FG_LIGHTGRAY
    $FG_WHITE

    $BG_BLACK
    $BG_GRAY
    $BG_BLUE
    $BG_LIGHTBLUE
    $BG_RED
    $BG_LIGHTRED
    $BG_GREEN
    $BG_LIGHTGREEN
    $BG_MAGENTA
    $BG_LIGHTMAGENTA
    $BG_CYAN
    $BG_LIGHTCYAN
    $BG_BROWN
    $BG_YELLOW
    $BG_LIGHTGRAY
    $BG_WHITE

    $ATTR_NORMAL
    $ATTR_INVERSE

ATTR_NORMAL is set to gray foreground on black background (DOS's
standard colors).


=head2 Microsoft's Documentation

Documentation for the Win32 Console and Character mode Functions can
be found on Microsoft's site at this URL:

http://www.microsoft.com/msdn/sdk/platforms/doc/sdk/win32/sys/src/conchar.htm

A reference of the available functions is at:

http://www.microsoft.com/msdn/sdk/platforms/doc/sdk/win32/sys/src/conchar_34.htm


=head1 AUTHOR

Aldo Calpini <a.calpini@romagiubileo.it>

=head1 CREDITS

Thanks to: Jesse Dougherty, Dave Roth, ActiveWare, and the
Perl-Win32-Users community.

=head1 DISCLAIMER

This program is FREE; you can redistribute, modify, disassemble, or
even reverse engineer this software at your will.  Keep in mind,
however, that NOTHING IS GUARANTEED to work and everything you do is
AT YOUR OWN RISK - I will not take responsibility for any damage, loss
of money and/or health that may arise from the use of this program!

This is distributed under the terms of Larry Wall's Artistic License.
