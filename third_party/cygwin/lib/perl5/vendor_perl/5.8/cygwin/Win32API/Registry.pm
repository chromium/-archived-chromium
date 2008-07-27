# Registry.pm -- Low-level access to functions/constants from WINREG.h

package Win32API::Registry;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS); #@EXPORT_FAIL);
$VERSION= '0.24';

require Exporter;
require DynaLoader;
@ISA= qw(Exporter DynaLoader);

@EXPORT= qw();
%EXPORT_TAGS= (
    Func =>	[qw(		regConstant		regLastError
	AllowPriv		AbortSystemShutdown	InitiateSystemShutdown
	RegCloseKey		RegConnectRegistry	RegCreateKey
	RegCreateKeyEx		RegDeleteKey		RegDeleteValue
	RegEnumKey		RegEnumKeyEx		RegEnumValue
	RegFlushKey		RegGetKeySecurity	RegLoadKey
	RegNotifyChangeKeyValue	RegOpenKey		RegOpenKeyEx
	RegQueryInfoKey		RegQueryMultipleValues	RegQueryValue
	RegQueryValueEx		RegReplaceKey		RegRestoreKey
	RegSaveKey		RegSetKeySecurity	RegSetValue
	RegSetValueEx		RegUnLoadKey )],
    FuncA =>	[qw(
	AbortSystemShutdownA	InitiateSystemShutdownA
	RegConnectRegistryA	RegCreateKeyA		RegCreateKeyExA
	RegDeleteKeyA		RegDeleteValueA		RegEnumKeyA
	RegEnumKeyExA		RegEnumValueA		RegLoadKeyA
	RegOpenKeyA		RegOpenKeyExA		RegQueryInfoKeyA
	RegQueryMultipleValuesA	RegQueryValueA		RegQueryValueExA
	RegReplaceKeyA		RegRestoreKeyA		RegSaveKeyA
	RegSetValueA		RegSetValueExA		RegUnLoadKeyA )],
    FuncW =>	[qw(
	AbortSystemShutdownW	InitiateSystemShutdownW
	RegConnectRegistryW	RegCreateKeyW		RegCreateKeyExW
	RegDeleteKeyW		RegDeleteValueW		RegEnumKeyW
	RegEnumKeyExW		RegEnumValueW		RegLoadKeyW
	RegOpenKeyW		RegOpenKeyExW		RegQueryInfoKeyW
	RegQueryMultipleValuesW	RegQueryValueW		RegQueryValueExW
	RegReplaceKeyW		RegRestoreKeyW		RegSaveKeyW
	RegSetValueW		RegSetValueExW		RegUnLoadKeyW )],
    HKEY_ =>	[qw(
	HKEY_CLASSES_ROOT	HKEY_CURRENT_CONFIG	HKEY_CURRENT_USER
	HKEY_DYN_DATA		HKEY_LOCAL_MACHINE	HKEY_PERFORMANCE_DATA
	HKEY_USERS )],
    KEY_ =>	[qw(
	KEY_QUERY_VALUE		KEY_SET_VALUE		KEY_CREATE_SUB_KEY
	KEY_ENUMERATE_SUB_KEYS	KEY_NOTIFY		KEY_CREATE_LINK
	KEY_READ		KEY_WRITE		KEY_EXECUTE
	KEY_ALL_ACCESS),
	'KEY_DELETE',		# DELETE          (0x00010000L)
	'KEY_READ_CONTROL',	# READ_CONTROL    (0x00020000L)
	'KEY_WRITE_DAC',	# WRITE_DAC       (0x00040000L)
	'KEY_WRITE_OWNER',	# WRITE_OWNER     (0x00080000L)
	'KEY_SYNCHRONIZE',	# SYNCHRONIZE     (0x00100000L) (not used)
	],
    REG_ =>	[qw(
	REG_OPTION_RESERVED	REG_OPTION_NON_VOLATILE	REG_OPTION_VOLATILE
	REG_OPTION_CREATE_LINK	REG_OPTION_BACKUP_RESTORE
	REG_OPTION_OPEN_LINK	REG_LEGAL_OPTION	REG_CREATED_NEW_KEY
	REG_OPENED_EXISTING_KEY	REG_WHOLE_HIVE_VOLATILE	REG_REFRESH_HIVE
	REG_NO_LAZY_FLUSH	REG_NOTIFY_CHANGE_ATTRIBUTES
	REG_NOTIFY_CHANGE_NAME	REG_NOTIFY_CHANGE_LAST_SET
	REG_NOTIFY_CHANGE_SECURITY			REG_LEGAL_CHANGE_FILTER
	REG_NONE		REG_SZ			REG_EXPAND_SZ
	REG_BINARY		REG_DWORD		REG_DWORD_LITTLE_ENDIAN
	REG_DWORD_BIG_ENDIAN	REG_LINK		REG_MULTI_SZ
	REG_RESOURCE_LIST	REG_FULL_RESOURCE_DESCRIPTOR
	REG_RESOURCE_REQUIREMENTS_LIST )],
    SE_ =>	[qw(
	SE_ASSIGNPRIMARYTOKEN_NAME	SE_AUDIT_NAME
	SE_BACKUP_NAME			SE_CHANGE_NOTIFY_NAME
	SE_CREATE_PAGEFILE_NAME		SE_CREATE_PERMANENT_NAME
	SE_CREATE_TOKEN_NAME		SE_DEBUG_NAME
	SE_INCREASE_QUOTA_NAME		SE_INC_BASE_PRIORITY_NAME
	SE_LOAD_DRIVER_NAME		SE_LOCK_MEMORY_NAME
	SE_MACHINE_ACCOUNT_NAME		SE_PROF_SINGLE_PROCESS_NAME
	SE_REMOTE_SHUTDOWN_NAME		SE_RESTORE_NAME
	SE_SECURITY_NAME		SE_SHUTDOWN_NAME
	SE_SYSTEMTIME_NAME		SE_SYSTEM_ENVIRONMENT_NAME
	SE_SYSTEM_PROFILE_NAME		SE_TAKE_OWNERSHIP_NAME
	SE_TCB_NAME			SE_UNSOLICITED_INPUT_NAME )],
);
@EXPORT_OK= ();
{ my $ref;
    foreach $ref (  values(%EXPORT_TAGS)  ) {
	push( @EXPORT_OK, @$ref )   unless  $ref->[0] =~ /^SE_/;
    }
}
$EXPORT_TAGS{ALL}= [ @EXPORT_OK ];	# \@EXPORT_OK once SE_* settles down.
# push( @EXPORT_OK, "JHEREG_TACOSALAD" );	# Used to test Mkconst2perl
push( @EXPORT_OK, @{$EXPORT_TAGS{SE_}} );

bootstrap Win32API::Registry $VERSION;

# Preloaded methods go here.

# To convert C constants to Perl code in cRegistry.pc
# [instead of C or C++ code in cRegistry.h]:
#    * Modify F<Makefile.PL> to add WriteMakeFile() =>
#      CONST2PERL/postamble => [[ "Win32API::Registry" => ]] WRITE_PERL => 1.
#    * Either comment out C<#include "cRegistry.h"> from F<Registry.xs>
#      or make F<cRegistry.h> an empty file.
#    * Make sure the following C<if> block is not commented out.
#    * "nmake clean", "perl Makefile.PL", "nmake"

if(  ! defined &REG_NONE  ) {
    require "Win32API/Registry/cRegistry.pc";
}

# This would be convenient but inconsistant and hard to explain:
#push( @{$EXPORT_TAGS{ALL}}, @{$EXPORT_TAGS{SE_}} )
#  if  defined &SE_TCB_NAME;

sub regConstant
{
    my( $name )= @_;
    if(  1 != @_  ||  ! $name  ||  $name =~ /\W/  ) {
	require Carp;
	Carp::croak( 'Usage: ',__PACKAGE__,'::regConstant("CONST_NAME")' );
    }
    my $proto= prototype $name;
    if(  defined \&$name
     &&  defined $proto
     &&  "" eq $proto  ) {
	no strict 'refs';
	return &$name;
    }
    return undef;
}

# We provide this for backwards compatibility:
sub constant
{
    my( $name )= @_;
    my $value= regConstant( $name );
    if(  defined $value  ) {
	$!= 0;
	return $value;
    }
    $!= 11; # EINVAL
    return 0;
}

