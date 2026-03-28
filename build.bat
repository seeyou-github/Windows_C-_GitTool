@echo off
setlocal

echo ===== Build GitVisualTool =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Wall -Wextra
set LDFLAGS=-municode -mwindows -lcomctl32 -lshell32 -lole32 -luuid -ldwmapi -lgdiplus

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\MainWindow.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error

%CXX% -c src\GitRunner.cpp %CXXFLAGS% -o build\GitRunner.o
if errorlevel 1 goto error

%CXX% -c src\CacheDatabase.cpp %CXXFLAGS% -o build\CacheDatabase.o
if errorlevel 1 goto error

%CXX% -c src\Config.cpp %CXXFLAGS% -o build\Config.o
if errorlevel 1 goto error

%CXX% -c src\ProjectStore.cpp %CXXFLAGS% -o build\ProjectStore.o
if errorlevel 1 goto error

%CXX% -c src\CommitRepository.cpp %CXXFLAGS% -o build\CommitRepository.o
if errorlevel 1 goto error

%CXX% -c src\DarkTheme.cpp %CXXFLAGS% -o build\DarkTheme.o
if errorlevel 1 goto error

%RC% res\app_resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\MainWindow.o build\GitRunner.o build\CacheDatabase.o build\Config.o build\ProjectStore.o build\CommitRepository.o build\DarkTheme.o build\resource.o -o build\GitVisualTool.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\GitVisualTool.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
