@echo off
setlocal

pushd %~dp0

rem you can set the COVDIR variable to your coverity path
if exist ..\..\cov-analysis-win64-8.5.0 (
  set "COVDIR=..\..\cov-analysis-win64-8.5.0"
)
if not defined COVDIR if exist ..\..\cov-analysis-win32-8.5.0 (
  set "COVDIR=..\..\cov-analysis-win32-8.5.0"
)
if not defined COVDIR set "COVDIR=C:\cov-analysis"
if defined COVDIR if not exist "%COVDIR%" (
  echo.
  echo ERROR: Coverity not found in "%COVDIR%"
  goto End
)

:cleanup
if exist "cov-int" rd /q /s "cov-int"
if exist "BowPad.lzma" del "BowPad.lzma"
if exist "BowPad.tar"  del "BowPad.tar"
if exist "BowPad.tgz"  del "BowPad.tgz"


:main
call "%VS140COMNTOOLS%\vsvars32.bat"
if %ERRORLEVEL% neq 0 (
  echo vsvars32.bat call failed.
  goto End
)

rem the actual coverity command
title "%COVDIR%\bin\cov-build.exe" --dir "cov-int" nant -buildfile:../default.build BowPad
"%COVDIR%\bin\cov-build.exe" --dir "cov-int" nant -buildfile:../default.build BowPad


:tar
rem try the tar tool in case it's in PATH
set PATH=C:\MSYS\bin;%PATH%
tar --version 1>&2 2>nul || (echo. & echo ERROR: tar not found & goto SevenZip)
title Creating "BowPad.lzma"...
tar caf "BowPad.lzma" "cov-int"
if exist cov-upload.tmpl (
  SubWCRev .. cov-upload.tmpl cov-upload.bat
)
goto End


:SevenZip
call :SubDetectSevenzipPath

rem Coverity is totally bogus with lzma...
rem And since I cannot replicate the arguments with 7-Zip, just use tar/gzip.
if exist "%SEVENZIP%" (
  title Creating "BowPad.tar"...
  "%SEVENZIP%" a -ttar "BowPad.tar" "cov-int"
  "%SEVENZIP%" a -tgzip "BowPad.tgz" "BowPad.tar"
  if exist "BowPad.tar" del "BowPad.tar"
  if exist cov-upload.tmpl (
    SubWCRev .. cov-upload.tmpl cov-upload.bat
  )
  goto End
)


:SubDetectSevenzipPath
for %%g in (7z.exe) do (set "SEVENZIP_PATH=%%~$path:g")
if exist "%SEVENZIP_PATH%" (set "SEVENZIP=%SEVENZIP_PATH%" & exit /b)

for %%g in (7za.exe) do (set "SEVENZIP_PATH=%%~$path:g")
if exist "%SEVENZIP_PATH%" (set "SEVENZIP=%SEVENZIP_PATH%" & exit /b)

for /f "tokens=2*" %%a in (
  'reg query "HKLM\SOFTWARE\7-Zip" /v "path" 2^>nul ^| find "REG_SZ" ^|^|
   reg query "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "path" 2^>nul ^| find "REG_SZ"') do set "SEVENZIP=%%b\7z.exe"
exit /b


:End
popd
echo. & echo Press any key to close this window...
pause >nul
endlocal
exit /b
