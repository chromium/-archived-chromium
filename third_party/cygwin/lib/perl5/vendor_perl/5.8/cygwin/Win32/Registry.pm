package Win32::Registry;

=head1 NAME

Win32::Registry - accessing the Windows registry [obsolete, use Win32::TieRegistry]

=head1 SYNOPSIS

    use Win32::Registry;
    my $tips;
    $::HKEY_LOCAL_MACHINE->Open("SOFTWARE\\Microsoft\\Windows"
                               ."\\CurrentVersion\\Explorer\\Tips", $tips)
        or die "Can't open tips: $^E";
    my ($type, $value);
    $tips->QueryValueEx("18", $type, $value) or die "No tip #18: $^E";
    print "Here's a tip: $value\n";

=head1 DESCRIPTION

    NOTE: This module provides a very klunky interface to access the
    Windows registry, and is not currently being developed actively.  It
    only exists for backward compatibility with old code that uses it.
    For more powerful and flexible ways to access the registry, use
    Win32::TieRegistry.

Win32::Registry provides an object oriented interface to the Windows
Registry.

The following "root" registry objects are exported to the main:: name
space.  Additional keys must be opened by calling the provided methods
on one of these.

    $HKEY_CLASSES_ROOT
    $HKEY_CURRENT_USER
    $HKEY_LOCAL_MACHINE
    $HKEY_USERS
    $HKEY_PERFORMANCE_DATA
    $HKEY_CURRENT_CONFIG
    $HKEY_DYN_DATA

=cut

use strict;
require Exporter;
require DynaLoader;
use Win32::WinError;

require Win32 unless defined &Win32::GetLastError;

