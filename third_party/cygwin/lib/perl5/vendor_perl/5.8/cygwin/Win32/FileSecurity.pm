package Win32::FileSecurity;

#
# FileSecurity.pm
# By Monte Mitzelfelt, monte@conchas.nm.org
# Larry Wall's Artistic License applies to all related Perl
#  and C code for this module
# Thanks to the guys at ActiveWare!
# ver 0.67 ALPHA 1997.07.07
#

require Exporter;
require DynaLoader;
use Carp ;

$VERSION = '1.04';

require Win32 unless defined &Win32::IsWinNT;
croak "The Win32::FileSecurity module works only on Windows NT" unless Win32::IsWinNT();

@ISA= qw( Exporter DynaLoader );

require Exporter ;
require DynaLoader ;

@ISA = qw(Exporter DynaLoader) ;
@EXPORT_OK = qw(
	Get
	Set
	EnumerateRights
	MakeMask
	DELETE
	READ_CONTROL
	WRITE_DAC
	WRITE_OWNER
	SYNCHRONIZE
	STANDARD_RIGHTS_REQUIRED
	STANDARD_RIGHTS_READ
	STANDARD_RIGHTS_WRITE
	STANDARD_RIGHTS_EXECUTE
	STANDARD_RIGHTS_ALL
	SPECIFIC_RIGHTS_ALL
	ACCESS_SYSTEM_SECURITY
	MAXIMUM_ALLOWED
	GENERIC_READ
	GENERIC_WRITE
	GENERIC_EXECUTE
	GENERIC_ALL
	F
	FULL
	R
	READ
	C
	CHANGE
	A
	ADD
	       ) ;

