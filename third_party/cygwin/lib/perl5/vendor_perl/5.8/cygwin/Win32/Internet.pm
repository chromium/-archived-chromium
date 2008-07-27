#######################################################################
#
# Win32::Internet - Perl Module for Internet Extensions
# ^^^^^^^^^^^^^^^
# This module creates an object oriented interface to the Win32
# Internet Functions (WININET.DLL).
#
# Version: 0.08  (14 Feb 1997)
# Version: 0.081 (25 Sep 1999)
# Version: 0.082 (04 Sep 2001)
#
#######################################################################

# changes:
# - fixed 2 bugs in Option(s) related subs
# - works with build 30x also

package Win32::Internet;

require Exporter;       # to export the constants to the main:: space
require DynaLoader;     # to dynuhlode the module.

# use Win32::WinError;    # for windows constants.

@ISA= qw( Exporter DynaLoader );
@EXPORT = qw(
    HTTP_ADDREQ_FLAG_ADD
    HTTP_ADDREQ_FLAG_REPLACE
    HTTP_QUERY_ALLOW
    HTTP_QUERY_CONTENT_DESCRIPTION
    HTTP_QUERY_CONTENT_ID
    HTTP_QUERY_CONTENT_LENGTH
    HTTP_QUERY_CONTENT_TRANSFER_ENCODING
    HTTP_QUERY_CONTENT_TYPE
    HTTP_QUERY_COST
    HTTP_QUERY_CUSTOM
    HTTP_QUERY_DATE
    HTTP_QUERY_DERIVED_FROM
    HTTP_QUERY_EXPIRES
    HTTP_QUERY_FLAG_REQUEST_HEADERS
    HTTP_QUERY_FLAG_SYSTEMTIME
    HTTP_QUERY_LANGUAGE
    HTTP_QUERY_LAST_MODIFIED
    HTTP_QUERY_MESSAGE_ID
    HTTP_QUERY_MIME_VERSION
    HTTP_QUERY_PRAGMA
    HTTP_QUERY_PUBLIC
    HTTP_QUERY_RAW_HEADERS
    HTTP_QUERY_RAW_HEADERS_CRLF
    HTTP_QUERY_REQUEST_METHOD
    HTTP_QUERY_SERVER
    HTTP_QUERY_STATUS_CODE
    HTTP_QUERY_STATUS_TEXT
    HTTP_QUERY_URI
    HTTP_QUERY_USER_AGENT
    HTTP_QUERY_VERSION
    HTTP_QUERY_WWW_LINK
    ICU_BROWSER_MODE
    ICU_DECODE
    ICU_ENCODE_SPACES_ONLY
    ICU_ESCAPE
    ICU_NO_ENCODE
    ICU_NO_META
    ICU_USERNAME
    INTERNET_FLAG_PASSIVE
    INTERNET_FLAG_ASYNC
    INTERNET_HYPERLINK
    INTERNET_FLAG_KEEP_CONNECTION
    INTERNET_FLAG_MAKE_PERSISTENT
    INTERNET_FLAG_NO_AUTH
    INTERNET_FLAG_NO_AUTO_REDIRECT
    INTERNET_FLAG_NO_CACHE_WRITE
    INTERNET_FLAG_NO_COOKIES
    INTERNET_FLAG_READ_PREFETCH
    INTERNET_FLAG_RELOAD
    INTERNET_FLAG_RESYNCHRONIZE
    INTERNET_FLAG_TRANSFER_ASCII
    INTERNET_FLAG_TRANSFER_BINARY
    INTERNET_INVALID_PORT_NUMBER
    INTERNET_INVALID_STATUS_CALLBACK
    INTERNET_OPEN_TYPE_DIRECT
    INTERNET_OPEN_TYPE_PROXY
    INTERNET_OPEN_TYPE_PROXY_PRECONFIG
    INTERNET_OPTION_CONNECT_BACKOFF
    INTERNET_OPTION_CONNECT_RETRIES
    INTERNET_OPTION_CONNECT_TIMEOUT
    INTERNET_OPTION_CONTROL_SEND_TIMEOUT
    INTERNET_OPTION_CONTROL_RECEIVE_TIMEOUT
    INTERNET_OPTION_DATA_SEND_TIMEOUT
    INTERNET_OPTION_DATA_RECEIVE_TIMEOUT
    INTERNET_OPTION_HANDLE_SIZE
    INTERNET_OPTION_LISTEN_TIMEOUT
    INTERNET_OPTION_PASSWORD
    INTERNET_OPTION_READ_BUFFER_SIZE
    INTERNET_OPTION_USER_AGENT
    INTERNET_OPTION_USERNAME
    INTERNET_OPTION_VERSION
    INTERNET_OPTION_WRITE_BUFFER_SIZE
    INTERNET_SERVICE_FTP
    INTERNET_SERVICE_GOPHER
    INTERNET_SERVICE_HTTP
    INTERNET_STATUS_CLOSING_CONNECTION
    INTERNET_STATUS_CONNECTED_TO_SERVER    
    INTERNET_STATUS_CONNECTING_TO_SERVER
    INTERNET_STATUS_CONNECTION_CLOSED
    INTERNET_STATUS_HANDLE_CLOSING
    INTERNET_STATUS_HANDLE_CREATED
    INTERNET_STATUS_NAME_RESOLVED
    INTERNET_STATUS_RECEIVING_RESPONSE
    INTERNET_STATUS_REDIRECT    
    INTERNET_STATUS_REQUEST_COMPLETE    
    INTERNET_STATUS_REQUEST_SENT    
    INTERNET_STATUS_RESOLVING_NAME    
    INTERNET_STATUS_RESPONSE_RECEIVED
    INTERNET_STATUS_SENDING_REQUEST    
);


