package Win32API::Net;

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw();	# don't pollute callees namespace

%EXPORT_TAGS=(
    User => [ qw(
        FILTER_INTERDOMAIN_TRUST_ACCOUNT FILTER_NORMAL_ACCOUNT
        FILTER_SERVER_TRUST_ACCOUNT FILTER_TEMP_DUPLICATE_ACCOUNTS
        FILTER_WORKSTATION_TRUST_ACCOUNT
        USER_ACCT_EXPIRES_PARMNUM USER_AUTH_FLAGS_PARMNUM
        USER_CODE_PAGE_PARMNUM USER_COMMENT_PARMNUM USER_COUNTRY_CODE_PARMNUM
        USER_FLAGS_PARMNUM USER_FULL_NAME_PARMNUM USER_HOME_DIR_DRIVE_PARMNUM
        USER_HOME_DIR_PARMNUM USER_LAST_LOGOFF_PARMNUM USER_LAST_LOGON_PARMNUM
        USER_LOGON_HOURS_PARMNUM USER_LOGON_SERVER_PARMNUM
        USER_MAX_STORAGE_PARMNUM USER_NAME_PARMNUM USER_NUM_LOGONS_PARMNUM
        USER_PAD_PW_COUNT_PARMNUM USER_PARMS_PARMNUM USER_PASSWORD_AGE_PARMNUM
        USER_PASSWORD_PARMNUM USER_PRIMARY_GROUP_PARMNUM USER_PRIV_ADMIN
        USER_PRIV_GUEST USER_PRIV_MASK USER_PRIV_PARMNUM USER_PRIV_USER
        USER_PROFILE_PARMNUM USER_PROFILE_PARMNUM USER_SCRIPT_PATH_PARMNUM
        USER_UNITS_PER_WEEK_PARMNUM USER_USR_COMMENT_PARMNUM
        USER_WORKSTATIONS_PARMNUM USER_BAD_PW_COUNT_PARMNUM LG_INCLUDE_INDIRECT
        UF_ACCOUNTDISABLE UF_ACCOUNT_TYPE_MASK UF_DONT_EXPIRE_PASSWD
        UF_HOMEDIR_REQUIRED UF_INTERDOMAIN_TRUST_ACCOUNT UF_LOCKOUT
        UF_MACHINE_ACCOUNT_MASK UF_NORMAL_ACCOUNT UF_PASSWD_CANT_CHANGE
        UF_PASSWD_NOTREQD UF_SCRIPT UF_SERVER_TRUST_ACCOUNT UF_SETTABLE_BITS
        UF_TEMP_DUPLICATE_ACCOUNT UF_WORKSTATION_TRUST_ACCOUNT
        UserAdd UserChangePassword UserDel UserEnum UserGetGroups UserGetInfo 
        UserGetLocalGroups UserModalsGet UserModalsSet UserSetGroups
        UserSetInfo
    )],
    Get => [ qw(
        GetDCName
    )],
    Group => [ qw(
        GROUP_ATTRIBUTES_PARMNUM GROUP_COMMENT_PARMNUM GROUP_NAME_PARMNUM
        GroupAdd GroupAddUser GroupDel GroupDelUser GroupEnum GroupGetInfo 
        GroupGetUsers GroupSetInfo GroupSetUsers 
    )],
    LocalGroup => [ qw(
        LOCALGROUP_COMMENT_PARMNUM LOCALGROUP_NAME_PARMNUM
        LocalGroupAdd LocalGroupAddMember LocalGroupAddMembers LocalGroupDel 
        LocalGroupDelMember LocalGroupDelMembers LocalGroupEnum 
        LocalGroupGetInfo LocalGroupGetMembers LocalGroupSetInfo 
        LocalGroupSetMembers 
    )],
);

@EXPORT_OK= ();
{ my $ref;
    foreach $ref (  values(%EXPORT_TAGS)  ) {
        push( @EXPORT_OK, @$ref );
    }
}
$EXPORT_TAGS{ALL}= \@EXPORT_OK;

$VERSION = '0.10';

