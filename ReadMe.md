# SqlBak: command-line SQL Server backup utility #

## Usage ##
    sqlbak backup|restore <database> [options]
    Options:
      -q Quiet, don't print messages to STDERR
      -i instancename

## Examples ##
Backup AdventureWorks database:

    sqlbak backup AdventureWorks > AdventureWorks.bak

Restore AdventureWorks:

    sqlbak restore AdventureWorks < AdventureWorks.bak

Duplicate a database:

    sqlbak backup AdventureWorks | sqlbak restore AdventureWorks_Copy

Backup and gzip compress AdventureWorks (assumes 7-Zip's 7z.exe is in `%PATH%`):

    sqlbak backup AdventureWorks | 7z a -si AdventureWorks.gz

Restore gzip compressed AdventureWorks (assumes 7-Zip's 7z.exe is in `%PATH%`):

    7z e -so AdventureWorks.gz | sqlbak restore AdventureWorks