#######################################################################
# This AUTOLOAD is used to 'autoload' constants from the constant()
# XS function.  If a constant is not found then control is passed
# to the AUTOLOAD in AutoLoader.
#

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    #reset $! to zero to reset any current errors.
    local $! = 0;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {

        # [dada] This results in an ugly Autoloader error
        #if ($! =~ /Invalid/) {
        #  $AutoLoader::AUTOLOAD = $AUTOLOAD;
        #  goto &AutoLoader::AUTOLOAD;
        #} else {
      
        # [dada] ... I prefer this one :)
  
            ($pack,$file,$line) = caller; undef $pack;
            die "Win32::Internet::$constname is not defined, used at $file line $line.";
  
        #}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}


#######################################################################
# STATIC OBJECT PROPERTIES
#
$VERSION = "0.082";

%callback_code = ();
%callback_info = ();


#######################################################################
# PUBLIC METHODS
#

#======== ### CLASS CONSTRUCTOR
sub new {
#========
    my($class, $useragent, $opentype, $proxy, $proxybypass, $flags) = @_;
    my $self = {};  

    if(ref($useragent) and ref($useragent) eq "HASH") {
        $opentype       = $useragent->{'opentype'};
        $proxy          = $useragent->{'proxy'};
        $proxybypass    = $useragent->{'proxybypass'};
        $flags          = $useragent->{'flags'};
        my $myuseragent = $useragent->{'useragent'};
        undef $useragent;
        $useragent      = $myuseragent;
    }

    $useragent = "Perl-Win32::Internet/".$VERSION       unless defined($useragent);
    $opentype = constant("INTERNET_OPEN_TYPE_DIRECT",0) unless defined($opentype);
    $proxy = ""                                         unless defined($proxy);
    $proxybypass = ""                                   unless defined($proxybypass);
    $flags = 0                                          unless defined($flags);


    my $handle = InternetOpen($useragent, $opentype, $proxy, $proxybypass, $flags);
    if ($handle) {
        $self->{'connections'} = 0;
        $self->{'pasv'}        = 0;
        $self->{'handle'}      = $handle; 
        $self->{'useragent'}   = $useragent;
        $self->{'proxy'}       = $proxy;
        $self->{'proxybypass'} = $proxybypass;
        $self->{'flags'}       = $flags;
        $self->{'Type'}        = "Internet";
    
        # [dada] I think it's better to call SetStatusCallback explicitly...
        #if($flags & constant("INTERNET_FLAG_ASYNC",0)) {
        #  my $callbackresult=InternetSetStatusCallback($handle);
        #  if($callbackresult==&constant("INTERNET_INVALID_STATUS_CALLBACK",0)) {
        #    $self->{'Error'} = -2;
        #  }
        #}

        bless $self;
    } else {
        $self->{'handle'} = undef;
        bless $self;
    }
    $self;
}  


#============
sub OpenURL {
#============
    my($self,$new,$URL) = @_;
    return undef unless ref($self);

    my $newhandle=InternetOpenUrl($self->{'handle'},$URL,"",0,0,0);
    if(!$newhandle) {
        $self->{'Error'} = "Cannot open URL.";
        return undef;
    } else {
        $self->{'connections'}++;
        $_[1] = _new($newhandle);
        $_[1]->{'Type'} = "URL";
        $_[1]->{'URL'}  = $URL;
        return $newhandle;
    }
}


#================
sub TimeConvert {
#================
    my($self, $sec, $min, $hour, $day, $mon, $year, $wday, $rfc) = @_;
    return undef unless ref($self);

    if(!defined($rfc)) {
        return InternetTimeToSystemTime($sec);
    } else {
        return InternetTimeFromSystemTime($sec, $min, $hour, 
                                          $day, $mon, $year, 
                                          $wday, $rfc);
    }
}


#=======================
sub QueryDataAvailable {
#=======================
    my($self) = @_;
    return undef unless ref($self);
  
    return InternetQueryDataAvailable($self->{'handle'});
}


#=============
sub ReadFile {
#=============
    my($self, $buffersize) = @_;
    return undef unless ref($self);

    my $howmuch = InternetQueryDataAvailable($self->{'handle'});
    $buffersize = $howmuch unless defined($buffersize);
    return InternetReadFile($self->{'handle'}, ($howmuch<$buffersize) ? $howmuch 
                                                                      : $buffersize);
}


#===================
sub ReadEntireFile {
#===================
    my($handle) = @_;
    my $content    = "";
    my $buffersize = 16000;
    my $howmuch    = 0;
    my $buffer     = "";

    $handle = $handle->{'handle'} if defined($handle) and ref($handle);

    $howmuch = InternetQueryDataAvailable($handle);
    # print "\nReadEntireFile: $howmuch bytes to read...\n";
  
    while($howmuch>0) {
        $buffer = InternetReadFile($handle, ($howmuch<$buffersize) ? $howmuch 
                                                                   : $buffersize);
        # print "\nReadEntireFile: ", length($buffer), " bytes read...\n";
    
        if(!defined($buffer)) {
            return undef;
        } else {
            $content .= $buffer;
        }
        $howmuch = InternetQueryDataAvailable($handle);
        # print "\nReadEntireFile: still $howmuch bytes to read...\n";
    
    }
    return $content;
}


#=============
sub FetchURL {
#=============
    # (OpenURL+Read+Close)...
    my($self, $URL) = @_;
    return undef unless ref($self);

    my $newhandle = InternetOpenUrl($self->{'handle'}, $URL, "", 0, 0, 0);
    if(!$newhandle) {
        $self->{'Error'} = "Cannot open URL.";
        return undef;
    } else {
        my $content = ReadEntireFile($newhandle);
        InternetCloseHandle($newhandle);
        return $content;
    }
}


#================
sub Connections {
#================
    my($self) = @_;
    return undef unless ref($self);

    return $self->{'connections'} if $self->{'Type'} eq "Internet";
    return undef;
}


#================
sub GetResponse {
#================
    my($num, $text) = InternetGetLastResponseInfo();
    return $text;
}

#===========
sub Option {
#===========
    my($self, $option, $value) = @_;
    return undef unless ref($self);

    my $retval = 0;

    $option = constant("INTERNET_OPTION_USER_AGENT", 0) unless defined($option);
  
    if(!defined($value)) {
        $retval = InternetQueryOption($self->{'handle'}, $option);
    } else {
        $retval = InternetSetOption($self->{'handle'}, $option, $value);
    }
    return $retval;
}


#==============
sub UserAgent {
#==============
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_USER_AGENT", 0), $value);
}


#=============
sub Username {
#=============
    my($self, $value) = @_;
    return undef unless ref($self);
  
    if($self->{'Type'} ne "HTTP" and $self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Username() only on FTP or HTTP sessions.";
        return undef;
    }

    return Option($self, constant("INTERNET_OPTION_USERNAME", 0), $value);
}


#=============
sub Password {
#=============
    my($self, $value)=@_;
    return undef unless ref($self);

    if($self->{'Type'} ne "HTTP" and $self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Password() only on FTP or HTTP sessions.";
        return undef;
    }

    return Option($self, constant("INTERNET_OPTION_PASSWORD", 0), $value);
}


#===================
sub ConnectTimeout {
#===================
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_CONNECT_TIMEOUT", 0), $value);
}


#===================
sub ConnectRetries {
#===================
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_CONNECT_RETRIES", 0), $value);
}


#===================
sub ConnectBackoff {
#===================
    my($self,$value)=@_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_CONNECT_BACKOFF", 0), $value);
}


#====================
sub DataSendTimeout {
#====================
    my($self,$value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_DATA_SEND_TIMEOUT", 0), $value);
}


#=======================
sub DataReceiveTimeout {
#=======================
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_DATA_RECEIVE_TIMEOUT", 0), $value);
}


#==========================
sub ControlReceiveTimeout {
#==========================
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_CONTROL_RECEIVE_TIMEOUT", 0), $value);
}


#=======================
sub ControlSendTimeout {
#=======================
    my($self, $value) = @_;
    return undef unless ref($self);

    return Option($self, constant("INTERNET_OPTION_CONTROL_SEND_TIMEOUT", 0), $value);
}


#================
sub QueryOption {
#================
    my($self, $option) = @_;
    return undef unless ref($self);

    return InternetQueryOption($self->{'handle'}, $option);
}


#==============
sub SetOption {
#==============
    my($self, $option, $value) = @_;
    return undef unless ref($self);

    return InternetSetOption($self->{'handle'}, $option, $value);
}


#=============
sub CrackURL {
#=============
    my($self, $URL, $flags) = @_;
    return undef unless ref($self);

    $flags = constant("ICU_ESCAPE", 0) unless defined($flags);
  
    my @newurl = InternetCrackUrl($URL, $flags);
  
    if(!defined($newurl[0])) {
        $self->{'Error'} = "Cannot crack URL.";
        return undef;
    } else {
        return @newurl;
    }
}


#==============
sub CreateURL {
#==============
    my($self, $scheme, $hostname, $port, 
       $username, $password, 
       $path, $extrainfo, $flags) = @_;
    return undef unless ref($self);

    if(ref($scheme) and ref($scheme) eq "HASH") {
        $flags       = $hostname;
        $hostname    = $scheme->{'hostname'};
        $port        = $scheme->{'port'};
        $username    = $scheme->{'username'};
        $password    = $scheme->{'password'};
        $path        = $scheme->{'path'};
        $extrainfo   = $scheme->{'extrainfo'};
        my $myscheme = $scheme->{'scheme'};
        undef $scheme;
        $scheme      = $myscheme;
    }

    $hostname  = ""                    unless defined($hostname);
    $port      = 0                     unless defined($port);
    $username  = ""                    unless defined($username);
    $password  = ""                    unless defined($password);
    $path      = ""                    unless defined($path);
    $extrainfo = ""                    unless defined($extrainfo);
    $flags = constant("ICU_ESCAPE", 0) unless defined($flags);
  
    my $newurl = InternetCreateUrl($scheme, $hostname, $port,
                                   $username, $password,
                                   $path, $extrainfo, $flags);
    if(!defined($newurl)) {
        $self->{'Error'} = "Cannot create URL.";
        return undef;
    } else {
        return $newurl;
    }
}


#====================
sub CanonicalizeURL {
#====================
    my($self, $URL, $flags) = @_;
    return undef unless ref($self);
  
    my $newurl = InternetCanonicalizeUrl($URL, $flags);
    if(!defined($newurl)) {
        $self->{'Error'} = "Cannot canonicalize URL.";
        return undef;
    } else {
        return $newurl;
    }
}


#===============
sub CombineURL {
#===============
    my($self, $baseURL, $relativeURL, $flags) = @_;
    return undef unless ref($self);
  
    my $newurl = InternetCombineUrl($baseURL, $relativeURL, $flags);
    if(!defined($newurl)) {
        $self->{'Error'} = "Cannot combine URL(s).";
        return undef;
    } else {
        return $newurl;
    }
}


#======================
sub SetStatusCallback {
#======================
    my($self) = @_;
    return undef unless ref($self);
  
    my $callback = InternetSetStatusCallback($self->{'handle'});
    print "callback=$callback, constant=",constant("INTERNET_INVALID_STATUS_CALLBACK", 0), "\n";
    if($callback == constant("INTERNET_INVALID_STATUS_CALLBACK", 0)) {
        return undef;
    } else {
        return $callback;
    }
}


#======================
sub GetStatusCallback {
#======================
    my($self, $context) = @_;
    $context = $self if not defined $context;
    return($callback_code{$context}, $callback_info{$context});
}


#==========
sub Error {
#==========
    my($self) = @_;
    return undef unless ref($self);

    require Win32 unless defined &Win32::GetLastError;
    my $errtext = "";
    my $tmp     = "";
    my $errnum  = Win32::GetLastError();

    if($errnum < 12000) {
        $errtext =  Win32::FormatMessage($errnum);
        $errtext =~ s/[\r\n]//g;
    } elsif($errnum == 12003) {
        ($tmp, $errtext) = InternetGetLastResponseInfo();
        chomp $errtext;
        1 while($errtext =~ s/(.*)\n//); # the last line should be significative... 
                                         # otherwise call GetResponse() to get it whole
    } elsif($errnum >= 12000) {
        $errtext = FormatMessage($errnum);
        $errtext =~ s/[\r\n]//g;        
    } else {
        $errtext="Error";
    }
    if($errnum == 0 and defined($self->{'Error'})) { 
        if($self->{'Error'} == -2) {
            $errnum  = -2;
            $errtext = "Asynchronous operations not available.";
        } else {
            $errnum  = -1;
            $errtext = $self->{'Error'};
        }
    }
    return (wantarray)? ($errnum, $errtext) : "\[".$errnum."\] ".$errtext;
}


#============
sub Version {
#============
    my $dll =  InternetDllVersion();
       $dll =~ s/\0//g;
    return (wantarray)? ($Win32::Internet::VERSION,    $dll) 
                      :  $Win32::Internet::VERSION."/".$dll;
}


#==========
sub Close {
#==========
    my($self, $handle) = @_;
    if(!defined($handle)) {
        return undef unless ref($self);
        $handle = $self->{'handle'};
    }
    InternetCloseHandle($handle);
}



#######################################################################
# FTP CLASS METHODS
#

#======== ### FTP CONSTRUCTOR
sub FTP {
#========
    my($self, $new, $server, $username, $password, $port, $pasv, $context) = @_;    
    return undef unless ref($self);

    if(ref($server) and ref($server) eq "HASH") {
        $port        = $server->{'port'};
        $username    = $server->{'username'};
        $password    = $password->{'host'};
        my $myserver = $server->{'server'};
        $pasv        = $server->{'pasv'};
        $context     = $server->{'context'};
        undef $server;
        $server      = $myserver;
    }

    $server   = ""          unless defined($server);
    $username = "anonymous" unless defined($username);
    $password = ""          unless defined($password);
    $port     = 21          unless defined($port);
    $context  = 0           unless defined($context);

    $pasv = $self->{'pasv'} unless defined $pasv;
    $pasv = $pasv ? constant("INTERNET_FLAG_PASSIVE",0) : 0;

    my $newhandle = InternetConnect($self->{'handle'}, $server, $port,
                                    $username, $password,
                                    constant("INTERNET_SERVICE_FTP", 0),
                                    $pasv, $context);
    if($newhandle) {
        $self->{'connections'}++;
        $_[1] = _new($newhandle);
        $_[1]->{'Type'}     = "FTP";
        $_[1]->{'Mode'}     = "bin";
        $_[1]->{'pasv'}     = $pasv;
        $_[1]->{'username'} = $username;
        $_[1]->{'password'} = $password;
        $_[1]->{'server'}   = $server;
        return $newhandle;
    } else {
        return undef;
    }
}

#========
sub Pwd {
#========
    my($self) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP" or !defined($self->{'handle'})) {
        $self->{'Error'} = "Pwd() only on FTP sessions.";
        return undef;
    }
  
    return FtpGetCurrentDirectory($self->{'handle'});
}


#=======
sub Cd {
#=======
    my($self, $path) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP" || !defined($self->{'handle'})) {
        $self->{'Error'} = "Cd() only on FTP sessions.";
        return undef;
    }
  
    my $retval = FtpSetCurrentDirectory($self->{'handle'}, $path);
    if(!defined($retval)) {
        return undef;
    } else {
        return $path;
    }
}
#====================
sub Cwd   { Cd(@_); }
sub Chdir { Cd(@_); }
#====================


#==========
sub Mkdir {
#==========
    my($self, $path) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP" or !defined($self->{'handle'})) {
        $self->{'Error'} = "Mkdir() only on FTP sessions.";
        return undef;
    }
  
    my $retval = FtpCreateDirectory($self->{'handle'}, $path);
    $self->{'Error'} = "Can't create directory." unless defined($retval);
    return $retval;
}
#====================
sub Md { Mkdir(@_); }
#====================


