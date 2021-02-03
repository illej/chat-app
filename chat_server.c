#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void
htop(unsigned int ip4, unsigned short port, char *buf, size_t len)
{
    uint8_t *octets = (uint8_t *) &ip4;
    snprintf(buf, len, "%u.%u.%u.%u:%u",
             octets[0], octets[1], octets[2], octets[3], port);
}

int
main(int argc, char *argv[])
{
    if (enet_initialize() == 0)
    {
        ENetHost *server;

        ENetAddress address;

        /* Bind the server to the default localhost.     */
        /* A specific host address can be specified by   */
        /* enet_address_set_host (& address, "x.x.x.x"); */
        address.host = ENET_HOST_ANY;

        /* Bind the server to port 1234. */
        address.port = 1337;
        server = enet_host_create (&address /* the address to bind the server host to */,
                             32      /* allow up to 32 clients and/or outgoing connections */,
                              2      /* allow up to 2 channels to be used, 0 and 1 */,
                              0      /* assume any amount of incoming bandwidth */,
                              0      /* assume any amount of outgoing bandwidth */);
        if (server)
        {
            printf ("Starting chat server\n");

            ENetEvent event;

            while (1)
            {
                if (enet_host_service (server, &event, 1000) > 0)
                {
                    ENetPacket *packet;
                    char buffer[255];

                    switch (event.type)
                    {
                        case ENET_EVENT_TYPE_CONNECT:
                        {

                            char str[255];

                            htop(event.peer->address.host, event.peer->address.port, str, sizeof(str));

                            printf ("A new client connected [%s].\n", str);

                            event.peer->data = strdup(str);
                        } break;
                        case ENET_EVENT_TYPE_RECEIVE:
                        {
                            printf("[%s]: %s", (char *) event.peer->data, (char *) event.packet->data);

                            snprintf(buffer, sizeof(buffer), "[%s]: %s",
                                    (char *) event.peer->data,
                                    (char *) event.packet->data);

                            packet = enet_packet_create(buffer, strlen(buffer) + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_host_broadcast(server, 0, packet);
                            enet_host_flush(server);

                            enet_packet_destroy (event.packet);
                        } break;
                        case ENET_EVENT_TYPE_DISCONNECT:
                        {
                            printf ("%s disconnected.\n", (char *) event.peer->data);

                            event.peer->data = NULL;
                        } break;
                    }
                }
            }

            enet_host_destroy(server);
        }
        else
        {
            printf ("An error occurred while trying to create an ENet server host.\n");
        }
    }

    enet_deinitialize();

    return 0;
}