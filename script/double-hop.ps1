$ErrorActionPreference = "Stop";

$ComputerName = $args[0];
# $Command = $args[1];
$Command = "Invoke-Item -Path \\winprint.mst.edu\ece-mfp-01";

$OutputFile = "comp-stat-neo-test.log";
$OutputPathLocal = "C:\$OutputFile";
$OutputPathRemote = "\\$ComputerName\c$\$OutputFile";

$TASK_ACTION_EXEC = 0;
$TASK_CREATE_OR_UPDATE = 6;
$TASK_LOGON_PASSWORD = 1;
$TASK_RUN_AS_SELF = 1;
$TASK_RUN_IGNORE_CONSTRAINTS = 2;
$TASK_STATE_RUNNING = 4;

$User = (whoami);
$TaskName = "Computer Status Neo";

Write-Output "Task Scheduler: Creating new task...";
$Scheduler = New-Object -ComObject Schedule.Service;
$Task = $Scheduler.NewTask(0);

Write-Output "Task Scheduler: Configuring task with the given command...";
$Task.Principal.RunLevel = 1;
$RegistrationInfo = $Task.RegistrationInfo;
$RegistrationInfo.Description = $TaskName;
$RegistrationInfo.Author = $User.Name;
$Settings = $Task.Settings;
$Settings.Enabled = $True;
$Settings.StartWhenAvailable = $True;
$Settings.DisallowStartIfOnBatteries = $False;
$Settings.StopIfGoingOnBatteries = $False;
$Settings.WakeToRun = $True;
$Settings.Hidden = $False;
$Action = $Task.Actions.Create($TASK_ACTION_EXEC);
$Action.Path = "powershell";
$Action.Arguments = "-Command `"& {$Command}`" *> $OutputPathLocal";

Write-Output $Action.Arguments

Write-Output "Task Scheduler: Connecting to $ComputerName...";
$Scheduler.Connect($ComputerName);

Write-Output "Task Scheduler: Registering task on remote computer...";
$RootFolder = $Scheduler.GetFolder("\");
$RootFolder.RegisterTaskDefinition(
    $TaskName,
    $Task,
    $TASK_CREATE_OR_UPDATE,
    "SYSTEM",
    $Null,
    $TASK_LOGON_PASSWORD
) | Out-Null;

# Create the output file or clear it on the off-chance that it exists
Write-Output "" > $OutputPathRemote;

Write-Output "Task Scheduler: Triggering task...";
$RegisteredTask = $RootFolder.GetTask($TaskName);
$RegisteredTask.RunEx(
    $Null,
    1,
    $Null,
    $Null
);

# Set the remote computer to print the task's output in realtime.
# Running as a job to allow this script to keep running at the same time.
$OutputWatcherJob = Start-Job -ScriptBlock {
    Get-Content -Wait $Using:OutputPathRemote;
};

# Poll for output as long as the command is running.
while ($RegisteredTask.State -eq $TASK_STATE_RUNNING) {
    $Output = Receive-Job $OutputWatcherJob.ChildJobs[0] -Keep; 
    if ($Output.Length -gt 0) {
        Write-Output $Output;
    };
};

# Output doesn't always come in instantly
timeout 1 | Out-Null;

Receive-Job $OutputWatcherJob.ChildJobs[0];
Stop-Job $OutputWatcherJob;
Remove-Job $OutputWatcherJob;
