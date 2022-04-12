# libxiv
[![sourcehut](https://img.shields.io/badge/repository-sourcehut-lightgrey.svg?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZlcnNpb249IjEuMSINCiAgICB3aWR0aD0iMTI4IiBoZWlnaHQ9IjEyOCI+DQogIDxkZWZzPg0KICAgIDxmaWx0ZXIgaWQ9InNoYWRvdyIgeD0iLTEwJSIgeT0iLTEwJSIgd2lkdGg9IjEyNSUiIGhlaWdodD0iMTI1JSI+DQogICAgICA8ZmVEcm9wU2hhZG93IGR4PSIwIiBkeT0iMCIgc3RkRGV2aWF0aW9uPSIxLjUiDQogICAgICAgIGZsb29kLWNvbG9yPSJibGFjayIgLz4NCiAgICA8L2ZpbHRlcj4NCiAgICA8ZmlsdGVyIGlkPSJ0ZXh0LXNoYWRvdyIgeD0iLTEwJSIgeT0iLTEwJSIgd2lkdGg9IjEyNSUiIGhlaWdodD0iMTI1JSI+DQogICAgICA8ZmVEcm9wU2hhZG93IGR4PSIwIiBkeT0iMCIgc3RkRGV2aWF0aW9uPSIxLjUiDQogICAgICAgIGZsb29kLWNvbG9yPSIjQUFBIiAvPg0KICAgIDwvZmlsdGVyPg0KICA8L2RlZnM+DQogIDxjaXJjbGUgY3g9IjUwJSIgY3k9IjUwJSIgcj0iMzglIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjQlIg0KICAgIGZpbGw9Im5vbmUiIGZpbHRlcj0idXJsKCNzaGFkb3cpIiAvPg0KICA8Y2lyY2xlIGN4PSI1MCUiIGN5PSI1MCUiIHI9IjM4JSIgc3Ryb2tlPSJ3aGl0ZSIgc3Ryb2tlLXdpZHRoPSI0JSINCiAgICBmaWxsPSJub25lIiBmaWx0ZXI9InVybCgjc2hhZG93KSIgLz4NCjwvc3ZnPg0KCg==)](https://git.sr.ht/~redstrate/libxiv)
[![GitHub
mirror](https://img.shields.io/badge/mirror-GitHub-black.svg?logo=github)](https://github.com/redstrate/libxiv)
[![ryne.moe
mirror](https://img.shields.io/badge/mirror-ryne.moe-red.svg?logo=git)](https://git.ryne.moe/redstrate/libxiv)

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