#=========
sub Mode {
#=========
    my($self, $value) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP" or !defined($self->{'handle'})) {
        $self->{'Error'} = "Mode() only on FTP sessions.";
        return undef;
    }
  
    if(!defined($value)) {
        return $self->{'Mode'};
    } else {
        my $modesub = ($value =~ /^a/i) ? "Ascii" : "Binary";
        $self->$modesub($_[0]);
    }
    return $self->{'Mode'};
}


#==========
sub Rmdir {
#==========
    my($self, $path) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP" or !defined($self->{'handle'})) {
        $self->{'Error'} = "Rmdir() only on FTP sessions.";
        return undef;
    }
    my $retval = FtpRemoveDirectory($self->{'handle'}, $path);
    $self->{'Error'} = "Can't remove directory." unless defined($retval);
    return $retval;
}
#====================
sub Rd { Rmdir(@_); }
#====================


#=========
sub Pasv {
#=========
    my($self, $value) = @_;
    return undef unless ref($self);

    if(defined($value) and $self->{'Type'} eq "Internet") {
        if($value == 0) {
            $self->{'pasv'} = 0;
        } else {
            $self->{'pasv'} = 1;
        }
    }
    return $self->{'pasv'};
}

#=========
sub List {
#=========
    my($self, $pattern, $retmode) = @_;
    return undef unless ref($self);

    my $retval = "";
    my $size   = ""; 
    my $attr   = ""; 
    my $ctime  = ""; 
    my $atime  = ""; 
    my $mtime  = "";
    my $csec = 0; my $cmin = 0; my $chou = 0; my $cday = 0; my $cmon = 0; my $cyea = 0;
    my $asec = 0; my $amin = 0; my $ahou = 0; my $aday = 0; my $amon = 0; my $ayea = 0;
    my $msec = 0; my $mmin = 0; my $mhou = 0; my $mday = 0; my $mmon = 0; my $myea = 0;
    my $newhandle = 0;
    my $nextfile  = 1;
    my @results   = ();
    my ($filename, $altname, $file);
  
    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "List() only on FTP sessions.";
        return undef;
    }
  
    $pattern = "" unless defined($pattern);
    $retmode = 1  unless defined($retmode);

    if($retmode == 2) {
  
        ( $newhandle,$filename, $altname, $size, $attr,         
          $csec, $cmin, $chou, $cday, $cmon, $cyea,
          $asec, $amin, $ahou, $aday, $amon, $ayea,
          $msec, $mmin, $mhou, $mday, $mmon, $myea
        ) = FtpFindFirstFile($self->{'handle'}, $pattern, 0, 0);
    
        if(!$newhandle) {
            $self->{'Error'} = "Can't read FTP directory.";
            return undef;
        } else {
    
            while($nextfile) {
                $ctime = join(",", ($csec, $cmin, $chou, $cday, $cmon, $cyea));
                $atime = join(",", ($asec, $amin, $ahou, $aday, $amon, $ayea));
                $mtime = join(",", ($msec, $mmin, $mhou, $mday, $mmon, $myea));
                push(@results, $filename, $altname, $size, $attr, $ctime, $atime, $mtime);
        
                ( $nextfile, $filename, $altname, $size, $attr,
                  $csec, $cmin, $chou, $cday, $cmon, $cyea,
                  $asec, $amin, $ahou, $aday, $amon, $ayea,
                  $msec, $mmin, $mhou, $mday, $mmon, $myea
                ) = InternetFindNextFile($newhandle);      
        
            }
            InternetCloseHandle($newhandle);
            return @results;
      
        }
    
    } elsif($retmode == 3) {
  
        ( $newhandle,$filename, $altname, $size, $attr,
          $csec, $cmin, $chou, $cday, $cmon, $cyea,
          $asec, $amin, $ahou, $aday, $amon, $ayea,
          $msec, $mmin, $mhou, $mday, $mmon, $myea
        ) = FtpFindFirstFile($self->{'handle'}, $pattern, 0, 0);
    
        if(!$newhandle) {
            $self->{'Error'} = "Can't read FTP directory.";
            return undef;
       
        } else {
     
            while($nextfile) {
                $ctime = join(",", ($csec, $cmin, $chou, $cday, $cmon, $cyea));
                $atime = join(",", ($asec, $amin, $ahou, $aday, $amon, $ayea));
                $mtime = join(",", ($msec, $mmin, $mhou, $mday, $mmon, $myea));
                $file = { "name"     => $filename,
                          "altname"  => $altname,
                          "size"     => $size,
                          "attr"     => $attr,
                          "ctime"    => $ctime,
                          "atime"    => $atime,
                          "mtime"    => $mtime,
                };
                push(@results, $file);
         
                ( $nextfile, $filename, $altname, $size, $attr,
                  $csec, $cmin, $chou, $cday, $cmon, $cyea,
                  $asec, $amin, $ahou, $aday, $amon, $ayea,
                  $msec, $mmin, $mhou, $mday, $mmon, $myea
                ) = InternetFindNextFile($newhandle);
         
            }
            InternetCloseHandle($newhandle);
            return @results;
        }
    
    } else {
    
        ($newhandle, $filename) = FtpFindFirstFile($self->{'handle'}, $pattern, 0, 0);
    
        if(!$newhandle) {
            $self->{'Error'} = "Can't read FTP directory.";
            return undef;
      
        } else {
    
            while($nextfile) {
                push(@results, $filename);
        
                ($nextfile, $filename) = InternetFindNextFile($newhandle);  
                # print "List.no more files\n" if !$nextfile;
        
            }
            InternetCloseHandle($newhandle);
            return @results;
        }
    }
}
#====================
sub Ls  { List(@_); }
sub Dir { List(@_); }
#====================


#=================
sub FileAttrInfo {
#=================
    my($self,$attr) = @_;
    my @attrinfo = ();
    push(@attrinfo, "READONLY")   if $attr & 1;
    push(@attrinfo, "HIDDEN")     if $attr & 2;
    push(@attrinfo, "SYSTEM")     if $attr & 4;
    push(@attrinfo, "DIRECTORY")  if $attr & 16;
    push(@attrinfo, "ARCHIVE")    if $attr & 32;
    push(@attrinfo, "NORMAL")     if $attr & 128;
    push(@attrinfo, "TEMPORARY")  if $attr & 256;
    push(@attrinfo, "COMPRESSED") if $attr & 2048;
    return (wantarray)? @attrinfo : join(" ", @attrinfo);
}


#===========
sub Binary {
#===========
    my($self) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Binary() only on FTP sessions.";
        return undef;
    }
    $self->{'Mode'} = "bin";
    return undef;
}
#======================
sub Bin { Binary(@_); }
#======================


#==========
sub Ascii {
#==========
    my($self) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Ascii() only on FTP sessions.";
        return undef;
    }
    $self->{'Mode'} = "asc";
    return undef;
}
#=====================
sub Asc { Ascii(@_); }
#=====================


#========
sub Get {
#========
    my($self, $remote, $local, $overwrite, $flags, $context) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Get() only on FTP sessions.";
        return undef;
    }
    my $mode = ($self->{'Mode'} eq "asc" ? 1 : 2);
 
    $remote    = ""      unless defined($remote);
    $local     = $remote unless defined($local);
    $overwrite = 0       unless defined($overwrite);
    $flags     = 0       unless defined($flags);
    $context   = 0       unless defined($context);
  
    my $retval = FtpGetFile($self->{'handle'},
                            $remote,
                            $local,
                            $overwrite,
                            $flags,
                            $mode,
                            $context);
    $self->{'Error'} = "Can't get file." unless defined($retval);
    return $retval;
}


#===========
sub Rename {
#===========
    my($self, $oldname, $newname) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Rename() only on FTP sessions.";
        return undef;
    }

    my $retval = FtpRenameFile($self->{'handle'}, $oldname, $newname);
    $self->{'Error'} = "Can't rename file." unless defined($retval);
    return $retval;
}
#======================
sub Ren { Rename(@_); }
#======================


#===========
sub Delete {
#===========
    my($self, $filename) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Delete() only on FTP sessions.";
        return undef;
    }
    my $retval = FtpDeleteFile($self->{'handle'}, $filename);
    $self->{'Error'} = "Can't delete file." unless defined($retval);
    return $retval;
}
#======================
sub Del { Delete(@_); }
#======================


#========
sub Put {
#========
    my($self, $local, $remote, $context) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "FTP") {
        $self->{'Error'} = "Put() only on FTP sessions.";
        return undef;
    }
    my $mode = ($self->{'Mode'} eq "asc" ? 1 : 2);

    $context = 0 unless defined($context);
  
    my $retval = FtpPutFile($self->{'handle'}, $local, $remote, $mode, $context);
    $self->{'Error'} = "Can't put file." unless defined($retval);
    return $retval;
}


#######################################################################
# HTTP CLASS METHODS
#

#========= ### HTTP CONSTRUCTOR
sub HTTP {
#=========
    my($self, $new, $server, $username, $password, $port, $flags, $context) = @_;    
    return undef unless ref($self);

    if(ref($server) and ref($server) eq "HASH") {
        my $myserver = $server->{'server'};
        $username    = $server->{'username'};
        $password    = $password->{'host'};
        $port        = $server->{'port'};    
        $flags       = $server->{'flags'};
        $context     = $server->{'context'};
        undef $server;
        $server      = $myserver;
    }

    $server   = ""          unless defined($server);
    $username = "anonymous" unless defined($username);
    $password = ""          unless defined($password);
    $port     = 80          unless defined($port);
    $flags    = 0           unless defined($flags);
    $context  = 0           unless defined($context);
  
    my $newhandle = InternetConnect($self->{'handle'}, $server, $port,
                                    $username, $password,
                                    constant("INTERNET_SERVICE_HTTP", 0),
                                    $flags, $context);
    if($newhandle) {
        $self->{'connections'}++;
        $_[1] = _new($newhandle);
        $_[1]->{'Type'}     = "HTTP";
        $_[1]->{'username'} = $username;
        $_[1]->{'password'} = $password;
        $_[1]->{'server'}   = $server;
        $_[1]->{'accept'}   = "text/*\0image/gif\0image/jpeg\0\0";
        return $newhandle;
    } else {
        return undef;
    }
}