sub AUTOLOAD {
    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    local $! = 0;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined Win32API::Net macro $constname";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Win32API::Net $VERSION;

1;
__END__

=head1 NAME

Win32API::Net - Perl interface to the Windows NT LanManager API account management functions.

=head1 SYNOPSIS

use Win32API::Net;

=head1 NOTE ON VERSIONS PRIOR TO 0.08

As of version 0.08 of this module, the behaviour relating to empty strings
in input hashes has changed. The old behaviour converted such strings to
the NULL pointer. The underlying API uses this value as an indication to
not change the value stored for a given field. This meant that you were not
able to clear (say) the logonScript field for a user using UserSetInfo().

The new behaviour is to leave the string as an empty C string which will
allow fields to be cleared.  To pass a NULL pointer to the underlying
API call (and thus, to leave the field as it was), you need to set the
corresponding field to C<undef>.

WARNING: B<THIS IS AN INCOMPATIBLE CHANGE>.
B<EXISTING SCRIPTS THAT RELIED ON PRIOR BEHAVIOR MAY NEED TO BE MODIFIED>.

=head1 DESCRIPTION

Win32API::Net provides a more complete wrapper for the account management
parts of the NT LanManager API than do other similar packages. Most of what
you can achieve with the native C++ API is possible with this package - albeit
in a more Perl like manner by using references to pass information to and
from functions.

For an understanding of the environment in which these functions operate see
L<DATA STRUCTURES>.

The following groups of functions are available:

=over 8

=item L<NET USER FUNCTIONS>

=item L<NET GROUP FUNCTIONS>

=item L<NET LOCAL GROUP FUNCTIONS>

=item L<NET GET FUNCTIONS>

=back

All functions return 0 on failure and 1 on success. Use the
C<Win32::GetLastError()> function to find out more information on why a
function failed. In addition, some functions that take a hash reference
to pass information in (e.g. C<UserAdd()>) have a last argument that will
allow more detailed information on which key/value pair was not properly
specified.

=head2 Using References

References to hashes and arrays are used throughout this package to pass
information into and out of functions.

=over 8

=item Using Hash References

Where a hash reference is required you can use anything that evaluates to a
hash reference. e.g.

	$href = \%someHash;
	UserAdd(server, 2, $hRef);

Or more directly:

	UserAdd(server, 2, \%someHash);

=item Using Array references

Array references are used in a similar manner to hash references. e.g.

	$aref = \@someArray;
	UserEnum(server, $aref);

Or more directly:

	UserEnum(server, \@someArray);

=back

Please note: Any C<*Get*()> or C<*Enum()> operation will first clear the
contents of the input hash or array being referenced.

See L<EXAMPLES> and the test.pl script for examples of usage.

=head1 DATA STRUCTURES

Most the the functions in the underlying API allow the programmer to pass
specify at runtime the amount of information that is supplied to the
function. For example, the C<NetUserGetInfo()> call allows the programmer to
specify levels of 0, 1, 2, 3 (and others). Having specified this level, the
function returns a structure that will contain different fields. For a
level C<0>, the function returns a structure that has only one field. For a
supplied level of 1, the function returns a structure with C<8> fields. The
programmer needs to know in advance what fields should be provided or will
be returned for a given level. This mechanism works very will since it
effectively overloads functions without having to use different function
prototypes. Perl provides better higher level data structures in the form
of arrays and hashes. This package uses hashes as the means to pass these
variable size structure into and out of functions.

For any function that takes a reference to a hash as input, the programmer
is expected to provide appropriate keys and corresponding values as well as
the level parameter. The called function will then takes the values out of
the supplied hash and build the approprite structure to pass to the
underlying API function.

For any function that takes a reference to a hash to recieve output, the
function will first clear any keys an corresponding values in the supplied
hash. It will call the underlying API call and will then return in the hash
any keys and values that are applicable at the requested level.

Example:

The C<UserGetInfo()> can takes a number of levels. If called with level C<0>
the supplied hash will, on return from the function, contain a single key
and value - namely B<name>/B<requested-users-name>. If called with a level
of C<1> the supplied hash will, on return from the function, contain 8 keys
and values. The returned keys are C<name, password>, C<passwordAge>,
C<priv>, C<homeDir>, C<comment>, C<flags>, C<scriptPath>. See
L<USER INFO FIELDS> for more information on what these represent.


=head1 EXPORTS

By default, Win32API::Net exports no symbols into the callers namespace.
The following tags can be used to selectively import symbols into the
main namespace.

=over 8

=item C<:User>

Exports all symbols needed for the C<User*()> functions.
See L<NET USER FUNCTIONS>.

=item C<:Get>

Exports all symbols needed for the C<Get*()> functions.
See L<NET GET FUNCTIONS>.

=item C<:Group>

Exports all symbols needed for the C<Group*()> functions.
See L<NET GROUP FUNCTIONS>.

=item C<:LocalGroup>

Exports all symbols needed for the C<LocalGroup*()> functions.
See L<NET LOCAL GROUP FUNCTIONS>.

=back


=head1 NET USER FUNCTIONS

The C<User*()> functions operate on NT user accounts.

Administrator or Account Operator group membership is required to
successfully execute most of these functions on a remote server or on a
computer that has local security enabled. Administrator privileges are
required to add an Administrator Privilege account.  There are some
exceptions to this whereby a user can change some of their own settings
where these don't conflict with 'administrative information' (e.g. full
name).

The C<server> field can be the empty string, in which case the function
defaults to running on the local computer. If you leave this field blank
then you should ensure that you are running the function on a PDC or BDC
for your current domain. Use the support function C<GetDCName()> to find out
what the domain controller is, should you not be running this on the PDC.

All functions in this section are 'DOMAIN functions'. This means that,
for example, the C<UserGetLocalGroups()> function actually lists the
domain's local groups of which the named user is a member.

The following functions are available.


=head2 UserAdd(server, level, hash, error)

Add a new user account. The user name is taken from the C<name>-key's
value in the supplied hash.

=over 8

=item C<server> - Scalar String

The server on which to add the account.

=item C<level> - Scalar Int

Level of information provided in hash. This can be either 1, 2 or 3.
See L<USER INFO LEVELS>.

=item C<hash> - Hash Reference

The information to use to add this account. This should have all the
appropriate keys and values required for C<level>.

=item C<error> - Scalar Int

Provides information on which field in the hash was not properly specified.
See L<USER FIELD ERRORS> for more information about what values this can
take.

=back

=head2 UserChangePassword(server, user, old, new)

Changes the password for C<user>. If the policy of the machine/domain
only allows password changes if the C<user> is logged on then the C<user>
must be logged on to execute this function. With Administrator or Account
Operator privilege you can use this function to change anyone's password,
so long as you know the old password.

=over 8

=item C<server> - Scalar String

The C<server> on which to change the password.

=item C<user> - Scalar String

The name of the C<user> whose password is being changed.

=item C<old> - Scalar String

The existing password for C<user>.

=item C<new> - Scalar String

The new password for C<user>.

=back


=head2 UserDel(server, user)

