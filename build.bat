@echo off
echo ImGui projesi derleniyor...

REM Build klasorunu temizle
if exist build rmdir /s /q build

REM Conan dependencies yukle
echo Conan dependencies yukleniyor...
conan install . --output-folder=build --build=missing
if errorlevel 1 (
    echo Conan install basarisiz!
    pause
    exit /b 1
)

REM CMake configure
echo CMake configure ediliyor...
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo CMake configure basarisiz!
    pause
    exit /b 1
)

REM Build
echo Proje derleniyor...
cmake --build . --config Release
if errorlevel 1 (
    echo Build basarisiz!
    pause
    exit /b 1
)

echo Derleme tamamlandi!
echo Calistirmak icin: build\Release\ImGuiProject.exe
pause
