package Win32::Job;

use strict;
use base qw(DynaLoader);
use vars qw($VERSION);

$VERSION = '0.01';

use constant WIN32s => 0;
use constant WIN9X  => 1;
use constant WINNT  => 2;

require Win32 unless defined &Win32::GetOSVersion;
my @ver = Win32::GetOSVersion;
die "Win32::Job is not supported on $ver[0]" unless (
    $ver[4] == WINNT and (
	$ver[1] > 5 or
	($ver[1] == 5 and $ver[2] > 0) or
	($ver[1] == 5 and $ver[2] == 0 and $ver[3] >= 0)
    )
);

Win32::Job->bootstrap($VERSION);

1;

__END__

=head1 NAME

Win32::Job - Run sub-processes in a "job" environment

=head1 SYNOPSIS

   use Win32::Job;
   
   my $job = Win32::Job->new;

   # Run 'perl Makefile.PL' for 10 seconds
   $job->spawn($Config{perlpath}, "perl Makefile.PL");
   $job->run(10);

=head1 PLATFORMS

Win32::Job requires Windows 2000 or later. Windows 95, 98, NT, and Me are not
supported.

=head1 DESCRIPTION

Windows 2000 introduced the concept of a "job": a collection of processes
which can be controlled as a single unit. For example, you can reliably kill a
process and all of its children by launching the process in a job, then
telling Windows to kill all processes in the job.  Win32::Job makes this
feature available to Perl.

For example, imagine you want to allow 2 minutes for a process to complete.
If you have control over the child process, you can probably just run it in
the background, then poll every second to see if it has finished.

That's fine as long as the child process doesn't spawn any child processes.
What if it does? If you wrote the child process yourself and made an effort to
clean up your child processes before terminating, you don't have to worry.
If not, you will leave hanging processes (called "zombie" processes in Unix).

With Win32::Job, just create a new Job, then use the job to spawn the child
process. All I<its> children will also be created in the new Job. When you
time out, just call the job's kill() method and the entire process tree will
be terminated.

=head1 Using Win32::Job

The following methods are available:

=over 4

=item 1

new()

   new();

Creates a new Job object using the Win32 API call CreateJobObject(). The job
is created with a default security context, and is unnamed.

Note: this method returns C<undef> if CreateJobObject() fails. Look at C<$^E>
for more detailed error information.

=item 2

spawn()

   spawn($exe, $args, \%opts);

Creates a new process and associates it with the Job. The process is initially
suspended, and can be resumed with one of the other methods. Uses the Win32
API call CreateProcess(). Returns the PID of the newly created process.

Note: this method returns C<undef> if CreateProcess() fails. See C<$^E> for
more detailed error information. One reason this will fail is if the calling
process is itself part of a job, and the job's security context does not allow
child processes to be created in a different job context than the parent.

The arguments are described here:

=over 4

=item 1

$exe

The executable program to run. This may be C<undef>, in which case the first
argument in $args is the program to run.

If this has path information in it, it is used "as is" and passed to
CreateProcess(), which interprets it as either an absolute path, or a
path relative to the current drive and directory. If you did not specify an
extension, ".exe" is assumed.

If there are no path separators (either backslashes or forward slashes),
then Win32::Job will search the current directory and your PATH, looking
for the file. In addition, if you did not specify an extension, then
Win32::Job checks ".exe", ".com", and ".bat" in order. If it finds a ".bat"
file, Win32::Job will actually call F<cmd.exe> and prepend "cmd.exe" to the
$args.

For example, assuming a fairly normal PATH:

   spawn(q{c:\winnt\system\cmd.exe}, q{cmd /C "echo %PATH%"})
   exefile: c:\winnt\system\cmd.exe
   cmdline: cmd /C "echo %PATH%"

   spawn("cmd.exe", q{cmd /C "echo %PATH%"})
   exefile: c:\winnt\system\cmd.exe
   cmdline: cmd /C "echo %PATH%"

=item 2

$args

The commandline to pass to the executable program. The first word will be
C<argv[0]> to an EXE file, so you should repeat the command name in $args.

For example:

   $job->spawn($Config{perlpath}, "perl foo.pl");

In this case, the "perl" is ignored, since perl.exe doesn't use it.

=item 3

%opts

A hash reference for advanced options. This parameter is optional.
the following keys are recognized:

=over 4

=item cwd

A string specifying the current directory of the new process.

By default, the process shares the parent's current directory, C<.>.

=item new_console

A boolean; if true, the process is started in a new console window.

By default, the process shares the parent's console. This has no effect on GUI
programs which do not interact with the console.

=item window_attr

Either C<minimized>, which displays the new window minimized; C<maximimzed>,
which shows the new window maximized; or C<hidden>, which does not display the
new window.