Deletes the specified C<user> account. Administrator or Account Operator
privilege is required to execute this function.

=over 8

=item C<server> - Scalar String

The C<server> on which to delete the C<user>.

=item C<user> - Scalar String

The C<user> account to delete.

=back

=head2 UserEnum(server, array[, filter])

Enumerates all the accounts on server that satisfy C<filter>. Unlike the
C<NetUserEnum()> function in the API, this function does not allow you
to specify a level (internally it is hardcoded to 0). In Perl it is
trivial to implement the equivalent function (should you need it) - see
L<Example 1>.

=over 8

=item C<server> - Scalar String

The C<server> on which to enumerate the accounts satisfying C<filter>.

=item C<array> - Array Reference

The array that will hold the names of all users on C<server> whose
accounts match C<filter>.

=item C<filter> - Scalar Int (optional)

The filter to apply (see L<USER ENUM FILTER>). This argument is optional
and if not present a default of C<FILTER_NORMAL_ACCOUNT> is used.

=back

=head2 UserGetGroups(server, user, array)

Get the global groups for which C<user> is a member. It returns the group
names in C<array>. Unlike the C<NetUserGetGroups()> function in the API,
this function does not allow you to specify a level (internally is
hardcoded to 0). In Perl it is trivial to implement the equivalent function
(in the unlikely event that you might need it).

=over 8

=item C<server> - Scalar String

The C<server> from which to get the groups of which C<user> is a member.

=item C<user> - Scalar String

The C<user> whose group membership you wish to examine.

=item C<array> - Scalar String

The array that will contain the group names to which C<user> belongs.

=back

=head2 UserGetInfo(server, user, level, hash)

Returns the information at the specified C<level> for the named C<user>
in C<hash>.

=over 8

=item C<server> - Scalar String

The C<server> from which to get the requested information about C<user>.

=item C<user> - Scalar String

The C<user> whose information you want.

=item C<level> - Scalar Int

One of: 0, 1, 2, 3, 10, 11 and 20. See L<USER INFO LEVELS>.

=item C<hash> - Hash Reference

The hash that will contain the keys and values for the information
requested. See L<USER INFO FIELDS> for information about which keys are
present in a given level.

=back

=head2 UserGetLocalGroups(server, user, array[, flags])

Gets the names of the local groups of which C<user> is a member. Unlike
the C<NetUserEnum()> function in the API, this function does not allow you
to specify a level. Since the underlying API restricts you to level 0 there
really isn't any need to include it...

=over 8

=item C<server> - Scalar String

The server from which to get the local groups of which C<user> is a member.

=item C<user> - Scalar String

The C<user> whose local group membership you wish to enumerate.

=item C<array> - Array Reference

The array that will hold the names of the local groups to which C<user>
belongs.

=item C<flags> - Scalar Int <em>(optional)</em>

Either C<Win32API::Net::LG_INCLUDE_INDIRECT()> or 0. if C<flags> is
omitted, the function internally uses 0. Specifying C<LG_INCLUDE_INDIRECT()>
will include in the list the names of the groups of which the C<user> is
indirectly a member (e.g. by being in a global group that is a member of a
local group).

This field can take no other values.

=back


=head2 UserModalsGet()

This function is not currently implemented.


=head2 UserModalsSet()

This function is not currently implemented.


=head2 UserSetGroups(server, user, array)

Sets the (global) group membership for C<user> to the specified groups.
Unlike the API function C<NetUserSetGroups()>, this function does not take a
C<level> parameter (mainly because this option is largely redundant).

=over 8

=item C<server> - Scalar String

The C<server> on which you wish to set the group membership for C<user>.

=item C<user> - Scalar String

The C<user> whose group membership you wish to set.

=item C<array> - Array Reference

The array containing the (global) group names to set the C<user>s
membership of.

=back

This function will fail if any of the group names specified do not exist.

=head2 UserSetInfo(server, user, level, hash, error)

Sets the info for C<user> according to the information contained in C<hash>
for C<level> (see L<USER INFO LEVELS>).

=over 8

=item C<server> - Scalar String

The C<server> on which you wish to change the info for C<user>.

=item C<user> - Scalar String

The C<user> whose info you wish to change.

=item C<level> - Scalar Int

One of 0, 1, 2, 3, or 20 (according to Microsoft documentation). In
practice, you can use all the 10xx levels as well to change most of the
individual properties of the named C<user> - although this may not be
supported in future...

=item C<hash> - Hash Reference

The hash that will contain the necessary key/value pairs required for
C<level> (see L<USER INFO LEVELS>).

=item C<error> - Scalar Int

Provides information on which field in C<hash> were not properly
specified. See L<USER FIELD ERRORS> for more information about what
values can be returned in this field.

=back

=head1 NET GROUP FUNCTIONS

The C<Group*()> functions all operate only on global groups. To modify
local groups, use the corresponding C<LocalGroup*()> functions.

Administrator or Account Operator group membership is required to
successfully execute most of these functions on a remote server or on
a computer that has local security enabled.

The C<server> field can be the empty string, in which case the function
defaults to running on the local computer. If you leave this field blank
then you should ensure that you are running the function on a PDC or BDC
for your current domain. Use the support function C<GetDCName()> to find out
what the domain controller is, should you not be running this on the PDC.

The following functions are available.


=head2 GroupAdd(server, level, hash, error)

Adds the specified group.

=over 8

=item C<server> - Scalar String

The C<server> on which to add the group.

=item C<level> - Scalar String

The C<level> of information contained in C<hash>. This can be one of 0, 1
or 2. See L<GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

