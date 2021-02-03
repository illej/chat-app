# chat-app

Minimal command-line chat application written in C using Enet (http://enet.bespin.org/index.html).

## Setup
### Windows
Requires Microsoft's C compiler (MSVC), which can be obtained via the Visual Studio BuildTools (https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=16).

Run setup.bat to download Enet and build the apps (chat_server.exe and chat_client.exe).

### Linux
Requires GCC and libpthread

Run setup.sh to download Enet and build the apps (chat.srv and chat.cl).

## Running
Once built, both server and client can be run from separate prompts on the same machine out of the box (see server.cfg).