# BEGIN {
#     my $code= 'return _regLastError(@_)';
#     local( $!, $^E )= ( 1, 1 );
#     if(  $! ne $^E  ) {
# 	$code= '
# 	    local( $^E )= _regLastError(@_);
# 	    my $ret= $^E;
# 	    return $ret;
# 	';
#     }
#     eval "sub regLastError { $code }";
#     die "$@"   if  $@;
# }

package Win32API::Registry::_error;

use overload
    '""' => sub {
	require Win32 unless defined &Win32::FormatMessage;
	$_ = Win32::FormatMessage(Win32API::Registry::_regLastError());
	tr/\r\n//d;
	return $_;
    },
    '0+' => sub { Win32API::Registry::_regLastError() },
    'fallback' => 1;

sub new { return bless {}, shift }
sub set { Win32API::Registry::_regLastError($_[1]); return $_[0] }

package Win32API::Registry;

my $_error = new Win32API::Registry::_error;

sub regLastError {
    require Carp;
    Carp::croak('Usage: ',__PACKAGE__,'::regLastError( [$setWin32ErrCode] )') if @_ > 1;
    $_error->set($_[0]) if defined $_[0];
    return $_error;
}

# Since we ISA DynaLoader which ISA AutoLoader, we ISA AutoLoader so we
# need this next chunk to prevent Win32API::Registry->nonesuch() from
# looking for "nonesuch.al" and producing confusing error messages:
use vars qw($AUTOLOAD);
sub AUTOLOAD {
    require Carp;
    Carp::croak(
      "Can't locate method $AUTOLOAD via package Win32API::Registry" );
}

# Replace "&rout;" with "goto &rout;" when that is supported on Win32.

# Let user omit all buffer sizes:
sub RegEnumKeyExA {
    if(  6 == @_  ) {	splice(@_,4,0,[]);  splice(@_,2,0,[]);  }
    &_RegEnumKeyExA;
}
sub RegEnumKeyExW {
    if(  6 == @_  ) {	splice(@_,4,0,[]);  splice(@_,2,0,[]);  }
    &_RegEnumKeyExW;
}
sub RegEnumValueA {
    if(  6 == @_  ) {	splice(@_,2,0,[]);  push(@_,[]);  }
    &_RegEnumValueA;
}
sub RegEnumValueW {
    if(  6 == @_  ) {	splice(@_,2,0,[]);  push(@_,[]);  }
    &_RegEnumValueW;
}
sub RegQueryInfoKeyA {
    if(  11 == @_  ) {	splice(@_,2,0,[]);  }
    &_RegQueryInfoKeyA;
}
sub RegQueryInfoKeyW {
    if(  11 == @_  ) {	splice(@_,2,0,[]);  }
    &_RegQueryInfoKeyW;
}

sub RegEnumKeyA {
    push(@_,[])   if  3 == @_;
    &_RegEnumKeyA;
}
sub RegEnumKeyW {
    push(@_,[])   if  3 == @_;
    &_RegEnumKeyW;
}
sub RegGetKeySecurity {
    push(@_,[])   if  3 == @_;
    &_RegGetKeySecurity;
}
sub RegQueryMultipleValuesA {
    push(@_,[])   if  4 == @_;
    &_RegQueryMultipleValuesA;
}
sub RegQueryMultipleValuesW {
    push(@_,[])   if  4 == @_;
    &_RegQueryMultipleValuesW;
}
sub RegQueryValueA {
    push(@_,[])   if  3 == @_;
    &_RegQueryValueA;
}
sub RegQueryValueW {
    push(@_,[])   if  3 == @_;
    &_RegQueryValueW;
}
sub RegQueryValueExA {
    push(@_,[])   if  5 == @_;
    &_RegQueryValueExA;
}
sub RegQueryValueExW {
    push(@_,[])   if  5 == @_;
    &_RegQueryValueExW;
}
sub RegSetValueA {
    push(@_,0)   if  4 == @_;
    &_RegSetValueA;
}
sub RegSetValueW {
    push(@_,0)   if  4 == @_;
    &_RegSetValueW;
}
sub RegSetValueExA {
    push(@_,0)   if  5 == @_;
    &_RegSetValueExA;
}
sub RegSetValueExW {
    push(@_,0)   if  5 == @_;
    &_RegSetValueExW;
}

# Aliases for non-Unicode functions:
sub AbortSystemShutdown		{ &AbortSystemShutdownA; }
sub InitiateSystemShutdown	{ &InitiateSystemShutdownA; }
sub RegConnectRegistry		{ &RegConnectRegistryA; }
sub RegCreateKey		{ &RegCreateKeyA; }
sub RegCreateKeyEx		{ &RegCreateKeyExA; }
sub RegDeleteKey		{ &RegDeleteKeyA; }
sub RegDeleteValue		{ &RegDeleteValueA; }
sub RegEnumKey			{ &RegEnumKeyA; }
sub RegEnumKeyEx		{ &RegEnumKeyExA; }
sub RegEnumValue		{ &RegEnumValueA; }
sub RegLoadKey			{ &RegLoadKeyA; }
sub RegOpenKey			{ &RegOpenKeyA; }
sub RegOpenKeyEx		{ &RegOpenKeyExA; }
sub RegQueryInfoKey		{ &RegQueryInfoKeyA; }
sub RegQueryMultipleValues	{ &RegQueryMultipleValuesA; }
sub RegQueryValue		{ &RegQueryValueA; }
sub RegQueryValueEx		{ &RegQueryValueExA; }
sub RegReplaceKey		{ &RegReplaceKeyA; }
sub RegRestoreKey		{ &RegRestoreKeyA; }
sub RegSaveKey			{ &RegSaveKeyA; }
sub RegSetValue			{ &RegSetValueA; }
sub RegSetValueEx		{ &RegSetValueExA; }
sub RegUnLoadKey		{ &RegUnLoadKeyA; }

1;
__END__

=head1 NAME

Win32API::Registry - Low-level access to Win32 system API calls from WINREG.H

=head1 SYNOPSIS

  use Win32API::Registry 0.21 qw( :ALL );

  RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\Disk", 0, KEY_READ, $key );
    or  die "Can't open HKEY_LOCAL_MACHINE\\SYSTEM\\Disk: ",
	    regLastError(),"\n";
  RegQueryValueEx( $key, "Information", [], $type, $data, [] );
    or  die "Can't read HKEY_L*MACHINE\\SYSTEM\\Disk\\Information: ",
	    regLastError(),"\n";
  [...]
  RegCloseKey( $key )
    or  die "Can't close HKEY_LOCAL_MACHINE\\SYSTEM\\Disk: ",
	    regLastError(),"\n";

=head1 DESCRIPTION

This provides fairly low-level access to the Win32 System API
calls dealing with the Registry [mostly from WINREG.H].  This
is mostly intended to be used by other modules such as
C<Win32::TieRegistry> [which provides an extremely Perl-friendly
method for using the Registry].

For a description of the logical structure of the Registry, see
the documentation for the C<Win32::TieRegistry> module.

To pass in C<NULL> as the pointer to an optional buffer, pass in
an empty list reference, C<[]>.

Beyond raw access to the API calls and related constants, this module
handles smart buffer allocation and translation of return codes.

All calls return a true value for success and a false value for
failure.  After any failure, C<$^E> should automatically be set
to indicate the reason.  However, current versions of Perl often
overwrite C<$^E> too quickly, so you can use C<regLastError()>
instead, which is only set by Win32API::Registry routines.

Note that C<$!> is not set by these routines except by
C<Win32API::Registry::constant()> when a constant is not defined.

=head2 Exports

Nothing is exported by default.  The following tags can be used to
have sets of symbols exported.

[Note that much of the following documentation refers to the
behavior of the underlying API calls which may vary in current
and future versions of the Win32 API without any changes to this
module.  Therefore you should check the Win32 API documentation
directly when needed.]

=over

=item :Func

The basic function names:

=over

=item AllowPriv

=item C<AllowPriv( $sPrivName, $bEnable )>

Not a Win32 API call.  Enables or disables a specific privilege for
the current process.  Returns a true value if successful and a false
value [and sets C<$^E>/C<regLastError()>] on failure.  This routine
does not provide a way to tell if a privilege is currently enabled.

