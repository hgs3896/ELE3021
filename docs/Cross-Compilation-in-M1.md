# Cross-Compilation in M1

Since Apple’s new chip, M1, appears very recently, there are few virtual machine softwares supporting M1.  
Therefore, I struggled to find a way to cross-compile xv6 binaries in order to emulate xv6 without any virtualization softwares except `qemu`.

Here is how I can cross-compile a xv6-public( https://github.com/mit-pdos/xv6-public ).

0.  Git clone the **xv6-public** repo.

```
git clone https://github.com/mit-pdos/xv6-public
```

1.  Install **Brew** Package Manager (unless you install it)

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

2.  Install QEMU

```
brew install qemu
```

3.  Install gnu cross compiler toolchains for Intel x86\_64 binary

```
brew install x86_64-elf-gcc
brew install x86_64-elf-gdb
brew install x86_64-elf-binutils
```

4.  Modify **Makefile** to use the cross-compiler you installed on your m1

```
# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf
```

\[AS IS\]

```
# Cross-compiling (e.g., on Mac OS X)
ifeq ($(shell uname), Darwin) # it only works when in OSX!
 TOOLPREFIX = x86_64-elf-
endif
```

\[TO BE\]

```
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
```

\[AS IS\]

```
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer -Wno-error=stringop-overflow
```

\[TO BE\]

5.  Enjoy your xv6 programming.

```
make CPUS=1 qemu-nox
```

I cannot guarantee the behavior of cross-compiled xv6 is exactly same as that of ubuntu 18.04’s, so you have to check it by yourself.

Thanks for reading.