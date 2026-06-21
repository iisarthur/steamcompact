Compile in MSVC x64:
  rc.exe resource.rc
  cl.exe /utf-8 /EHsc /O2 /std:c++17 main.cpp resource.res /link /SUBSYSTEM:WINDOWS
