#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>

#define SERVER_CONFIG "server.cfg"

static ENetHost *client;
static bool connected = false;

static DWORD WINAPI
enet_work(LPVOID lpParam)
{
    ENetEvent event;

    while (connected)
    {
        while (enet_host_service(client, &event, 1000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    printf("%s", (char *) event.packet->data);

                    enet_packet_destroy (event.packet);
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    connected = false;
                } break;
            }
        }
    }

    return 0;
}

static bool
read_server_config(char *ip4, int *port)
{
    bool ok = false;
    FILE *fp = fopen(SERVER_CONFIG, "r");

    if (fp)
    {
        if (fscanf(fp, "%s %d", ip4, port) == 2)
        {
            ok = true;
        }
        else
        {
            printf("Error: could not read file [%s]\n", SERVER_CONFIG);
        }

        fclose(fp);
    }

    return ok;
}

int
main(int argc, char *argv[])
{
    char ip[255] = {0};
    int port = -1;

    if (read_server_config(ip, &port) && enet_initialize() == 0)
    {
        client = enet_host_create (NULL, 1, 2, 0, 0);
        if (client)
        {
            printf ("Starting chat client\n");

            ENetAddress address;
            ENetEvent event;
            ENetPeer *peer;

            enet_address_set_host (&address, ip);
            address.port = port;

            peer = enet_host_connect (client, &address, 2, 0);
            if (peer)
            {
                if (enet_host_service (client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
                {
                    printf ("Connection to [%s:%d] succeeded.\n", ip, port);
                    connected = true;

                    DWORD thread;
                    CreateThread(0, 0, enet_work, 0, 0, &thread);

                    char line[255];
                    while (fgets(line, sizeof(line), stdin))
                    {
                        ENetPacket *packet = enet_packet_create (line, strlen (line) + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send (peer, 0, packet);
                        enet_host_flush(client);

                        memset(line, 0, sizeof(line));
                    }
                }
                else
                {
                    enet_peer_reset (peer);
                    printf ("Connection to [%s:%d] failed.\n", ip, port);
                }
            }
            else
            {
                printf("No available peers for initiating an ENet connection.\n");
            }

            enet_host_destroy(client);
        }
        else
        {
            printf ("An error occurred while trying to create an ENet client host.\n");
        }
    }

    enet_deinitialize();

    return 0;
}


