# SqlPipe: command-line SQL Server backup utility #

A command-line utilty that does backups/restores to STDOUT/STDIN.

## Usage ##

    sqlpipe backup|restore "database" [options]
    Options:
      -q Quiet, don't print messages to STDERR
      -i "instancename"

## Examples ##

Basic backup:

    sqlpipe backup AdventureWorks > AdventureWorks.bak

Backup with named intance SQLEXPRESS:

    sqlpipe backup AdventureWorks -i SQLEXPRESS > AdventureWorks.bak

Basic Restore:

    sqlpipe restore AdventureWorks < AdventureWorks.bak

Duplicate a database:

    sqlpipe backup AdventureWorks | sqlpipe restore AdventureWorks_Copy

Backup with gzip compression (assumes 7-Zip's 7z.exe is in `%PATH%`):

    sqlpipe backup AdventureWorks | 7z a -si AdventureWorks.gz

Restore gzip compressed database (assumes 7-Zip's 7z.exe is in `%PATH%`):

    7z e -so AdventureWorks.gz | sqlpipe restore AdventureWorks

## Implementation details ##

Uses SQL Server's Virtual Backup Device Interface as detailed in the [SQL Server 2005 Virtual Backup Device Interface (VDI) Specification](http://www.microsoft.com/downloads/en/details.aspx?familyid=416f8a51-65a3-4e8e-a4c8-adfe15e850fc). It's not a million miles away from the `simple.cpp` example there.