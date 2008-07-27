package Win32::Process;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);

$VERSION = '0.10';

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	CREATE_DEFAULT_ERROR_MODE
	CREATE_NEW_CONSOLE
	CREATE_NEW_PROCESS_GROUP
	CREATE_NO_WINDOW
	CREATE_SEPARATE_WOW_VDM
	CREATE_SUSPENDED
	CREATE_UNICODE_ENVIRONMENT
	DEBUG_ONLY_THIS_PROCESS
	DEBUG_PROCESS
	DETACHED_PROCESS
	HIGH_PRIORITY_CLASS
	IDLE_PRIORITY_CLASS
	INFINITE
	NORMAL_PRIORITY_CLASS
	REALTIME_PRIORITY_CLASS
	THREAD_PRIORITY_ABOVE_NORMAL
	THREAD_PRIORITY_BELOW_NORMAL
	THREAD_PRIORITY_ERROR_RETURN
	THREAD_PRIORITY_HIGHEST
	THREAD_PRIORITY_IDLE
	THREAD_PRIORITY_LOWEST
	THREAD_PRIORITY_NORMAL
	THREAD_PRIORITY_TIME_CRITICAL
);

@EXPORT_OK = qw(
	STILL_ACTIVE
);

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    local $! = 0;
    my $val = constant($constname);
    if ($! != 0) {
        my ($pack,$file,$line) = caller;
        die "Your vendor has not defined Win32::Process macro $constname, used at $file line $line.";
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
} # end AUTOLOAD

bootstrap Win32::Process;

# Win32::IPC > 1.02 uses the get_Win32_IPC_HANDLE method:
*get_Win32_IPC_HANDLE = \&get_process_handle;

1;
__END__

=head1 NAME

Win32::Process - Create and manipulate processes.

=head1 SYNOPSIS

    use Win32::Process;
    use Win32;

    sub ErrorReport{
	print Win32::FormatMessage( Win32::GetLastError() );
    }

    Win32::Process::Create($ProcessObj,
				"C:\\winnt\\system32\\notepad.exe",
				"notepad temp.txt",
				0,
				NORMAL_PRIORITY_CLASS,
				".")|| die ErrorReport();

    $ProcessObj->Suspend();
    $ProcessObj->Resume();
    $ProcessObj->Wait(INFINITE);

=head1  DESCRIPTION

This module provides access to the process control functions in the
Win32 API.

=head1 METHODS

=over 8

=item Win32::Process::Create($obj,$appname,$cmdline,$iflags,$cflags,$curdir)

Creates a new process.

    Args:

	$obj		container for process object
	$appname	full path name of executable module
	$cmdline	command line args
	$iflags		flag: inherit calling processes handles or not
	$cflags		flags for creation (see exported vars below)
	$curdir		working dir of new process

Returns non-zero on success, 0 on failure.

=item Win32::Process::Open($obj,$pid,$iflags)

Creates a handle Perl can use to an existing process as identified by $pid.
The $iflags is the inherit flag that is passed to OpenProcess.  Currently
Win32::Process objects created using Win32::Process::Open cannot Suspend
or Resume the process.  All other calls should work.

Win32::Process::Open returns non-zero on success, 0 on failure.

=item Win32::Process::KillProcess($pid, $exitcode)

Terminates any process identified by $pid.  $exitcode will be set to
the exit code of the process.

=item $ProcessObj->Suspend()

Suspend the process associated with the $ProcessObj.

=item $ProcessObj->Resume()

Resume a suspended process.

=item $ProcessObj->Kill($exitcode)

Kill the associated process, have it terminate with exit code $ExitCode.

=item $ProcessObj->GetPriorityClass($class)

Get the priority class of the process.

=item $ProcessObj->SetPriorityClass($class)

Set the priority class of the process (see exported values below for
options).

=item $ProcessObj->GetProcessAffinityMask($processAffinityMask, $systemAffinityMask)

Get the process affinity mask.  This is a bitvector in which each bit
represents the processors that a process is allowed to run on.

=item $ProcessObj->SetProcessAffinityMask($processAffinityMask)

Set the process affinity mask.  Only available on Windows NT.

=item $ProcessObj->GetExitCode($exitcode)

Retrieve the exitcode of the process. Will return STILL_ACTIVE if the
process is still running. The STILL_ACTIVE constant is only exported
by explicit request.

=item $ProcessObj->Wait($timeout)

Wait for the process to die.  $timeout should be specified in milliseconds.
To wait forever, specify the constant C<INFINITE>.

=item $ProcessObj->GetProcessID()

Returns the Process ID.

=item Win32::Process::GetCurrentProcessID()

Returns the current process ID, which is the same as $$. But not on cygwin,
where $$ is the cygwin-internal PID and not the windows PID.
On cygwin GetCurrentProcessID() returns the windows PID as needed for all
the Win32::Process functions.

=back

=head1 EXPORTS

The following constants are exported by default:

	CREATE_DEFAULT_ERROR_MODE
	CREATE_NEW_CONSOLE
	CREATE_NEW_PROCESS_GROUP
	CREATE_NO_WINDOW
	CREATE_SEPARATE_WOW_VDM
	CREATE_SUSPENDED
	CREATE_UNICODE_ENVIRONMENT
	DEBUG_ONLY_THIS_PROCESS
	DEBUG_PROCESS
	DETACHED_PROCESS
	HIGH_PRIORITY_CLASS
	IDLE_PRIORITY_CLASS
	INFINITE
	NORMAL_PRIORITY_CLASS
	REALTIME_PRIORITY_CLASS
	THREAD_PRIORITY_ABOVE_NORMAL
	THREAD_PRIORITY_BELOW_NORMAL
	THREAD_PRIORITY_ERROR_RETURN
	THREAD_PRIORITY_HIGHEST
	THREAD_PRIORITY_IDLE
	THREAD_PRIORITY_LOWEST
	THREAD_PRIORITY_NORMAL
	THREAD_PRIORITY_TIME_CRITICAL

The following additional constants are exported by request only:

	STILL_ACTIVE

=cut

# Local Variables:
# tmtrack-file-task: "Win32::Process"
# End:
