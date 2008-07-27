package Win32;

#
#  Documentation for all Win32:: functions are in Win32.pod, which is a
#  standard part of Perl 5.6, and later.
#

BEGIN {
    use strict;
    use vars qw|$VERSION @ISA @EXPORT @EXPORT_OK|;

    require Exporter;
    require DynaLoader;

    @ISA = qw|Exporter DynaLoader|;
    $VERSION = '0.26';

    @EXPORT = qw(
	NULL
	WIN31_CLASS
	OWNER_SECURITY_INFORMATION
	GROUP_SECURITY_INFORMATION
	DACL_SECURITY_INFORMATION
	SACL_SECURITY_INFORMATION
	MB_ICONHAND
	MB_ICONQUESTION
	MB_ICONEXCLAMATION
	MB_ICONASTERISK
	MB_ICONWARNING
	MB_ICONERROR
	MB_ICONINFORMATION
	MB_ICONSTOP
    );
    @EXPORT_OK = qw(
        GetOSName
        SW_HIDE
        SW_SHOWNORMAL
        SW_SHOWMINIMIZED
        SW_SHOWMAXIMIZED
        SW_SHOWNOACTIVATE

        CSIDL_DESKTOP
        CSIDL_PROGRAMS
        CSIDL_PERSONAL
        CSIDL_FAVORITES
        CSIDL_STARTUP
        CSIDL_RECENT
        CSIDL_SENDTO
        CSIDL_STARTMENU
        CSIDL_MYMUSIC
        CSIDL_MYVIDEO
        CSIDL_DESKTOPDIRECTORY
        CSIDL_NETHOOD
        CSIDL_FONTS
        CSIDL_TEMPLATES
        CSIDL_COMMON_STARTMENU
        CSIDL_COMMON_PROGRAMS
        CSIDL_COMMON_STARTUP
        CSIDL_COMMON_DESKTOPDIRECTORY
        CSIDL_APPDATA
        CSIDL_PRINTHOOD
        CSIDL_LOCAL_APPDATA
        CSIDL_COMMON_FAVORITES
        CSIDL_INTERNET_CACHE
        CSIDL_COOKIES
        CSIDL_HISTORY
        CSIDL_COMMON_APPDATA
        CSIDL_WINDOWS
        CSIDL_SYSTEM
        CSIDL_PROGRAM_FILES
        CSIDL_MYPICTURES
        CSIDL_PROFILE
        CSIDL_PROGRAM_FILES_COMMON
        CSIDL_COMMON_TEMPLATES
        CSIDL_COMMON_DOCUMENTS
        CSIDL_COMMON_ADMINTOOLS
        CSIDL_ADMINTOOLS
        CSIDL_COMMON_MUSIC
        CSIDL_COMMON_PICTURES
        CSIDL_COMMON_VIDEO
        CSIDL_RESOURCES
        CSIDL_RESOURCES_LOCALIZED
        CSIDL_CDBURN_AREA
    );
}

# Routines available in core:
# Win32::GetLastError
# Win32::LoginName
# Win32::NodeName
# Win32::DomainName
# Win32::FsType
# Win32::GetCwd
# Win32::GetOSVersion
# Win32::FormatMessage ERRORCODE
# Win32::Spawn COMMAND, ARGS, PID
# Win32::GetTickCount
# Win32::IsWinNT
# Win32::IsWin95

# We won't bother with the constant stuff, too much of a hassle. Just hard
# code it here.

sub NULL 				{ 0 }
sub WIN31_CLASS 			{ &NULL }

sub OWNER_SECURITY_INFORMATION		{ 0x00000001 }
sub GROUP_SECURITY_INFORMATION		{ 0x00000002 }
sub DACL_SECURITY_INFORMATION		{ 0x00000004 }
sub SACL_SECURITY_INFORMATION		{ 0x00000008 }

sub MB_ICONHAND				{ 0x00000010 }
sub MB_ICONQUESTION			{ 0x00000020 }
sub MB_ICONEXCLAMATION			{ 0x00000030 }
sub MB_ICONASTERISK			{ 0x00000040 }
sub MB_ICONWARNING			{ 0x00000030 }
sub MB_ICONERROR			{ 0x00000010 }
sub MB_ICONINFORMATION			{ 0x00000040 }
sub MB_ICONSTOP				{ 0x00000010 }

#
# Newly added constants.  These have an empty prototype, unlike the
# the ones above, which aren't prototyped for compatibility reasons.
#
sub SW_HIDE           ()		{ 0 }
sub SW_SHOWNORMAL     ()		{ 1 }
sub SW_SHOWMINIMIZED  ()		{ 2 }
sub SW_SHOWMAXIMIZED  ()		{ 3 }
sub SW_SHOWNOACTIVATE ()		{ 4 }

