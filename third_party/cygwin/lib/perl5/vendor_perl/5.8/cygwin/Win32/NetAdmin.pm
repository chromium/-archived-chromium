package Win32::NetAdmin;

#
#NetAdmin.pm
#Written by Douglas_Lankshear@ActiveWare.com
#

$VERSION = '0.08';

require Exporter;
require DynaLoader;

require Win32 unless defined &Win32::IsWinNT;
die "The Win32::NetAdmin module works only on Windows NT" unless Win32::IsWinNT();

@ISA= qw( Exporter DynaLoader );
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	DOMAIN_ALIAS_RID_ACCOUNT_OPS
	DOMAIN_ALIAS_RID_ADMINS
	DOMAIN_ALIAS_RID_BACKUP_OPS
	DOMAIN_ALIAS_RID_GUESTS
	DOMAIN_ALIAS_RID_POWER_USERS
	DOMAIN_ALIAS_RID_PRINT_OPS
	DOMAIN_ALIAS_RID_REPLICATOR
	DOMAIN_ALIAS_RID_SYSTEM_OPS
	DOMAIN_ALIAS_RID_USERS
	DOMAIN_GROUP_RID_ADMINS
	DOMAIN_GROUP_RID_GUESTS
	DOMAIN_GROUP_RID_USERS
	DOMAIN_USER_RID_ADMIN
	DOMAIN_USER_RID_GUEST
	FILTER_TEMP_DUPLICATE_ACCOUNT
	FILTER_NORMAL_ACCOUNT
	FILTER_INTERDOMAIN_TRUST_ACCOUNT
	FILTER_WORKSTATION_TRUST_ACCOUNT
	FILTER_SERVER_TRUST_ACCOUNT
	SV_TYPE_WORKSTATION
	SV_TYPE_SERVER
	SV_TYPE_SQLSERVER
	SV_TYPE_DOMAIN_CTRL
	SV_TYPE_DOMAIN_BAKCTRL
	SV_TYPE_TIMESOURCE
	SV_TYPE_AFP
	SV_TYPE_NOVELL
	SV_TYPE_DOMAIN_MEMBER
	SV_TYPE_PRINT
	SV_TYPE_PRINTQ_SERVER
	SV_TYPE_DIALIN
	SV_TYPE_DIALIN_SERVER
	SV_TYPE_XENIX_SERVER
	SV_TYPE_NT
	SV_TYPE_WFW
	SV_TYPE_POTENTIAL_BROWSER
	SV_TYPE_BACKUP_BROWSER
	SV_TYPE_MASTER_BROWSER
	SV_TYPE_DOMAIN_MASTER
	SV_TYPE_DOMAIN_ENUM
	SV_TYPE_SERVER_UNIX
	SV_TYPE_SERVER_MFPN
	SV_TYPE_SERVER_NT
	SV_TYPE_SERVER_OSF
	SV_TYPE_SERVER_VMS
	SV_TYPE_WINDOWS
	SV_TYPE_DFS
	SV_TYPE_ALTERNATE_XPORT
	SV_TYPE_LOCAL_LIST_ONLY
	SV_TYPE_ALL
	UF_TEMP_DUPLICATE_ACCOUNT
	UF_NORMAL_ACCOUNT
	UF_INTERDOMAIN_TRUST_ACCOUNT
	UF_WORKSTATION_TRUST_ACCOUNT
	UF_SERVER_TRUST_ACCOUNT
	UF_MACHINE_ACCOUNT_MASK
	UF_ACCOUNT_TYPE_MASK
	UF_DONT_EXPIRE_PASSWD
	UF_SETTABLE_BITS
	UF_SCRIPT
	UF_ACCOUNTDISABLE
	UF_HOMEDIR_REQUIRED
	UF_LOCKOUT
	UF_PASSWD_NOTREQD
	UF_PASSWD_CANT_CHANGE
	USE_FORCE
	USE_LOTS_OF_FORCE
	USE_NOFORCE
	USER_PRIV_MASK
	USER_PRIV_GUEST
	USER_PRIV_USER
	USER_PRIV_ADMIN
);

