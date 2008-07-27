#---------------------------------------------------------------------
package Win32::ChangeNotify;
#
# Copyright 1998 Christopher J. Madsen
#
# Created: 3 Feb 1998 from the ActiveWare version
#   (c) 1995 Microsoft Corporation. All rights reserved.
#       Developed by ActiveWare Internet Corp., http://www.ActiveWare.com
#
#   Other modifications (c) 1997 by Gurusamy Sarathy <gsar@activestate.com>
#
# Author: Christopher J. Madsen <cjm@pobox.com>
# Version: 1.02 (13-Jun-1999)
#
# This program is free software; you can redistribute it and/or modify
# it under the same terms as Perl itself.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either the
# GNU General Public License or the Artistic License for more details.
#
# Monitor directory for changes
#---------------------------------------------------------------------
# 1.04 -Minor changes by Yves Orton to fix the trueness of $subtree (Dec 2002)

$VERSION = '1.05';

use Carp;
use Win32::IPC 1.00 '/./';      # Import everything
require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader Win32::IPC);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	FILE_NOTIFY_CHANGE_ATTRIBUTES
	FILE_NOTIFY_CHANGE_DIR_NAME
	FILE_NOTIFY_CHANGE_FILE_NAME
	FILE_NOTIFY_CHANGE_LAST_WRITE
	FILE_NOTIFY_CHANGE_SECURITY
	FILE_NOTIFY_CHANGE_SIZE
	INFINITE
);
@EXPORT_OK = qw(
  wait_all wait_any
);

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    if ($constname =~ /^(?:FILE_NOTIFY_CHANGE_|INFINITE)/) {
	local $! = 0;
        my $val = constant($constname);
        croak("$constname is not defined by Win32::ChangeNotify") if $! != 0;
        eval "sub $AUTOLOAD { $val }";
        goto &$AUTOLOAD;
    }
} # end AUTOLOAD

bootstrap Win32::ChangeNotify;

sub new {
    my ($class,$path,$subtree,$filter) = @_;

    if ($filter =~ /\A[\s|A-Z_]+\Z/i) {
        $filter = 0;
        foreach (split(/[\s|]+/, $_[3])) {
            $filter |= constant("FILE_NOTIFY_CHANGE_" . uc $_);
            carp "Invalid filter $_" if $!;
        }
    }
    _new($class,$path,$subtree,$filter);
} # end new

sub Close { &close }

sub FindFirst { $_[0] = Win32::ChangeNotify->_new(@_[1..3]); }

sub FindNext { &reset }

1;
__END__

=head1 NAME

Win32::ChangeNotify - Monitor events related to files and directories

=head1 SYNOPSIS

	require Win32::ChangeNotify;

	$notify = Win32::ChangeNotify->new($Path,$WatchSubTree,$Events);
	$notify->wait or warn "Something failed: $!\n";
	# There has been a change.

=head1 DESCRIPTION

This module allows the user to use a Win32 change notification event
object from Perl.  This allows the Perl program to monitor events
relating to files and directory trees.

Unfortunately, the Win32 API which implements this feature does not
provide any indication of I<what> triggered the notification (as far
as I know).  If you're monitoring a directory for file changes, and
you need to know I<which> file changed, you'll have to find some other
way of determining that.  Depending on exactly what you're trying to
do, you may be able to check file timestamps to find recently changed
files.  Or, you may need to cache the directory contents somewhere and
compare the current contents to your cached copy when you receive a
change notification.

The C<wait> method and C<wait_all> & C<wait_any> functions are
inherited from the L<"Win32::IPC"> module.

=head2 Methods

=over 4

=item $notify = Win32::ChangeNotify->new($path, $subtree, $filter)

Constructor for a new ChangeNotification object.  C<$path> is the
directory to monitor.  If C<$subtree> is true, then all directories
under C<$path> will be monitored.  C<$filter> indicates what events
should trigger a notification.  It should be a string containing any
of the following flags (separated by whitespace and/or C<|>).

   ATTRIBUTES	Any attribute change
   DIR_NAME     Any directory name change
   FILE_NAME    Any file name change (creating/deleting/renaming)
   LAST_WRITE   Any change to a file's last write time
   SECURITY     Any security descriptor change
   SIZE         Any change in a file's size

(C<$filter> can also be an integer composed from the
C<FILE_NOTIFY_CHANGE_*> constants.)

=item $notify->close

Shut down monitoring.  You could just C<undef $notify> instead (but
C<close> works even if there are other copies of the object).  This
happens automatically when your program exits.

=item $notify->reset

Resets the ChangeNotification object after a change has been detected.
The object will become signalled again after the next change.  (It is
OK to call this immediately after C<new>, but it is not required.)

=item $notify->wait

See L<"Win32::IPC">.  Remember to call C<reset> afterwards if you want
to continue monitoring.

=back

=head2 Deprecated Functions and Methods

B<Win32::ChangeNotify> still supports the ActiveWare syntax, but its
use is deprecated.

=over 4

=item FindFirst($Obj,$PathName,$WatchSubTree,$Filter)

Use

  $Obj = Win32::ChangeNotify->new($PathName,$WatchSubTree,$Filter)

instead.

=item $obj->FindNext()

Use C<$obj-E<gt>reset> instead.

=item $obj->Close()

Use C<$obj-E<gt>close> instead.

=back

=head1 AUTHOR

Christopher J. Madsen E<lt>F<cjm@pobox.com>E<gt>

Loosely based on the original module by ActiveWare Internet Corp.,
F<http://www.ActiveWare.com>

=cut

# Local Variables:
# tmtrack-file-task: "Win32::ChangeNotify"
# End:
