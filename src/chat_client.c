#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

#ifdef __linux__
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#endif

#include "chat.h"

#define SERVER_CONFIG           "server.cfg"
#define OUTGOING_CONNECTION_MAX 1
#define NUM_CHANNELS            2
#define BANDWIDTH_IN            0 /* 0 for unlimited - default */
#define BANDWIDTH_OUT           0 /* 0 for unlimited - default */

#ifdef __linux__
  #define WORK_RET   void *
  #define WORK_PARAM void *
  #define WORK_COND  (sem_trywait (&info->stop_semaphore) != 0 && errno == EAGAIN)
#else
  #define WORK_RET   DWORD WINAPI
  #define WORK_PARAM LPVOID
  #define WORK_COND  (WaitForSingleObject (info->stop_event, 0) != WAIT_OBJECT_0)
#endif

struct entry
{
    char data[128];
};

struct queue
{
    uint32_t volatile read_index;
    uint32_t volatile write_index;

    uint32_t volatile entry_count;
    struct entry entries[32];
};

struct thread_info
{
#ifdef __linux__
    pthread_t thread;
    sem_t stop_semaphore;
#else
    HANDLE thread;
    HANDLE stop_event;
    HANDLE read_event;
#endif

    char name[16];

    ENetHost *client;
    ENetPeer *peer;

    struct queue send_queue;
    int send_channel;
};

static bool g__debug = false;

static void
enqueue (struct queue *queue, char *data)
{
    uint32_t new_write_index = (queue->write_index + 1) % array_len (queue->entries);
    assert (new_write_index != queue->read_index);

    struct entry *entry = &queue->entries[queue->write_index];
    snprintf (entry->data, sizeof (entry->data), "%s", data);

    ++queue->entry_count;
#ifdef __linux__
    asm volatile ("" ::: "memory");
#else
    _WriteBarrier ();
#endif
    queue->write_index = new_write_index;
}

static bool
dequeue (struct queue *queue, char *buf, size_t len)
{
    bool ok = false;
    uint32_t original_read_index = queue->read_index;
    uint32_t new_read_index = (original_read_index + 1) % array_len (queue->entries);

    if (original_read_index != queue->write_index)
    {
        struct entry *entry = &queue->entries[original_read_index];
        snprintf (buf, len, "%s", entry->data);

        --queue->entry_count;
#ifdef __linux__
        asm volatile ("" ::: "memory");
#else
        _WriteBarrier ();
#endif
        queue->read_index = new_read_index;
        ok = true;
    }

    return ok;
}

