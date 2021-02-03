@echo off

set ext_include_dir=external/include
set ext_lib_dir=external/lib

set compiler_flags=-nologo /I %ext_include_dir%
set link_flags=/LIBPATH:%ext_lib_dir%
set libs=ws2_32.lib winmm.lib enet64.lib

cl %compiler_flags% chat_server.c /link %link_flags% %libs%
cl %compiler_flags% chat_client.c /link %link_flags% %libs%