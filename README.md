# libhoudini-patch-for-termux-x86_64

## compile your program using in emulator
```
clang --target=aarch64-linux-android \
      -march=armv8-a \
      -I$PREFIX/include \
      -I$PREFIX/include/aarch64-linux-android \
      -L$PREFIX/aarch64-linux-android/lib \
      -static \
      -o add_numbers add_numbers.c
```
## and then run 
```
./main ./add_numbers
```
