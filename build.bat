@echo off
echo DMC3 Mod - Build
echo.

where cmake >nul 2>nul || (echo CMake not found. Install it. & pause & exit /b 1)

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A Win32 2>nul || cmake .. -G "Visual Studio 16 2019" -A Win32 2>nul || (echo CMake failed. Check VS install. & cd .. & pause & exit /b 1)

cmake --build . --config Release || (echo Build failed. & cd .. & pause & exit /b 1)

echo.
echo Done. Output in build\out\Release\
echo   injector.exe + mod2006.dll + dmc3_mod.ini
echo.
echo Copy all 3 files next to dmc3se.exe to use.
cd ..
pause
