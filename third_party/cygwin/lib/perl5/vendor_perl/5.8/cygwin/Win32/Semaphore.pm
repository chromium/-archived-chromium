#---------------------------------------------------------------------
package Win32::Semaphore;
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
# Use Win32 semaphore objects for synchronization
#---------------------------------------------------------------------

$VERSION = '1.02';

use Win32::IPC 1.00 '/./';      # Import everything
require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader Win32::IPC);
@EXPORT_OK = qw(
  wait_all wait_any
);

bootstrap Win32::Semaphore;

sub Create  { $_[0] = new('Win32::Semaphore',@_[1..3]) }
sub Open  { $_[0] = Win32::Semaphore->open($_[1]) }
sub Release { &release }

1;
__END__

=head1 NAME

Win32::Semaphore - Use Win32 semaphore objects from Perl

=head1 SYNOPSIS
	require Win32::Semaphore;

	$sem = Win32::Semaphore->new($initial,$maximum,$name);
	$sem->wait;

=head1 DESCRIPTION

This module allows access to Win32 semaphore objects.  The C<wait>
method and C<wait_all> & C<wait_any> functions are inherited from the
L<"Win32::IPC"> module.

=head2 Methods

=over 4

=item $semaphore = Win32::Semaphore->new($initial, $maximum, [$name])

Constructor for a new semaphore object.  C<$initial> is the initial
count, and C<$maximum> is the maximum count for the semaphore.  If
C<$name> is omitted, creates an unnamed semaphore object.

If C<$name> signifies an existing semaphore object, then C<$initial>
and C<$maximum> are ignored and the object is opened.  If this
happens, C<$^E> will be set to 183 (ERROR_ALREADY_EXISTS).

=item $semaphore = Win32::Semaphore->open($name)

Constructor for opening an existing semaphore object.

=item $semaphore->release([$increment, [$previous]])

Increment the count of C<$semaphore> by C<$increment> (default 1).
If C<$increment> plus the semaphore's current count is more than its
maximum count, the count is not changed.  Returns true if the
increment is successful.

The semaphore's count (before incrementing) is stored in the second
argument (if any).

It is not necessary to wait on a semaphore before calling C<release>,
but you'd better know what you're doing.

=item $semaphore->wait([$timeout])

Wait for C<$semaphore>'s count to be nonzero, then decrement it by 1.
See L<"Win32::IPC">.

=back

=head2 Deprecated Functions and Methods

B<Win32::Semaphore> still supports the ActiveWare syntax, but its use
is deprecated.

=over 4

=item Win32::Semaphore::Create($SemObject,$Initial,$Max,$Name)

Use C<$SemObject = Win32::Semaphore-E<gt>new($Initial,$Max,$Name)> instead.

=item Win32::Semaphore::Open($SemObject, $Name)

Use C<$SemObject = Win32::Semaphore-E<gt>open($Name)> instead.

=item $SemObj->Release($Count,$LastVal)

Use C<$SemObj-E<gt>release($Count,$LastVal)> instead.

=back

=head1 AUTHOR

Christopher J. Madsen E<lt>F<cjm@pobox.com>E<gt>

Loosely based on the original module by ActiveWare Internet Corp.,
F<http://www.ActiveWare.com>

=cut
