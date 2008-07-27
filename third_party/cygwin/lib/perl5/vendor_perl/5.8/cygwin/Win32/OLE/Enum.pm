# The documentation is at the __END__

package Win32::OLE::Enum;
1;

# everything is pure XS in Win32::OLE::Enum
# - new
# - DESTROY
#
# - All
# - Clone
# - Next
# - Reset
# - Skip

__END__

=head1 NAME

Win32::OLE::Enum - OLE Automation Collection Objects

=head1 SYNOPSIS

    my $Sheets = $Excel->Workbooks(1)->Worksheets;
    my $Enum = Win32::OLE::Enum->new($Sheets);
    my @Sheets = $Enum->All;

    while (defined(my $Sheet = $Enum->Next)) { ... }

=head1 DESCRIPTION

This module provides an interface to OLE collection objects from
Perl.  It defines an enumerator object closely mirroring the
functionality of the IEnumVARIANT interface.

Please note that the Reset() method is not available in all implementations
of OLE collections (like Excel 7).  In that case the Enum object is good
only for a single walk through of the collection.

=head2 Functions/Methods

=over 8

=item Win32::OLE::Enum->new($object)

Creates an enumerator for $object, which must be a valid OLE collection
object.  Note that correctly implemented collection objects must support
the C<Count> and C<Item> methods, so creating an enumerator is not always
necessary.

=item $Enum->All()

Returns a list of all objects in the collection.  You have to call
$Enum->Reset() before the enumerator can be used again.  The previous
position in the collection is lost.

This method can also be called as a class method:

	my @list = Win32::OLE::Enum->All($Collection);

=item $Enum->Clone()

Returns a clone of the enumerator maintaining the current position within
the collection (if possible).  Note that the C<Clone> method is often not
implemented.  Use $Enum->Clone() in an eval block to avoid dying if you
are not sure that Clone is supported.

=item $Enum->Next( [$count] )

Returns the next element of the collection.  In a list context the optional
$count argument specifies the number of objects to be returned.  In a scalar
context only the last of at most $count retrieved objects is returned.  The
default for $count is 1.

=item $Enum->Reset()

Resets the enumeration sequence to the beginning.  There is no guarantee that
the exact same set of objects will be enumerated again (e.g. when enumerating
files in a directory).  The methods return value indicates the success of the
operation.  (Note that the Reset() method seems to be unimplemented in some
applications like Excel 7.  Use it in an eval block to avoid dying.)

=item $Enum->Skip( [$count] )

Skip the next $count elements of the enumeration.  The default for $count is 1.
The functions returns TRUE if at least $count elements could be skipped.  It
returns FALSE if not enough elements were left.

=back

=head1 AUTHORS/COPYRIGHT

This module is part of the Win32::OLE distribution.

=cut
