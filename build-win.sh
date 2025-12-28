#!/bin/bash
# build_windows.sh

echo "Cleaning previous builds..."
rm -rf build-win output/libnacfetch.a
mkdir -p build-win output

cd build-win

echo "Configuring with MinGW..."
cmake .. \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
  -DCMAKE_SHARED_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
  -DBUILD_TESTS=ON

echo "Building..."
cmake --build . -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo "📁 Executables are in: output/"
    echo ""
    echo "To test on Windows:"
    echo "1. Copy main.exe to a Windows machine"
    echo "2. Run it from Command Prompt or PowerShell"
    echo ""
    echo "File sizes:"
    ls -lh output/*.exe
else
    echo "❌ Build failed!"
    exit 1
fi