#================
sub OpenRequest {
#================
    # alternatively to Request:
    # it creates a new HTTP_Request object
    # you can act upon it with AddHeader, SendRequest, ReadFile, QueryInfo, Close, ...

    my($self, $new, $path, $method, $version, $referer, $accept, $flags, $context) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "HTTP") {
        $self->{'Error'} = "OpenRequest() only on HTTP sessions.";
        return undef;
    }

    if(ref($path) and ref($path) eq "HASH") {
        $method    = $path->{'method'};
        $version   = $path->{'version'};
        $referer   = $path->{'referer'};
        $accept    = $path->{'accept'};
        $flags     = $path->{'flags'};
        $context   = $path->{'context'};
        my $mypath = $path->{'path'};
        undef $path;
        $path      = $mypath;
    }

    $method  = "GET"             unless defined($method);
    $path    = "/"               unless defined($path);
    $version = "HTTP/1.0"        unless defined($version); 
    $referer = ""                unless defined($referer);
    $accept  = $self->{'accept'} unless defined($accept);
    $flags   = 0                 unless defined($flags);
    $context = 0                 unless defined($context);
  
    $path = "/".$path if substr($path,0,1) ne "/";  
    # accept string list needs to be terminated by double-NULL
    $accept .= "\0\0" unless $accept =~ /\0\0\z/;
  
    my $newhandle = HttpOpenRequest($self->{'handle'},
                                    $method,
                                    $path,
                                    $version,
                                    $referer,
                                    $accept,
                                    $flags,
                                    $context);
    if($newhandle) {
        $_[1] = _new($newhandle);
        $_[1]->{'Type'}    = "HTTP_Request";
        $_[1]->{'method'}  = $method;
        $_[1]->{'request'} = $path;
        $_[1]->{'accept'}  = $accept;
        return $newhandle;
    } else {
        return undef;
    }
}

#================
sub SendRequest {
#================
    my($self, $postdata) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "HTTP_Request") {
        $self->{'Error'} = "SendRequest() only on HTTP requests.";
        return undef;
    }
  
    $postdata = "" unless defined($postdata);

    return HttpSendRequest($self->{'handle'}, "", $postdata);
}


#==============
sub AddHeader {
#==============
    my($self, $header, $flags) = @_;
    return undef unless ref($self);
  
    if($self->{'Type'} ne "HTTP_Request") {
        $self->{'Error'} = "AddHeader() only on HTTP requests.";
        return undef;
    }
  
    $flags = constant("HTTP_ADDREQ_FLAG_ADD", 0) if (!defined($flags) or $flags == 0);

    return HttpAddRequestHeaders($self->{'handle'}, $header, $flags);
}


#==============
sub QueryInfo {
#==============
    my($self, $header, $flags) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "HTTP_Request") {
        $self->{'Error'}="QueryInfo() only on HTTP requests.";
        return undef;
    }
  
    $flags = constant("HTTP_QUERY_CUSTOM", 0) if (!defined($flags) and defined($header));
    my @queryresult = HttpQueryInfo($self->{'handle'}, $flags, $header);
    return (wantarray)? @queryresult : join(" ", @queryresult);
}


#============
sub Request {
#============
    # HttpOpenRequest+HttpAddHeaders+HttpSendRequest+InternetReadFile+HttpQueryInfo
    my($self, $path, $method, $version, $referer, $accept, $flags, $postdata) = @_;
    return undef unless ref($self);

    if($self->{'Type'} ne "HTTP") {
        $self->{'Error'} = "Request() only on HTTP sessions.";
        return undef;
    }

    if(ref($path) and ref($path) eq "HASH") {
        $method    = $path->{'method'};
        $version   = $path->{'version'};
        $referer   = $path->{'referer'};
        $accept    = $path->{'accept'};
        $flags     = $path->{'flags'};
        $postdata  = $path->{'postdata'};
        my $mypath = $path->{'path'};
        undef $path;
        $path      = $mypath;
    }

    my $content     = "";
    my $result      = "";
    my @queryresult = ();
    my $statuscode  = "";
    my $headers     = "";
  
    $path     = "/"               unless defined($path);
    $method   = "GET"             unless defined($method);
    $version  = "HTTP/1.0"        unless defined($version); 
    $referer  = ""                unless defined($referer);
    $accept   = $self->{'accept'} unless defined($accept);
    $flags    = 0                 unless defined($flags);
    $postdata = ""                unless defined($postdata);

    $path = "/".$path if substr($path,0,1) ne "/";  
    # accept string list needs to be terminated by double-NULL
    $accept .= "\0\0" unless $accept =~ /\0\0\z/;

    my $newhandle = HttpOpenRequest($self->{'handle'},
                                    $method,
                                    $path,
                                    $version,
                                    $referer,
                                    $accept,
                                    $flags,
				    0);

    if($newhandle) {

        $result = HttpSendRequest($newhandle, "", $postdata);

        if(defined($result)) {
            $statuscode = HttpQueryInfo($newhandle,
                                        constant("HTTP_QUERY_STATUS_CODE", 0), "");
            $headers = HttpQueryInfo($newhandle,
                                     constant("HTTP_QUERY_RAW_HEADERS_CRLF", 0), "");
            $content = ReadEntireFile($newhandle);
               
            InternetCloseHandle($newhandle);
      
            return($statuscode, $headers, $content);
        } else {
            return undef;
        }
    } else {
        return undef;
    }
}


#######################################################################
# END OF THE PUBLIC METHODS
#


#========= ### SUB-CLASSES CONSTRUCTOR
sub _new {
#=========
    my $self = {};
    if ($_[0]) {
        $self->{'handle'} = $_[0];
        bless $self;
    } else {
        undef($self);
    }
    $self;
}


#============ ### CLASS DESTRUCTOR
sub DESTROY {
#============
    my($self) = @_;
    # print "Closing handle $self->{'handle'}...\n";
    InternetCloseHandle($self->{'handle'});
    # [dada] rest in peace
}


#=============
sub callback {
#=============
    my($name, $status, $info) = @_;
    $callback_code{$name} = $status;
    $callback_info{$name} = $info;
}

#######################################################################
# dynamically load in the Internet.pll module.
#

bootstrap Win32::Internet;

# Preloaded methods go here.

#Currently Autoloading is not implemented in Perl for win32
# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Win32::Internet - Access to WININET.DLL functions

=head1 INTRODUCTION

This extension to Perl implements the Win32 Internet APIs (found in
F<WININET.DLL>). They give a complete support for HTTP, FTP and GOPHER
connections.

See the L<"Version History"> and the L<"Functions Table"> for a list
of the currently supported features. You should also get a copy of the
L<"Microsoft Win32 Internet Functions"> documentation.

=head1 REFERENCE

To use this module, first add the following line at the beginning of
your script:

    use Win32::Internet;

Then you have to open an Internet connection with this command:

    $Connection = new Win32::Internet();

This is required to use any of the function of this module.  It will
create an Internet object in Perl on which you can act upon with the
L<"General Internet Functions"> explained later.

The objects available are:

=over

=item *

Internet connections (the main object, see C<new>)

=item *

URLs (see C<OpenURL>)

=item *

FTP sessions (see C<FTP>)

=item *

HTTP sessions (see C<HTTP>)

=item *

HTTP requests (see C<OpenRequest>)

=back

As in the good Perl tradition, there are in this extension different
ways to do the same thing; there are, in fact, different levels of
implementation of the Win32 Internet Functions.  Some routines use
several Win32 API functions to perform a complex task in a single
call; they are simpler to use, but of course less powerful.

There are then other functions that implement nothing more and nothing
less than the corresponding API function, so you can use all of their
power, but with some additional programming steps.

To make an example, there is a function called C<FetchURL> that you
can use to fetch the content of any HTTP, FTP or GOPHER URL with this
simple commands:

    $INET = new Win32::Internet();
    $file = $INET->FetchURL("http://www.yahoo.com");

You can have the same result (and this is actually what is done by
C<FetchURL>) this way:

    $INET = new Win32::Internet();
    $URL = $INET->OpenURL("http://www.yahoo.com");
    $file = $URL->ReadFile();
    $URL->Close();

Or, you can open a complete HTTP session:

    $INET = new Win32::Internet();
    $HTTP = $INET->HTTP("www.yahoo.com", "anonymous", "dada@divinf.it");
    ($statuscode, $headers, $file) = $HTTP->Request("/");
    $HTTP->Close();

Finally, you can choose to manage even the HTTP request:

    $INET = new Win32::Internet();
    $HTTP = $INET->HTTP("www.yahoo.com", "anonymous", "dada@divinf.it");
    $HTTP->OpenRequest($REQ, "/");
    $REQ->AddHeader("If-Modified-Since: Saturday, 16-Nov-96 15:58:50 GMT");
    $REQ->SendRequest();
    $statuscode = $REQ->QueryInfo("",HTTP_QUERY_STATUS_CODE);
    $lastmodified = $REQ->QueryInfo("Last-Modified");
    $file = $REQ->ReadEntireFile();
    $REQ->Close();
    $HTTP->Close();

To open and control a complete FTP session, type:

    $Connection->FTP($Session, "ftp://ftp.activeware.com", "anonymous", "dada\@divinf.it");

This will create an FTP object in Perl to which you can apply the L<"FTP
functions"> provided by the package:

    $Session->Cd("/ntperl/perl5.001m/CurrentBuild");
    $Session->Ascii();
    $Session->Get("110-i86.zip");
    $Session->Close();

For a more complete example, see the TEST.PL file that comes with the
package.

=head2 General Internet Functions

B<General Note>

All methods assume that you have the line:

    use Win32::Internet;

somewhere before the method calls, and that you have an Internet
object called $INET which was created using this call:

    $INET = new Win32::Internet();

See C<new> for more information.

B<Methods>

=over

=item CanonicalizeURL URL, [flags]

