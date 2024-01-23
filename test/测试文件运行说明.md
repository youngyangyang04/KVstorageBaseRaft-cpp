
## defer_run.cpp
编译命令：g++ defer_run.cpp

注意：如果编译失败提示如下：
```
✗  g++ defer_run.cpp -o test_defer
In file included from defer_run.cpp:6:
defer_run.cpp: In function ‘int main()’:
include/defer.h:27:55: error: missing template arguments before ‘defer_only_places22’
27 | #define _MAKE_DEFER_(line) ExecuteOnScopeExit _CONCAT(defer_only_places, line) = [&]()
|                                                       ^~~~~~~~~~~~~~~~~
include/defer.h:26:23: note: in definition of macro ‘_CONCAT’
26 | #define _CONCAT(a, b) a##b
|                       ^
include/defer.h:30:15: note: in expansion of macro ‘_MAKE_DEFER_’
30 | #define DEFER _MAKE_DEFER_(__LINE__)
|               ^~~~~~~~~~~~
defer_run.cpp:22:5: note: in expansion of macro ‘DEFER’
22 |     DEFER {
|     ^~~~~
```
尝试指定编译的c++标准到17及其以上，比如20。或者升级g++版本，一个可行的版本是
```
✗ g++ -v           
Using built-in specs.
COLLECT_GCC=g++
COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-linux-gnu/11/lto-wrapper
OFFLOAD_TARGET_NAMES=nvptx-none:amdgcn-amdhsa
OFFLOAD_TARGET_DEFAULT=1
Target: x86_64-linux-gnu
Configured with: ../src/configure -v --with-pkgversion='Ubuntu 11.4.0-1ubuntu1~22.04' --with-bugurl=file:///usr/share/doc/gcc-11/README.Bugs --enable-languages=c,ada,c++,go,brig,d,fortran,objc,obj-c++,m2 --prefix=/usr --with-gcc-major-version-only --program-suffix=-11 --program-prefix=x86_64-linux-gnu- --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --enable-bootstrap --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-vtable-verify --enable-plugin --enable-default-pie --with-system-zlib --enable-libphobos-checking=release --with-target-system-zlib=auto --enable-objc-gc=auto --enable-multiarch --disable-werror --enable-cet --with-arch-32=i686 --with-abi=m64 --with-multilib-list=m32,m64,mx32 --enable-multilib --with-tune=generic --enable-offload-targets=nvptx-none=/build/gcc-11-XeT9lY/gcc-11-11.4.0/debian/tmp-nvptx/usr,amdgcn-amdhsa=/build/gcc-11-XeT9lY/gcc-11-11.4.0/debian/tmp-gcn/usr --without-cuda-driver --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --with-build-config=bootstrap-lto-lean --enable-link-serialization=2
Thread model: posix
Supported LTO compression algorithms: zlib zstd
gcc version 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04) 
```