By default, the window is displayed using its application's defaults.

=item new_group

A boolean; if true, the process is the root of a new process group. This
process group includes all descendents of the child.

By default, the process is in the parent's process group (but in a new job).

=item no_window

A boolean; if true, the process is run without a console window. This flag is
only valid when starting a console application, otherwise it is ignored. If you
are launching a GUI application, use the C<window_attr> tag instead.

By default, the process shares its parent's console.

=item stdin

An open input filehandle, or the name of an existing file. The resulting
filehandle will be used for the child's standard input handle.

By default, the child process shares the parent's standard input.

=item stdout

An open output filehandle or filename (will be opened for append). The
resulting filehandle will be used for the child's standard output handle.

By default, the child process shares the parent's standard output.

=item stderr

An open output filehandle or filename (will be opened for append). The
resulting filehandle will be used for the child's standard error handle.

By default, the child process shares the parent's standard error.

=back

Unrecognized keys are ignored.

=back

=item 3

run()

   run($timeout, $which);

Provides a simple way to run the programs with a time limit. The
$timeout is in seconds with millisecond accuracy. This call blocks for
up to $timeout seconds, or until the processes finish.

The $which parameter specifies whether to wait for I<all> processes to
complete within the $timeout, or whether to wait for I<any> process to
complete. You should set this to a boolean, where a true value means to
wait for I<all> the processes to complete, and a false value to wait
for I<any>. If you do not specify $which, it defaults to true (C<all>).

Returns a boolean indicating whether the processes exited by themselves,
or whether the time expired. A true return value means the processes
exited normally; a false value means one or more processes was killed
will $timeout.

You can get extended information on process exit codes using the
status() method.

For example, this is how to build two perl modules at the same time,
with a 5 minute timeout:

   use Win32::Job;
   $job = Win32::Job->new;
   $job->spawn("cmd", q{cmd /C "cd Mod1 && nmake"});
   $job->spawn("cmd", q{cmd /C "cd Mod2 && nmake"});
   $ok = $job->run(5 * 60);
   print "Mod1 and Mod2 built ok!\n" if $ok;

=item 4

watch()

   watch(\&handler, $interval, $which);

   handler($job);

Provides more fine-grained control over how to stop the programs.  You specify
a callback and an interval in seconds, and Win32::Job will call the "watchdog"
function at this interval, either until the processes finish or your watchdog
tells Win32::Job to stop. You must return a value indicating whether to stop: a
true value means to terminate the processes immediately.

The $which parameter has the same meaning as run()'s.

Returns a boolean with the same meaning as run()'s.

The handler may do anything it wants. One useful application of the watch()
method is to check the filesize of the output file, and terminate the Job if
the file becomes larger than a certain limit:

   use Win32::Job;
   $job = Win32::Job->new;
   $job->spawn("cmd", q{cmd /C "cd Mod1 && nmake"}, {
       stdin  => 'NUL', # the NUL device
       stdout => 'stdout.log',
       stderr => 'stdout.log',
   });
   $ok = $job->watch(sub {
       return 1 if -s "stdout.log" > 1_000_000;
   }, 1);
   print "Mod1 built ok!\n" if $ok;

=item 5

status()

   status()

Returns a hash containing information about the processes in the job.
Only returns valid information I<after> calling either run() or watch();
returns an empty hash if you have not yet called them. May be called from a
watch() callback, in which case the C<exitcode> field should be ignored.

The keys of the hash are the process IDs; the values are a subhash
containing the following keys:

=over 4

=item exitcode

The exit code returned by the process. If the process was killed because
of a timeout, the value is 293.

=item time

The time accumulated by the process. This is yet another subhash containing
the subkeys (i) C<user>, the amount of time the process spent in user
space; (ii) C<kernel>, the amount of time the process spent in kernel space;
and (iii) C<elapsed>, the total time the process was running.

=back

=item 6

kill()

   kill();

Kills all processes and subprocesses in the Job. Has no return value.
Sets the exit code to all processes killed to 293, which you can check
for in the status() return value.

=back

=head1 SEE ALSO

For more information about jobs, see Microsoft's online help at

   http://msdn.microsoft.com/

For other modules which do similar things (but not as well), see:

=over 4

=item 1

Win32::Process

Low-level access to creating processes in Win32. See L<Win32::Process>.

=item 2

Win32::Console

Low-level access to consoles in Win32. See L<Win32::Console>.

=item 3

Win32::ProcFarm

Manage pools of threads to perform CPU-intensive tasks on Windows. See
L<Win32::ProcFarm>.

=back

=head1 AUTHOR

ActiveState (support@ActiveState.com)

=head1 COPYRIGHT

Copyright (c) 2002, ActiveState Corporation. All Rights Reserved.

=cut
