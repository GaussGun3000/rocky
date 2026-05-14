cd ..
set PATH=%CD%\install\bin;%PATH%
set VCPKG_DIR=%CD%\build\vcpkg_installed
set PATH=%VCPKG_DIR%\x64-windows\bin;%PATH%
set PATH=%VCPKG_DIR%\x64-windows\plugins;%PATH%
set PATH=%VCPKG_DIR%\x64-windows\tools\proj;%PATH%
set GDAL_DATA=%VCPKG_DIR%\x64-windows\share\gdal
set PROJ_DATA=%VCPKG_DIR%\x64-windows\share\proj
