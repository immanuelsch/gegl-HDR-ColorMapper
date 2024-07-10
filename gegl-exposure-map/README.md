The blur-sepia GEGL plug-in
===========================


Sample for GEGL plug-in tutorial.

You can run it from the GIMP GEGL Graph plug-in like this:

```
blur-sepia
```

Or you can use it as a GEGL Operation, in GIMP or elsewhere.

It takes two optional parameters: blur-radius and sepia-strength.

# Installation

There is a Linux binary, but ideally you should recompile it.

## Where to Put GEGL Filter binaries 

### Windows

```
C:\\Users\alfonso\AppData\Local\gegl-0.4\plug-ins
```
Change `alfonso` to your Windows username. You will need a .dll file, not the .so file.
 
### Linux 

```
/home/alfonso/.local/share/gegl-0.4/plug-ins/
```
Again, change alfonso to your username, or use $HOME or `~` instead of `/home/alfonso`

If you installed GIMP via a flatpak, you will need to find the `gegl-0.4` folder (or 0.5 maybe, in the future). A good place to look is:

```
$HOME/.var/app/org.gimp.GIMP/data/gegl-0.4/plug-ins
```


## Compiling and Installing

### Linux and Unix

To compile and install you will need the GEGL header files (`libgegl-dev` on
Debian based distributions or `gegl` on Arch Linux) and meson (`meson` on
most distributions). You will also need `pango`, `cairo` and `pangocairo` and their respective development packages.

```bash
SRC_DIR=$(pwd)
BUILD_DIR=${SRC_DIR}/obj-$(arch)
mkdir -p $BUILD_DIR && cd $BUILD_DIR && meson -Dprefix=$PREFIX --buildtype=release $SRC_DIR && ninja && ninja install
```
This will create a .o file in the obj-x86_64 folder (it might have a different name depending on your CPU type and operating system).

If you have an older version of gegl you may need to copy to `~/.local/share/gegl-0.3/plug-ins`
instead



### Windows

The easiest way to compile this project on Windows is by using msys2.  Download
and install it from here: https://www.msys2.org/

Open a msys2 terminal with `C:\msys64\mingw64.exe`.  Run the following to
install required build dependencies:

```bash
pacman --noconfirm -S base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-meson mingw-w64-x86_64-gegl
```

Then build the same way you would on Linux:

```bash
SRC_DIR=$(pwd)
BUILD_DIR=${SRC_DIR}/obj-$(arch)
mkdir -p $BUILD_DIR && cd $BUILD_DIR && meson -Dprefix=$PREFIX --buildtype=release $SRC_DIR && ninja
```

Then copy the DLL file into this folder:
```
C:\\Users\<YOUR NAME>\AppData\Local\gegl-0.4\plug-ins
```

### Macos

I do not have access to a Mac; you will likely need to follow the Linux instructions as best you can, and then mark the resulting dynamic library (the .so file) as not needing signing,

```bash
sudo xattr ~/Library/Application\ Support/gegl/0.4/plug-ins/pango-markup.so
```

## Acknowledgements

See more GEGL plug-ins at https://github.com/LinuxBeaver?tab=repositories
Beaver’s requests for help and subsequent interstellar progress prompted me to write this article.