A hash containing the required key/value pairs for C<level>.

=item C<error> - Scalar Int

Provides information on which field in C<hash> was not properly specified.
See L<GROUP FIELD ERRORS> for more information about what values can be
returned in this field.

=back


=head2 GroupAddUser(server, group, user)

Adds the specified C<user> to the specified C<group>.

=over 8

=item C<server> - Scalar String

The C<server> on which to add the C<user> to C<group>.

=item C<group> - Scalar String

The C<group> to add the C<user> to.

=item C<user> - Scalar String

The C<user> to add to C<group>.

=back



=head2 GroupDel(server, group)

Deletes the specified global group.

=over 8

=item C<server> - Scalar String

The C<server> on which to delete the named C<group>.

=item C<group> -Scalar String

The C<group> to delete.

=back



=head2 GroupDelUser(server, group, user)

Deletes the specified user from the specified group.

=over 8

=item C<server> - Scalar String

The C<server> on which to delete C<user> from C<group>.

=item C<group> - Scalar String

The C<group> from which to delete C<user>.

=item C<user> - Scalar String

The C<user> to delete from C<group>.

=back


=head2 GroupEnum(server, array)

Enumerates all the global groups on the server. Unlike the API call
C<NetGroupEnum()>, this function does not allow you to specify a level
(internally it is hardcoded to 0). In Perl it is trivial to implement
the equivalent function (should you need it).

=over 8

=item C<server> - Scalar String

The server on which to enumerate the (global) C<groups>.

=item C<array> - Array Reference

An array that, on return, will contain the C<group> names.

=back


=head2 GroupGetInfo(server, group, level, hash)

Retrieves C<level> information for C<group> returning information in
C<hash>.

=over 8

=item C<server> - Scalar String

The C<server> from which to get the group information.

=item C<group> - Scalar String

The C<group> whose information you wish to obtain.

=item C<level> - Scalar Int

The C<level> of information you wish to retrieve. This can be one of 1, 2
or 3. See L<GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

The hash that will contain the information.

=back


=head2 GroupGetUsers(server, group, array)

Returns (in C<array>) the users belonging to C<group>. Unlike the API
call C<NetGroupGetUsers()>, this function does not allow you to specify
a level (internally it is hardcoded to 0). In Perl it is trivial to
implement the equivalent function (should you need it).

=over 8

=item C<server> - Scalar String

The C<server> from which to get the group information.

=item C<group> - Scalar String

The C<group> whose users you wish to obtain.

=item C<array> - Array Reference

The array to hold the user names retrieved.

=back

=head2 GroupSetInfo(server, group, level, hash, error)

Sets the information for C<group> according to C<level>.

=over 8

=item C<server> - Scalar String

The C<server> on which to set the C<group> information.

=item C<group> - Scalar String

The C<group> whose information you wish to set.

=item C<level> - Scalar Int

The C<level> of information you are supplying in C<hash>.  Level can be
one of 0, 1 or 2. See L<GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

The hash containing the required key/value pairs for C<level>.

=item C<error> - Scalar String

On failure, the C<error> parameter will contain a value which specifies
which field caused the error. See L<GROUP FIELD ERRORS>.

=back

=head2 GroupSetUsers(server, group, array)

Sets the membership of C<group> to contain only those users specified
in C<array>. This function will fail if any user names contained in the
array are not valid users on C<server>.  On successful completion
C<group> will contain only the users specified in C<array>.  Use the
functions C<GroupAddUser()/GroupDelUser()> to add and delete individual
users from a group.

=over 8

=item C<server> - Scalar String

The C<server> on which to set the C<group> membership.

=item C<group> - Scalar String

The C<group> to set the membership of.

=item C<array> - Array Reference

The array containing the names of all users who will be members of C<group>.

=back

=head1 NET LOCAL GROUP FUNCTIONS

The C<LocalGroup*()> functions operate on local groups. If these
functions are run on a PDC then these functions operate on the domains
local groups.

Administrator or Account Operator group membership is required to
successfully execute most of these functions on a remote server or on a
computer that has local security enabled.

The C<server> field can be the empty string, in which case the function
defaults to running on the local computer. If you leave this field blank
then you should ensure that you are running the function on a PDC or BDC
for your current domain. Use the support function C<GetDCName()> to find
out what the domain controller is, should you not be running this on the PDC.

The following functions are available.

=head2 LocalGroupAdd(server, level, hash, error)

Adds the specified group. The name of the group is contained in the C<name>
key of C<hash>.

=over 8

=item C<server> - Scalar String

The C<server> on which to add the group.

=item C<level> - Scalar String

The C<level> of information contained in C<hash>. This can be one of 0 or 1.
See L<LOCAL GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

A hash containing the required key/value pairs for C<level>.

=item C<error> - Scalar Int

Provides information on which field in C<hash> wasn't properly specified.
See L<LOCAL GROUP FIELD ERRORS> for more information about what values
this can take.

=back

=head2 LocalGroupAddMember()

This function is obselete in the underlying API and has therefore not
been implemented.  Use C<LocalGroupAddMembers> instead.

=head2 LocalGroupAddMembers(server, group, array)

Adds the specified users (members) to the local group. Unlike the API
function C<NetLocalGroupAddMembers()>, this function does not allow you
to specify a level (internally it is hardcoded to 3).
This was done to simplify the implementation. To add a 'local' user, you
need only specify the C<name>. You can also specify users using the
C<DOMAIN\user> syntax.

=over 8

=item C<server> - Scalar String

The C<server> on which to add the members to C<group>.

=item C<group> - Scalar String

