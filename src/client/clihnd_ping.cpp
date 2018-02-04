/*
 * handler_ping.cpp
 *
 *  
 *
 *  Created on: 2018年2月4日
 *      Author: Hong
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/buffer.h>

#include "common/log.h"
#include "common/header.h"
#include "common/uri.h"
#include "common/cevent.h"
#include "common/packer.h"
#include "client_ping.h"

clihnd_ping g_clihnd_ping;

static void clihnd_ping_conn_readcb (struct bufferevent *bev, void *arg);
static void clihnd_ping_conn_writecb(struct bufferevent *bev, void *arg);
static void clihnd_ping_conn_eventcb(struct bufferevent *bev, short events, void *arg);

void clihnd_ping_init(clihnd_ping* self)
{
    connector* contor = &self->conntor;
    strncpy(contor->name, SERVICE_PING_NAME, MAX_SERVICE_NAME_LEN);
    contor->type = SERVICE_PING;

    contor->server_ip = inet_addr("127.0.0.1");
    contor->server_port = 12345;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = contor->server_ip;
    sin.sin_port = htons(contor->server_port);

    contor->sin = sin;

    contor->handler.readcb  = clihnd_ping_conn_readcb;
    contor->handler.writecb = clihnd_ping_conn_writecb;
    contor->handler.eventcb = clihnd_ping_conn_eventcb;
}


static void clihnd_ping_conn_readcb(struct bufferevent *bev, void *arg)
{
    cli_ping* client = (cli_ping*)arg;
    struct evbuffer *ebuf = bufferevent_get_input(bev);

    int pack_len = 0;
    for (int buf_len = evbuffer_get_length(ebuf); buf_len >= HEADER_SIZE; buf_len -= pack_len)
    {
        evbuffer_copyout(ebuf, &pack_len, sizeof(pack_len));
        if (pack_len > buf_len) {
            ldebug("message incomplete pack_len:%d buf_len:%d", pack_len, buf_len);
            break;
        }

        //TODO:: count package length
        uint8_t* content = NULL;
        if (client->conn->rxpblen < pack_len) {
            content = (uint8_t*)malloc(pack_len);
        } else {
            content = client->conn->rxpb;
        }

        evbuffer_remove(ebuf, content, pack_len);
        message_header* header = (message_header*)content;

        if (0 != message_header_check(header)) {    // 协议异常，断开连接
            svr_ping_release(svr);
            free(client);
            if (client->conn->rxpblen < pack_len) {
                free(content);
            }
            return;
        }

        cli_ping_request_handler(client, content + sizeof(*header), pack_len - sizeof(*header), header->uri);

        if (client->conn->rxpblen < pack_len) {
            free(content);
        }
    }
}

static void clihnd_ping_conn_writecb(struct bufferevent *bev, void *arg)
{
//    struct evbuffer *ebuf = bufferevent_get_output(bev);
//    ldebug("clihnd_ping_conn_writecb");
}

static void clihnd_ping_conn_eventcb(struct bufferevent *bev, short events, void *arg)
{
    cli_ping* client = (cli_ping*)arg;

    ldebug("echosrv_conn_eventcb, events:%u args:%p", events, arg);

    if (events & BEV_EVENT_CONNECTED) {
        linfo("Connection server success.");
        client->conn->status = CONN_OK;
        return;
    } else if (events & BEV_EVENT_EOF) {
        linfo("Connection closed.");
        client->conn->status = CONN_CLOSE;
    } else if (events & BEV_EVENT_ERROR) {
        lwarn("Got an error on the connection: %s.", strerror(errno));
    }
    /* None of the other events can happen here, since we haven't enabled
     * timeouts */
    bufferevent_free(bev);

    linfo("client connection stop.");
    cevent_base_loopexit();
}
