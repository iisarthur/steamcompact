# Steam Compact

Alternative lightweight Steam launcher for Windows

## Compile from source

You can compile from source using MSVS

```bash
rc.exe resource.rc
cl.exe /utf-8 /EHsc /O2 /std:c++17 main.cpp resource.res /link /SUBSYSTEM:WINDOWS
```