The C<group> to add the members to.

=item C<array> - Array Reference

The array containing the members to add to C<group>.

=back

=head2 LocalGroupDel(server, group)

Delete the specified local group.

=over 8

=item C<server> - Scalar String

The C<server> on which to delete the named C<group>.

=item C<group> -Scalar String

The C<group> to delete.

=back

=head2 LocalGroupDelMember()

This function is obselete in the underlying API and has therefore not
been implemented.  Use C<LocalGroupDelMembers()> instead.

=head2 LocalGroupDelMembers(server, group, array)

Delete the specified users (members) of the local group. Unlike the API
function C<NetLocalGroupDelMembers()>, this function does not allow you
to specify a level (internally it is hardcoded to 3). This was done to
simplify the implementation. To delete a 'local' user, you need only
specify the C<name>. You can also specify users using the C<DOMAIN\user>
syntax.

=over 8

=item C<server> - Scalar String

The C<server> on which to delete the members from C<group>.

=item C<group> - Scalar String

The C<group> to delete the members from.

=item C<array> - Array Reference

The array containing the members to delete from C<group>.

=back

=head2 LocalGroupEnum(server, array)

Enumerates all the local groups on the server. Unlike the API call
C<NetLocalGroupEnum()>, this function does not allow you to specify a
level (internally it is hardcoded to 0). In Perl it is trivial to
implement the equivalent function (should you need it).

=over 8

=item C<server> - Scalar String

The server on which to enumerate the (local) C<groups>.

=item C<array> - Array Reference

The array to hold the C<group> names.

=back

=head2 LocalGroupGetInfo(server, group, level, hash)

Retrieves C<level> information for C<group>.

=over 8

=item C<server> - Scalar String

The C<server> from which to get the group information.

=item C<group> - Scalar String

The C<group> whose information you wish to obtain.

=item C<level> - Scalar Int

The C<level> of information you wish to retrieve. This can be 0 or 1.
See L<LOCAL GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

The hash that will contain the information.

=back

=head2 LocalGroupGetMembers(server, group, hash)

Retrieves the users belonging to C<group>. Unlike the API call
C<NetLocalGroupGetUsers()>, this function does not allow you to specify
a level (internally it is hardcoded to 0). In Perl it is trivial to
implement the equivalent function (should you need it).

=over 8

=item C<server> - Scalar String

The C<server> from which to retrieve the group information.

=item C<group> - Scalar String

The C<group> whose users you wish to obtain.

=item C<array> - Array Reference

The array to hold the user names retrieved.

=back

=head2 LocalGroupSetInfo(server, level, hash, error)

Sets the information for C<group> according to C<level>.

=over 8

=item C<server> - Scalar String

The C<server> on which to set the C<group> information.

=item C<group> - Scalar String

The C<group> whose information you wish to set.

=item C<level> - Scalar Int

The C<level> of information you are supplying in C<hash>.
Level can be one of 0 or 1. See L<LOCAL GROUP INFO LEVELS>.

=item C<hash> - Hash Reference

The hash containing the required key/value pairs for C<level>.

=item C<error> - Scalar String

On failure, the C<error> parameter will contain a value which specifies
which field caused the error. See L<LOCAL GROUP FIELD ERRORS>.

=back

=head2 LocalGroupSetMembers()

This function has not been implemented at present.


=head1 NET GET FUNCTIONS

=head2 GetDCName(server, domain, domain-controller)

Gets the C<domain-controller> name for C<server> and C<domain>.

=over 8

=item C<server> - Scalar String

The C<server> whose domain controller you wish to locate.

=item C<domain> - Scalar String

The C<domain> that C<server> is a member of whose domain-controller
you wish the locate.

=item C<domain-controller> - Scalar String (output)

The name of the C<domain-controller> for the requested C<domain>.

=back

Note: This module does not implement the C<NetGetAnyDCName()>API function
as this is obsolete.



=head1 USER INFO LEVELS

Most of the C<User*()> functions take a C<level> parameter. This C<level>
specifies how much detail the corresponding C<hash> should contain (or in
the case of a C<UserGet*()> function, will contain after the call). The
following C<level> descriptions provide information on what fields should
be present for a given level. See L<USER INFO FIELDS> for a description of
the fields.

=over 8

=item Level 0

name

=item Level 1

name, password, passwordAge, priv, homeDir, comment, flags, scriptPath

=item Level 2

name, password, passwordAge, priv, homeDir, comment, flags, scriptPath,
authFlags, fullName, usrComment, parms, workstations, lastLogon,
lastLogoff, acctExpires, maxStorage, unitsPerWeek, logonHours, badPwCount,
numLogons, logonServer, countryCode, codePage

=item Level 3

name, password, passwordAge, priv, homeDir, comment, flags, scriptPath,
authFlags, fullName, usrComment, parms, workstations, lastLogon, lastLogoff,
acctExpires, maxStorage, unitsPerWeek, logonHours, badPwCount, numLogons,
logonServer, countryCode, codePage, userId, primaryGroupId, profile,
homeDirDrive, passwordExpired

=item Level 10

name, comment, usrComment, fullName

=item Level 11

name, comment, usrComment, fullName, priv, authFlags, passwordAge, homeDir,
parms, lastLogon, lastLogoff, badPwCount, numLogons, logonServer,
countryCode, workstations, maxStorage, unitsPerWeek, logonHours, codePage

=item Level 20

name, fullName, comment, flags, userId

=item Level 21

B<Not available in this implementation>

=item Level 22

B<Not available in this implementation>

=item Level 1003

password

