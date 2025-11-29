rm -rf build
cmake -B build
cmake --build build --target gitlite
./build/gitlite init
cd .gitlite
cat HEAD
cd refs/heads
cat master