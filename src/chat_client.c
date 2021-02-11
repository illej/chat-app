#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>
#include <signal.h>

#ifdef __linux__
#include <pthread.h>
#include <string.h>
#endif

/*
 * TODO: maybe we have the IO read in a worker thread, then
 *       when a line is read we put it in a buffer and signal
 *       the main thread (with a semaphore?) to wake up and
 *       send it in a packet
 *
 *       we are currently NOT thread-safe, accessing the client
 *       structure from both threads!!!
 *
 *       definitely feels like we need to be isolating threads,
 *       i.e., one for IO and one for ENet processing
 *
 *       non-blocking read()? and go full immediate-mode?
 * */
#define SERVER_CONFIG "server.cfg"

enum pkt_type
{
    PKT_TYPE_INIT = 0,
    PKT_TYPE_CONTENT,

    PKT_TYPE_MAX,
};

struct packet
{
    enum pkt_type type;
    bool valid;
    char data[256];
    size_t len;
};

enum msg_type
{
    MSG_TYPE_INIT = 0,
    MSG_TYPE_CONTENT,

    MSG_TYPE_MAX,
};

struct thread_info
{
#ifdef __linux__
    pthread_t thread;
#else
    HANDLE thread;
    HANDLE stop_event;
    HANDLE read_event;
#endif
};

static ENetHost *client;

#ifdef __linux__
static void *
enet_work (void *unused)
#else
static DWORD WINAPI
enet_work (LPVOID lpParam)
#endif
{
    struct thread_info *info = (struct thread_info *) lpParam;

    while (WaitForSingleObject (info->stop_event, 0) != WAIT_OBJECT_0)
    {
        ENetEvent event;
        if (enet_host_service (client, &event, 100) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    printf ("%s\n", (char *) event.packet->data);

                    enet_packet_destroy (event.packet);
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf ("Disconnected by server\n");
                } break;
            }
        }
    }

    return 0;
}

static bool
read_server_config (char *ip4, int *port)
{
    bool ok = false;
    FILE *fp = fopen (SERVER_CONFIG, "r");

    if (fp)
    {
        if (fscanf (fp, "%s %d", ip4, port) == 2)
        {
            ok = true;
        }
        else
        {
            printf ("Error: could not read file [%s]\n", SERVER_CONFIG);
        }

        fclose (fp);
    }

    return ok;
}

#ifdef __linux__
static void
stop_work (struct thread_info *info)
{
    if (pthread_kill (info->thread, 0) == 0 &&
        pthread_cancel (info->thread) == 0 &&
        pthread_join (info->thread, NULL) == 0)
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
stop_work (struct thread_info *info)
{
    printf ("stopping thread\n");

    if (SetEvent (info->stop_event) != 0)
    {
        if (WaitForSingleObject (info->thread, 1000) != WAIT_OBJECT_0)
        {
            printf ("thread took too long to exit. Killing\n");
            TerminateThread (info->thread, 0);
        }

        printf ("thread finished. closing handles\n");

        CloseHandle (info->thread);
        CloseHandle (info->stop_event);
    }
    else
    {
        printf ("Failed to set event\n");
    }
}
#endif

static void
signal_handler (int unused)
{
    /* TODO: set a global variable (volatile?) here
     * and then check if outside the while (fgets ())
     * loop */
    fclose (stdin);
}

int
main(int argc, char *argv[])
{
    char ip[255] = {0};
    int port = -1;
    bool connected = false;
    bool init = false;

    // TODO: sigaction instead of signal?
    if (signal (SIGINT, signal_handler) != SIG_ERR &&
        read_server_config(ip, &port) &&
        enet_initialize() == 0)
    {
        // TODO: defines for the lib function params
        client = enet_host_create (NULL, 1, 2, 0, 0);
        if (client)
        {
            printf ("Starting chat client. CTRL-D, CTRL-C, or \"!quit\" to quit.\n");

            ENetAddress address;
            ENetEvent event;
            ENetPeer *peer;

            enet_address_set_host (&address, ip);
            address.port = port;

            peer = enet_host_connect (client, &address, 2, 0);
            if (peer)
            {
                if (enet_host_service (client, &event, 5000) > 0 &&
                    event.type == ENET_EVENT_TYPE_CONNECT)
                {
                    connected = true;
                    printf ("Connection to [%s:%d] succeeded.\n", ip, port);

                    /* Create a worker thread to handle incoming server messages */
                    struct thread_info info = {0};

#ifdef __linux__
                    pthread_create(&info.thread, NULL, enet_work, NULL);
#else
                    DWORD dummy;
                    info.thread = CreateThread(0, 0, enet_work, &info, 0, &dummy);
                    info.stop_event = CreateEventA (NULL, true, false, "Stop event");
#endif

                    // TODO: send INIT packet here

                    /* Block and read from input */
                    char line[255];
                    while (fgets (line, sizeof (line), stdin))
                    {
                        struct packet pkt = {0};

                        if (strncmp (line, "!quit", 5) == 0)
                        {
                            break;
                        }

                        /* trim trailing newline */
                        if (line[0] != '\0')
                        {
                            size_t last = strlen (line) - 1;
                            if (line[last] == '\n')
                            {
                                line[last] = '\0';
                            }
                        }

                        if (!init)
                        {
                            pkt.type = MSG_TYPE_INIT;
                            // TODO: get username from argv
                            snprintf (pkt.data, sizeof (pkt.data), "%s", "Elliot");
                            pkt.len = strlen (pkt.data) + 1;
                            pkt.valid = true;

                            init = true;
                        }
                        else if (line[0] != '\0')
                        {
                            pkt.type = MSG_TYPE_CONTENT;
                            snprintf (pkt.data, sizeof (pkt.data), "%s", line);
                            pkt.len = strlen (pkt.data) + 1;
                            pkt.valid = true;
                        }

                        if (pkt.valid)
                        {
                            ENetPacket *packet = enet_packet_create (&pkt,
                                                                     sizeof (struct packet),
                                                                     ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send (peer, 0, packet);
                        }

                        memset (line, 0, sizeof (line));
                    }

                    /* We've exited out of the I/O read loop
                     * so clean up and quit */
                    stop_work (&info);

                    printf ("sending disconnect to server\n");

                    enet_peer_disconnect (peer, 0);

                    /* Allow up to 3 seconds for the disconnect
                     * to succeed and drop any received packets. */
                    while (connected && enet_host_service (client, &event, 3000) > 0)
                    {
                        switch (event.type)
                        {
                            case ENET_EVENT_TYPE_RECEIVE:
                                enet_packet_destroy (event.packet);
                                break;
                            case ENET_EVENT_TYPE_DISCONNECT:
                                printf ("disconnect successful\n");
                                connected = false;
                                break;
                        }
                    }

                    if (connected)
                    {
                        printf ("still connected - killing connection\n");

                        /* If we've arrived here  the disconnect attempt
                         * didn't succeed yet.  Force the connection down. */
                        enet_peer_reset (peer);
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