=item Level 1005

priv

=item Level 1006

homeDir

=item Level 1007

comment

=item Level 1008

flags

=item Level 1009

scriptPath

=item Level 1010

authFlags

=item Level 1011

fullName

=item Level 1012

usrComment

=item Level 1013

parms

=item Level 1014

workstations

=item Level 1017

acctExpires

=item Level 1018

maxStorage

=item Level 1020

unitsPerWeek, logonHours

=item Level 1023

logonServer

=item Level 1024

countryCode

=item Level 1025

codePage

=item Level 1051

primaryGroupId

=item Level 1052

profile

=item Level 1053

homeDirDrive

=back



=head1 USER INFO FIELDS

The following is an alphabetical listing of each possible field, together
with the data type that the field is expected to contain.

=over 8

=item C<acctExpires> - Scalar Int (UTC)

The time (as the number of seconds since 00:00:00, 1st January 1970) when
the account expires. A -1 in this field specifies that the account never
expires.

=item C<authFlags> - Scalar Int (See USER_AUTH_FLAGS).

The level of authority that this use has. The value this can take depends
on the users group membership - this value is therefore read only and
cannot be set using C<UserAdd()> or C<UserSetInfo()>. Its value can be one
of:

=back


	User belongs to group		Flag value
	---------------------		----------
	Print Operators			Win32API::Net::AF_OP_PRINT()
	Server Operators		Win32API::Net::AF_OP_SERVER()
	Account Operators		Win32API::Net::AF_OP_ACCOUNTS()


=over 8

=item C<badPwCount> - Scalar Int

The number of times that the user has failed to logon by specifying an
incorrect password.

=item C<codePage> - Scalar Int

The code page that this user uses.

=item C<comment> - Scalar String

The comment associated with this user account. This can be any string
(apparently of any length).

=item C<countryCode> - Scalar Int

The country code that this user uses.

=item C<flags> - Scalar Int (Bitwise OR of USER_FLAGS)

The flags for this user. See L<USER FLAGS>.

=item C<fullName> - Scalar String

The users' full name.

=item C<homeDir> - Scalar String

The home directory of the user. This can be either a UNC path or an
absolute path (drive letter + path). Can be the empty string ("").

=item C<homeDirDrive> - Scalar String

The home directory drive that the users home directory is mapped to
(assuming that the specified home directory is a UNC path).

=item C<lastLogon> - Scalar Int (UTC)

The time (as the number of seconds since 00:00:00, 1st January 1970)
that the user last logged on.

=item C<lastLogoff> - Scalar Int (UTC)

The time (as the number of seconds since 00:00:00, 1st January 1970)
that the user last logged off .

=item C<logonHours> - Reference to Array of Integers (length 21 elements)

The times at which the user can logon. This should be an integer array
with 21 elements.  Each element represents an 8 hour period and each bit
represents represents an hour. Only the lower byte of each integer is
used. If this is left undefined then no restrictions are placed on the
account.

=item C<logonServer> - Scalar String

The logon server for this user. Under Windows NT, this value cannot be
set and will always have the value '\\*' when queried.

=item C<maxStorage> - Scalar Int

The current release of Windows NT does not implement disk quotas so
it is believed that the value of this key is ignored.

=item C<name> - Scalar String

The user name that this request applies to. Most of the functions take
the user name as a separate argument. In general, the user name provided
should be the same as that in the one provided in the hash.

=item C<numLogons> - Scalar Int

The number of times that the named user has successfully logged on to
this machine/domain.

=item C<parms> - Scalar String

The value of this key can be used by applications. There are none known
to to author that use it, although it could be used to hold adminitrative
information.

=item C<password> - Scalar String

The password to be set. The password is never returned in a C<UserGet()>
operation.

=item C<passwordAge> - Scalar Int (UTC)

The current age of the password (stored as the number of seconds since
00:00:00, 1st January 1970).

=item C<passwordExpired> - Scalar Int

The value of this key is used in two different ways. When queried via
C<UserGetInfo()> the return value is 0 is the password has not expired
and 1 if it has. When setting the value via C<UserAdd()> or
C<UserSetInfo()> a value of 0 indicates that the users' password has
not expired whereas a value of 1 will force the user to change their
password at the next logon.

=item C<primaryGroupId> - Scalar Int

The id of the primary group that this user belongs to. When creating
accounts with C<UserAdd()> you should use a value of 0x201.

=item C<priv> - Scalar Int (Bitwise OR of USER_PRIVILEGE_FLAGS)

The privilege level that this user has. This is never returned from a
C<UserGet()> call. See L<USER PRIVILEGE FLAGS>.

=item C<profile> - Scalar String

The profile that is associated with the named user. This can be UNC path,
a local path or undefined.

=item C<scriptPath> - Scalar String

The path to the logon script for this user. This should be specified as a
relative path and will cause the logon script to be run from (relative
location) in the logon servers export directory.

=item C<unitsPerWeek> - Scalar Int

The value of this key represents the granularity of the logonHours array.
Its use is beyond the scope of this package.

=item C<usrComment> - Scalar String

The user comment field (contrasted with the comment field ;-).

=item C<workstations> - Scalar String

A comma-separated string containing upto 8 workstation that the named
user can login to.  Setting a value for this key will then allow the
named user to login to only those computers named.

=item C<userId> - Scalar Int

The user id associated with this user This value is generated by the
system and cannot be set or changed using the C<UserAdd()> or
C<UserSetInfo()> calls.

=back



=head1 USER FLAGS

