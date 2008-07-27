package Win32::File;

#
# File.pm
# Written by Douglas_Lankshear@ActiveWare.com
#
# subsequent hacks:
#   Gurusamy Sarathy
#

$VERSION = '0.05';

require Exporter;
require DynaLoader;

@ISA= qw( Exporter DynaLoader );
@EXPORT = qw(
		ARCHIVE
		COMPRESSED
		DIRECTORY
		HIDDEN
		NORMAL
		OFFLINE
		READONLY
		SYSTEM
		TEMPORARY
	    );
@EXPORT_OK = qw(GetAttributes SetAttributes);

=head1 NAME

Win32::File - manage file attributes in perl

=head1 SYNOPSIS

	use Win32::File;

=head1 DESCRIPTION

This module offers the retrieval and setting of file attributes.

=head1 Functions

=head2 NOTE

All of the functions return FALSE (0) if they fail, unless otherwise noted.
The function names are exported into the caller's namespace by request.

=over 10

=item GetAttributes(filename, returnedAttributes)

Gets the attributes of a file or directory. returnedAttributes will be set
to the OR-ed combination of the filename attributes.

=item SetAttributes(filename, newAttributes)

Sets the attributes of a file or directory. newAttributes must be an OR-ed
combination of the attributes.

=back

=head1 Constants

The following constants are exported by default.

=over 10

=item ARCHIVE

=item COMPRESSED

=item DIRECTORY

=item HIDDEN

=item NORMAL

=item OFFLINE

=item READONLY

=item SYSTEM

=item TEMPORARY

=back

=cut

sub AUTOLOAD 
{
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname);
    if($! != 0)
	{
		if($! =~ /Invalid/)
		{
			$AutoLoader::AUTOLOAD = $AUTOLOAD;
			goto &AutoLoader::AUTOLOAD;
		}
		else 
		{
			($pack,$file,$line) = caller;
			die "Your vendor has not defined Win32::File macro $constname, used in $file at line $line.";
		}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Win32::File;

1;
__END__
