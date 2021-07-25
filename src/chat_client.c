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
    uint32_t volatile completion_goal;
    uint32_t volatile completion_count;

    uint32_t volatile next_entry_to_write;
    uint32_t volatile next_entry_to_read;

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
    uint32_t new_next_entry_to_write = (queue->next_entry_to_write + 1) % array_len (queue->entries);
    assert (new_next_entry_to_write != queue->next_entry_to_read);

    struct entry *entry = &queue->entries[queue->next_entry_to_write];
    snprintf (entry->data, sizeof (entry->data), "%s", data);
    ++queue->completion_goal;

#ifdef __linux__
    asm volatile ("" ::: "memory");
#else
    _WriteBarrier ();
#endif

    queue->next_entry_to_write = new_next_entry_to_write;
}

static bool
dequeue (struct queue *queue, char *buf, size_t len)
{
    bool ok = false;
    uint32_t original_next_entry_to_read = queue->next_entry_to_read;
    uint32_t new_next_entry_to_read = (original_next_entry_to_read + 1) % array_len (queue->entries);

    if (original_next_entry_to_read != queue->next_entry_to_write)
    {
#ifdef __linux__
        uint32_t index = __sync_val_compare_and_swap (&queue->next_entry_to_read,
                                                      original_next_entry_to_read,
                                                      new_next_entry_to_read);
#else
        uint32_t index = InterlockedCompareExchange ((LONG volatile *) &queue->next_entry_to_read,
                                                    new_next_entry_to_read,
                                                    original_next_entry_to_read);
#endif
        if (index == original_next_entry_to_read)
        {
            struct entry *entry = &queue->entries[index];
            snprintf (buf, len, "%s", entry->data);

#ifdef __linux__
            __sync_fetch_and_add (&queue->completion_count, 1);
#else
            InterlockedIncrement ((LONG volatile *) &queue->completion_count);
#endif
            ok = true;
        }
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

    DEBUG ("sending disconnect to server\n");
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
                DEBUG ("disconnect successful\n");
                connected = false;
                break;
        }
    }

    if (connected)
    {
        DEBUG ("still connected - killing connection\n");

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
            DEBUG ("received event=%d\n", event.type);

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

        while (info->send_queue.completion_goal != info->send_queue.completion_count)
        {
            DEBUG ("goal=%u, count=%u\n", info->send_queue.completion_goal, info->send_queue.completion_count);

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

        info->send_queue.completion_count = 0;
        info->send_queue.completion_goal = 0;
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
        ERROR ("thread not running\n");
    }
    else if (sem_post (&info->stop_semaphore) != 0)
    {
        ERROR ("failed to signal semaphore\n");
    }
    else if (pthread_join (info->thread, NULL) != 0)
    {
        ERROR ("failed to join thread\n");
    }
    else
    {
        DEBUG ("stopped worker thread\n");
    }
#else
    if (SetEvent (info->stop_event) != 0)
    {
        if (WaitForSingleObject (info->thread, 1000) != WAIT_OBJECT_0)
        {
            ERROR ("thread took too long to exit. Killing\n");
            TerminateThread (info->thread, 0);
        }

        DEBUG ("thread finished. closing handles\n");

        CloseHandle (info->thread);
        CloseHandle (info->stop_event);
    }
    else
    {
        ERROR ("Failed to set event\n");
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
                    ERROR ("Connection to [%s:%d] failed.\n", ip, port);
                }
            }
            else
            {
                ERROR ("No available peers for initiating an ENet connection.\n");
            }
            enet_host_destroy(client);
        }
        else
        {
            ERROR ("An error occurred while trying to create an ENet client host.\n");
        }
    }
    enet_deinitialize();

    return 0;
}