sub CSIDL_DESKTOP              ()       { 0x0000 }     # <desktop>
sub CSIDL_PROGRAMS             ()       { 0x0002 }     # Start Menu\Programs
sub CSIDL_PERSONAL             ()       { 0x0005 }     # "My Documents" folder
sub CSIDL_FAVORITES            ()       { 0x0006 }     # <user name>\Favorites
sub CSIDL_STARTUP              ()       { 0x0007 }     # Start Menu\Programs\Startup
sub CSIDL_RECENT               ()       { 0x0008 }     # <user name>\Recent
sub CSIDL_SENDTO               ()       { 0x0009 }     # <user name>\SendTo
sub CSIDL_STARTMENU            ()       { 0x000B }     # <user name>\Start Menu
sub CSIDL_MYMUSIC              ()       { 0x000D }     # "My Music" folder
sub CSIDL_MYVIDEO              ()       { 0x000E }     # "My Videos" folder
sub CSIDL_DESKTOPDIRECTORY     ()       { 0x0010 }     # <user name>\Desktop
sub CSIDL_NETHOOD              ()       { 0x0013 }     # <user name>\nethood
sub CSIDL_FONTS                ()       { 0x0014 }     # windows\fonts
sub CSIDL_TEMPLATES            ()       { 0x0015 }
sub CSIDL_COMMON_STARTMENU     ()       { 0x0016 }     # All Users\Start Menu
sub CSIDL_COMMON_PROGRAMS      ()       { 0x0017 }     # All Users\Start Menu\Programs
sub CSIDL_COMMON_STARTUP       ()       { 0x0018 }     # All Users\Startup
sub CSIDL_COMMON_DESKTOPDIRECTORY ()    { 0x0019 }     # All Users\Desktop
sub CSIDL_APPDATA              ()       { 0x001A }     # Application Data, new for NT4
sub CSIDL_PRINTHOOD            ()       { 0x001B }     # <user name>\PrintHood
sub CSIDL_LOCAL_APPDATA        ()       { 0x001C }     # non roaming, user\Local Settings\Application Data
sub CSIDL_COMMON_FAVORITES     ()       { 0x001F }
sub CSIDL_INTERNET_CACHE       ()       { 0x0020 }
sub CSIDL_COOKIES              ()       { 0x0021 }
sub CSIDL_HISTORY              ()       { 0x0022 }
sub CSIDL_COMMON_APPDATA       ()       { 0x0023 }     # All Users\Application Data
sub CSIDL_WINDOWS              ()       { 0x0024 }     # GetWindowsDirectory()
sub CSIDL_SYSTEM               ()       { 0x0025 }     # GetSystemDirectory()
sub CSIDL_PROGRAM_FILES        ()       { 0x0026 }     # C:\Program Files
sub CSIDL_MYPICTURES           ()       { 0x0027 }     # "My Pictures", new for Win2K
sub CSIDL_PROFILE              ()       { 0x0028 }     # USERPROFILE
sub CSIDL_PROGRAM_FILES_COMMON ()       { 0x002B }     # C:\Program Files\Common
sub CSIDL_COMMON_TEMPLATES     ()       { 0x002D }     # All Users\Templates
sub CSIDL_COMMON_DOCUMENTS     ()       { 0x002E }     # All Users\Documents
sub CSIDL_COMMON_ADMINTOOLS    ()       { 0x002F }     # All Users\Start Menu\Programs\Administrative Tools
sub CSIDL_ADMINTOOLS           ()       { 0x0030 }     # <user name>\Start Menu\Programs\Administrative Tools
sub CSIDL_COMMON_MUSIC         ()       { 0x0035 }     # All Users\My Music
sub CSIDL_COMMON_PICTURES      ()       { 0x0036 }     # All Users\My Pictures
sub CSIDL_COMMON_VIDEO         ()       { 0x0037 }     # All Users\My Video
sub CSIDL_RESOURCES            ()       { 0x0038 }     # %windir%\Resources\, For theme and other windows resources.
sub CSIDL_RESOURCES_LOCALIZED  ()       { 0x0039 }     # %windir%\Resources\<LangID>, for theme and other windows specific resources.
sub CSIDL_CDBURN_AREA          ()       { 0x003B }     # <user name>\Local Settings\Application Data\Microsoft\CD Burning

### This method is just a simple interface into GetOSVersion().  More
### specific or demanding situations should use that instead.

my ($found_os, $found_desc);

sub GetOSName {
    my ($os,$desc,$major, $minor, $build, $id)=("","");
    unless (defined $found_os) {
        # If we have a run this already, we have the results cached
        # If so, return them

        # Use the standard API call to determine the version
        ($desc, $major, $minor, $build, $id) = Win32::GetOSVersion();

        # If id==0 then its a win32s box -- Meaning Win3.11
        unless($id) {
            $os = 'Win32s';
        }
	else {
	    # Magic numbers from MSDN documentation of OSVERSIONINFO
	    # Most version names can be parsed from just the id and minor
	    # version
	    $os = {
		1 => {
		    0  => "95",
		    10 => "98",
		    90 => "Me"
		},
		2 => {
		    0  => "NT4",
		    1  => "XP/.Net",
                    2  => "2003",
		    51 => "NT3.51"
		}
	    }->{$id}->{$minor};
	}

        # This _really_ shouldnt happen. At least not for quite a while
        # Politely warn and return undef
        unless (defined $os) {
            warn qq[Windows version [$id:$major:$minor] unknown!];
            return undef;
        }

        my $tag = "";

        # But distinguising W2k and Vista from NT4 requires looking at the major version
        if ($os eq "NT4") {
	    $os = {5 => "2000", 6 => "Vista"}->{$major} || "NT4";
        }

        # For the rest we take a look at the build numbers and try to deduce
	# the exact release name, but we put that in the $desc
        elsif ($os eq "95") {
            if ($build eq '67109814') {
                    $tag = '(a)';
            }
	    elsif ($build eq '67306684') {
                    $tag = '(b1)';
            }
	    elsif ($build eq '67109975') {
                    $tag = '(b2)';
            }
        }
	elsif ($os eq "98" && $build eq '67766446') {
            $tag = '(2nd ed)';
        }

	if (length $tag) {
	    if (length $desc) {
	        $desc = "$tag $desc";
	    }
	    else {
	        $desc = $tag;
	    }
	}

        # cache the results, so we dont have to do this again
        $found_os      = "Win$os";
        $found_desc    = $desc;
    }

    return wantarray ? ($found_os, $found_desc) : $found_os;
}

bootstrap Win32;

1;