sub AUTOLOAD {
    local($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    $val = constant($constname);
    if($! != 0) {
	if($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    ($pack,$file,$line) = caller;
	    die "Your vendor has not defined Win32::FileSecurity macro "
	       ."$constname, used in $file at line $line.";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Win32::FileSecurity;

1;

__END__

=head1 NAME

Win32::FileSecurity - manage FileSecurity Discretionary Access Control Lists in perl

=head1 SYNOPSIS

	use Win32::FileSecurity;

=head1 DESCRIPTION

This module offers control over the administration of system FileSecurity DACLs.  
You may want to use Get and EnumerateRights to get an idea of what mask values
correspond to what rights as viewed from File Manager.

=head1 CONSTANTS

  DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER,
  SYNCHRONIZE, STANDARD_RIGHTS_REQUIRED, 
  STANDARD_RIGHTS_READ, STANDARD_RIGHTS_WRITE,
  STANDARD_RIGHTS_EXECUTE, STANDARD_RIGHTS_ALL,
  SPECIFIC_RIGHTS_ALL, ACCESS_SYSTEM_SECURITY, 
  MAXIMUM_ALLOWED, GENERIC_READ, GENERIC_WRITE,
  GENERIC_EXECUTE, GENERIC_ALL, F, FULL, R, READ,
  C, CHANGE

=head1 FUNCTIONS

=head2 NOTE:

All of the functions return false if they fail, unless otherwise noted.
Errors returned via $! containing both Win32 GetLastError() and a text message
indicating Win32 function that failed.

=over 10

=item constant( $name, $set )

Stores the value of named constant $name into $set.
Same as C<$set = Win32::FileSecurity::NAME_OF_CONSTANT();>.

=item Get( $filename, \%permisshash )

Gets the DACLs of a file or directory.

=item Set( $filename, \%permisshash )

Sets the DACL for a file or directory.

=item EnumerateRights( $mask, \@rightslist )

Turns the bitmask in $mask into a list of strings in @rightslist.

=item MakeMask( qw( DELETE READ_CONTROL ) )

Takes a list of strings representing constants and returns a bitmasked
integer value.

=back

=head2 %permisshash

Entries take the form $permisshash{USERNAME} = $mask ;

=head1 EXAMPLE1

    # Gets the rights for all files listed on the command line.
    use Win32::FileSecurity qw(Get EnumerateRights);
    
    foreach( @ARGV ) {
	next unless -e $_ ;
	if ( Get( $_, \%hash ) ) {
	    while( ($name, $mask) = each %hash ) {
		print "$name:\n\t"; 
		EnumerateRights( $mask, \@happy ) ;
		print join( "\n\t", @happy ), "\n";
	    }
	}
	else {
	    print( "Error #", int( $! ), ": $!" ) ;
	}
    }

=head1 EXAMPLE2

    # Gets existing DACL and modifies Administrator rights
    use Win32::FileSecurity qw(MakeMask Get Set);
    
    # These masks show up as Full Control in File Manager
    $file = MakeMask( qw( FULL ) );
    
    $dir = MakeMask( qw(
	    FULL
	GENERIC_ALL
    ) );
    
    foreach( @ARGV ) {
	s/\\$//;
	next unless -e;
	Get( $_, \%hash ) ;
	$hash{Administrator} = ( -d ) ? $dir : $file ;
	Set( $_, \%hash ) ;
    }

=head1 COMMON MASKS FROM CACLS AND WINFILE

=head2 READ

	MakeMask( qw( FULL ) ); # for files
	MakeMask( qw( READ GENERIC_READ GENERIC_EXECUTE ) ); # for directories

=head2 CHANGE

	MakeMask( qw( CHANGE ) ); # for files
	MakeMask( qw( CHANGE GENERIC_WRITE GENERIC_READ GENERIC_EXECUTE ) ); # for directories

=head2 ADD & READ

	MakeMask( qw( ADD GENERIC_READ GENERIC_EXECUTE ) ); # for directories only!

=head2 FULL

	MakeMask( qw( FULL ) ); # for files
	MakeMask( qw( FULL  GENERIC_ALL ) ); # for directories

=head1 RESOURCES

From Microsoft: check_sd
	http://premium.microsoft.com/download/msdn/samples/2760.exe

(thanks to Guert Schimmel at Sybase for turning me on to this one)

=head1 VERSION

1.03 ALPHA	97-12-14

=head1 REVISION NOTES

=over 10

=item 1.03 ALPHA 1998.01.11

Imported diffs from 0.67 (parent) version

=item 1.02 ALPHA 1997.12.14

Pod fixes, @EXPORT list additions <gsar@activestate.com>

Fix unitialized vars on unknown ACLs <jmk@exc.bybyte.de>

=item 1.01 ALPHA 1997.04.25

CORE Win32 version imported from 0.66 <gsar@activestate.com>

=item 0.67 ALPHA 1997.07.07

Kludged bug in mapping bits to separate ACE's.  Notably, this screwed
up CHANGE access by leaving out a delete bit in the
C<INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE> Access Control Entry.

May need to rethink...

=item 0.66 ALPHA 1997.03.13

Fixed bug in memory allocation check

=item 0.65 ALPHA 1997.02.25

Tested with 5.003 build 303

Added ISA exporter, and @EXPORT_OK

Added F, FULL, R, READ, C, CHANGE as composite pre-built mask names.

Added server\ to keys returned in hash from Get

Made constants and MakeMask case insensitive (I don't know why I did that)

Fixed mask comparison in ListDacl and Enumerate Rights from simple & mask
to exact bit match ! ( ( x & y ) ^ x ) makes sure all bits in x
are set in y

Fixed some "wild" pointers

=item 0.60 ALPHA 1996.07.31

Now suitable for file and directory permissions

Included ListDacl.exe in bundle for debugging

Added "intuitive" inheritance for directories, basically functions like FM
triggered by presence of GENERIC_ rights this may need to change

see EXAMPLE2

Changed from AddAccessAllowedAce to AddAce for control over inheritance

=item 0.51 ALPHA 1996.07.20

Fixed memory allocation bug

=item 0.50 ALPHA 1996.07.29

Base functionality

Using AddAccessAllowedAce

Suitable for file permissions

=back

=head1 KNOWN ISSUES / BUGS

=over 10

=item 1

May not work on remote drives.

=item 2

Errors croak, don't return via $! as documented.

=cut