static void
send_init_packet (ENetPeer *peer, char *name, int channel)
{
    struct packet p = {0};

    p.type = PACKET_TYPE_INIT;
    p.len = strlen (name) + 1;
    snprintf (p.data, sizeof (p.data), "%s", name);

    ENetPacket *packet = enet_packet_create (&p, sizeof (struct packet),
                                             ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send (peer, channel, packet);
}

static void
disconnect(ENetHost *client, ENetPeer *peer)
{
    bool connected = true;

    debug ("sending disconnect to server\n");
    enet_peer_disconnect_later (peer, 0);

    /* Allow up to 3 seconds for the disconnect
    * to succeed and drop any received packets. */
    ENetEvent event;
    while (connected && enet_host_service (client, &event, 3000) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy (event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                debug ("disconnect successful\n");
                connected = false;
                break;
        }
    }

    if (connected)
    {
        debug ("still connected - killing connection\n");

        /* If we've arrived here  the disconnect attempt
         * didn't succeed yet.  Force the connection down. */
        enet_peer_reset (peer);
    }
}

static WORK_RET
enet_work (WORK_PARAM param)
{
    struct thread_info *info = (struct thread_info *) param;

    send_init_packet (info->peer, info->name, info->send_channel);

    while (WORK_COND)
    {
        ENetEvent event;
        if (enet_host_service (info->client, &event, 100) > 0)
        {
            debug ("received event=%d\n", event.type);

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

        while (info->send_queue.entry_count > 0)
        {
            debug ("entry_count=%u (read=%u, write=%u)\n",
                   info->send_queue.entry_count,
                   info->send_queue.read_index,
                   info->send_queue.write_index);

            char message[256];
            if (dequeue (&info->send_queue, message, sizeof (message)))
            {
                struct packet p = {0};

                p.type = PACKET_TYPE_CONTENT;
                p.len = strlen (message) + 1;
                snprintf (p.data, sizeof (p.data), "%s", message);

                ENetPacket *packet = enet_packet_create (&p, sizeof (struct packet),
                                                         ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send (info->peer, info->send_channel, packet);
            }
        }
    }

    disconnect (info->client, info->peer);

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

static void
stop_work (struct thread_info *info)
{
#ifdef __linux__
    if (pthread_kill (info->thread, 0) != 0)
    {
        error ("thread not running\n");
    }
    else if (sem_post (&info->stop_semaphore) != 0)
    {
        error ("failed to signal semaphore\n");
    }
    else if (pthread_join (info->thread, NULL) != 0)
    {
        error ("failed to join thread\n");
    }
    else
    {
        debug ("stopped worker thread\n");
    }
#else
    if (SetEvent (info->stop_event) != 0)
    {
        if (WaitForSingleObject (info->thread, 1000) != WAIT_OBJECT_0)
        {
            error ("thread took too long to exit. Killing\n");
            TerminateThread (info->thread, 0);
        }

        debug ("thread finished. closing handles\n");

        CloseHandle (info->thread);
        CloseHandle (info->stop_event);
    }
    else
    {
        error ("Failed to set event\n");
    }
#endif
}

static void
signal_handler (int unused)
{
    fclose (stdin);
}

static void
usage (void)
{
    printf ("Usage:\n\n");
    printf ("  client NAME [-d]\n\n");
}

int
main(int argc, char *argv[])
{
    char ip[255] = {0};
    int port = -1;

    if (argc < 2)
    {
        usage ();
        return -1;
    }

    char *name = argv[1];
    if (argc == 3 &&
        argv[2] && argv[2][0] != '\0' &&
        strcmp (argv[2], "-d") == 0)
    {
        g__debug = true;
    }
    // TODO: sigaction instead of signal?
    if (signal (SIGINT, signal_handler) != SIG_ERR &&
        read_server_config(ip, &port) &&
        enet_initialize() == 0)
    {
        ENetHost *client = enet_host_create (NULL, OUTGOING_CONNECTION_MAX, NUM_CHANNELS,
                                   BANDWIDTH_IN, BANDWIDTH_OUT);
        if (client)
        {
            printf ("Starting chat client. CTRL-C or \"!quit\" to quit.\n");

            ENetAddress address;
            enet_address_set_host (&address, ip);
            address.port = port;

            ENetPeer *peer = enet_host_connect (client, &address, NUM_CHANNELS, 0);
            if (peer)
            {
                ENetEvent event;
                if (enet_host_service (client, &event, 5000) > 0 &&
                    event.type == ENET_EVENT_TYPE_CONNECT)
                {
                    printf ("Connection to [%s:%d] succeeded.\n", ip, port);

                    struct thread_info info = {0};
                    info.client = client;
                    info.peer = peer;
                    info.send_channel = 0;
                    snprintf (info.name, sizeof (info.name), "%s", name);
#ifdef __linux__
                    sem_init (&info.stop_semaphore, 0, 0);
                    pthread_create(&info.thread, NULL, enet_work, &info);
#else
                    DWORD dummy;
                    info.stop_event = CreateEventA (NULL, true, false, NULL);
                    info.thread = CreateThread(0, 0, enet_work, &info, 0, &dummy);
#endif

                    /* Block and read from input */
                    char line[255];
                    while (fgets (line, sizeof (line), stdin))
                    {
                        if (strncmp (line, "!quit", 5) == 0)
                        {
                            break;
                        }

                        if (line[0] != '\0')
                        {
                            /* trim trailing newline */
                            size_t last = strlen (line) - 1;
                            if (line[last] == '\n')
                            {
                                line[last] = '\0';
                            }
                        }

                        if (line[0] != '\0')
                        {
                            enqueue (&info.send_queue, line);
                        }

                        memset (line, 0, sizeof (line));
                    }

                    /* We've exited out of the I/O read loop
                     * so clean up and quit */
                    stop_work (&info);
                }
                else
                {
                    enet_peer_reset (peer);
                    error ("Connection to [%s:%d] failed.\n", ip, port);
                }
            }
            else
            {
                error ("No available peers for initiating an ENet connection.\n");
            }
            enet_host_destroy(client);
        }
        else
        {
            error ("An error occurred while trying to create an ENet client host.\n");
        }
    }
    enet_deinitialize();

    return 0;
}


