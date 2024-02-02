@rem Based upon the msvcbuild.bat of Mike Pall

@rem Script to build Teascript with MSVC.
@rem
@rem Open a "Visual Studio Command Prompt" (either x86 or x64).
@rem Then cd to this directory and run this script. Use the following
@rem options (in order), if needed. The default is a dynamic release build.
@rem
@rem   debug    emit debug symbols
@rem   onetea   single file build
@rem   static   static linkage

@if not defined INCLUDE goto :FAIL

@setlocal
@rem Add more debug flags here
@set DEBUGCFLAGS=
@set TEACOMPILE=cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline
@set TEALINK=link /nologo
@set TEAMT=mt /nologo
@set TEALIB=lib /nologo /nodefaultlib
@set TEADLLNAME=tea0.dll
@set TEALIBNAME=tea0.lib
@set BUILDTYPE=release

@if "%1" neq "debug" goto :NODEBUG
@shift
@set BUILDTYPE=debug
@set TEACOMPILE=%TEACOMPILE% /Zi %DEBUGCFLAGS%
@set TEALINK=%TEALINK% /opt:ref /opt:icf /incremental:no
:NODEBUG
@set TEALINK=%TEALINK% /%BUILDTYPE%
@if "%1"=="onetea" goto :ONETEADLL
@if "%1"=="static" goto :STATIC
%TEACOMPILE% /MD /DTEA_BUILD_AS_DLL tea_*.c lib_*.c
@if errorlevel 1 goto :BAD
%TEALINK% /DLL /out:%TEADLLNAME% tea_*.obj lib_*.obj
@if errorlevel 1 goto :BAD
@goto :MTDLL
:STATIC
%TEACOMPILE% tea_*.c
@if errorlevel 1 goto :BAD
%TEALIB% /OUT:%TEALIBNAME% tea_*.obj lib_*.obj
@if errorlevel 1 goto :BAD
@goto :MTDLL
:ONETEADLL
%TEACOMPILE% /MD /DTEA_BUILD_AS_DLL onetea.c
@if errorlevel 1 goto :BAD
%TEALINK% /DLL /out:%TEADLLNAME% onetea.obj
@if errorlevel 1 goto :BAD
:MTDLL
if exist %TEADLLNAME%.manifest^
  %TEAMT% -manifest %TEADLLNAME%.manifest -outputresource:%TEADLLNAME%;2

%TEACOMPILE% tea.c
@if errorlevel 1 goto :BAD
%TEALINK% /out:tea.exe tea.obj %TEALIBNAME%
@if errorlevel 1 goto :BAD
if exist tea.exe.manifest^
  %TEAMT% -manifest tea.exe.manifest -outputresource:tea.exe

del *.obj *.manifest
@echo.
@echo === Successfully built Teascript for Windows ===

@goto :END
:BAD
@echo.
@echo *******************************************************
@echo *** Build FAILED -- Please check the error messages ***
@echo *******************************************************
@goto :END
:FAIL
@echo You must open a "Visual Studio Command Prompt" to run this script
:END