The following is an alphabetical listing of the user flags.
The C<flags> key (see L<USER INFO FIELDS>) should be the bitwise OR of one
or more of these values.

=over 8

=item C<UF_ACCOUNTDISABLE()>

This account has been disabled.

=item C<UF_DONT_EXPIRE_PASSWD()>

Never expire the password on this account.

=item C<UF_HOMEDIR_REQUIRED()>

A home directory must be specified (ignored for NT).

=item C<UF_INTERDOMAIN_TRUST_ACCOUNT()>

The account represents a interdomain trust account.

=item C<UF_LOCKOUT()>

Lock out this account (or this account has been locked out due to
security policy - i.e.  badLogonCount is greater than your policy allows).
This value can be cleared but not set by a C<UserSetInfo()> call.

=item C<UF_NORMAL_ACCOUNT()>

The account is a normal user account.

=item C<UF_PASSWD_CANT_CHANGE()>

The password for this account cannot be changed (execpt by an Administrator
using one of the above calls).

=item C<UF_PASSWD_NOTREQD()>

A password is not required for this account.

=item C<UF_SCRIPT()>

This <strong>must be set when creating account on Windows NT.

=item C<UF_SERVER_TRUST_ACCOUNT()>

The account represents a Windows NT Backup Domain Controller account in
the domain.

=item C<UF_TEMP_DUPLICATE_ACCOUNT()>

To quote the Microsoft Documentation <em>&quot;This is an account for
users whose primary account is in another domain. This account provides
user access to this domain, but not to any domain that trusts this domain.
The User Manager refers to this account type as a local user account.

=item C<UF_WORKSTATION_TRUST_ACCOUNT()>

The account represents a computer account for a workstation or server in
the domain.

=back

Please note that these are implemented as functions and are therefore
called in the same way as other functions. You should typically use them
like:

	$ufScript = Win32API::Net::UF_SCRIPT();

=head1 USER PRIVILEGE FLAGS

These following values are used in the C<priv> key. This field is never
initialised on a C<UserGet*()> call and once set cannot be changed in a
C<UserSetInfo()> call.

=over 8

=item C<USER_PRIV_ADMIN()>

Account is an an administrative account.

=item C<USER_PRIV_GUEST()>

Account is a guest account.

=item C<USER_PRIV_USER()>

Account is a user account.

=back

Please note that these are implemented as functions and are therefore
called in the same way as other functions. You should typically use them
like:

	$userPrivUser = Win32API::Net::USER_PRIV_USER();

=head1 USER ENUM FILTER

These flags are used in the C<UserEnum()> function to specify which
accounts to retrieve. It should be a bitwise OR of some (or all) of
the following.

=over 8

=item C<FILTER_TEMP_DUPLICATE_ACCOUNT()>

Show temporary duplicate account (one presumes).

=item C<FILTER_NORMAL_ACCOUNT()>

Show normal user account.

=item C<FILTER_INTERDOMAIN_TRUST_ACCOUNT()>

Show interdomain trust accounts.

=item C<FILTER_WORKSTATION_TRUST_ACCOUNT()>

Show workstation trust accounts.

=item C<FILTER_SERVER_TRUST_ACCOUNT()>

Show server trust accounts.

=back

Please note that these are implemented as functions and are therefore
called in the same way as other functions. You should typically use them
like:

	$filterNormalAccounts = Win32API::Net::FILTER_NORMAL_ACCOUNT();

=head1 USER FIELD ERRORS

For the C<User*()> functions that take an C<error> parameter this variable
will, on failure, contain one of the following constants. Note that the
function may fail because more than one key/value was missing from the
input hash. You will only find out about the first one that was incorrectly
specified. This is only really useful in debugging.

=over 8

=item C<USER_ACCT_EXPIRES_PARMNUM()>

C<acctExpires> field was absent or not correctly specified.

=item C<USER_AUTH_FLAGS_PARMNUM()>

C<authFlags> field was absent or not correctly specified.

=item C<USER_BAD_PW_COUNT_PARMNUM()>

C<badPasswordCount> field was absent or not correctly specified.

=item C<USER_CODE_PAGE_PARMNUM()>

C<codePage> field was absent or not correctly specified.

=item C<USER_COMMENT_PARMNUM()>

C<comment> field was absent or not correctly specified.

=item C<USER_COUNTRY_CODE_PARMNUM()>

C<countryCode> field was absent or not correctly specified.

=item C<USER_FLAGS_PARMNUM()>

C<flags> field was absent or not correctly specified.

=item C<USER_FULL_NAME_PARMNUM()>

C<fullName> field was absent or not correctly specified.

=item C<USER_HOME_DIR_DRIVE_PARMNUM()>

C<homeDirDrive> field was absent or not correctly specified.

=item C<USER_HOME_DIR_PARMNUM()>

C<homeDir> field was absent or not correctly specified.

=item C<USER_LAST_LOGOFF_PARMNUM()>

C<lastLogoff> field was absent or not correctly specified.

=item C<USER_LAST_LOGON_PARMNUM()>

C<lastLogon> field was absent or not correctly specified.

=item C<USER_LOGON_HOURS_PARMNUM()>

C<logonHours> field was absent or not correctly specified.

=item C<USER_LOGON_SERVER_PARMNUM()>

C<logonServer> field was absent or not correctly specified.

=item C<USER_MAX_STORAGE_PARMNUM()>

C<maxStorage> field was absent or not correctly specified.

=item C<USER_NAME_PARMNUM()>

C<name> field was absent or not correctly specified.

=item C<USER_NUM_LOGONS_PARMNUM()>

C<numLogons> field was absent or not correctly specified.

