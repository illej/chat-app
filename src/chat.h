#ifndef _CHAT_PROTOCOL_H_
#define _CHAT_PROTOCOL_H_

#define debug(msg, ...) if (g__debug) fprintf (stdout, msg, ## __VA_ARGS__);
#define error(msg, ...) fprintf (stderr, msg, ## __VA_ARGS__)
#define array_len(A) (sizeof ((A)) / sizeof ((A)[0]))
#define assert(expr) if (!(expr)) { error ("Failure at %s():%d\n", __func__, __LINE__); *(int *) 0 = 0; }

enum packet_type
{
    PACKET_TYPE_INIT = 0,
    PACKET_TYPE_CONTENT,

    PACKET_TYPE_MAX,
};

struct packet
{
    enum packet_type type;
    char data[256];
    size_t len;
};

static char *
htop (unsigned int ip4, unsigned short port, char *buf, size_t len)
{
    uint8_t *octets = (uint8_t *) &ip4;

    snprintf (buf, len, "%u.%u.%u.%u:%u",
              octets[0], octets[1], octets[2], octets[3], port);

    return buf;
}

#endif /* _CHAT_PROTOCOL_H_ */
