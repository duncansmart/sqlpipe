# SqlPipe: command-line SQL Server backup utility #

A command-line utilty that does local SQL Server backups to STDOUT/STDIN. 

## Usage ##

    sqlpipe backup "database" [options]
    Options:
      -q Quiet, don't print messages to STDERR
      -i "instancename"
      -f "file" Write/read file instead of STDOUT/STDIN

**NOTE: restore currently does not work**

## Examples ##

Basic backup:

    sqlpipe backup AdventureWorks > AdventureWorks.bak

Backup from named instance SQLEXPRESS:

    sqlpipe backup AdventureWorks -i SQLEXPRESS > AdventureWorks.bak

Backup with gzip compression (assumes 7-Zip's 7z.exe is in `%PATH%`):

    sqlpipe backup AdventureWorks | 7z a -si AdventureWorks.gz

## Remote SQL Servers ##

SqlPipe itself can only connect to a local SQL Server, but by using something like [Sysinternals PSExec](http://technet.microsoft.com/en-us/sysinternals/bb897553) you can execute it remotely like so:

    psexec \\dbserver -cv sqlpipe backup AdventureWorks > C:\Backups\AdventureWorks.bak
    
Note that in this example that `C:\Backups\Inventory.bak` refers to a location on *your* machine not the remote server. Yes, the backup data was piped over the network! 

To write to a remote file locaton use the `-f` option:

    psexec \\dbserver -cv sqlpipe backup AdventureWorks -f C:\Backups\AdventureWorks.bak

Or a central network location:

    psexec \\dbserver -u domain\user -p P@55w0rd -cv sqlpipe backup AdventureWorks -f \\fileserver\Backups\AdventureWorks.bak

## Download ##

See https://github.com/duncansmart/sqlpipe/releases

## Implementation details ##

Uses SQL Server's Virtual Backup Device Interface as detailed in the [SQL Server 2005 Virtual Backup Device Interface (VDI) Specification](http://www.microsoft.com/downloads/en/details.aspx?familyid=416f8a51-65a3-4e8e-a4c8-adfe15e850fc). It's not a million miles away from the `simple.cpp` example there.
