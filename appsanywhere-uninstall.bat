::@echo off
echo Uninstalling AppsAnywhere...
net use R: \\minerfiles.mst.edu\dfs\
perl R:\software\itwindist\applications\SCCM\appsanywheredeploy.1_5_0\remove.pl
