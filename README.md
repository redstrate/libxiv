# libxiv
A modding framework for FFXIV written in C++. This is used in [Novus](https://git.sr.ht/~redstrate/novus) (my custom modding tool) and
[Astra](https://git.sr.ht/~redstrate/astra) (my custom launcher) but can easily be integrated into your own projects.

**Note:** This is still an experimental and in-development library. Thus, I have not tagged any stable releases. It's recommended just to checkout from main.

## Goals
* Easily integratable into other FFXIV launchers so they can have update/install support without having to write it themselves.
* Can export Penumbra/Lumina format mods, I have no interest in exporting in TexTools's format.
* Can export/edit some formats such as models, and metadata/exl files.
* Can be used on Windows/Linux/macOS and doesn't pull in a huge runtime (C#) or run in Wine.

## Features
* Easily extract game files and view excel sheets by name. See [gamedata.h](include/gamedata.h) for usage.
* Install patches (right now it's limited to boot patches). See [patch.h](include/patch.h) for usage.
* Install FFXIV by emulating the official installer, bypassing Wine and InstallShield. You can see how to use this in [installextract.h](include/installextract.h).
* Parse some game data:
  * [EXD](include/exdparser.h)
  * [EXH](include/exhparser.h)
  * [EXL](include/exlparser.h)
  * [FIIN](include/fiinparser.h)
  * [INDEX/INDEX2](include/indexparser.h)
  * [MDL](include/mdlparser.h)

## Dependencies
**Note:** Some of these dependencies will automatically be downloaded from the Internet if not found
on your system.

* [fmt](https://fmt.dev/latest/index.html) for formatting strings and logs.
* [zlib](https://www.zlib.net) for extracting data.
* [unshield](https://github.com/twogood/unshield) for extracting cab files from the FFXIV installer. This dependency is automatically skipped on Windows builds
because unshield lacks a port for it.
