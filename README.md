<div align="center">
# Steam Compact
</div>

<div align="center">
  Alternative lightweight Steam launcher
</div>

## Compile from source

You can compile from source using MSVS

```bash
rc.exe resource.rc
cl.exe /utf-8 /EHsc /O2 /std:c++17 main.cpp resource.res /link /SUBSYSTEM:WINDOWS
```
