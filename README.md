# chat-app

Minimal command-line chat application written in C using Enet (http://enet.bespin.org/index.html).

Requires Microsoft's C compiler (MSVC), which can be obtained via the Visual Studio BuildTools (https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=16).

Run setup.bat to download Enet and build the app.

Once built, both server (chat_server.exe) and client (chat_client.exe) can be run from separate prompts on the same machine out of the box (see server.cfg).

NOTE: server runs on windows and linux, but client is windows only for now.
