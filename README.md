# chat-app

Minimal command-line chat application written in C using ENet (http://enet.bespin.org/index.html).

## Setup
### Windows
Requires Microsoft's C compiler (MSVC), which can be obtained via the Visual Studio BuildTools (https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=16).

Run scripts\setup.bat to download ENet and build the apps, or scripts\build.bat to just recompile.

Once built they can be found in the build directory. Simply run these from the command prompt.

### Linux
Requires GCC

Run scripts/setup.sh to download and install ENet and build the apps (chat.srv and chat.cl), or scripts/build.sh to just recompile.

Once built they can be found in the build directory. However, to run them call scripts/run.sh with "server" or "client" as an argument. This is because the LD_LIBRARY_PATH environment variable needs to be set to find the ENet library.

## Configuration
server.cfg is used by the client app to determine the public IP and port of the server. Configure this as you wish.