use vars qw($VERSION $AUTOLOAD @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

$VERSION = '0.07';

@ISA	= qw( Exporter DynaLoader );
@EXPORT = qw(
	HKEY_CLASSES_ROOT
	HKEY_CURRENT_USER
	HKEY_LOCAL_MACHINE
	HKEY_PERFORMANCE_DATA
	HKEY_CURRENT_CONFIG
	HKEY_DYN_DATA
	HKEY_USERS
	KEY_ALL_ACCESS
	KEY_CREATE_LINK
	KEY_CREATE_SUB_KEY
	KEY_ENUMERATE_SUB_KEYS
	KEY_EXECUTE
	KEY_NOTIFY
	KEY_QUERY_VALUE
	KEY_READ
	KEY_SET_VALUE
	KEY_WRITE
	REG_BINARY
	REG_CREATED_NEW_KEY
	REG_DWORD
	REG_DWORD_BIG_ENDIAN
	REG_DWORD_LITTLE_ENDIAN
	REG_EXPAND_SZ
	REG_FULL_RESOURCE_DESCRIPTOR
	REG_LEGAL_CHANGE_FILTER
	REG_LEGAL_OPTION
	REG_LINK
	REG_MULTI_SZ
	REG_NONE
	REG_NOTIFY_CHANGE_ATTRIBUTES
	REG_NOTIFY_CHANGE_LAST_SET
	REG_NOTIFY_CHANGE_NAME
	REG_NOTIFY_CHANGE_SECURITY
	REG_OPENED_EXISTING_KEY
	REG_OPTION_BACKUP_RESTORE
	REG_OPTION_CREATE_LINK
	REG_OPTION_NON_VOLATILE
	REG_OPTION_RESERVED
	REG_OPTION_VOLATILE
	REG_REFRESH_HIVE
	REG_RESOURCE_LIST
	REG_RESOURCE_REQUIREMENTS_LIST
	REG_SZ
	REG_WHOLE_HIVE_VOLATILE
);

@EXPORT_OK = qw(
    RegCloseKey
    RegConnectRegistry
    RegCreateKey
    RegCreateKeyEx
    RegDeleteKey
    RegDeleteValue
    RegEnumKey
    RegEnumValue
    RegFlushKey
    RegGetKeySecurity
    RegLoadKey
    RegNotifyChangeKeyValue
    RegOpenKey
    RegOpenKeyEx
    RegQueryInfoKey
    RegQueryValue
    RegQueryValueEx
    RegReplaceKey
    RegRestoreKey
    RegSaveKey
    RegSetKeySecurity
    RegSetValue
    RegSetValueEx
    RegUnLoadKey
);
$EXPORT_TAGS{ALL}= \@EXPORT_OK;

bootstrap Win32::Registry;

sub import {
    my $pkg = shift;
    if ($_[0] && "Win32" eq $_[0]) {
	Exporter::export($pkg, "Win32", @EXPORT_OK);
	shift;
    }
    Win32::Registry->export_to_level(1+$Exporter::ExportLevel, $pkg, @_);
}

#######################################################################
# This AUTOLOAD is used to 'autoload' constants from the constant()
# XS function.  If a constant is not found then control is passed
# to the AUTOLOAD in AutoLoader.

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname, 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    my ($pack,$file,$line) = caller;
	    die "Unknown constant $constname in Win32::Registry "
	       . "at $file line $line.\n";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

#######################################################################
# _new is a private constructor, not intended for public use.
#

sub _new {
    my $self;
    if ($_[0]) {
	$self->{'handle'} = $_[0];
	bless $self;
    }
    $self;
}

#define the basic registry objects to be exported.
#these had to be hardwired unfortunately.
# XXX Yuck!

{
    package main;
    use vars qw(
		$HKEY_CLASSES_ROOT
		$HKEY_CURRENT_USER
		$HKEY_LOCAL_MACHINE
		$HKEY_USERS
		$HKEY_PERFORMANCE_DATA
		$HKEY_CURRENT_CONFIG
		$HKEY_DYN_DATA
	       );
}

$::HKEY_CLASSES_ROOT		= _new(&HKEY_CLASSES_ROOT);
$::HKEY_CURRENT_USER		= _new(&HKEY_CURRENT_USER);
$::HKEY_LOCAL_MACHINE		= _new(&HKEY_LOCAL_MACHINE);
$::HKEY_USERS			= _new(&HKEY_USERS);
$::HKEY_PERFORMANCE_DATA	= _new(&HKEY_PERFORMANCE_DATA);
$::HKEY_CURRENT_CONFIG		= _new(&HKEY_CURRENT_CONFIG);
$::HKEY_DYN_DATA		= _new(&HKEY_DYN_DATA);

=head2 Methods

The following methods are supported.  Note that subkeys can be
specified as a path name, separated by backslashes (which may
need to be doubled if you put them in double quotes).

=over 8

=item Open

    $reg_obj->Open($sub_key_name, $sub_reg_obj);

Opens a subkey of a registry object, returning the new registry object
in $sub_reg_obj.

=cut

sub Open {
    my $self = shift;
    die 'usage: $obj->Open($sub_key_name, $sub_reg_obj)' if @_ != 2;
    
    my ($subkey) = @_;
    my ($result,$subhandle);

    $result = RegOpenKey($self->{'handle'},$subkey,$subhandle);
    $_[1] = _new($subhandle);
    
    return 0 unless $_[1];
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item Close

    $reg_obj->Close();

Closes an open registry key.

=cut

sub Close {
    my $self = shift;
    die 'usage: $obj->Close()' if @_ != 0;

    return unless exists $self->{'handle'};
    my $result = RegCloseKey($self->{'handle'});
    if ($result) {
	delete $self->{'handle'};
    }
    else {
	$! = Win32::GetLastError();
    }
    return $result;
}

sub DESTROY {
    my $self = shift;
    return unless exists $self->{'handle'};
    RegCloseKey($self->{'handle'});
    delete $self->{'handle'};
}


=item Connect

    $reg_obj->Connect($node_name, $new_reg_obj);

Connects to a remote Registry on the node specified by $node_name,
returning it in $new_reg_obj.  Returns false if it fails.

=cut

sub Connect {
    my $self = shift;
    die 'usage: $obj->Connect($node_name, $new_reg_obj)' if @_ != 2;
     
    my ($node) = @_;
    my ($result,$subhandle);

    $result = RegConnectRegistry ($node, $self->{'handle'}, $subhandle);
    $_[1] = _new($subhandle);

    return 0 unless $_[1];
    $! = Win32::GetLastError() unless $result;
    return $result;
}  

=item Create

    $reg_obj->Create($sub_key_name, $new_reg_obj);

Opens the subkey specified by $sub_key_name, returning the new registry
object in $new_reg_obj.  If the specified subkey doesn't exist, it is
created.

=cut

sub Create {
    my $self = shift;
    die 'usage: $obj->Create($sub_key_name, $new_reg_obj)' if @_ != 2;

    my ($subkey) = @_;
    my ($result,$subhandle);

    $result = RegCreateKey($self->{'handle'},$subkey,$subhandle);
    $_[1] = _new ($subhandle);

    return 0 unless $_[1];
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item SetValue

    $reg_obj->SetValue($sub_key_name, $type, $value);

Sets the default value for a subkey specified by $sub_key_name.

=cut

sub SetValue {
    my $self = shift;
    die 'usage: $obj->SetValue($subkey, $type, $value)' if @_ != 3;
    my $result = RegSetValue($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item SetValueEx

    $reg_obj->SetValueEx($value_name, $reserved, $type, $value);

Sets the value for the value name identified by $value_name
in the key specified by $reg_obj.

=cut

sub SetValueEx {
    my $self = shift;
    die 'usage: $obj->SetValueEx($value_name, $reserved, $type, $value)' if @_ != 4;
    my $result = RegSetValueEx($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item QueryValue

    $reg_obj->QueryValue($sub_key_name, $value);

Gets the default value of the subkey specified by $sub_key_name.

=cut

sub QueryValue {
    my $self = shift;
    die 'usage: $obj->QueryValue($sub_key_name, $value)' if @_ != 2;
    my $result = RegQueryValue($self->{'handle'}, $_[0], $_[1]);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item QueryKey

    $reg_obj->QueryKey($classref, $number_of_subkeys, $number_of_values);

Gets information on a key specified by $reg_obj.

=cut

sub QueryKey {
    my $garbage;
    my $self = shift;
    die 'usage: $obj->QueryKey($classref, $number_of_subkeys, $number_of_values)'
    	if @_ != 3;

    my $result = RegQueryInfoKey($self->{'handle'}, $_[0],
    				 $garbage, $garbage, $_[1],
			         $garbage, $garbage, $_[2],
			         $garbage, $garbage, $garbage, $garbage);

    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item QueryValueEx

    $reg_obj->QueryValueEx($value_name, $type, $value);

Gets the value for the value name identified by $value_name
in the key specified by $reg_obj.

=cut

sub QueryValueEx {
    my $self = shift;
    die 'usage: $obj->QueryValueEx($value_name, $type, $value)' if @_ != 3;
    my $result = RegQueryValueEx($self->{'handle'}, $_[0], undef, $_[1], $_[2]);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item GetKeys

    my @keys;
    $reg_obj->GetKeys(\@keys);

Populates the supplied array reference with the names of all the keys
within the registry object $reg_obj.

=cut

sub GetKeys {
    my $self = shift;
    die 'usage: $obj->GetKeys($arrayref)' if @_ != 1 or ref($_[0]) ne 'ARRAY';

    my ($result, $i, $keyname);
    $keyname = "DummyVal";
    $i = 0;
    $result = 1;
    
    while ( $result ) {
	$result = RegEnumKey( $self->{'handle'},$i++, $keyname );
	if ($result) {
	    push( @{$_[0]}, $keyname );
	}
    }
    return(1);
}

=item GetValues

    my %values;
    $reg_obj->GetValues(\%values);

Populates the supplied hash reference with entries of the form

    $value_name => [ $value_name, $type, $data ]

for each value in the registry object $reg_obj.

=cut

sub GetValues {
    my $self = shift;
    die 'usage: $obj->GetValues($hashref)' if @_ != 1;

    my ($result,$name,$type,$data,$i);
    $name = "DummyVal";
    $i = 0;
    while ( $result=RegEnumValue( $self->{'handle'},
				  $i++,
				  $name,
				  undef,
				  $type,
				  $data ))
    {
	$_[0]->{$name} = [ $name, $type, $data ];
    }
    return(1);
}

=item DeleteKey

    $reg_obj->DeleteKey($sub_key_name);

Deletes a subkey specified by $sub_key_name from the registry.

=cut

sub DeleteKey {
    my $self = shift;
    die 'usage: $obj->DeleteKey($sub_key_name)' if @_ != 1;
    my $result = RegDeleteKey($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item DeleteValue

    $reg_obj->DeleteValue($value_name);

Deletes a value identified by $value_name from the registry.

=cut

sub DeleteValue {
    my $self = shift;
    die 'usage: $obj->DeleteValue($value_name)' if @_ != 1;
    my $result = RegDeleteValue($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item Save

    $reg_obj->Save($filename);

Saves the hive specified by $reg_obj to a file.

=cut

sub Save {
    my $self = shift;
    die 'usage: $obj->Save($filename)' if @_ != 1;
    my $result = RegSaveKey($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item Load

    $reg_obj->Load($sub_key_name, $file_name);

Loads a key specified by $sub_key_name from a file.

=cut

sub Load {
    my $self = shift;
    die 'usage: $obj->Load($sub_key_name, $file_name)' if @_ != 2;
    my $result = RegLoadKey($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

=item UnLoad

    $reg_obj->Unload($sub_key_name);

Unloads a registry hive.

=cut

sub UnLoad {
    my $self = shift;
    die 'usage: $obj->UnLoad($sub_key_name)' if @_ != 1;
    my $result = RegUnLoadKey($self->{'handle'}, @_);
    $! = Win32::GetLastError() unless $result;
    return $result;
}

1;
__END__