=item C<USER_PARMS_PARMNUM()>

C<parms> field was absent or not correctly specified.

=item C<USER_PASSWORD_AGE_PARMNUM()>

C<passwordAge> field was absent or not correctly specified.

=item C<USER_PASSWORD_PARMNUM()>

C<password> field was absent or not correctly specified.

=item C<USER_PRIMARY_GROUP_PARMNUM()>

C<primaryGroup> field was absent or not correctly specified.

=item C<USER_PRIV_PARMNUM()>

C<priv> field was absent or not correctly specified.

=item C<USER_PROFILE_PARMNUM()>

C<profile> field was absent or not correctly specified.

=item C<USER_SCRIPT_PATH_PARMNUM()>

C<scriptPath> field was absent or not correctly specified.

=item C<USER_UNITS_PER_WEEK_PARMNUM()>

C<unitPerWeek> field was absent or not correctly specified.

=item C<USER_USR_COMMENT_PARMNUM()>

C<usrComment> field was absent or not correctly specified.

=item C<USER_WORKSTATIONS_PARMNUM()>

C<workstations> field was absent or not correctly specified.

=back


=head1 GROUP INFO LEVELS

Some of the C<Group*()> functions take a C<level> parameter. This C<level>
specifies how much detail the corresponding C<hash> should contain (or in
the case of a C<GroupGetInfo()> function, will contain after the call).
The following C<level> descriptions provide information on what fields
should be present for a given level. See L<GROUP INFO FIELDS>
for a description of the fields.

=over 8

=item C<Level 0>

name.

=item C<Level 1>

name, comment.

=item C<Level 2>

name, comment, groupId, attributes.

=item C<Level 1002>

comment.

=item C<Level 1005>

attributes.

=back


=head1 GROUP INFO FIELDS

=over 8

=item C<attributes> - Scalar Int

The attributes of the group. These are no longer settable in Windows NT 4.0
and they are not currently supported in this package either.

=item C<comment> - Scalar String

The C<comment> that applies to this group. This is the only value that
can be set via a GroupSetInfo call.

=item C<groupId> - Scalar Int

The groups Id.

=item C<name> - Scalar String

The groups name.

=back



=head1 GROUP FIELD ERRORS

For the C<Group*()> functions that take an C<error> parameter
this variable will, on failure, contain one of the following constants.
Note that the function may fail because more than one key/value was
missing from the input hash. You will only find out about the first one
that was incorrectly specified. This is only really useful for debugging
purposes.

=over 8

=item C<GROUP_ATTRIBUTES_PARMNUM()>

C<attributes> field was absent or not correctly specified.

=item C<GROUP_COMMENT_PARMNUM()>

C<comment> field was absent or not correctly specified.

=item C<GROUP_NAME_PARMNUM()>

C<name> field was absent or not correctly specified.

=back



=head1 GROUP USERS INFO LEVELS

The C<GroupGetUsers()> function can take a level of 0 or 1. These will
return the following:

=over 8

=item C<Level 0>

name.

=item C<Level 1>

name, attributes.

=back


=head1 GROUP USERS INFO FIELDS

=over 8

=item C<name> - Scalar String

The user's name.

=item C<attributes> - Scalar Int

The attributes of the group. These are no longer settable in Windows NT
4.0 and they are not currently supported in this package either.

=back



=head1 LOCAL GROUP INFO LEVELS

=over 8

=item C<Level 0>

name

=item C<Level 1>

name, comment

=item C<Level 1002>

comment


=back



=head1 LOCAL GROUP INFO FIELDS

=over 8

=item C<name> - Scalar String

The groups name

=item C<comment> - Scalar String

The groups 'comment'

=back

=head1 LOCAL GROUP FIELD ERRORS

For the C<LocalGroup*()> functions that take an C<error> parameter this
variable will, on failure, contain one of the following constants. Note
that the function may fail because more than one key/value was missing
or incorrectly specified in the input hash. You will only find out about
the first one that was incorrectly specified. This is only really useful
for debugging purposes.

=over 8

=item C<LOCALGROUP_NAME_PARMNUM()>

The C<name> field was absent or not correctly specified.

=item C<LOCALGROUP_COMMENT_PARMNUM()>

The C<comment> field wasabsent or not correctly specified.

=back

=head1 EXAMPLES

The following example shows how you can create a function in Perl that
has the same functionality as the C<NetUserEnum()> API call. The Perl
version doesn't have the level parameter so you must first use the
C<UserEnum()> function to retrieve all the account names and then iterate
through the returned array issuing C<UserGetInfo()> calls.

    sub userEnumAtLevel {
       my($server, $level, $filter) = @_;
       my(@array);
       Win32API::Net::UserEnum($server, \@array, $filter);
       for $user (@array) {
	  Win32API::Net::UserGetInfo($server, $user, $level, \%hash);
	  print "This could access all level $level settings for $user - eg fullName $hash{fullName}\n";
       }
    }
    userEnumAtLevel("", 2, 0);



=head1 AUTHOR

Bret Giddings, <bret@essex.ac.uk>


=head1 SEE ALSO

C<perl(1)>


=head1 ACKNOWEDGEMENTS

This work was built upon work done by HiP Communications along with
modifications to HiPs code by <michael@ecel.uwa.edu.au> and <rothd@roth.net>.
In addition, I would like to thank Jenny Emby at GEC Marconi, U.K. for
proof reading this manual page and making many suggestions that have led
to its current layout. Last but not least I would like to thank Larry Wall
and all the other Perl contributors for making this truly wonderful
language.

=cut
