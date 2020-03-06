mkdir cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_CXX_FLAGS="-fsanitize=undefined,address -O1 -g  -fno-omit-frame-pointer --coverage" .. && cmake --build ./ -- -j 8
