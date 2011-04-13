
"%PROGRAMFILES%\7-Zip\7z.exe" a "C:\Users\duncans\Dropbox\Code\SqlBak backup\%random%.7z" "%~dp0" ^
    -xr!*\bin ^
    -xr!*\obj ^
    -xr!*\.svn ^
    -xr!*\packages ^
    -xr!*.suo ^
    -xr!*\Debug ^
    -xr!*\Release ^
    -xr!*\ipch ^
    -xr!*.sdf
