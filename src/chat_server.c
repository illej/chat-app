#include <stdio.h>
#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include "chat.h"

#define INET_ADDRSTRLEN 16

static volatile sig_atomic_t G__running = false;

static void
signal_handler (int signal)
{
    if (signal == SIGINT)
    {
        G__running = false;
    }
}

static void
usage (void)
{
    printf ("Usage:\n\n");
    printf ("  server <port>\n\n");
}

int
main(int argc, char *argv[])
{
    if (argc != 2)
    {
        usage ();
        return -1;
    }

    int port = atoi (argv[1]);

    if (signal (SIGINT, signal_handler) != SIG_ERR &&
        enet_initialize() == 0)
    {
        ENetHost *server;

        ENetAddress address;

        /* Bind the server to the default localhost.     */
        /* A specific host address can be specified by   */
        /* enet_address_set_host (& address, "x.x.x.x"); */
        address.host = ENET_HOST_ANY;

        /* Bind the server to port 1234. */
        address.port = port;
        server = enet_host_create (&address, /* the address to bind the server host to */
                                   32,       /* allow up to 32 clients and/or outgoing connections */
                                   2,        /* allow up to 2 channels to be used, 0 and 1 */
                                   0,        /* assume any amount of incoming bandwidth */
                                   0);       /* assume any amount of outgoing bandwidth */
        if (server)
        {
            G__running = true;
            printf ("Starting chat server [%d]\n", port);

            while (G__running)
            {
                ENetEvent event;
                int ret = enet_host_service (server, &event, 100);
                // printf ("service ret: %d\n", ret);
                if (ret > 0)
                {
                    ENetPacket *packet;
                    char buf[256] = {0};

                    // printf ("event type: %d channel: %u\n", event.type, event.channelID);
                    switch (event.type)
                    {
                        case ENET_EVENT_TYPE_CONNECT:
                        {
                            char ip[INET_ADDRSTRLEN];

                            htop (event.peer->address.host, event.peer->address.port, ip, sizeof (ip));

                            printf ("A new client connected [%s] inid=%u outid=%d \n",
                                    ip, event.peer->incomingPeerID, event.peer->outgoingPeerID);

                            // event.peer->data = strdup (ip);
                        } break;
                        case ENET_EVENT_TYPE_RECEIVE:
                        {
                            char *ip = (char *) event.peer->data;
                            uint8_t *data = (uint8_t *) event.packet->data;
                            char str[256] = {0};

                            // printf ("pkt> dataLength        : %zu\n", event.packet->dataLength);
                            // printf ("pkt> struct packet len : %zu\n", sizeof (struct packet));

                            if (event.packet->dataLength == sizeof (struct packet))
                            {
                                struct packet *p = (struct packet *) data;

                                // printf ("pkt> type  : %d\n", pkt->type);
                                // printf ("pkt> data  : %s\n", pkt->data);
                                // printf ("pkt> len   : %zu\n", pkt->len);

                                switch (p->type)
                                {
                                    case PACKET_TYPE_INIT:
                                    {
                                        event.peer->data = strdup (p->data);
                                    } break;
                                    case PACKET_TYPE_CONTENT:
                                    {
                                        snprintf (str, sizeof (str), "%s", p->data);
                                    } break;
                                    default:
                                    {
                                        printf ("Unkown packet type: %d\n", p->type);
                                    } break;
                                }
                            }

                            if (ip && ip[0] != '\0' &&
                                str && str[0] != '\0')
                            {
                                printf("[%s]: %s (ping=%u)\n", ip, str, event.peer->roundTripTime);

                                if (strncmp (str, "!who", 4) == 0)
                                {
                                    snprintf (buf, sizeof (buf), "[SERVER] connected peers: %zu/%zu",
                                              server->connectedPeers, server->peerCount);
                                }
                                else
                                {
                                    snprintf (buf, sizeof (buf), "[%s]: %s", ip, str);
                                }
                            }

                            if (buf[0] != '\0')
                            {
                                packet = enet_packet_create (buf, strlen (buf) + 1,
                                                             ENET_PACKET_FLAG_RELIABLE);
                                enet_host_broadcast (server, 1, packet);
                                enet_host_flush (server);

                                enet_packet_destroy (event.packet);
                            }
                        } break;
                        case ENET_EVENT_TYPE_DISCONNECT:
                        {
#if 0
typedef struct _ENetPeer
{
   ENetListNode  dispatchList;
   struct _ENetHost * host;
   enet_uint16   outgoingPeerID;
   enet_uint16   incomingPeerID;
   enet_uint32   connectID;
   enet_uint8    outgoingSessionID;
   enet_uint8    incomingSessionID;
   ENetAddress   address;            /**< Internet address of the peer */
   void *        data;               /**< Application private data, may be freely modified */
   ENetPeerState state;
   ENetChannel * channels;
   size_t        channelCount;       /**< Number of channels allocated for communication with peer */
   enet_uint32   incomingBandwidth;  /**< Downstream bandwidth of the client in bytes/second */
   enet_uint32   outgoingBandwidth;  /**< Upstream bandwidth of the client in bytes/second */
   enet_uint32   incomingBandwidthThrottleEpoch;
   enet_uint32   outgoingBandwidthThrottleEpoch;
   enet_uint32   incomingDataTotal;
   enet_uint32   outgoingDataTotal;
   enet_uint32   lastSendTime;
   enet_uint32   lastReceiveTime;
   enet_uint32   nextTimeout;
   enet_uint32   earliestTimeout;
   enet_uint32   packetLossEpoch;
   enet_uint32   packetsSent;
   enet_uint32   packetsLost;
   enet_uint32   packetLoss;          /**< mean packet loss of reliable packets as a ratio with respect to the constant ENET_PEER_PACKET_LOSS_SCALE */
   enet_uint32   packetLossVariance;
   enet_uint32   packetThrottle;
   enet_uint32   packetThrottleLimit;
   enet_uint32   packetThrottleCounter;
   enet_uint32   packetThrottleEpoch;
   enet_uint32   packetThrottleAcceleration;
   enet_uint32   packetThrottleDeceleration;
   enet_uint32   packetThrottleInterval;
   enet_uint32   pingInterval;
   enet_uint32   timeoutLimit;
   enet_uint32   timeoutMinimum;
   enet_uint32   timeoutMaximum;
   enet_uint32   lastRoundTripTime;
   enet_uint32   lowestRoundTripTime;
   enet_uint32   lastRoundTripTimeVariance;
   enet_uint32   highestRoundTripTimeVariance;
   enet_uint32   roundTripTime;            /**< mean round trip time (RTT), in milliseconds, between sending a reliable packet and receiving its acknowledgement */
   enet_uint32   roundTripTimeVariance;
   enet_uint32   mtu;
   enet_uint32   windowSize;
   enet_uint32   reliableDataInTransit;
   enet_uint16   outgoingReliableSequenceNumber;
   ENetList      acknowledgements;
   ENetList      sentReliableCommands;
   ENetList      sentUnreliableCommands;
   ENetList      outgoingReliableCommands;
   ENetList      outgoingUnreliableCommands;
   ENetList      dispatchedCommands;
   enet_uint16   flags;
   enet_uint8    roundTripTimeRemainder;
   enet_uint8    roundTripTimeVarianceRemainder;
   enet_uint16   incomingUnsequencedGroup;
   enet_uint16   outgoingUnsequencedGroup;
   enet_uint32   unsequencedWindow [ENET_PEER_UNSEQUENCED_WINDOW_SIZE / 32];
   enet_uint32   eventData;
   size_t        totalWaitingData;
} ENetPeer;

typedef enum _ENetPeerState
{
   ENET_PEER_STATE_DISCONNECTED                = 0,
   ENET_PEER_STATE_CONNECTING                  = 1,
   ENET_PEER_STATE_ACKNOWLEDGING_CONNECT       = 2,
   ENET_PEER_STATE_CONNECTION_PENDING          = 3,
   ENET_PEER_STATE_CONNECTION_SUCCEEDED        = 4,
   ENET_PEER_STATE_CONNECTED                   = 5,
   ENET_PEER_STATE_DISCONNECT_LATER            = 6,
   ENET_PEER_STATE_DISCONNECTING               = 7,
   ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT    = 8,
   ENET_PEER_STATE_ZOMBIE                      = 9
} ENetPeerState;

#endif
                            if (event.peer->data)
                            {
                                char buf[INET_ADDRSTRLEN];

                                printf ("%s disconnected (ip=%s, state=%d, inpeerid=%u outpeerid=%u connid=%u insess=%u outsess=%u).\n",
                                        (char *) event.peer->data,
                                        htop (event.peer->address.host, event.peer->address.port, buf, sizeof (buf)),
                                        event.peer->state,
                                        event.peer->incomingPeerID,
                                        event.peer->outgoingPeerID,
                                        event.peer->connectID,
                                        event.peer->incomingSessionID,
                                        event.peer->outgoingSessionID);

                                free (event.peer->data);
                                event.peer->data = NULL;
                            }
                            else
                            {
                                printf ("%s disconnected.\n", "???");
                            }
                        } break;
                        default:
                        {
                            printf ("unknown event type: %d\n", event.type);
                        } break;
                    }
                }
                else if (ret < 0)
                {
                    printf ("An error occured trying to pump messages\n");
                }
            }

            printf ("exiting\n");
            // TODO: disconnect all peers

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