@EXPORT_OK = qw(
    GetError
    GetDomainController
    GetAnyDomainController
    UserCreate
    UserDelete
    UserGetAttributes
    UserSetAttributes
    UserChangePassword
    UsersExist
    GetUsers
    GroupCreate
    GroupDelete
    GroupGetAttributes
    GroupSetAttributes
    GroupAddUsers
    GroupDeleteUsers
    GroupIsMember
    GroupGetMembers
    LocalGroupCreate
    LocalGroupDelete
    LocalGroupGetAttributes
    LocalGroupSetAttributes
    LocalGroupIsMember
    LocalGroupGetMembers
    LocalGroupGetMembersWithDomain
    LocalGroupAddUsers
    LocalGroupDeleteUsers
    GetServers
    GetTransports
    LoggedOnUsers
    GetAliasFromRID
    GetUserGroupFromRID
    GetServerDisks
);
$EXPORT_TAGS{ALL}= \@EXPORT_OK;

=head1 NAME

Win32::NetAdmin - manage network groups and users in perl

=head1 SYNOPSIS

	use Win32::NetAdmin;

=head1 DESCRIPTION

This module offers control over the administration of groups and users over a
network.

=head1 FUNCTIONS

=head2 NOTE

All of the functions return false if they fail, unless otherwise noted.
When a function fails call Win32::NetAdmin::GetError() rather than
GetLastError() or $^E to retrieve the error code.

C<server> is optional for all the calls below. If not given the local machine is
assumed.

=over 10

=item GetError()

Returns the error code of the last call to this module.

=item GetDomainController(server, domain, returnedName)

Returns the name of the domain controller for server.

=item GetAnyDomainController(server, domain, returnedName)

Returns the name of any domain controller for a domain that is directly trusted
by the server.

=item UserCreate(server, userName, password, passwordAge, privilege, homeDir, comment, flags, scriptPath)

Creates a user on server with password, passwordAge, privilege, homeDir, comment,
flags, and scriptPath.

=item UserDelete(server, user)

Deletes a user from server.

=item UserGetAttributes(server, userName, password, passwordAge, privilege, homeDir, comment, flags, scriptPath)

Gets password, passwordAge, privilege, homeDir, comment, flags, and scriptPath
for user.

=item UserSetAttributes(server, userName, password, passwordAge, privilege, homeDir, comment, flags, scriptPath)

Sets password, passwordAge, privilege, homeDir, comment, flags, and scriptPath
for user.

=item UserChangePassword(domainname, username, oldpassword, newpassword)

Changes a users password. Can be run under any account.

=item UsersExist(server, userName)

Checks if a user exists.

=item GetUsers(server, filter, userRef)

Fills userRef with user names if it is an array reference and with the user
names and the full names if it is a hash reference.

=item GroupCreate(server, group, comment)

Creates a group.

=item GroupDelete(server, group)

Deletes a group.

=item GroupGetAttributes(server, groupName, comment)

Gets the comment.

=item GroupSetAttributes(server, groupName, comment)

Sets the comment.

=item GroupAddUsers(server, groupName, users)

Adds a user to a group.

=item GroupDeleteUsers(server, groupName, users)

Deletes a users from a group.

=item GroupIsMember(server, groupName, user)

Returns TRUE if user is a member of groupName.

=item GroupGetMembers(server, groupName, userArrayRef)

Fills userArrayRef with the members of groupName.

=item LocalGroupCreate(server, group, comment)

Creates a local group.

=item LocalGroupDelete(server, group)

Deletes a local group.

=item LocalGroupGetAttributes(server, groupName, comment)

Gets the comment.

=item LocalGroupSetAttributes(server, groupName, comment)

Sets the comment.

=item LocalGroupIsMember(server, groupName, user)

Returns TRUE if user is a member of groupName.

=item LocalGroupGetMembers(server, groupName, userArrayRef)

Fills userArrayRef with the members of groupName.

=item LocalGroupGetMembersWithDomain(server, groupName, userRef)

This function is similar LocalGroupGetMembers but accepts an array or
a hash reference. Unlike LocalGroupGetMembers it returns each user name
as C<DOMAIN\USERNAME>. If a hash reference is given, the function
returns to each user or group name the type (group, user, alias etc.).
The possible types are as follows:

  $SidTypeUser = 1;
  $SidTypeGroup = 2;
  $SidTypeDomain = 3;
  $SidTypeAlias = 4;
  $SidTypeWellKnownGroup = 5;
  $SidTypeDeletedAccount = 6;
  $SidTypeInvalid = 7;
  $SidTypeUnknown = 8;

=item LocalGroupAddUsers(server, groupName, users)

Adds a user to a group.

=item LocalGroupDeleteUsers(server, groupName, users)