Converts a URL to a canonical format, which includes converting unsafe
characters to escape sequences.  Returns the canonicalized URL or
C<undef> on errors.  For the possible values of I<flags>, refer to the
L<"Microsoft Win32 Internet Functions"> document.  See also
C<CombineURL> and C<OpenURL>.

Example:

    $cURL = $INET->CanonicalizeURL($URL);
    $URL = $INET->CanonicalizeURL($cURL, ICU_DECODE);

=item Close

=item Close object

Closes an Internet connection.  This can be applied to any
Win32::Internet object (Internet connections, URLs, FTP sessions,
etc.).  Note that it is not "strictly" required to close the
connections you create, since the Win32::Internet objects are
automatically closed when the program ends (or when you elsehow
destroy such an object).

Example:

    $INET->Close();
    $FTP->Close();
    $INET->Close($FTP); # same as above...

=item CombineURL baseURL, relativeURL, [flags]

Combines a base and relative URL into a single URL.  Returns the
(canonicalized) combined URL or C<undef> on errors.  For the possible
values of I<flags>, refer to the L<"Microsoft Win32 Internet
Functions"> document.  See also C<CombineURL> and C<OpenURL>.

Example:

    $URL = $INET->CombineURL("http://www.divinf.it/dada/perl/internet", "..");

=item ConnectBackoff [value]

Reads or sets the delay value, in milliseconds, to wait between
connection retries.  If no I<value> parameter is specified, the
current value is returned; otherwise, the delay between retries is set
to I<value>.  See also C<ConnectTimeout>, C<ConnectRetries>,
C<QueryOption> and C<SetOption>.

Example:

    $HTTP->ConnectBackoff(2000);
    $backoff = $HTTP->ConnectBackoff();

=item ConnectRetries [value]

Reads or sets the number of times a connection is retried before
considering it failed.  If no I<value> parameter is specified, the
current value is returned; otherwise, the number of retries is set to
I<value>.  The default value for C<ConnectRetries> is 5.  See also
C<ConnectBackoff>, C<ConnectTimeout>, C<QueryOption> and C<SetOption>.

Example:

    $HTTP->ConnectRetries(20);
    $retries = $HTTP->ConnectRetries();

=item ConnectTimeout [value]

Reads or sets the timeout value (in milliseconds) before a connection
is considered failed.  If no I<value> parameter is specified, the
current value is returned; otherwise, the timeout is set to I<value>.
The default value for C<ConnectTimeout> is infinite.  See also
C<ConnectBackoff>, C<ConnectRetries>, C<QueryOption> and C<SetOption>.

Example:

    $HTTP->ConnectTimeout(10000);
    $timeout = $HTTP->ConnectTimeout();

=item ControlReceiveTimeout [value]

Reads or sets the timeout value (in milliseconds) to use for non-data
(control) receive requests before they are canceled.  Currently, this
value has meaning only for C<FTP> sessions.  If no I<value> parameter
is specified, the current value is returned; otherwise, the timeout is
set to I<value>.  The default value for C<ControlReceiveTimeout> is
infinite.  See also C<ControlSendTimeout>, C<QueryOption> and
C<SetOption>.

Example:

    $HTTP->ControlReceiveTimeout(10000);
    $timeout = $HTTP->ControlReceiveTimeout();

=item ControlSendTimeout [value]

Reads or sets the timeout value (in milliseconds) to use for non-data
(control) send requests before they are canceled.  Currently, this
value has meaning only for C<FTP> sessions.  If no I<value> parameter
is specified, the current value is returned; otherwise, the timeout is
set to I<value>.  The default value for C<ControlSendTimeout> is
infinite.  See also C<ControlReceiveTimeout>, C<QueryOption> and
C<SetOption>.

Example:

    $HTTP->ControlSendTimeout(10000);
    $timeout = $HTTP->ControlSendTimeout();

=item CrackURL URL, [flags]

Splits an URL into its component parts and returns them in an array.
Returns C<undef> on errors, otherwise the array will contain the
following values: I<scheme, host, port, username, password, path,
extrainfo>.

For example, the URL "http://www.divinf.it/index.html#top" can be
splitted in:

    http, www.divinf.it, 80, anonymous, dada@divinf.it, /index.html, #top

If you don't specify a I<flags> parameter, ICU_ESCAPE will be used by
default; for the possible values of I<flags> refer to the L<"Microsoft
Win32 Internet Functions"> documentation.  See also C<CreateURL>.

Example:

    @parts=$INET->CrackURL("http://www.activeware.com");
    ($scheme, $host, $port, $user, $pass, $path, $extra) =
         $INET->CrackURL("http://www.divinf.it:80/perl-win32/index.sht#feedback");

=item CreateURL scheme, hostname, port, username, password, path, extrainfo, [flags]

=item CreateURL hashref, [flags]

Creates a URL from its component parts.  Returns C<undef> on errors,
otherwise the created URL.

If you pass I<hashref> (a reference to an hash array), the following
values are taken from the array:

    %hash=(
      "scheme"    => "scheme",
      "hostname"  => "hostname",
      "port"      => port,
      "username"  => "username",
      "password"  => "password",
      "path"      => "path",
      "extrainfo" => "extrainfo",
    );

If you don't specify a I<flags> parameter, ICU_ESCAPE will be used by
default; for the other possible values of I<flags> refer to the
L<"Microsoft Win32 Internet Functions"> documentation.  See also
C<CrackURL>.

Example:

    $URL=$I->CreateURL("http", "www.divinf.it", 80, "", "", "/perl-win32/index.sht", "#feedback");
    $URL=$I->CreateURL(\%params);

=item DataReceiveTimeout [value]

Reads or sets the timeout value (in milliseconds) to use for data
receive requests before they are canceled.  If no I<value> parameter
is specified, the current value is returned; otherwise, the timeout is
set to I<value>.  The default value for DataReceiveTimeout is
infinite.  See also C<DataSendTimeout>, C<QueryOption> and
C<SetOption>.

Example:

    $HTTP->DataReceiveTimeout(10000);
    $timeout = $HTTP->DataReceiveTimeout();

=item DataSendTimeout [value]

Reads or sets the timeout value (in milliseconds) to use for data send
requests before they are canceled.  If no I<value> parameter is
specified, the current value is returned; otherwise, the timeout is
set to I<value>.  The default value for DataSendTimeout is infinite.
See also C<DataReceiveTimeout>, C<QueryOption> and C<SetOption>.

Example:

    $HTTP->DataSendTimeout(10000);
    $timeout = $HTTP->DataSendTimeout();

=item Error

Returns the last recorded error in the form of an array or string
(depending upon the context) containing the error number and an error
description.  Can be applied on any Win32::Internet object (FTP
sessions, etc.).  There are 3 types of error you can encounter; they
are recognizable by the error number returned:

=over

=item * -1

A "trivial" error has occurred in the package.  For example, you tried
to use a method on the wrong type of object.

=item * 1 .. 11999

A generic error has occurred and the Win32::GetLastError error message
is returned.

=item * 12000 and higher

An Internet error has occurred; the extended Win32 Internet API error
message is returned.

=back

See also C<GetResponse>.

Example:

    die $INET->Error(), qq(\n);
    ($ErrNum, $ErrText) = $INET->Error();

=item FetchURL URL

Fetch the content of an HTTP, FTP or GOPHER URL.  Returns the content
of the file read (or C<undef> if there was an error and nothing was
read).  See also C<OpenURL> and C<ReadFile>.

Example:

    $file = $INET->FetchURL("http://www.yahoo.com/");
    $file = $INET->FetchURL("ftp://www.activeware.com/contrib/internet.zip");

=item FTP ftpobject, server, username, password, [port, pasv, context]

=item FTP ftpobject, hashref

Opens an FTP connection to server logging in with the given
I<username> and I<password>.

The parameters and their values are:

=over

=item * server

The server to connect to.  Default: I<none>.

=item * username

The username used to login to the server.  Default: anonymous.

=item * password

The password used to login to the server.  Default: I<none>.

=item * port

The port of the FTP service on the server.  Default: 21.

=item * pasv

If it is a value other than 0, use passive transfer mode.  Default is
taken from the parent Internet connection object; you can set this
value with the C<Pasv> method.

=item * context

A number to identify this operation if it is asynchronous.  See
C<SetStatusCallback> and C<GetStatusCallback> for more info on
asynchronous operations.  Default: I<none>.

=back

If you pass I<hashref> (a reference to an hash array), the following
values are taken from the array:

    %hash=(
      "server"   => "server",
      "username" => "username",
      "password" => "password",
      "port"     => port,
      "pasv"     => pasv,
      "context"  => context,
    );

This method returns C<undef> if the connection failed, a number
otherwise.  You can then call any of the L<"FTP functions"> as methods
of the newly created I<ftpobject>.

Example:

    $result = $INET->FTP($FTP, "ftp.activeware.com", "anonymous", "dada\@divinf.it");
    # and then for example...
    $FTP->Cd("/ntperl/perl5.001m/CurrentBuild");

    $params{"server"} = "ftp.activeware.com";
    $params{"password"} = "dada\@divinf.it";
    $params{"pasv"} = 0;
    $result = $INET->FTP($FTP,\%params);

=item GetResponse

Returns the text sent by a remote server in response to the last
function executed.  It applies on any Win32::Internet object,
particularly of course on L<FTP sessions|"FTP functions">.  See also
the C<Error> function.

Example:

    print $INET->GetResponse();
    $INET->FTP($FTP, "ftp.activeware.com", "anonymous", "dada\@divinf.it");
    print $FTP->GetResponse();

=item GetStatusCallback context

Returns information about the progress of the asynchronous operation
identified by I<context>; those informations consist of two values: a
status code (one of the INTERNET_STATUS_* L<"Constants">) and an
additional value depending on the status code; for example, if the
status code returned is INTERNET_STATUS_HANDLE_CREATED, the second
value will hold the handle just created.  For more informations on
those values, please refer to the L<"Microsoft Win32 Internet
Functions"> documentation.  See also C<SetStatusCallback>.

Example:

    ($status, $info) = $INET->GetStatusCallback(1);

=item HTTP httpobject, server, username, password, [port, flags, context]

=item HTTP httpobject, hashref

Opens an HTTP connection to I<server> logging in with the given
I<username> and I<password>.

The parameters and their values are:

=over

=item * server

The server to connect to.  Default: I<none>.

=item * username

The username used to login to the server.  Default: anonymous.

=item * password

