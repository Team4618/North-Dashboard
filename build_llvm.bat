@echo off
pushd source

IF NOT EXIST "../compiler" (echo directory "../compiler" not found, please unzip "../compiler.zip" && pause && exit)

set clang_exe=..\compiler\host\windows\bin\clang.exe
set clang_params=-c -target x86_64-pc-windows-msvc -nostdlibinc -isystem "../compiler/host/windows/lib/clang/8.0.0/include" -isystem "../compiler/target/windows/includes/vc_tools_msvc" -isystem "../compiler/target/windows/includes/shared" -isystem "../compiler/target/windows/includes/ucrt" -isystem "../compiler/target/windows/includes/um" -Wno-nonportable-include-path 
set lld_link_exe=..\compiler\host\windows\bin\lld-link.exe
set lld_link_params=-libpath:"../compiler/target/windows/libs/vc_tools_msvc" -libpath:"../compiler/target/windows/libs/ucrt/x64" -libpath:"../compiler/target/windows/libs/um/x64" kernel32.lib libcmt.lib

%clang_exe% %clang_params% -Wno-writable-strings -Wno-null-dereference -Wno-switch -O0 -g -gcodeview dashboard_win32.cpp
%lld_link_exe% %lld_link_params% -debug -out:dashboard_win32.exe dashboard_win32.o user32.lib gdi32.lib opengl32.lib ws2_32.lib shell32.lib
copy dashboard_win32.exe "../build/dashboard_win32.exe"
copy dashboard_win32.pdb "../build/dashboard_win32.pdb"
del *.o *.obj *.pdb *.ilk *.exe

popd
pause
exit