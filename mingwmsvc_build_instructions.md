# mingw-w64 / MSVC Build Instructions

## build environment:

### MSYS
* available by the MinGW Installation Manager
* http://sourceforge.net/projects/mingw/files/latest/download?source=files
* in msys\1.0\etc create a file 'fstab' and configure it according to fstab.sample to point to your mingw bin path
```
#Win32_Path		Mount_Point
c:/mingw		/mingw
```

### autotools
* available by the MinGW Installation Manager

### pkg-config
* Download:
  * http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip
  * http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip
  * http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip
* Extract to: ``/usr/local`` [``C:\MinGW\msys\1.0\local``]

### Mingw-w64 for 64bit build
* to compile 64bit install Mingw-w64 http://sourceforge.net/projects/mingw-w64/
* add both MinGW/bin and mingw64/bin to Path environment variable (if not availabe in msys already, or figure what to copy or where else to get msys and autotools for mingw64)
* to work along with MinGW MSYS and autotools, make sure to remove conflicting dlls and executables

### Verify MSYS build environment
```
which x86_64-w64-mingw32-gcc
which x86_64-w64-mingw32-g++
which automake
which autoconf
which libtool
which make
which pkg-config
```

## meshlink dependencies:

### zlib
included with mingw-w64 install
to build zlib on yourself (-fPIC required) however proceed with:
```
cd zlib-1.2.8
export "DESTDIR=/c/lib/zlib"
export "INCLUDE_PATH=/c/lib/zlib/include"
export "LIBRARY_PATH=/c/lib/zlib/lib"
export "BINARY_PATH=/c/lib/zlib/bin"
make -f win32/Makefile.gcc CFLAGS='-fPIC'
make install -f win32/Makefile.gcc
```


## autoreconf:
-f/--force   force to remake configure script<br/>
-s/--symlink install symbolic links to the missing auxiliary files instead of copying them<br/>
-i/--install install the missing auxiliary files in the package<br/>
-Wnone       no warnings
```
catta/autoreconf -fsiWnone
autoreconf -fsiWnone
```

## configure:
CFLAGS:
-g for debug symbols<br/>
-O0 for optimization level 0<br/>
-fPIC to generate position-independent code and if supported avoid any limit on the size of the global offset table<br/>
-fstack-protector-all add guards to check for buffer overflows, protect all functions<br/>
-std=c99 use c99 standard (be aware, this doesn't check on missing c99 format flag support with printf in Msvcrt.dll Microsoft C-Runtime Library)<br/>

--with-zlib-include=${ZLIB_INCLUDE_DIR}<br/>
--with-zlib-lib=${ZLIB_LIBRARY_DIR}<br/>
--prefix=[INSTALL_DIR]

Debug:
```
catta/configure CFLAGS='-fPIC -fstack-protector-all -std=c99 -g -O0' --prefix='/c/lib/catta'
configure CFLAGS='-fPIC -fstack-protector-all -std=c99 -g -O0' --prefix='/c/lib/meshlink' --with-zlib-lib=/c/lib/zlib/lib
```
Release:
```
catta/configure CFLAGS='-fPIC -fstack-protector-all -std=c99 -O3' --prefix='/c/lib/catta'
configure CFLAGS='-fPIC -fstack-protector-all -std=c99 -O3' --prefix='/c/lib/meshlink' --with-zlib-lib=/c/lib/zlib/lib
```

## build:
```
catta/make
make
```


## install:
```
catta/make install
make install
```
just a note: libtool, used by the autotools, doesn't allow to link static libraries to dynamic ones<br/>
even when build with -fPIC and perfectly legal it insists to link dependencies dynamicly or aborts the build<br/>
to circumvent this, it now builds passing -lz in LDFLAGS<br/>
however another approach I found is to just make a static build and convert to dll + def afterwards:<br/>
``gcc -shared -Wl,--whole-archive,--kill-at,--output-def=libmeshlink-0.def libmeshlink.a -Wl,--no-whole-archive -L../../catta/src/.libs -L/c/lib/zlib/lib -lpthread -liphlpapi -lssp -lws2_32 -lgdi32 -lcatta.dll -lz -o libmeshlink-0.dll``


## MSVC compability
make sure you have the C++ compiler package installed and VC folder of Microsoft Visual Studio is added to your PATH environment variable

### generate msvc import library for meshlink + catta (using cmd shell)
```
vcvarsall amd64
cd meshlink/catta/src/.libs
lib /machine:x64 /def:libcatta-0.def
cd meshlink/src/.libs
lib /machine:x64 /def:libmeshlink-0.def
```

### MSVC project setup
copy libcatta-0.lib and libmeshlink-0.lib to lib install folders<br/>
for usage don't forget to copy all the library dependencies to your exe path
```
libmeshlink-0.dll
libcatta-0.dll
libgcc_s_seh-1.dll
libssp-0.dll
libwinpthread-1.dll
```
Include Directories: ``C:\lib\catta\include;C:\lib\meshlink\include;``<br/>
Library Directories: ``C:\lib\catta\lib;C:\lib\meshlink\lib;``<br/>
Linker.Input Additional Dependencies: ``libmeshlink-0.lib;libcatta-0.lib``<br/>