The password used to login to the server.  Default: I<none>.

=item * port

The port of the HTTP service on the server.  Default: 80.

=item * flags

Additional flags affecting the behavior of the function.  Default:
I<none>.

=item * context

A number to identify this operation if it is asynchronous.  See
C<SetStatusCallback> and C<GetStatusCallback> for more info on
asynchronous operations.  Default: I<none>.

=back

Refer to the L<"Microsoft Win32 Internet Functions"> documentation for
more details on those parameters.

If you pass I<hashref> (a reference to an hash array), the following
values are taken from the array:

    %hash=(
      "server"   => "server",
      "username" => "username",
      "password" => "password",
      "port"     => port,
      "flags"    => flags,
      "context"  => context,
    );

This method returns C<undef> if the connection failed, a number
otherwise.  You can then call any of the L<"HTTP functions"> as
methods of the newly created I<httpobject>.

Example:

    $result = $INET->HTTP($HTTP,"www.activeware.com","anonymous","dada\@divinf.it");
    # and then for example...
    ($statuscode, $headers, $file) = $HTTP->Request("/gifs/camel.gif");

    $params{"server"} = "www.activeware.com";
    $params{"password"} = "dada\@divinf.it";
    $params{"flags"} = INTERNET_FLAG_RELOAD;
    $result = $INET->HTTP($HTTP,\%params);

=item new Win32::Internet [useragent, opentype, proxy, proxybypass, flags]

=item new Win32::Internet [hashref]

Creates a new Internet object and initializes the use of the Internet
functions; this is required before any of the functions of this
package can be used.  Returns C<undef> if the connection fails, a number
otherwise.  The parameters and their values are:

=over

=item * useragent

The user agent passed to HTTP requests.  See C<OpenRequest>.  Default:
Perl-Win32::Internet/I<version>.

=item * opentype

How to access to the Internet (eg. directly or using a proxy).
Default: INTERNET_OPEN_TYPE_DIRECT.

=item * proxy

Name of the proxy server (or servers) to use.  Default: I<none>.

=item * proxybypass

Optional list of host names or IP addresses, or both, that are known
locally.  Default: I<none>.

=item * flags

Additional flags affecting the behavior of the function.  Default:
I<none>.

=back

Refer to the L<"Microsoft Win32 Internet Functions"> documentation for
more details on those parameters.  If you pass I<hashref> (a reference to
an hash array), the following values are taken from the array:

    %hash=(
      "useragent"   => "useragent",
      "opentype"    => "opentype",
      "proxy"       => "proxy",
      "proxybypass" => "proxybypass",
      "flags"       => flags,
    );

Example:

    $INET = new Win32::Internet();
    die qq(Cannot connect to Internet...\n) if ! $INET;

    $INET = new Win32::Internet("Mozilla/3.0", INTERNET_OPEN_TYPE_PROXY, "www.microsoft.com", "");

    $params{"flags"} = INTERNET_FLAG_ASYNC;
    $INET = new Win32::Internet(\%params);

=item OpenURL urlobject, URL

Opens a connection to an HTTP, FTP or GOPHER Uniform Resource Location
(URL).  Returns C<undef> on errors or a number if the connection was
succesful.  You can then retrieve the URL content by applying the
methods C<QueryDataAvailable> and C<ReadFile> on the newly created
I<urlobject>.  See also C<FetchURL>.

Example:

    $INET->OpenURL($URL, "http://www.yahoo.com/");
    $bytes = $URL->QueryDataAvailable();
    $file = $URL->ReadEntireFile();
    $URL->Close();

=item Password [password]

Reads or sets the password used for an C<FTP> or C<HTTP> connection.
If no I<password> parameter is specified, the current value is
returned; otherwise, the password is set to I<password>.  See also
C<Username>, C<QueryOption> and C<SetOption>.

Example:

    $HTTP->Password("splurfgnagbxam");
    $password = $HTTP->Password();

=item QueryDataAvailable

Returns the number of bytes of data that are available to be read
immediately by a subsequent call to C<ReadFile> (or C<undef> on
errors).  Can be applied to URL or HTTP request objects.  See
C<OpenURL> or C<OpenRequest>.

Example:

    $INET->OpenURL($URL, "http://www.yahoo.com/");
    $bytes = $URL->QueryDataAvailable();

=item QueryOption option

Queries an Internet option.  For the possible values of I<option>,
refer to the L<"Microsoft Win32 Internet Functions"> document.  See
also C<SetOption>.

Example:

    $value = $INET->QueryOption(INTERNET_OPTION_CONNECT_TIMEOUT);
    $value = $HTTP->QueryOption(INTERNET_OPTION_USERNAME);

=item ReadEntireFile

Reads all the data available from an opened URL or HTTP request
object.  Returns what have been read or C<undef> on errors.  See also
C<OpenURL>, C<OpenRequest> and C<ReadFile>.

Example:

    $INET->OpenURL($URL, "http://www.yahoo.com/");
    $file = $URL->ReadEntireFile();

=item ReadFile bytes

Reads I<bytes> bytes of data from an opened URL or HTTP request
object.  Returns what have been read or C<undef> on errors.  See also
C<OpenURL>, C<OpenRequest>, C<QueryDataAvailable> and
C<ReadEntireFile>.

B<Note:> be careful to keep I<bytes> to an acceptable value (eg.  don't
tell him to swallow megabytes at once...).  C<ReadEntireFile> in fact
uses C<QueryDataAvailable> and C<ReadFile> in a loop to read no more
than 16k at a time.

Example:

    $INET->OpenURL($URL, "http://www.yahoo.com/");
    $chunk = $URL->ReadFile(16000);

=item SetOption option, value

Sets an Internet option.  For the possible values of I<option>, refer to
the L<"Microsoft Win32 Internet Functions"> document.  See also
C<QueryOption>.

Example:

    $INET->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT,10000);
    $HTTP->SetOption(INTERNET_OPTION_USERNAME,"dada");

=item SetStatusCallback

Initializes the callback routine used to return data about the
progress of an asynchronous operation.

Example:

    $INET->SetStatusCallback();

This is one of the step required to perform asynchronous operations;
the complete procedure is:

    # use the INTERNET_FLAG_ASYNC when initializing
    $params{'flags'}=INTERNET_FLAG_ASYNC;
    $INET = new Win32::Internet(\%params);

    # initialize the callback routine
    $INET->SetStatusCallback();

    # specify the context parameter (the last 1 in this case)
    $INET->HTTP($HTTP, "www.yahoo.com", "anonymous", "dada\@divinf.it", 80, 0, 1);

At this point, control returns immediately to Perl and $INET->Error()
will return 997, which means an asynchronous I/O operation is
pending.  Now, you can call

    $HTTP->GetStatusCallback(1);

in a loop to verify what's happening; see also C<GetStatusCallback>.

=item TimeConvert time

=item TimeConvert seconds, minute, hours, day, month, year,
                  day_of_week, RFC

The first form takes a HTTP date/time string and returns the date/time
converted in the following array: I<seconds, minute, hours, day,
month, year, day_of_week>.

The second form does the opposite (or at least it should, because
actually seems to be malfunctioning): it takes the values and returns
an HTTP date/time string, in the RFC format specified by the I<RFC>
parameter (OK, I didn't find yet any accepted value in the range
0..2000, let me know if you have more luck with it).

Example:

    ($sec, $min, $hour, $day, $mday, $year, $wday) =
       $INET->TimeConvert("Sun, 26 Jan 1997 20:01:52 GMT");

    # the opposite DOESN'T WORK! which value should $RFC have???
    $time = $INET->TimeConvert(52, 1, 20, 26, 1, 1997, 0, $RFC);

=item UserAgent [name]

Reads or sets the user agent used for HTTP requests.  If no I<name>
parameter is specified, the current value is returned; otherwise, the
user agent is set to I<name>.  See also C<QueryOption> and
C<SetOption>.

Example:

    $INET->UserAgent("Mozilla/3.0");
    $useragent = $INET->UserAgent();

=item Username [name]

Reads or sets the username used for an C<FTP> or C<HTTP> connection.
If no I<name> parameter is specified, the current value is returned;
otherwise, the username is set to I<name>.  See also C<Password>,
C<QueryOption> and SetOption.

Example:

    $HTTP->Username("dada");
    $username = $HTTP->Username();

=item Version

Returns the version numbers for the Win32::Internet package and the
WININET.DLL version, as an array or string, depending on the context.
The string returned will contain "package_version/DLL_version", while
the array will contain: "package_version", "DLL_version".

Example:

    $version = $INET->Version(); # should return "0.06/4.70.1215"
    @version = $INET->Version(); # should return ("0.06", "4.70.1215")

=back


=head2 FTP Functions

B<General Note>

All methods assume that you have the following lines:

    use Win32::Internet;
    $INET = new Win32::Internet();
    $INET->FTP($FTP, "hostname", "username", "password");

somewhere before the method calls; in other words, we assume that you
have an Internet object called $INET and an open FTP session called
$FTP.

See C<new> and C<FTP> for more information.


B<Methods>

=over

=item Ascii

=item Asc

Sets the ASCII transfer mode for this FTP session.  It will be applied
to the subsequent C<Get> functions.  See also the C<Binary> and
C<Mode> function.

Example:

    $FTP->Ascii();

=item Binary

=item Bin

Sets the binary transfer mode for this FTP session.  It will be
applied to the subsequent C<Get> functions.  See also the C<Ascii> and
C<Mode> function.

Example:

    $FTP->Binary();

=item Cd path

=item Cwd path

=item Chdir path

Changes the current directory on the FTP remote host.  Returns I<path>
or C<undef> on error.

Example:

    $FTP->Cd("/pub");

=item Delete file

=item Del file

Deletes a file on the FTP remote host.  Returns C<undef> on error.

Example:

    $FTP->Delete("110-i86.zip");

=item Get remote, [local, overwrite, flags, context]

Gets the I<remote> FTP file and saves it locally in I<local>.  If
I<local> is not specified, it will be the same name as I<remote>.
Returns C<undef> on error.  The parameters and their values are:

=over

=item * remote

The name of the remote file on the FTP server.  Default: I<none>.

=item * local

The name of the local file to create.  Default: remote.

=item * overwrite

If 0, overwrites I<local> if it exists; with any other value, the
function fails if the I<local> file already exists.  Default: 0.

=item * flags

Additional flags affecting the behavior of the function.  Default:
I<none>.

=item * context

