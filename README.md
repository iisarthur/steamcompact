# Steam Compact

Alternative optimized Steam launcher for Windows

## Download
[Download latest release for Windows](https://codeberg.org/uuiz/steamcompact/releases/download/latest/SteamCompact.exe)

## Features

- Game Overlay Toggle
- High CPU priority
- Closing Steam after exiting game
- Killing SteamWebHelper (saves around 600MB of RAM)

## Compile

You can compile from source using MSVS

```bash
rc.exe resource.rc
cl.exe /utf-8 /EHsc /O2 /std:c++17 main.cpp resource.res /link /SUBSYSTEM:WINDOWS
```