Deletes a users from a group.

=item GetServers(server, domain, flags, serverRef)

Gets an array of server names or an hash with the server names and the
comments as seen in the Network Neighborhood or the server manager.
For flags, see SV_TYPE_* constants.

=item GetTransports(server, transportRef)

Enumerates the network transports of a computer. If transportRef is an array
reference, it is filled with the transport names. If transportRef is a hash
reference then a hash of hashes is filled with the data for the transports.

=item LoggedOnUsers(server, userRef)

Gets an array or hash with the users logged on at the specified computer. If
userRef is a hash reference, the value is a semikolon separated string of
username, logon domain and logon server.

=item GetAliasFromRID(server, RID, returnedName)

=item GetUserGroupFromRID(server, RID, returnedName)

Retrieves the name of an alias (i.e local group) or a user group for a RID
from the specified server. These functions can be used for example to get the
account name for the administrator account if it is renamed or localized.

Possible values for C<RID>:

  DOMAIN_ALIAS_RID_ACCOUNT_OPS
  DOMAIN_ALIAS_RID_ADMINS
  DOMAIN_ALIAS_RID_BACKUP_OPS
  DOMAIN_ALIAS_RID_GUESTS
  DOMAIN_ALIAS_RID_POWER_USERS
  DOMAIN_ALIAS_RID_PRINT_OPS
  DOMAIN_ALIAS_RID_REPLICATOR
  DOMAIN_ALIAS_RID_SYSTEM_OPS
  DOMAIN_ALIAS_RID_USERS
  DOMAIN_GROUP_RID_ADMINS
  DOMAIN_GROUP_RID_GUESTS
  DOMAIN_GROUP_RID_USERS
  DOMAIN_USER_RID_ADMIN
  DOMAIN_USER_RID_GUEST

=item GetServerDisks(server, arrayRef)

Returns an array with the disk drives of the specified server. The array
contains two-character strings (drive letter followed by a colon).

=back

=head1 EXAMPLE

    # Simple script using Win32::NetAdmin to set the login script for
    # all members of the NT group "Domain Users".  Only works if you
    # run it on the PDC. (From Robert Spier <rspier@seas.upenn.edu>)
    #
    # FILTER_TEMP_DUPLICATE_ACCOUNTS
    #	Enumerates local user account data on a domain controller.
    #
    # FILTER_NORMAL_ACCOUNT
    #	Enumerates global user account data on a computer.
    #
    # FILTER_INTERDOMAIN_TRUST_ACCOUNT
    #	Enumerates domain trust account data on a domain controller.
    #
    # FILTER_WORKSTATION_TRUST_ACCOUNT
    #	Enumerates workstation or member server account data on a domain
    #	controller.
    #
    # FILTER_SERVER_TRUST_ACCOUNT
    #	Enumerates domain controller account data on a domain controller.


    use Win32::NetAdmin qw(GetUsers GroupIsMember
			   UserGetAttributes UserSetAttributes);

    my %hash;
    GetUsers("", FILTER_NORMAL_ACCOUNT , \%hash)
	or die "GetUsers() failed: $^E";

    foreach (keys %hash) {
	my ($password, $passwordAge, $privilege,
	    $homeDir, $comment, $flags, $scriptPath);
	if (GroupIsMember("", "Domain Users", $_)) {
	    print "Updating $_ ($hash{$_})\n";
	    UserGetAttributes("", $_, $password, $passwordAge, $privilege,
			      $homeDir, $comment, $flags, $scriptPath)
		or die "UserGetAttributes() failed: $^E";
	    $scriptPath = "dnx_login.bat"; # this is the new login script
	    UserSetAttributes("", $_, $password, $passwordAge, $privilege,
			      $homeDir, $comment, $flags, $scriptPath)
		or die "UserSetAttributes() failed: $^E";
	}
    }

=cut

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    ($pack,$file,$line) = caller;
	    die "Your vendor has not defined Win32::NetAdmin macro $constname, used in $file at line $line.";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

$SidTypeUser = 1;
$SidTypeGroup = 2;
$SidTypeDomain = 3;
$SidTypeAlias = 4;
$SidTypeWellKnownGroup = 5;
$SidTypeDeletedAccount = 6;
$SidTypeInvalid = 7;
$SidTypeUnknown = 8;

sub GetError() {
    our $__lastError;
    $__lastError;
}

bootstrap Win32::NetAdmin;

1;
__END__