A number to identify this operation if it is asynchronous.  See
C<SetStatusCallback> and C<GetStatusCallback> for more info on
asynchronous operations.  Default: I<none>.

=back

Refer to the L<"Microsoft Win32 Internet Functions"> documentation for
more details on those parameters.

Example:

    $FTP->Get("110-i86.zip");
    $FTP->Get("/pub/perl/languages/CPAN/00index.html", "CPAN_index.html");

=item List [pattern, listmode]

=item Ls [pattern, listmode]

=item Dir [pattern, listmode]

Returns a list containing the files found in this directory,
eventually matching the given I<pattern> (which, if omitted, is
considered "*.*").  The content of the returned list depends on the
I<listmode> parameter, which can have the following values:

=over

=item * listmode=1 (or omitted)

the list contains the names of the files found.  Example:

    @files = $FTP->List();
    @textfiles = $FTP->List("*.txt");
    foreach $file (@textfiles) {
      print "Name: ", $file, "\n";
    }

=item * listmode=2

the list contains 7 values for each file, which respectively are:

=over

=item * the file name

=item * the DOS short file name, aka 8.3

=item * the size

=item * the attributes

=item * the creation time

=item * the last access time

=item * the last modified time

=back

Example:

    @files = $FTP->List("*.*", 2);
    for($i=0; $i<=$#files; $i+=7) {
      print "Name: ", @files[$i], "\n";
      print "Size: ", @files[$i+2], "\n";
      print "Attr: ", @files[$i+3], "\n";
    }

=item * listmode=3

the list contains a reference to an hash array for each found file;
each hash contains:

=over

=item * name => the file name

=item * altname => the DOS short file name, aka 8.3

=item * size => the size

=item * attr => the attributes

=item * ctime => the creation time

=item * atime => the last access time

=item * mtime => the last modified time

=back

Example:

    @files = $FTP->List("*.*", 3);
    foreach $file (@files) {
      print $file->{'name'}, " ", $file->{'size'}, " ", $file->{'attr'}, "\n";
    }

B<Note:> all times are reported as strings of the following format:
I<second, hour, minute, day, month, year>.

Example:

    $file->{'mtime'} == "0,10,58,9,12,1996" stands for 09 Dec 1996 at 10:58:00

=back

=item Mkdir name

=item Md name

Creates a directory on the FTP remote host.  Returns C<undef> on error.

Example:

    $FTP->Mkdir("NextBuild");

=item Mode [mode]

If called with no arguments, returns the current transfer mode for
this FTP session ("asc" for ASCII or "bin" for binary).  The I<mode>
argument can be "asc" or "bin", in which case the appropriate transfer
mode is selected.  See also the Ascii and Binary functions.  Returns
C<undef> on errors.

Example:

    print "Current mode is: ", $FTP->Mode();
    $FTP->Mode("asc"); # ...  same as $FTP->Ascii();

=item Pasv [mode]

If called with no arguments, returns 1 if the current FTP session has
passive transfer mode enabled, 0 if not.