C<$sPrivname> is a Win32 privilege name [see L</:SE_>].  For example,
C<"SeBackupPrivilege"> [a.k.a. C<SE_BACKUP_NAME>] controls whether
you can use C<RegSaveKey()> and C<"SeRestorePrivilege"> [a.k.a.
C<SE_RESTORE_NAME>] controls whether you can use C<RegLoadKey()>.

If C<$bEnable> is true, then C<AllowPriv()> tries to enable the
privilege.  Otherwise it tries to disable the privilege.

=item AbortSystemShutdown

=item C<AbortSystemShutdown( $sComputerName )>

Tries to abort a remote shutdown request previously made via
C<InitiateSystemShutdown()>.  Returns a true value if successful
and a false value [and sets C<$^E>/C<regLastError()>] on failure.

=item InitiateSystemShutdown

=item C<InitiateSystemShutdown( $sComputer, $sMessage, $uTimeoutSecs, $bForce, $bReboot )>

Requests that a [remote] computer be shutdown or rebooted.
Returns a true value if successful and a false value [and
sets C<$^E>/C<regLastError()>] on failure.

C<$sComputer> is the name [or address] of the computer to be
shutdown or rebooted.  You can use C<[]> [for C<NULL>] or C<"">
to indicate the local computer.

C<$sMessage> is the message to be displayed in a pop-up window
on the desktop of the computer to be shutdown or rebooted until
the timeout expires or the shutdown is aborted via
C<AbortSystemShutdown()>.  With C<$iTimeoutSecs == 0>, the
message will never be visible.

C<$iTimeoutSecs> is the number of seconds to wait before starting
the shutdown.

If C<$bForce> is false, then any applications running on the remote
computer get a chance to prompt the remote user whether they want
to save changes.  Also, for any applications that do not exit quickly
enough, the operating system will prompt the user whether they wish
to wait longer for the application to exit or force it to exit now.
At any of these prompts the user can press B<CANCEL> to abort the
shutdown but if no applications have unsaved data, they will likely
all exit quickly and the shutdown will progress with the remote user
having no option to cancel the shutdown.

If C<$bForce> is true, all applications are told to exit immediately
and so will not prompt the user even if there is unsaved data.  Any
applications that take too long to exit will be forcibly killed after
a short time.  The only way to abort the shutdown is to call
C<AbortSystemShutdown()> before the timeout expires and there is no
way to abort the shutdown once it has begun.

If C<$bReboot> is true, the computer will automatically reboot once
the shutdown is complete.  If C<$bReboot> is false, then when the
shutdown is complete the computer will halt at a screen indicating
that the shutdown is complete and offering a way for the user to
start to boot the computer.

You must have the C<"SeRemoteShutdownPrivilege"> privilege
on the remote computer for this call to succeed.  If shutting
down the local computer, then the calling process must have
the C<"SeShutdownPrivilege"> privilege and have it enabled.

=item RegCloseKey

=item C<RegCloseKey( $hKey )>

Closes the handle to a Registry key returned by C<RegOpenKeyEx()>,
C<RegConnectRegistry()>, C<RegCreateKeyEx()>, or a few other
routines.  Returns a true value if successful and a false value
[and sets C<$^E>/C<regLastError()>] on failure.

=item RegConnectRegistry

=item C<RegConnectRegistry( $sComputer, $hRootKey, $ohKey )>

Connects to one of the root Registry keys of a remote computer.
Returns a true value if successful and a false value [and
sets C<$^E>/C<regLastError()>] on failure.

C<$sComputer> is the name [or address] of a remote computer
whose Registry you wish to access.

C<$hKey> must be either C<HKEY_LOCAL_MACHINE> or C<HKEY_USERS>
and specifies which root Registry key on the remote computer
you wish to have access to.

C<$phKey> will be set to the handle to be used to access the
remote Registry key if the call succeeds.

=item regConstant

=item C<$value= regConstant( $sConstantName )>

Fetch the value of a constant.  Returns C<undef> if C<$sConstantName>
is not the name of a constant supported by this module.  Never sets
C<$!> nor C<$^E>.

This function is rarely used since you will usually get the value of a
constant by having that constant imported into your package by listing
the constant name in the C<use Win32API::Registry> statement and then
simply using the constant name in your code [perhaps followed by
C<()>].  This function is useful for verifying constant names not in
Perl code, for example, after prompting a user to type in a constant
name.

=item RegCreateKey

=item C<RegCreateKey( $hKey, $sSubKey, $ohSubKey )>

This routine is meant only for compatibility with Windows version
3.1.  Use C<RegCreateKeyEx()> instead.

=item RegCreateKeyEx

=item C<RegCreateKeyEx( $hKey, $sSubKey, $uZero, $sClass, $uOpts, $uAccess, $pSecAttr, $ohNewKey, $ouDisp )>

Creates a new Registry subkey.  Returns a true value if successful and
a false value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sSubKey> is the name of the new subkey to be created.

C<$iZero> is reserved for future use and should always be specified
as C<0>.

C<$sClass> is a string to be used as the class for the new
subkey.  We are not aware of any current use for Registry key
class information so the empty string, C<"">, should usually
be used here.

C<$iOpts> is a numeric value containing bits that control options
used while creating the new subkey.  C<REG_OPTION_NON_VOLATILE>
is the default.  C<REG_OPTION_VOLATILE> [which is ignored on
Windows 95] means the data stored under this key is not kept
in a file and will not be preserved when the system reboots.
C<REG_OPTION_BACKUP_RESTORE> [also ignored on Windows 95] means
ignore the C<$iAccess> parameter and try to open the new key with
the access required to backup or restore the key.

C<$iAccess> is a numeric mask of bits specifying what type of
access is desired when opening the new subkey.  See C<RegOpenKeyEx()>.

C<$pSecAttr> is a C<SECURITY_ATTRIBUTES> structure packed into
a Perl string which controls whether the returned handle can be
inherited by child processes.  Normally you would pass C<[]> for
this parameter to have C<NULL> passed to the underlying API
indicating that the handle cannot be inherited.  If not under
Windows95, then C<$pSecAttr> also allows you to specify
C<SECURITY_DESCRIPTOR> that controls which users will have
what type of access to the new key -- otherwise the new key
inherits its security from its parent key.

C<$phKey> will be set to the handle to be used to access the new
subkey if the call succeeds.

C<$piDisp> will be set to either C<REG_CREATED_NEW_KEY> or
C<REG_OPENED_EXISTING_KEY> to indicate for which reason the
call succeeded.  Can be specified as C<[]> if you don't care.

If C<$phKey> and C<$piDisp> start out as integers, then they will
probably remain unchanged if the call fails.

=item RegDeleteKey

=item C<RegDeleteKey( $hKey, $sSubKey )>

