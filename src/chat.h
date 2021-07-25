#ifndef _CHAT_PROTOCOL_H_
#define _CHAT_PROTOCOL_H_

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

#endif /* _CHAT_PROTOCOL_H_ */