You can call it with a I<mode> parameter (0/1) only as a method of a
Internet object (see C<new>), in which case it will set the default
value for the next C<FTP> objects you create (read: set it before,
because you can't change this value once you opened the FTP session).

Example:

    print "Pasv is: ", $FTP->Pasv();

    $INET->Pasv(1);
    $INET->FTP($FTP,"ftp.activeware.com", "anonymous", "dada\@divinf.it");
    $FTP->Pasv(0); # this will be ignored...

=item Put local, [remote, context]

Upload the I<local> file to the FTP server saving it under the name
I<remote>, which if if omitted is the same name as I<local>.  Returns
C<undef> on error.

I<context> is a number to identify this operation if it is asynchronous.
See C<SetStatusCallback> and C<GetStatusCallback> for more info on
asynchronous operations.

Example:

    $FTP->Put("internet.zip");
    $FTP->Put("d:/users/dada/temp.zip", "/temp/dada.zip");

=item Pwd

Returns the current directory on the FTP server or C<undef> on errors.

Example:

    $path = $FTP->Pwd();

=item Rename oldfile, newfile

=item Ren oldfile, newfile

Renames a file on the FTP remote host.  Returns C<undef> on error.

Example:

    $FTP->Rename("110-i86.zip", "68i-011.zip");

=item Rmdir name

=item Rd name

Removes a directory on the FTP remote host.  Returns C<undef> on error.

Example:

    $FTP->Rmdir("CurrentBuild");

=back

=head2 HTTP Functions

B<General Note>

All methods assume that you have the following lines:

    use Win32::Internet;
    $INET = new Win32::Internet();
    $INET->HTTP($HTTP, "hostname", "username", "password");

somewhere before the method calls; in other words, we assume that you
have an Internet object called $INET and an open HTTP session called
$HTTP.

See C<new> and C<HTTP> for more information.


B<Methods>

=over

=item AddHeader header, [flags]

Adds HTTP request headers to an HTTP request object created with
C<OpenRequest>.  For the possible values of I<flags> refer to the
L<"Microsoft Win32 Internet Functions"> document.

Example:

    $HTTP->OpenRequest($REQUEST,"/index.html");
    $REQUEST->AddHeader("If-Modified-Since:  Sunday, 17-Nov-96 11:40:03 GMT");
    $REQUEST->AddHeader("Accept: text/html\r\n", HTTP_ADDREQ_FLAG_REPLACE);

=item OpenRequest requestobject, [path, method, version, referer, accept, flags, context]

=item OpenRequest requestobject, hashref

Opens an HTTP request.  Returns C<undef> on errors or a number if the
connection was succesful.  You can then use one of the C<AddHeader>,
C<SendRequest>, C<QueryInfo>, C<QueryDataAvailable> and C<ReadFile>
methods on the newly created I<requestobject>.  The parameters and
their values are:

=over

=item * path

The object to request.  This is generally a file name, an executable
module, etc.  Default: /

=item * method

The method to use; can actually be GET, POST, HEAD or PUT.  Default:
GET

=item * version

The HTTP version.  Default: HTTP/1.0

=item * referer

The URL of the document from which the URL in the request was
obtained.  Default: I<none>

=item * accept

A single string with "\0" (ASCII zero) delimited list of content
types accepted.  The string must be terminated by "\0\0".
Default: "text/*\0image/gif\0image/jpeg\0\0"

=item * flags

Additional flags affecting the behavior of the function.  Default:
I<none>

=item * context

A number to identify this operation if it is asynchronous.  See
C<SetStatusCallback> and C<GetStatusCallback> for more info on
asynchronous operations.  Default: I<none>

=back

Refer to the L<"Microsoft Win32 Internet Functions"> documentation for
more details on those parameters.  If you pass I<hashref> (a reference to
an hash array), the following values are taken from the array:

    %hash=(
      "path"        => "path",
      "method"      => "method",
      "version"     => "version",
      "referer"     => "referer",
      "accept"      => "accept",
      "flags"       => flags,
      "context"     => context,
    );

See also C<Request>.

Example:

    $HTTP->OpenRequest($REQUEST, "/index.html");
    $HTTP->OpenRequest($REQUEST, "/index.html", "GET", "HTTP/0.9");

    $params{"path"} = "/index.html";
    $params{"flags"} = "
    $HTTP->OpenRequest($REQUEST, \%params);

=item QueryInfo header, [flags]

Queries information about an HTTP request object created with
C<OpenRequest>.  You can specify an I<header> (for example,
"Content-type") and/or one or more I<flags>.  If you don't specify
I<flags>, HTTP_QUERY_CUSTOM will be used by default; this means that
I<header> should contain a valid HTTP header name.  For the possible
values of I<flags> refer to the L<"Microsoft Win32 Internet
Functions"> document.

Example:

    $HTTP->OpenRequest($REQUEST,"/index.html");
    $statuscode = $REQUEST->QueryInfo("", HTTP_QUERY_STATUS_CODE);
    $headers = $REQUEST->QueryInfo("", HTTP_QUERY_RAW_HEADERS_CRLF); # will get all the headers
    $length = $REQUEST->QueryInfo("Content-length");

=item Request [path, method, version, referer, accept, flags]

=item Request hashref

Performs an HTTP request and returns an array containing the status
code, the headers and the content of the file.  It is a one-step
procedure that makes an C<OpenRequest>, a C<SendRequest>, some
C<QueryInfo>, C<ReadFile> and finally C<Close>.  For a description of
the parameters, see C<OpenRequest>.

Example:

    ($statuscode, $headers, $file) = $HTTP->Request("/index.html");
    ($s, $h, $f) = $HTTP->Request("/index.html", "GET", "HTTP/1.0");

=item SendRequest [postdata]

Send an HTTP request to the destination server.  I<postdata> are any
optional data to send immediately after the request header; this is
generally used for POST or PUT requests.  See also C<OpenRequest>.

Example:

    $HTTP->OpenRequest($REQUEST, "/index.html");
    $REQUEST->SendRequest();

    # A POST request...
    $HTTP->OpenRequest($REQUEST, "/cgi-bin/somescript.pl", "POST");

    #This line is a must -> (thanks Philip :)
    $REQUEST->AddHeader("Content-Type: application/x-www-form-urlencoded");

    $REQUEST->SendRequest("key1=value1&key2=value2&key3=value3");

=back


=head1 APPENDIX


=head2 Microsoft Win32 Internet Functions

Complete documentation for the Microsoft Win32 Internet Functions can
be found, in both HTML and zipped Word format, at this address:

    http://www.microsoft.com/intdev/sdk/docs/wininet/

=head2 Functions Table

This table reports the correspondence between the functions offered by
WININET.DLL and their implementation in the Win32::Internet
extension. Functions showing a "---" are not currently
implemented. Functions enclosed in parens ( ) aren't implemented
straightforwardly, but in a higher-level routine, eg. together with
other functions.

    WININET.DLL                     Win32::Internet

    InternetOpen                    new Win32::Internet
    InternetConnect                 FTP / HTTP
    InternetCloseHandle             Close
    InternetQueryOption             QueryOption
    InternetSetOption               SetOption
    InternetSetOptionEx             ---
    InternetSetStatusCallback       SetStatusCallback
    InternetStatusCallback          GetStatusCallback
    InternetConfirmZoneCrossing     ---
    InternetTimeFromSystemTime      TimeConvert
    InternetTimeToSystemTime        TimeConvert
    InternetAttemptConnect          ---
    InternetReadFile                ReadFile
    InternetSetFilePointer          ---
    InternetFindNextFile            (List)
    InternetQueryDataAvailable      QueryDataAvailable
    InternetGetLastResponseInfo     GetResponse
    InternetWriteFile               ---
    InternetCrackUrl                CrackURL
    InternetCreateUrl               CreateURL
    InternetCanonicalizeUrl         CanonicalizeURL
    InternetCombineUrl              CombineURL
    InternetOpenUrl                 OpenURL
    FtpFindFirstFile                (List)
    FtpGetFile                      Get
    FtpPutFile                      Put
    FtpDeleteFile                   Delete
    FtpRenameFile                   Rename
    FtpOpenFile                     ---
    FtpCreateDirectory              Mkdir
    FtpRemoveDirectory              Rmdir
    FtpSetCurrentDirectory          Cd
    FtpGetCurrentDirectory          Pwd
    HttpOpenRequest                 OpenRequest
    HttpAddRequestHeaders           AddHeader
    HttpSendRequest                 SendRequest
    HttpQueryInfo                   QueryInfo
    InternetErrorDlg                ---


Actually, I don't plan to add support for Gopher, Cookie and Cache
functions. I will if there will be consistent requests to do so.

There are a number of higher-level functions in the Win32::Internet
that simplify some usual procedures, calling more that one WININET API
function. This table reports those functions and the relative WININET
functions they use.

    Win32::Internet                 WININET.DLL

    FetchURL                        InternetOpenUrl
                                    InternetQueryDataAvailable
                                    InternetReadFile
                                    InternetCloseHandle

    ReadEntireFile                  InternetQueryDataAvailable
                                    InternetReadFile

    Request                         HttpOpenRequest
                                    HttpSendRequest
                                    HttpQueryInfo
                                    InternetQueryDataAvailable
                                    InternetReadFile
                                    InternetCloseHandle

    List                            FtpFindFirstFile
                                    InternetFindNextFile


=head2 Constants

Those are the constants exported by the package in the main namespace
(eg. you can use them in your scripts); for their meaning and proper
use, refer to the Microsoft Win32 Internet Functions document.

    HTTP_ADDREQ_FLAG_ADD
    HTTP_ADDREQ_FLAG_REPLACE
    HTTP_QUERY_ALLOW
    HTTP_QUERY_CONTENT_DESCRIPTION
    HTTP_QUERY_CONTENT_ID
    HTTP_QUERY_CONTENT_LENGTH
    HTTP_QUERY_CONTENT_TRANSFER_ENCODING
    HTTP_QUERY_CONTENT_TYPE
    HTTP_QUERY_COST
    HTTP_QUERY_CUSTOM
    HTTP_QUERY_DATE
    HTTP_QUERY_DERIVED_FROM
    HTTP_QUERY_EXPIRES
    HTTP_QUERY_FLAG_REQUEST_HEADERS
    HTTP_QUERY_FLAG_SYSTEMTIME
    HTTP_QUERY_LANGUAGE
    HTTP_QUERY_LAST_MODIFIED
    HTTP_QUERY_MESSAGE_ID
    HTTP_QUERY_MIME_VERSION
    HTTP_QUERY_PRAGMA
    HTTP_QUERY_PUBLIC
    HTTP_QUERY_RAW_HEADERS
    HTTP_QUERY_RAW_HEADERS_CRLF
    HTTP_QUERY_REQUEST_METHOD
    HTTP_QUERY_SERVER
    HTTP_QUERY_STATUS_CODE
    HTTP_QUERY_STATUS_TEXT
    HTTP_QUERY_URI
    HTTP_QUERY_USER_AGENT
    HTTP_QUERY_VERSION
    HTTP_QUERY_WWW_LINK
    ICU_BROWSER_MODE
    ICU_DECODE
    ICU_ENCODE_SPACES_ONLY
    ICU_ESCAPE
    ICU_NO_ENCODE
    ICU_NO_META
    ICU_USERNAME
    INTERNET_FLAG_PASSIVE
    INTERNET_FLAG_ASYNC
    INTERNET_FLAG_HYPERLINK
    INTERNET_FLAG_KEEP_CONNECTION
    INTERNET_FLAG_MAKE_PERSISTENT
    INTERNET_FLAG_NO_AUTH
    INTERNET_FLAG_NO_AUTO_REDIRECT
    INTERNET_FLAG_NO_CACHE_WRITE
    INTERNET_FLAG_NO_COOKIES
    INTERNET_FLAG_READ_PREFETCH
    INTERNET_FLAG_RELOAD
    INTERNET_FLAG_RESYNCHRONIZE
    INTERNET_FLAG_TRANSFER_ASCII
    INTERNET_FLAG_TRANSFER_BINARY
    INTERNET_INVALID_PORT_NUMBER
    INTERNET_INVALID_STATUS_CALLBACK
    INTERNET_OPEN_TYPE_DIRECT
    INTERNET_OPEN_TYPE_PROXY
    INTERNET_OPEN_TYPE_PROXY_PRECONFIG
    INTERNET_OPTION_CONNECT_BACKOFF
    INTERNET_OPTION_CONNECT_RETRIES
    INTERNET_OPTION_CONNECT_TIMEOUT
    INTERNET_OPTION_CONTROL_SEND_TIMEOUT
    INTERNET_OPTION_CONTROL_RECEIVE_TIMEOUT
    INTERNET_OPTION_DATA_SEND_TIMEOUT
    INTERNET_OPTION_DATA_RECEIVE_TIMEOUT
    INTERNET_OPTION_HANDLE_TYPE
    INTERNET_OPTION_LISTEN_TIMEOUT
    INTERNET_OPTION_PASSWORD
    INTERNET_OPTION_READ_BUFFER_SIZE
    INTERNET_OPTION_USER_AGENT
    INTERNET_OPTION_USERNAME
    INTERNET_OPTION_VERSION
    INTERNET_OPTION_WRITE_BUFFER_SIZE
    INTERNET_SERVICE_FTP
    INTERNET_SERVICE_GOPHER
    INTERNET_SERVICE_HTTP
    INTERNET_STATUS_CLOSING_CONNECTION
    INTERNET_STATUS_CONNECTED_TO_SERVER
    INTERNET_STATUS_CONNECTING_TO_SERVER
    INTERNET_STATUS_CONNECTION_CLOSED
    INTERNET_STATUS_HANDLE_CLOSING
    INTERNET_STATUS_HANDLE_CREATED
    INTERNET_STATUS_NAME_RESOLVED
    INTERNET_STATUS_RECEIVING_RESPONSE
    INTERNET_STATUS_REDIRECT
    INTERNET_STATUS_REQUEST_COMPLETE
    INTERNET_STATUS_REQUEST_SENT
    INTERNET_STATUS_RESOLVING_NAME
    INTERNET_STATUS_RESPONSE_RECEIVED
    INTERNET_STATUS_SENDING_REQUEST


=head1 VERSION HISTORY

=over

=item * 0.082 (4 Sep 2001)

=over

=item *

Fix passive FTP mode.  INTERNET_FLAG_PASSIVE was misspelled in earlier
versions (as INTERNET_CONNECT_FLAG_PASSIVE) and wouldn't work.  Found
by Steve Raynesford <stever@evolvecomm.com>.

=back

=item * 0.081 (25 Sep 1999)

=over

=item *

Documentation converted to pod format by Jan Dubois <JanD@ActiveState.com>.

=item *

Minor changes from Perl 5.005xx compatibility.

=back

=item * 0.08 (14 Feb 1997)

=over

=item *

fixed 2 more bugs in Option(s) related subs (thanks to Brian
Helterline!).

=item *

Error() now gets error messages directly from WININET.DLL.

=item *

The PLL file now comes in 2 versions, one for Perl version 5.001
(build 100) and one for Perl version 5.003 (build 300 and
higher). Everybody should be happy now :)

=item *

added an installation program.

=back

=item * 0.07 (10 Feb 1997)

=over

=item *

fixed a bug in Version() introduced with 0.06...

=item *

completely reworked PM file, fixed *lots* of minor bugs, and removed
almost all the warnings with "perl -w".

=back

=item * 0.06 (26 Jan 1997)

=over

=item *

fixed another hideous bug in "new" (the 'class' parameter was still
missing).

=item *

added support for asynchronous operations (work still in embryo).

=item *

removed the ending \0 (ASCII zero) from the DLL version returned by
"Version".

=item *

added a lot of constants.

=item *

added safefree() after every safemalloc() in C... wonder why I didn't
it before :)

=item *

added TimeConvert, which actually works one way only.

=back

=item * 0.05f (29 Nov 1996)

=over

=item *

fixed a bug in "new" (parameters passed were simply ignored).

=item *

fixed another bug: "Chdir" and "Cwd" were aliases of RMDIR instead of
CD..

=back

=item * 0.05 (29 Nov 1996)

=over

=item *

added "CrackURL" and "CreateURL".

=item *

corrected an error in TEST.PL (there was a GetUserAgent instead of
UserAgent).

=back

=item * 0.04 (25 Nov 1996)

=over

=item *

added "Version" to retrieve package and DLL versions.

=item *

added proxies and other options to "new".

=item *

changed "OpenRequest" and "Request" to read parameters from a hash.

=item *

added "SetOption/QueryOption" and a lot of relative functions
(connect, username, password, useragent, etc.).

=item *

added "CanonicalizeURL" and "CombineURL".

=item *

"Error" covers a wider spectrum of errors.

=back

=item * 0.02 (18 Nov 1996)

=over

=item *

added support for HTTP sessions and requests.

=back

=item * 0.01 (11 Nov 1996)

=over

=item *

fetching of HTTP, FTP and GOPHER URLs.

=item *

complete set of commands to manage an FTP session.

=back

=back

=head1 AUTHOR

Version 0.08 (14 Feb 1997) by Aldo Calpini <a.calpini@romagiubileo.it>


=head1 CREDITS

Win32::Internet is based on the Win32::Registry code written by Jesse
Dougherty.

Additional thanks to: Carl Tichler for his help in the initial
development; Tore Haraldsen, Brian Helterline for the bugfixes; Dave
Roth for his great source code examples.


=head1 DISCLAIMER

This program is FREE; you can redistribute, modify, disassemble, or
even reverse engineer this software at your will. Keep in mind,
however, that NOTHING IS GUARANTEED to work and everything you do is
AT YOUR OWN RISK - I will not take responsability for any damage, loss
of money and/or health that may arise from the use of this program!

This is distributed under the terms of Larry Wall's Artistic License.
