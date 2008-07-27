#---------------------------------------------------------------------
package Win32::Mutex;
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
# Version: 1.00 (6-Feb-1998)
#
# This program is free software; you can redistribute it and/or modify
# it under the same terms as Perl itself.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either the
# GNU General Public License or the Artistic License for more details.
#
# Use Win32 mutex objects for synchronization
#---------------------------------------------------------------------

$VERSION = '1.02';

use Win32::IPC 1.00 '/./';      # Import everything
require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader Win32::IPC);
@EXPORT_OK = qw(
  wait_all wait_any
);

bootstrap Win32::Mutex;

sub Create  { $_[0] = Win32::Mutex->new(@_[1..2]) }
sub Open  { $_[0] = Win32::Mutex->open($_[1]) }
sub Release { &release }

1;
__END__

=head1 NAME

Win32::Mutex - Use Win32 mutex objects from Perl

=head1 SYNOPSIS

	require Win32::Mutex;

	$mutex = Win32::Mutex->new($initial,$name);
	$mutex->wait;

=head1 DESCRIPTION

This module allows access to the Win32 mutex objects.  The C<wait>
method and C<wait_all> & C<wait_any> functions are inherited from the
L<"Win32::IPC"> module.

=head2 Methods

=over 4

=item $mutex = Win32::Mutex->new([$initial, [$name]])

Constructor for a new mutex object.  If C<$initial> is true, requests
immediate ownership of the mutex (default false).  If C<$name> is
omitted, creates an unnamed mutex object.

If C<$name> signifies an existing mutex object, then C<$initial> is
ignored and the object is opened.  If this happens, C<$^E> will be set
to 183 (ERROR_ALREADY_EXISTS).

=item $mutex = Win32::Mutex->open($name)

Constructor for opening an existing mutex object.

=item $mutex->release

Release ownership of a C<$mutex>.  You should have obtained ownership
of the mutex through C<new> or one of the wait functions.  Returns
true if successful.

=item $mutex->wait([$timeout])

Wait for ownership of C<$mutex>.  See L<"Win32::IPC">.

=back

=head2 Deprecated Functions and Methods

B<Win32::Mutex> still supports the ActiveWare syntax, but its use is
deprecated.

=over 4

=item Create($MutObj,$Initial,$Name)

Use C<$MutObj = Win32::Mutex-E<gt>new($Initial,$Name)> instead.

=item Open($MutObj,$Name)

Use C<$MutObj = Win32::Mutex-E<gt>open($Name)> instead.

=item $MutObj->Release()

Use C<$MutObj-E<gt>release> instead.

=back

=head1 AUTHOR

Christopher J. Madsen E<lt>F<cjm@pobox.com>E<gt>

Loosely based on the original module by ActiveWare Internet Corp.,
F<http://www.ActiveWare.com>

=cut

# Local Variables:
# tmtrack-file-task: "Win32::Mutex"
# End:
