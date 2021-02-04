#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>
#include <signal.h>

#ifdef __linux__
#include <pthread.h>
#include <string.h>
#endif

#define SERVER_CONFIG "server.cfg"

static ENetHost *client;

#ifdef __linux__
static void *
enet_work (void *unused)
#else
static DWORD WINAPI
enet_work(LPVOID lpParam)
#endif
{
    ENetEvent event;

    while (true)
    {
        if (enet_host_service(client, &event, 1000) > 0)
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
                    printf ("Disconnected from server\n");
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

#ifdef __linux__
static void
stop_work (pthread_t thread)
{
    if (pthread_kill (thread, 0) == 0 &&
        pthread_cancel (thread) == 0 &&
        pthread_join (thread, NULL) == 0)
    {
        /* all ok */
    }
    else
    {
        printf ("Failed to stop worker thread\n");
    }
}
#else
static void
stop_work (HANDLE thread)
{
    printf ("stopping thread\n");
    WaitForSingleObject (thread, 0);
    CloseHandle (thread);
}
#endif

static void
signal_handler (int unused)
{
    fclose (stdin);
}

int
main(int argc, char *argv[])
{
    char ip[255] = {0};
    int port = -1;
    bool connected = false;

    if (signal (SIGINT, signal_handler) != SIG_ERR &&
        read_server_config(ip, &port) &&
        enet_initialize() == 0)
    {
        client = enet_host_create (NULL, 1, 2, 0, 0);
        if (client)
        {
            printf ("Starting chat client. CTRL-D or CTRL-C to quit.\n");

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
                    connected = true;
                    printf ("Connection to [%s:%d] succeeded.\n", ip, port);

                    /* Create a worker thread to handle incoming server messages */
#ifdef __linux__
                    pthread_t thread;
                    pthread_create(&thread, NULL, enet_work, NULL);
#else
                    DWORD thread_id;
                    HANDLE thread = CreateThread(0, 0, enet_work, 0, 0, &thread_id);
#endif

                    /* Block and read from input */
                    char line[255];
                    while (fgets(line, sizeof(line), stdin))
                    {
                        if (strncmp (line, "quit", 4) == 0)
                        {
                            break;
                        }

                        if (line[0] != '\n')
                        {
                            ENetPacket *packet = enet_packet_create (line, strlen (line) + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send (peer, 0, packet);
                            enet_host_flush(client);
                        }

                        memset(line, 0, sizeof(line));
                    }

                    /* Request disconnect from server */
                    enet_peer_disconnect (peer, 0);

                    /* Allow up to 3 seconds for the disconnect to succeed
                     * and drop any received packets. */
                    while (connected && enet_host_service (client, &event, 3000) > 0)
                    {
                        switch (event.type)
                        {
                            case ENET_EVENT_TYPE_RECEIVE:
                                enet_packet_destroy (event.packet);
                                break;
                            case ENET_EVENT_TYPE_DISCONNECT:
                                connected = false;
                                break;
                        }
                    }

                    if (connected)
                    {
                        /* We've arrived here, so the disconnect attempt didn't
                         * succeed yet.  Force the connection down. */
                        enet_peer_reset (peer);
                    }

                    stop_work (thread);
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


