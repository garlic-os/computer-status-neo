$args = 'r12ithelp', { whoami }
$ErrorActionPreference = "Stop"

$ComputerName = $args[0]
$Command = $args[1]

$OutputFile = "comp-stat-neo-test.log"
$OutputPathLocal = "C:\$OutputFile"


Write-Output "-ScriptBlock {$Command} *> $OutputPathLocal"
