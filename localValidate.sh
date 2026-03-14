cmake -B .build/Debug  -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/Debug --target all -j $(nproc)
ctest --test-dir .build/Debug --output-on-failure -j $(nproc)
