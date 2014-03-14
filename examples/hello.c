//  XRAP library
//  Hello World example of client and server

//  Imports libxrap, libczmq, libzmq APIs
#include "xrap.h"

static void *
client_task (void *args)
{
    zctx_t *ctx = zctx_new ();
    void *client = zsocket_new (ctx, ZMQ_DEALER);
    zsocket_connect (client, "tcp://127.0.0.1:5555");
    printf ("Setting up test...\n");
    zclock_sleep (100);

    int requests;
    int64_t start;

    printf ("Synchronous round-trip test...\n");
    start = zclock_time ();
    for (requests = 0; requests < 10000; requests++) {
        zstr_send (client, "hello");
        char *reply = zstr_recv (client);
        free (reply);
    }
    printf (" %d calls/second\n",
        (1000 * 10000) / (int) (zclock_time () - start));

    printf ("Asynchronous round-trip test...\n");
    start = zclock_time ();
    for (requests = 0; requests < 100000; requests++)
        zstr_send (client, "hello");
    for (requests = 0; requests < 100000; requests++) {
        char *reply = zstr_recv (client);
        free (reply);
    }
    printf (" %d calls/second\n",
        (1000 * 100000) / (int) (zclock_time () - start));

    zctx_destroy (&ctx);
    return NULL;
}

static void *
server_task (void *args)
{
    zctx_t *ctx = zctx_new ();
    void *server = zsocket_new (ctx, ZMQ_ROUTER);
    zsocket_bind (server, "tcp://127.0.0.1:5555");

    while (true) {
        zmsg_t *msg = zmsg_recv (server);
        zmsg_send (&msg, server);
    }
    zctx_destroy (&ctx);
    return NULL;
}

int main (void)
{
    zthread_new (client_task, NULL);
    zthread_new (server_task, NULL);

    sleep (2);

    return 0;
}
