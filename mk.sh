rm -rf build
cmake -B build
cmake --build build --target gitlite
cd test
