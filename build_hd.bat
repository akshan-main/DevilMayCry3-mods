@echo off
echo DMC3 HD Mod - Build (x64)
echo.

where cmake >nul 2>nul || (echo CMake not found. Install it. & pause & exit /b 1)

if not exist build_hd mkdir build_hd
cd build_hd

cmake .. -G "Visual Studio 17 2022" -A x64 2>nul || cmake .. -G "Visual Studio 16 2019" -A x64 2>nul || (echo CMake failed. Check VS install. & cd .. & pause & exit /b 1)

cmake --build . --config Release || (echo Build failed. & cd .. & pause & exit /b 1)

echo.
echo Done. Output in build_hd\out_hd\Release\
echo   dinput8.dll + modhd.dll + dmc3_mod.ini
echo.
echo Copy all 3 to the DMC3 HD Collection game folder.
cd ..
pause
