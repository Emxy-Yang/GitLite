rm -rf build
rm -rf test/.gitlite
cmake -B build
cmake --build build --target gitlite

