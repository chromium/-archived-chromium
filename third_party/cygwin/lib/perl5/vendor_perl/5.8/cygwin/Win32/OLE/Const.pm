# The documentation is at the __END__

package Win32::OLE::Const;

use strict;
use Carp;
use Win32::OLE;

my $Typelibs;
sub _Typelib {
    my ($clsid,$title,$version,$langid,$filename) = @_;
    # Filenames might have a resource index appended to it.
    $filename = $1 if $filename =~ /^(.*\.(?:dll|exe))(\\\d+)$/i;
    # Ignore if it looks like a file but doesn't exist.
    # We don't verify existance of monikers or filenames
    # without a full pathname.
    return unless -f $filename || $filename !~ /^\w:\\.*\.(exe|dll)$/;
    push @$Typelibs, \@_;
}
__PACKAGE__->_Typelibs;

sub import {
    my ($self,$name,$major,$minor,$language,$codepage) = @_;
    return unless defined($name) && $name !~ /^\s*$/;
    $self->Load($name,$major,$minor,$language,$codepage,scalar caller);
}

sub EnumTypeLibs {
    my ($self,$callback) = @_;
    foreach (@$Typelibs) { &$callback(@$_) }
    return;
}

sub Load {
    my ($self,$name,$major,$minor,$language,$codepage,$caller) = @_;

    if (UNIVERSAL::isa($name,'Win32::OLE')) {
	my $typelib = $name->GetTypeInfo->GetContainingTypeLib;
	return _Constants($typelib, undef);
    }

    undef $minor unless defined $major;
    my $typelib = $self->LoadRegTypeLib($name,$major,$minor,
					$language,$codepage);
    return _Constants($typelib, $caller);
}

sub LoadRegTypeLib {
    my ($self,$name,$major,$minor,$language,$codepage) = @_;
    undef $minor unless defined $major;

    unless (defined($name) && $name !~ /^\s*$/) {
	carp "Win32::OLE::Const->Load: No or invalid type library name";
	return;
    }

    my @found;
    foreach my $Typelib (@$Typelibs) {
	my ($clsid,$title,$version,$langid,$filename) = @$Typelib;
	next unless $title =~ /^$name/;
	next unless $version =~ /^([0-9a-fA-F]+)\.([0-9a-fA-F]+)$/;
	my ($maj,$min) = (hex($1), hex($2));
	next if defined($major) && $maj != $major;
	next if defined($minor) && $min < $minor;
	next if defined($language) && $language != $langid;
	push @found, [$clsid,$maj,$min,$langid,$filename];
    }

    unless (@found) {
	carp "No type library matching \"$name\" found";
	return;
    }

    @found = sort {
	# Prefer greater version number
	my $res = $b->[1] <=> $a->[1];
	$res = $b->[2] <=> $a->[2] if $res == 0;
	# Prefer default language for equal version numbers
	$res = -1 if $res == 0 && $a->[3] == 0;
	$res =  1 if $res == 0 && $b->[3] == 0;
	$res;
    } @found;

    #printf "Loading %s\n", join(' ', @{$found[0]});
    return _LoadRegTypeLib(@{$found[0]},$codepage);
}

1;

__END__

=head1 NAME

Win32::OLE::Const - Extract constant definitions from TypeLib

=head1 SYNOPSIS

    use Win32::OLE::Const 'Microsoft Excel';
    printf "xlMarkerStyleDot = %d\n", xlMarkerStyleDot;

    my $wd = Win32::OLE::Const->Load("Microsoft Word 8\\.0 Object Library");
    foreach my $key (keys %$wd) {
        printf "$key = %s\n", $wd->{$key};
    }

=head1 DESCRIPTION

This modules makes all constants from a registered OLE type library
available to the Perl program.  The constant definitions can be
imported as functions, providing compile time name checking.
Alternatively the constants can be returned in a hash reference
which avoids defining lots of functions of unknown names.

=head2 Functions/Methods

=over 4

=item use Win32::OLE::Const

The C<use> statement can be used to directly import the constant names
and values into the users namespace.

    use Win32::OLE::Const (TYPELIB,MAJOR,MINOR,LANGUAGE);

The TYPELIB argument specifies a regular expression for searching
through the registry for the type library.  Note that this argument is
implicitly prefixed with C<^> to speed up matches in the most common
cases.  Use a typelib name like ".*Excel" to match anywhere within the
description.  TYPELIB is the only required argument.

The MAJOR and MINOR arguments specify the requested version of
the type specification.  If the MAJOR argument is used then only
typelibs with exactly this major version number will be matched.  The
MINOR argument however specifies the minimum acceptable minor version.
MINOR is ignored if MAJOR is undefined.

If the LANGUAGE argument is used then only typelibs with exactly this
language id will be matched.

The module will select the typelib with the highest version number
satisfying the request.  If no language id is specified then a the default
language (0) will be preferred over the others.

Note that only constants with valid Perl variable names will be exported,
i.e. names matching this regexp: C</^[a-zA-Z_][a-zA-Z0-9_]*$/>.

=item Win32::OLE::Const->Load

The Win32::OLE::Const->Load method returns a reference to a hash of
constant definitions.

    my $const = Win32::OLE::Const->Load(TYPELIB,MAJOR,MINOR,LANGUAGE);

The parameters are the same as for the C<use> case.

This method is generally preferrable when the typelib uses a non-english
language and the constant names contain locale specific characters not
allowed in Perl variable names.

Another advantage is that all available constants can now be enumerated.

The load method also accepts an OLE object as a parameter.  In this case
the OLE object is queried about its containing type library and no registry
search is done at all.  Interestingly this seems to be slower.

=back

=head1 EXAMPLES

The first example imports all Excel constants names into the main namespace
and prints the value of xlMarkerStyleDot (-4118).

    use Win32::OLE::Const ('Microsoft Excel 8.0 Object Library');
    print "xlMarkerStyleDot = %d\n", xlMarkerStyleDot;

The second example returns all Word constants in a hash ref.

    use Win32::OLE::Const;
    my $wd = Win32::OLE::Const->Load("Microsoft Word 8.0 Object Library");
    foreach my $key (keys %$wd) {
        printf "$key = %s\n", $wd->{$key};
    }
    printf "wdGreen = %s\n", $wd->{wdGreen};

The last example uses an OLE object to specify the type library:

    use Win32::OLE;
    use Win32::OLE::Const;
    my $Excel = Win32::OLE->new('Excel.Application', 'Quit');
    my $xl = Win32::OLE::Const->Load($Excel);


=head1 AUTHORS/COPYRIGHT

This module is part of the Win32::OLE distribution.

=cut