Deletes a subkey of an open Registry key provided that the subkey
contains no subkeys of its own [but the subkey may contain values].
Returns a true value if successful and a false value [and sets
C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sSubKey> is the name of the subkey to be deleted.

=item RegDeleteValue

=item C<RegDeleteValue( $hKey, $sValueName )>

Deletes a value from an open Registry key.  Returns a true value if
successful and a false value [and sets C<$^E>/C<regLastError()>] on
failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sValueKey> is the name of the value to be deleted.

=item RegEnumKey

=item C<RegEnumKey( $hKey, $uIndex, $osName, $ilNameSize )>

This routine is meant only for compatibility with Windows version
3.1.  Use C<RegEnumKeyEx()> instead.

=item RegEnumKeyEx

=item C<RegEnumKeyEx( $hKey, $uIndex, $osName, $iolName, $pNull, $osClass, $iolClass, $opftLastWrite )>

Lets you enumerate the names of all of the subkeys directly under
an open Registry key.  Returns a true value if successful and a false
value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$iIndex> is the sequence number of the immediate subkey that
you want information on.  Start with this value as C<0> then
repeat the call incrementing this value each time until the
call fails with C<$^E>/C<regLastError()> numerically equal to
C<ERROR_NO_MORE_ITEMS>.

C<$sName> will be set to the name of the subkey.  Can be C<[]> if
you don't care about the name.

C<$plName> initially specifies the [minimum] buffer size to be
allocated for C<$sName>.  Will be set to the length of the subkey
name if the requested subkey exists even if C<$sName> isn't
successfully set to the subkey name.  See L<Buffer sizes> for
more information.

C<$pNull> is reserved for future used and should be passed as C<[]>.

C<$sClass> will be set to the class name for the subkey.  Can be
C<[]> if you don't care about the class.

C<$plClass> initially specifies the [minimum] buffer size to be
allocated for C<$sClass> and will be set to the length of the
subkey class name if the requested subkey exists.  See L<Buffer
sizes> for more information.

C<$pftLastWrite> will be set to a C<FILETIME> structure packed
into a Perl string and indicating when the subkey was last changed.
Can be C<[]>.

You may omit both C<$plName> and C<$plClass> to get the same effect
as passing in C<[]> for each of them.

=item RegEnumValue

=item C<RegEnumValue( $hKey, $uIndex, $osValName, $iolValName, $pNull, $ouType, $opValData, $iolValData )>

Lets you enumerate the names of all of the values contained in an
open Registry key.  Returns a true value if successful and a false
value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$iIndex> is the sequence number of the value that you want
information on.  Start with this value as C<0> then repeat the
call incrementing this value each time until the call fails with
C<ERROR_NO_MORE_ITEMS>.

C<$sValName> will be set to the name of the value.  Can be C<[]>
if you don't care about the name.

C<$plValName> initially specifies the [minimum] buffer size to be
allocated for C<$sValName>.  Will be set to the length of the value
name if the requested value exists even if C<$sValName> isn't
successfully set to the value name.  See L<Buffer sizes> for
more information.

C<$pNull> is reserved for future used and should be passed as C<[]>.

C<$piType> will be set to the type of data stored in the value data.
If the call succeeds, it will be set to a C<REG_*> value unless
passed in as C<[]>.

C<$pValData> will be set to the data [packed into a Perl string]
that is stored in the requested value.  Can be C<[]> if you don't
care about the value data.

C<$plValData> initially specifies the [minimum] buffer size to be
allocated for C<$sValData> and will be set to the length of the
value data if the requested value exists.  See L<Buffer sizes> for
more information.

You may omit both C<$plValName> and C<$plValData> to get the same
effect as passing in C<[]> for each of them.

=item RegFlushKey

=item C<RegFlushKey( $hKey )>

Forces the data stored under an open Registry key to be flushed
to the disk file where the data is preserved between reboots.
Forced flushing is not guaranteed to be efficient so this routine
should almost never be called.  Returns a true value if successful
and a false value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

=item RegGetKeySecurity

=item C<RegGetKeySecurity( $hKey, $uSecInfo, $opSecDesc, $iolSecDesc )>

Retrieves one of the C<SECURITY_DESCRIPTOR> structures describing
part of the security for an open Registry key.  Returns a true value
if successful and a false value [and sets C<$^E>/C<regLastError()>]
on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$iSecInfo> is a numeric C<SECURITY_INFORMATION> value that
specifies which parts of the C<SECURITY_DESCRIPTOR> structure
to retrieve.  Should be C<OWNER_SECURITY_INFORMATION>,
C<GROUP_SECURITY_INFORMATION>, C<DACL_SECURITY_INFORMATION>, or
or C<SACL_SECURITY_INFORMATION> or two or more of these bits
combined using C<|>.

C<$pSecDesc> will be set to the requested C<SECURITY_DESCRIPTOR>
structure [packed into a Perl string].

C<$plSecDesc> initially specifies the [minimum] buffer size to be
allocated for C<$sSecDesc> and will be set to the length of the
security descriptor.  See L<Buffer sizes> for more information.
You may omit this parameter to get the same effect as passing in
C<[]> for it.

=item regLastError

=item C<$svError= regLastError();>

=item C<regLastError( $uError );>

Returns the last error encountered by a routine from this module. 
It is just like C<$^E> except it isn't changed by anything except
routines from this module.  Ideally you could just use C<$^E>, but
current versions of Perl often overwrite C<$^E> before you get a
chance to check it and really old versions of Perl don't really
support C<$^E> under Win32.

Just like C<$^E>, in a numeric context C<regLastError()> returns
the numeric error value while in a string context it returns a
text description of the error [actually it returns a Perl scalar
that contains both values so C<$x= regLastError()> causes C<$x>
to give different values in string vs. numeric contexts].

The last form sets the error returned by future calls to
C<regLastError()> and should not be used often.  C<$uError> must
be a numeric error code.  Also returns the dual-valued version
of C<$uError>.

=item RegLoadKey

=item C<RegLoadKey( $hKey, $sSubKey, $sFileName )>

Loads a hive file.  That is, it creates a new subkey in the
Registry and associates that subkey with a disk file that contains
a Registry hive so that the new subkey can be used to access the
keys and values stored in that hive.  Hives are usually created
via C<RegSaveKey()>.  Returns a true value if successful and a
false value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key that can have hives
loaded to it.  This must be C<HKEY_LOCAL_MACHINE>, C<HKEY_USERS>,
or a remote version of one of these from a call to
C<RegConnectRegistry()>.

C<$sSubKey> is the name of the new subkey to created and associated
with the hive file.

C<$sFileName> is the name of the hive file to be loaded.  This
file name is interpretted relative to the
C<%SystemRoot%/System32/config> directory on the computer where
the C<$hKey> key resides.  If C<$sFileName> is on a FAT file
system, then its name must not have an extension.

You must have the C<SE_RESTORE_NAME> privilege to use this routine.

WARNING:  Loading of hive files via a network share may silently
corrupt the hive and so should not be attempted [this is a problem
in at least some versions of the underlying API which this module
does not try to fix or avoid].  To access a hive file located on a
remote computer, connect to the remote computer's Registry and load
the hive via that.

=item RegNotifyChangeKeyValue

=item C<RegNotifyChangeKeyValue( $hKey, $bWatchSubtree, $uNotifyFilter, $hEvent, $bAsync )>

Arranges for your process to be notified when part of the Registry
is changed.  Returns a true value if successful and a false value
[and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call] for which you wish to be notified when any changes
are made to it.

If C<$bWatchSubtree> is true, then changes to any subkey or
descendant of C<$hKey> are also reported.

C<$iNotifyFilter> controllers what types of changes are reported.  It
is a numeric value containing one or more of the following bit masks:

=over

=item C<REG_NOTIFY_CHANGE_NAME>

Notify if a subkey is added or deleted to a monitored key.

=item C<REG_NOTIFY_CHANGE_LAST_SET>

Notify if a value in a monitored key is added, deleted, or modified.

=item C<REG_NOTIFY_CHANGE_SECURITY>

Notify if a security descriptor of a monitored key is changed.

=item C<REG_NOTIFY_CHANGE_ATTRIBUTES>

Notify if any attributes of a monitored key are changed [class
name or security descriptors].

=back

C<$hEvent> is ignored unless C<$bAsync> is true.  Otherwise, C<$hEvent>
is a handle to a Win32 I<event> that will be signaled when changes are
to be reported.

If C<$bAsync> is true, then C<RegNotifyChangeKeyValue()> returns
immediately and uses C<$hEvent> to notify your process of changes.
If C<$bAsync> is false, then C<RegNotifyChangeKeyValue()> does
not return until there is a change to be notified of.

This routine does not work with Registry keys on remote computers.

=item RegOpenKey

=item C<RegOpenKey( $hKey, $sSubKey, $ohSubKey )>

This routine is meant only for compatibility with Windows version
3.1.  Use C<RegOpenKeyEx()> instead.

=item RegOpenKeyEx

=item C<RegOpenKeyEx( $hKey, $sSubKey, $uOptions, $uAccess, $ohSubKey )>

Opens an existing Registry key.  Returns a true value if successful
and a false value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sSubKey> is the name of an existing subkey to be opened.
Can be C<""> or C<[]> to open an additional handle to the
key specified by C<$hKey>.

C<$iOptions> is a numeric value containing bits that control options
used while opening the subkey.  There are currently no supported
options so this parameter should be specified as C<0>.

C<$iAccess> is a numeric mask of bits specifying what type of
access is desired when opening the new subkey.  Should be a
combination of one or more of the following bit masks:

=over

=item C<KEY_ALL_ACCESS>

    KEY_READ | KEY_WRITE | KEY_CREATE_LINK

=item C<KEY_READ>

    KEY_QUERY_VALUE | KEY_ENUMERATE_SUBKEYS | KEY_NOTIFY | STANDARD_RIGHTS_READ

=item C<KEY_WRITE>

    KEY_SET_VALUE | KEY_CREATE_SUB_KEY | STANDARD_RIGHTS_WRITE

=item C<KEY_QUERY_VALUE>

=item C<KEY_SET_VALUE>

=item C<KEY_ENUMERATE_SUB_KEYS>

=item C<KEY_CREATE_SUB_KEY>

=item C<KEY_NOTIFY>

Allows you to use C<RegNotifyChangeKeyValue()> on the opened key.

=item C<KEY_EXECUTE>

Same as C<KEY_READ>.

=item C<KEY_CREATE_LINK>

Gives you permission to create a symbolic link like
C<HKEY_CLASSES_ROOT> and C<HKEY_CURRENT_USER>, though the method for
doing so is not documented [and probably requires use of the mostly
undocumented "native" routines, C<Nt*()> a.k.a. C<Zw*()>].

=back

C<$phKey> will be set to the handle to be used to access the new subkey
if the call succeeds.

=item RegQueryInfoKey

=item C<RegQueryInfoKey( $hKey, $osClass, $iolClass, $pNull, $ocSubKeys, $olSubKey, $olSubClass, $ocValues, $olValName, $olValData, $olSecDesc, $opftTime )>

Gets miscellaneous information about an open Registry key.
Returns a true value if successful and a false value [and
sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sClass> will be set to the class name for the key.  Can be
C<[]> if you don't care about the class.

C<$plClass> initially specifies the [minimum] buffer size to be
allocated for C<$sClass> and will be set to the length of the
key's class name.  See L<Buffer sizes> for more information.
You may omit this parameter to get the same effect as passing in
C<[]> for it.

C<$pNull> is reserved for future use and should be passed as C<[]>.

C<$pcSubKeys> will be set to the count of the number of subkeys
directly under this key.  Can be C<[]>.

C<$plSubKey> will be set to the length of the longest subkey name.
Can be C<[]>.

C<$plSubClass> will be set to the length of the longest class name
used with an immediate subkey of this key.  Can be C<[]>.

C<$pcValues> will be set to the count of the number of values in
this key.  Can be C<[]>.

C<$plValName> will be set to the length of the longest value name
in this key.  Can be C<[]>.

C<$plValData> will be set to the length of the longest value data
in this key.  Can be C<[]>.

C<$plSecDesc> will be set to the length of this key's full security
descriptor.

C<$pftTime> will be set to a C<FILETIME> structure packed
into a Perl string and indicating when this key was last changed.
Can be C<[]>.

=item RegQueryMultipleValues

=item C<RegQueryMultipleValues( $hKey, $ioarValueEnts, $icValueEnts, $opBuffer, $iolBuffer )>

Allows you to use a single call to query several values from a single
open Registry key to maximize efficiency.  Returns a true value if
successful and a false value [and sets C<$^E>/C<regLastError()>] on
failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$pValueEnts> should contain a list of C<VALENT> structures packed
into a single Perl string.  Each C<VALENT> structure should have
the C<ve_valuename> entry [the first 4 bytes] pointing to a string
containing the name of a value stored in this key.  The remaining
fields are set if the function succeeds.

C<$cValueEnts> should contain the count of the number of C<VALENT>
structures contained in C<$pValueEnts>.

C<$pBuffer> will be set to the data from all of the requested values
concatenated into a single Perl string.

C<$plBuffer> initially specifies the [minimum] buffer size to be
allocated for C<$sBuffer> and will be set to the total length of
the data to be written to C<$sBuffer>.  See L<Buffer sizes> for
more information.  You may omit this parameter to get the same
effect as passing in C<[]> for it.

Here is sample code to populate C<$pValueEnts>:

    # @ValueNames= ...list of value name strings...;
    $cValueEnts= @ValueNames;
    $pValueEnts= pack( " p x4 x4 x4 " x $cValueEnts, @ValueNames );

Here is sample code to retrieve the data type and data length
returned in C<$pValueEnts>:

    @Lengths= unpack( " x4 L x4 x4 " x $cValueEnts, $pValueEnts );
    @Types=   unpack( " x4 x4 x4 L " x $cValueEnts, $pValueEnts );

Given the above, and assuming you haven't modified C<$sBuffer> since
the call, you can also extract the value data strings from C<$sBuffer>
by using the pointers returned in C<$pValueEnts>:

    @Data=    unpack(  join( "", map {" x4 x4 P$_ x4 "} @Lengths ),
    		$pValueEnts  );

Much better is to use the lengths and extract directly from
C<$sBuffer> using C<unpack()> [or C<substr()>]:

    @Data= unpack( join("",map("P$_",@Lengths)), $sBuffer );

=item RegQueryValue

=item C<RegQueryValue( $hKey, $sSubKey, $osValueData, $iolValueData )>

This routine is meant only for compatibility with Windows version
3.1.  Use C<RegQueryValueEx()> instead.  This routine can only
query unamed values [a.k.a. "default values"], that is, values with
a name of C<"">.

=item RegQueryValueEx

=item C<RegQueryValueEx( $hKey, $sValueName, $pNull, $ouType, $opValueData, $iolValueData )>

Lets you look up value data stored in an open Registry key by
specifying the value name.  Returns a true value if successful
and a false value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sValueName> is the name of the value whose data you wish to
retrieve.

C<$pNull> this parameter is reserved for future use and should be
specified as C<[]>.

C<$piType> will be set to indicate what type of data is stored in
the named value.  Will be set to a C<REG_*> value if the function
succeeds.

C<$pValueData> will be set to the value data [packed into a Perl
string] that is stored in the named value.  Can be C<[]> if you
don't care about the value data.

C<$plValueData> initially specifies the [minimum] buffer size to be
allocated for C<$sValueData> and will be set to the size [always
in bytes] of the data to be written to C<$sValueData>, even if
C<$sValueData> is not successfully written to.  See L<Buffer sizes>
for more information.

=item RegReplaceKey

=item C<RegReplaceKey( $hKey, $sSubKey, $sNewFile, $sOldFile )>

Lets you replace an entire hive when the system is next booted. 
Returns a true value if successful and a false value [and sets
C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key that has hive(s)
loaded in it.  This must be C<HKEY_LOCAL_MACHINE>,
C<HKEY_USERS>, or a remote version of one of these from
a call to C<RegConnectRegistry()>.

C<$sSubKey> is the name of the subkey of C<$hKey> whose hive
you wish to have replaced on the next reboot.

C<$sNewFile> is the name of a file that will replace the existing
hive file when the system reboots.

C<$sOldFile> is the file name to save the current hive file to
when the system reboots.

C<$sNewFile> and C<$sOldFile> are interpretted relative to the
C<%SystemRoot%/System32/config> directory on the computer where
the C<$hKey> key resides [I think].  If either file is [would be]
on a FAT file system, then its name must not have an extension.

You must have the C<SE_RESTORE_NAME> privilege to use this routine.

=item RegRestoreKey

=item C<RegRestoreKey( $hKey, $sFileName, $uFlags )>

Reads in a hive file and copies its contents over an existing
Registry tree.  Returns a true value if successful and a false
value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sFileName> is the name of the hive file to be read.  For each
value and subkey in this file, a value or subkey will be added
or replaced in C<$hKey>.

C<$uFlags> is usally C<0>.  It can also be C<REG_WHOLE_HIVE_VOLATILE>
which, rather than copying the hive over the existing key,
replaces the existing key with a temporary, memory-only Registry
key and then copies the hive contents into it.  This option only
works if C<$hKey> is C<HKEY_LOCAL_MACHINE>, C<HKEY_USERS>, or a
remote version of one of these from a call to C<RegConnectRegistry()>.

C<RegRestoreKey> does I<not> delete values nor keys from the
existing Registry tree when there is no corresponding value/key
in the hive file.

=item RegSaveKey

=item C<RegSaveKey( $hKey, $sFileName, $pSecAttr )>

Dumps any open Registry key and all of its subkeys and values into
a new hive file.  Returns a true value if successful and a false
value [and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sFileName> is the name of the file that the Registry tree
should be saved to.  It is interpretted relative to the
C<%SystemRoot%/System32/config> directory on the computer where
the C<$hKey> key resides.  If C<$sFileName> is on a FAT file system,
then it must not have an extension.

C<$pSecAttr> contains a C<SECURITY_ATTRIBUTES> structure that specifies
the permissions to be set on the new file that is created.  This can
be C<[]>.

You must have the C<SE_RESTORE_NAME> privilege to use this routine.

=item RegSetKeySecurity

=item C<RegSetKeySecurity( $hKey, $uSecInfo, $pSecDesc )>

Sets [part of] the C<SECURITY_DESCRIPTOR> structure describing part
of the security for an open Registry key.  Returns a true value if
successful and a false value [and sets C<$^E>/C<regLastError()>] on
failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$uSecInfo> is a numeric C<SECURITY_INFORMATION> value that
specifies which C<SECURITY_DESCRIPTOR> structure to set.  Should
be C<OWNER_SECURITY_INFORMATION>, C<GROUP_SECURITY_INFORMATION>,
C<DACL_SECURITY_INFORMATION>, or C<SACL_SECURITY_INFORMATION>
or two or more of these bits combined using C<|>.

C<$pSecDesc> contains the new C<SECURITY_DESCRIPTOR> structure
packed into a Perl string.

=item RegSetValue

=item C<RegSetValue( $hKey, $sSubKey, $uType, $sValueData, $lValueData )>

This routine is meant only for compatibility with Windows version
3.1.  Use C<RegSetValueEx()> instead.  This routine can only
set unamed values [a.k.a. "default values"].

=item RegSetValueEx

=item C<RegSetValueEx( $hKey, $sName, $uZero, $uType, $pData, $lData )>

Adds or replaces a value in an open Registry key.  Returns
a true value if successful and a false value [and sets
C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key [either C<HKEY_*> or from
a previous call].

C<$sName> is the name of the value to be set.

C<$uZero> is reserved for future use and should be specified as C<0>.

C<$uType> is the type of data stored in C<$pData>.  It should
be a C<REG_*> value.

C<$pData> is the value data packed into a Perl string.

C<$lData> is the length of the value data that is stored in C<$pData>.
You will usually omit this parameter or pass in C<0> to have
C<length($pData)> used.  In both of these cases, if C<$iType> is
C<REG_SZ> or C<REG_EXPAND_SZ>, C<RegSetValueEx()> will append a
trailing C<'\0'> to the end of C<$pData> [unless there is already
one].

=item RegUnLoadKey

=item C<RegUnLoadKey( $hKey, $sSubKey )>

Unloads a previously loaded hive file.  That is, closes the
hive file then deletes the subkey that was providing access
to it.  Returns a true value if successful and a false value
[and sets C<$^E>/C<regLastError()>] on failure.

C<$hKey> is the handle to a Registry key that has hives
loaded in it.  This must be C<HKEY_LOCAL_MACHINE>, C<HKEY_USERS>,
or a remote version of one of these from a call to
C<RegConnectRegistry()>.

C<$sSubKey> is the name of the subkey whose hive you wish to
have unloaded.

=item :FuncA

The ASCII-specific function names.

Each of these is identical to the version listed above without the
trailing "A":

	AbortSystemShutdownA	InitiateSystemShutdownA
	RegConnectRegistryA	RegCreateKeyA		RegCreateKeyExA
	RegDeleteKeyA		RegDeleteValueA		RegEnumKeyA
	RegEnumKeyExA		RegEnumValueA		RegLoadKeyA
	RegOpenKeyA		RegOpenKeyExA		RegQueryInfoKeyA
	RegQueryMultipleValuesA	RegQueryValueA		RegQueryValueExA
	RegReplaceKeyA		RegRestoreKeyA		RegSaveKeyA
	RegSetValueA		RegSetValueExA		RegUnLoadKeyA

=item :FuncW

The UNICODE-specific function names.  These are the same as the
versions listed above without the trailing "W" except that string
parameters are UNICODE strings rather than ASCII strings, as
indicated.

=item AbortSystemShutdownW

=item C<AbortSystemShutdownW( $swComputerName )>

C<$swComputerName> is UNICODE.

=item InitiateSystemShutdownW

=item C<InitiateSystemShutdownW( $swComputer, $swMessage, $uTimeoutSecs, $bForce, $bReboot )>

C<$swComputer> and C<$swMessage> are UNICODE.

=item RegConnectRegistryW

=item C<RegConnectRegistryW( $swComputer, $hRootKey, $ohKey )>

C<$swComputer> is UNICODE.

=item RegCreateKeyW

=item C<RegCreateKeyW( $hKey, $swSubKey, $ohSubKey )>

C<$swSubKey> is UNICODE.

=item RegCreateKeyExW

=item C<RegCreateKeyExW( $hKey, $swSubKey, $uZero, $swClass, $uOpts, $uAccess, $pSecAttr, $ohNewKey, $ouDisp )>

C<$swSubKey> and C<$swClass> are UNICODE.

=item RegDeleteKeyW

=item C<RegDeleteKeyW( $hKey, $swSubKey )>

C<$swSubKey> is UNICODE.

=item RegDeleteValueW

=item C<RegDeleteValueW( $hKey, $swValueName )>

C<$swValueName> is UNICODE.

=item RegEnumKeyW

=item C<RegEnumKeyW( $hKey, $uIndex, $oswName, $ilwNameSize )>

C<$oswName> is UNICODE and C<$ilwNameSize> is measured as number of
C<WCHAR>s.

=item RegEnumKeyExW

=item C<RegEnumKeyExW( $hKey, $uIndex, $oswName, $iolwName, $pNull, $oswClass, $iolwClass, $opftLastWrite )>

C<$swName> and C<$swClass> are UNICODE and C<$iolwName> and C<$iolwClass>
are measured as number of C<WCHAR>s.

=item RegEnumValueW

=item C<RegEnumValueW( $hKey, $uIndex, $oswName, $iolwName, $pNull, $ouType, $opData, $iolData )>

C<$oswName> is UNICODE and C<$iolwName> is measured as number
of C<WCHAR>s.

C<$opData> is UNICODE if C<$piType> is C<REG_SZ>, C<REG_EXPAND_SZ>,
or C<REG_MULTI_SZ>.  Note that C<$iolData> is measured as number
of bytes even in these cases.

=item RegLoadKeyW

=item C<RegLoadKeyW( $hKey, $swSubKey, $swFileName )>

C<$swSubKey> and C<$swFileName> are UNICODE.

=item RegOpenKeyW

=item C<RegOpenKeyW( $hKey, $swSubKey, $ohSubKey )>

C<$swSubKey> is UNICODE.

=item RegOpenKeyExW

=item C<RegOpenKeyExW( $hKey, $swSubKey, $uOptions, $uAccess, $ohSubKey )>

C<$swSubKey> is UNICODE.

=item RegQueryInfoKeyW

=item C<RegQueryInfoKeyW( $hKey, $oswClass, $iolwClass, $pNull, $ocSubKeys, $olwSubKey, $olwSubClass, $ocValues, $olwValName, $olValData, $olSecDesc, $opftTime )>

C<$swClass> is UNICODE.  C<$iolwClass>, C<$olwSubKey>, C<$olwSubClass>,
and C<$olwValName> are measured as number of C<WCHAR>s.  Note that
C<$olValData> is measured as number of bytes.

=item RegQueryMultipleValuesW

=item C<RegQueryMultipleValuesW( $hKey, $ioarValueEnts, $icValueEnts, $opBuffer, $iolBuffer )>

The C<ve_valuename> fields of the C<VALENT> [actually C<VALENTW>]
structures in C<$ioarValueEnts> are UNICODE.  Values of type C<REG_SZ>,
C<REG_EXPAND_SZ>, and C<REG_MULTI_SZ> are written to C<$opBuffer>
in UNICODE.  Note that C<$iolBuffer> and the C<ve_valuelen> fields
of the C<VALENT> [C<VALENTW>] structures are measured as number of
bytes.

=item RegQueryValueW

=item C<RegQueryValueW( $hKey, $swSubKey, $oswValueData, $iolValueData )>

C<$swSubKey> and C<$oswValueData> are UNICODE.  Note that
C<$iolValueData> is measured as number of bytes.

=item RegQueryValueExW

=item C<RegQueryValueExW( $hKey, $swName, $pNull, $ouType, $opData, $iolData )>

C<$swName> is UNICODE.

C<$opData> is UNICODE if C<$ouType> is C<REG_SZ>, C<REG_EXPAND_SZ>,
or C<REG_MULTI_SZ>.  Note that C<$iolData> is measured as number of
bytes even in these cases.

=item RegReplaceKeyW

=item C<RegReplaceKeyW( $hKey, $swSubKey, $swNewFile, $swOldFile )>

C<$swSubKey>, C<$swNewFile>, and C<$swOldFile> are UNICODE.

=item RegRestoreKeyW

=item C<RegRestoreKeyW( $hKey, $swFileName, $uFlags )>

C<$swFileName> is UNICODE.

=item RegSaveKeyW

=item C<RegSaveKeyW( $hKey, $swFileName, $pSecAttr )>

C<$swFileName> is UNICODE.

=item RegSetValueW

=item C<RegSetValueW( $hKey, $swSubKey, $uType, $swValueData, $lValueData )>

C<$swSubKey> and C<$swValueData> are UNICODE.  Note that
C<$lValueData> is measured as number of bytes even though
C<$swValueData> is always UNICODE.

=item RegSetValueExW

=item C<RegSetValueExW( $hKey, $swName, $uZero, $uType, $pData, $lData )>

C<$swName> is UNICODE.

C<$pData> is UNICODE if C<$uType> is C<REG_SZ>, C<REG_EXPAND_SZ>,
or C<REG_MULTI_SZ>.  Note that C<$lData> is measured as number of
bytes even in these cases.

=item RegUnLoadKeyW

=item C<RegUnLoadKeyW( $hKey, $swSubKey )>

C<$swSubKey> is UNICODE.

=item :HKEY_

All C<HKEY_*> constants:

	HKEY_CLASSES_ROOT	HKEY_CURRENT_CONFIG	HKEY_CURRENT_USER
	HKEY_DYN_DATA		HKEY_LOCAL_MACHINE	HKEY_PERFORMANCE_DATA
	HKEY_USERS

=item :KEY_

All C<KEY_*> constants:

	KEY_QUERY_VALUE		KEY_SET_VALUE		KEY_CREATE_SUB_KEY
	KEY_ENUMERATE_SUB_KEYS	KEY_NOTIFY		KEY_CREATE_LINK
	KEY_READ		KEY_WRITE		KEY_EXECUTE
	KEY_ALL_ACCESS

=item :REG_

All C<REG_*> constants:

	REG_CREATED_NEW_KEY		REG_OPENED_EXISTING_KEY

	REG_LEGAL_CHANGE_FILTER		REG_NOTIFY_CHANGE_ATTRIBUTES
	REG_NOTIFY_CHANGE_NAME		REG_NOTIFY_CHANGE_LAST_SET
	REG_NOTIFY_CHANGE_SECURITY	REG_LEGAL_OPTION

	REG_OPTION_BACKUP_RESTORE	REG_OPTION_CREATE_LINK
	REG_OPTION_NON_VOLATILE		REG_OPTION_OPEN_LINK
	REG_OPTION_RESERVED		REG_OPTION_VOLATILE

	REG_WHOLE_HIVE_VOLATILE		REG_REFRESH_HIVE
	REG_NO_LAZY_FLUSH

	REG_NONE			REG_SZ
	REG_EXPAND_SZ			REG_BINARY
	REG_DWORD			REG_DWORD_LITTLE_ENDIAN
	REG_DWORD_BIG_ENDIAN		REG_LINK
	REG_MULTI_SZ			REG_RESOURCE_LIST
	REG_FULL_RESOURCE_DESCRIPTOR	REG_RESOURCE_REQUIREMENTS_LIST

=item :ALL

All of the above.

=item :SE_

The strings for the following privilege names:

	SE_ASSIGNPRIMARYTOKEN_NAME	SE_AUDIT_NAME
	SE_BACKUP_NAME			SE_CHANGE_NOTIFY_NAME
	SE_CREATE_PAGEFILE_NAME		SE_CREATE_PERMANENT_NAME
	SE_CREATE_TOKEN_NAME		SE_DEBUG_NAME
	SE_INCREASE_QUOTA_NAME		SE_INC_BASE_PRIORITY_NAME
	SE_LOAD_DRIVER_NAME		SE_LOCK_MEMORY_NAME
	SE_MACHINE_ACCOUNT_NAME		SE_PROF_SINGLE_PROCESS_NAME
	SE_REMOTE_SHUTDOWN_NAME		SE_RESTORE_NAME
	SE_SECURITY_NAME		SE_SHUTDOWN_NAME
	SE_SYSTEMTIME_NAME		SE_SYSTEM_ENVIRONMENT_NAME
	SE_SYSTEM_PROFILE_NAME		SE_TAKE_OWNERSHIP_NAME
	SE_TCB_NAME			SE_UNSOLICITED_INPUT_NAME

It can be difficult to successfully build this module in a way
that makes these constants available.  So some builds of this
module may not make them available.  For such builds, trying
to export any of these constants will cause a fatal error.
For this reason, none of these symbols are currently included
in the C<":ALL"> grouping.

=back

=head2 The Win32API:: heirarchy

This and the other Win32API:: modules are meant to expose the
nearly raw API calls so they can be used from Perl code in any
way they might be used from C code.  This provides the following
advantages:

=over

=item Many modules can be written by people that don't have a C compiler.

=item Encourages more module code to be written in Perl [not C].

Perl code is often much easier to inspect, debug, customize, and
enhance than XS code.

=item Allows those already familiar with the Win32 API to get
off to a quick start.

=item Provides an interactive tool for exploring even obscure
details of the Win32 API.

It can be very useful to interactively explore ad-hoc calls into
parts of the Win32 API using:

    perl -de 0

=item Ensures that native Win32 data structures can be used.

This allows maximum efficiency.  It also allows data from one
module [for example, time or security information from the
C<Win32API::Registry> or C<Win32API::File> modules] to be used
with other modules [for example, C<Win32API::Time> and
C<Win32API::SecDesc>].

=item Provides a single version of the XS interface to each API
call where improvements can be collected.

=back

=head2 Buffer sizes

For each parameter that specifies a buffer size, a value of C<0>
can be passed.  For parameter that are pointers to buffer sizes,
you can also pass in C<NULL> by specifying an empty list reference,
C<[]>.  Both of these cases will ensure that the variable has
I<some> buffer space allocated to it and pass in that buffer's
allocated size.  Many of the calls indicate, via C<ERROR_MORE_DATA>,
that the buffer size was not sufficient and the F<Registry.xs>
code will automatically enlarge the buffer to the required size
and repeat the call.

Numeric buffer sizes are used as minimum initial sizes for the
buffers.  The larger of this size and the size of space already
allocated to the scalar will be passed to the underlying routine. 
If that size was insufficient, and the underlying call provides
an easy method for determining the needed buffer size, then the
buffer will be enlarged and the call repeated as above.

The underlying calls define buffer size parameter as unsigned, so
negative buffer sizes are treated as very large positive buffer
sizes which usually cause C<malloc()> to fail.

To force the F<Registry.xs> code to pass in a specific value for
a buffer size, preceed the size with an equals sign via C<"=".>. 
Buffer sizes that are passed in as strings starting with an equals
sign will have the equal sign stripped and the remainder of the string
interpretted as a number [via C's C<strtoul()> using only base 10]
which will be passed to the underlying routine [even if the allocated
buffer is actually larger].  The F<Registry.xs> code will enlarge the
buffer to the specified size, if needed, but will not enlarge the
buffer based on the underlying routine requesting more space.

Some Reg*() calls may not currently set the buffer size when they
return C<ERROR_MORE_DATA>.  But some that are not documented as
doing so, currently do so anyway.  So the code assumes that any
routine I<might> do this and resizes any buffers and repeats the
call.   We hope that eventually all routines will provide this
feature.

When you use C<[]> for a buffer size, you can still find the
length of the data returned by using C<length($buffer)>.  Note
that this length will be in bytes while a few of the buffer
sizes would have been in units of wide characters.

Note that the RegQueryValueEx*() and RegEnumValue*() calls
will trim the trailing C<'\0'> [if present] from the returned data
values of type C<REG_SZ> or C<REG_EXPAND_SZ> but only if the
value data length parameter is omitted [or specified as C<[]>].

The RegSetValueEx*() calls will add a trailing C<'\0'> [if
missing] to the supplied data values of type C<REG_SZ> and
C<REG_EXPAND_SZ> but only if the value data length parameter
is omitted [or specified as C<0>].

=head2 Hungarian Notation

The following abbreviations are used at the start of each parameter
name to hint at aspects of how the parameter is used.  The prefix
is always in lower case and followed by a capital letter that starts
the descriptive part of the parameter name.  Several of the following
abbreviations can be combined into a single prefix.

Probably not all of these prefix notations are used by this module. 
This document section may be included in any C<Win32API> module and
so covers some notations not used by this specific module.

=over

=item s

A string.  In C, a C<'\0'>-terminated C<char *>.  In Perl, just a
string except that it will be truncated at the first C<"\0">, if
it contains one.

=item sw

A wide [UNICODE] string.  In C, a C<L'\0'>-terminated C<WCHAR *>.
In Perl, a string that contains UNICODE data.  You can convert a
string to UNICODE in Perl via:

    $string= "This is an example string";
    $unicode= pack( "S*", unpack("C*",$string), 0 );

Note how C<, 0> above causes an explicit C<L'\0'> to be added since
Perl's implicit C<'\0'> that it puts after each of its strings is not
wide enough to terminate a UNICODE string.  So UNICODE strings are
different than regular strings in that the Perl version of a regular
string will not include the trialing C<'\0'> while the Perl version
of a UNICODE string must include the trailing C<L'\0'>.

If a UNICODE string contains no non-ASCII characters, then you
can convert it back into a normal string via:

    $string= pack( "C*", unpack("S*",$unicode) );
    $string =~ s/\0$//;

=item p

A pointer to some buffer [usually containing some C<struct>].  In C,
a C<void *> or some other pointer type.  In Perl, a string that is
usually manipulated using C<pack> and C<unpack>.  The "p" is usually
followed by more prefix character(s) to indicate what type of data is
stored in the bufffer.

=item a

A packed array.  In C, an array [usually of C<struct>s].  In Perl, a
string containing the packed data.  The "a" is usually followed by
more prefix character(s) to indicate the data type of the elements.

These packed arrays are also called "vectors" in places to avoid
confusion with Perl arrays.

=item n

A generic number.   In C, any of the integer or floating point data
types.  In Perl, a number; either an integer, unsigned, or double
[IV, UV, or NV, respectively].  Usually an integer.

=item iv

A signed integral value.  In C, any of the signed integer data types. 
In Perl, an integer [IV].

=item u

An unsigned integral value.  In C, any of the unsigned integer data
types.  In Perl, an unsigned integer [UV].

=item d

A floating-point number.  In C, a C<float> or C<double> or, perhaps,
a C<long double>.  In Perl, a double-precision floating-point number
[NV].

=item b

A Boolean value.  In C, any integer data type, though usually via
a type alias of C<bool> or C<BOOL>, containing either a 0 [false] or
non-zero [true] value.  In Perl, a scalar containing a Boolean value
[C<0>, C<"">, or C<undef> for "false" and anything else for "true"].

=item c

A count of items.  In C, any integer data type.  In Perl, an unsigned
integer [UV].  Usually used in conjunction with a "vector" parameter
[see L</a> above] to indicate the number of elements.

=item l

A length [in bytes].  In C, any integer data type.  In Perl, an
unsigned integer [UV].  Usually used in conjunction with a "string"
or "pointer" parameter [see L</s> and L</p> above] to indicate the
buffer size or the size of the value stored in the buffer.

For strings, there is no general rule as to whether the trailing
C<'\0'> is included in such sizes.  For this reason, the C<Win32API>
modules follow the Perl rule of always allocating one extra byte
and reporting buffer sizes as being one smaller than allocated in
case the C<'\0'> is not included in the size.

=item lw

A length measured as number of UNICODE characters.  In C, a count
of C<WCHAR>s.  In Perl, an unsigned integer [UV] counting "shorts"
[see "s" and "S" in C<pack> and C<unpack>].

For UNICODE strings, the trailing C<L'\0'> may or may not be
included in a length so, again, we always allocate extra room
for one and don't report that extra space.

=item h

A handle.  In C, a C<HANDLE> or more-specific handle data type. 
In Perl, an unsigned integer [UV].  In C, these handles are often
actually some type of pointer, but Perl just treats them as opaque
numbers, as it should.  This prefix is also used for other pointers
that are treated as integers in Perl code.

=item r

A record.  In C, almost always a C<struct> or perhaps C<union>.  Note
that C C<struct>s are rarely passed by value so the "r" is almost
always preceeded by a "p" or "a" [see L</p> and L</a> above].  For
the very rare unadorned "r", Perl stores the record in the same way
as a "pr", that is, in a string.  For the very rare case where Perl
explicitly stores a pointer to the C<struct> rather than storing the
C<struct> directly in a Perl string, the prefix "pp" or "ppr" or even
"par" is used.

=item sv

=item rv

=item hv

=item av

=item cv

A Perl data type.  Respectively, a scalar value [SV], a reference
[RV] [usually to a scalar], a hash [HV], a Perl array [AV], or a Perl
code reference [PVCV].  For the "hv", "av", and "cv" prefixes, a
leading "rv" is usually assumed.  For a parameter to an XS subroutine,
a prefix of "sv" means the parameter is a scalar and so may be a string
or a number [or C<undef>] or even both at the same time.  So "sv"
doesn't imply a leading "rv".

=item Input or Output

Whether a parameter is for input data, output data, or both is usually
not reflected by the data type prefix.  In cases where this is not
obvious nor reflected in the parameter name proper, we may use the
following in front of the data type prefix.

=over

=item i

An input parameter given to the API [usually omitted].

=item o

An output-only parameter taken from the API.  You should not get a
warning if such a parameter is C<undef> when you pass it into the
function.  You should get an error if such a parameter is read-only.
You can [usually] pass in C<[]> for such a parameter to have the
parameter silently ignored.

The output may be written directly into the Perl variable passed
to the subroutine, the same way the buffer parameter to Perl's
C<sysread()>.  This method is often avoided in Perl because
the call then lacks any visual cue that some parameters are being
overwritten.   But this method closely matches the C API which is
what we are trying to do.

=item io

Input given to the API then overwritten with output taken from the
API.  You should get a warning [if B<-w> is in effect] if such a
parameter is C<undef> when you pass it into the function [unless it
is a buffer or buffer length parameter].  If the value is read-only,
then [for most parameters] the output is silently not written.  This
is because it is often convenient to pass in read-only constants for
many such parameters.  You can also usually pass in C<[]> for such
parameters.

=back

=item pp

=item ppr

=item par

=item pap

These are just unusual combinations of prefix characters described above.

For each, a pointer is stored in a [4-byte] Perl string.  You can
usually use C<unpack "P"> to access the real data from Perl.

For "ppr" [and often for "pp"], the pointer points directly at a
C C<struct>.  For "par", the pointer points to the first element
of a C [packed] array of C<struct>s.  For "pap", the pointer points
to a C [packed] array of pointers to other things.

=item ap

Here we have a list of pointers packed into a single Perl string.

=back

=head1 BUGS

The old ActiveState ports of Perl for Win32 [but not, ActivePerl, the
ActiveState distributions of standard Perl 5.004 and beyond] do not support
the tools for building extensions and so do not support this extension.

No routines are provided for using the data returned in the C<FILETIME>
buffers.  Those are in the C<Win32API::Time> module.

No routines are provided for dealing with UNICODE data effectively. 
See L</:FuncW> above for some simple-minded UNICODE methods.

Parts of the module test will fail if used on a version of Perl
that does not yet set C<$^E> based on C<GetLastError()>.

On NT 4.0 [at least], the RegEnum*() calls do not set the required
buffer sizes when returning C<ERROR_MORE_DATA> so this module will
not grow the buffers in such cases.  C<Win32::TieRegistry> overcomes
this by using values from C<RegQueryInfoKey()> for buffer sizes in
RegEnum* calls.

On NT 4.0 [at least], C<RegQueryInfoKey()> on C<HKEY_PERFORMANCE_DATA>
never succeeds.  Also, C<RegQueryValueEx()> on C<HKEY_PERFORMANCE_DATA>
never returns the required buffer size.  To access C<HKEY_PERFORMANCE_DATA>
you will need to keep growing the data buffer until the call succeeds.

Because C<goto &subroutine> seems to be buggy under Win32 Perl,
it is not used in the stubs in F<Registry.pm>.

=head1 AUTHOR

Tye McQueen, tye@metronet.com, http://www.metronet.com/~tye/.

=head1 SEE ALSO

=over

=item L<Win32::TieRegistry>

=item L<Win32::Registry>

=back

=cut
