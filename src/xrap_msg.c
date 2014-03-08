/*  =========================================================================
    xrap_msg - xrap serialization

    Generated codec implementation for xrap_msg
    -------------------------------------------------------------------------
    Copyright (C) 2014 the Authors                                                  
                                                                                    
    Permission is hereby granted, free of charge, to any person obtaining a copy of 
    this software and associated documentation files (the "Software"), to deal in   
    the Software without restriction, including without limitation the rights to    
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so, 
    subject to the following conditions:                                            
                                                                                    
    The above copyright notice and this permission notice shall be included in all  
    copies or substantial portions of the Software.                                 
                                                                                    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER  
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN         
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      
    =========================================================================
*/

/*
@header
    xrap_msg - xrap serialization
@discuss
@end
*/

#include <czmq.h>
#include "../include/xrap_msg.h"

//  Structure of our class

struct _xrap_msg_t {
    zframe_t *address;          //  Address of peer if any
    int id;                     //  xrap_msg message ID
    byte *needle;               //  Read/write pointer for serialization
    byte *ceiling;              //  Valid upper limit for read pointer
    char *whatever;             //  
    zhash_t *headers;           //  
    size_t headers_bytes;       //  Size of dictionary content
    zframe_t *content;          //  
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Strings are encoded with 1-byte length
#define STRING_MAX  255

//  Put a block to the frame
#define PUT_BLOCK(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
    }

//  Get a block from the frame
#define GET_BLOCK(host,size) { \
    if (self->needle + size > self->ceiling) \
        goto malformed; \
    memcpy ((host), self->needle, size); \
    self->needle += size; \
    }

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) self->needle = (host); \
    self->needle++; \
    }

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    self->needle [0] = (byte) (((host) >> 8)  & 255); \
    self->needle [1] = (byte) (((host))       & 255); \
    self->needle += 2; \
    }

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    self->needle [0] = (byte) (((host) >> 24) & 255); \
    self->needle [1] = (byte) (((host) >> 16) & 255); \
    self->needle [2] = (byte) (((host) >> 8)  & 255); \
    self->needle [3] = (byte) (((host))       & 255); \
    self->needle += 4; \
    }

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    self->needle [0] = (byte) (((host) >> 56) & 255); \
    self->needle [1] = (byte) (((host) >> 48) & 255); \
    self->needle [2] = (byte) (((host) >> 40) & 255); \
    self->needle [3] = (byte) (((host) >> 32) & 255); \
    self->needle [4] = (byte) (((host) >> 24) & 255); \
    self->needle [5] = (byte) (((host) >> 16) & 255); \
    self->needle [6] = (byte) (((host) >> 8)  & 255); \
    self->needle [7] = (byte) (((host))       & 255); \
    self->needle += 8; \
    }

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (self->needle + 1 > self->ceiling) \
        goto malformed; \
    (host) = *(byte *) self->needle; \
    self->needle++; \
    }

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) \
        goto malformed; \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
    }

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) \
        goto malformed; \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
    }

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) \
        goto malformed; \
    (host) = ((uint64_t) (self->needle [0]) << 56) \
           + ((uint64_t) (self->needle [1]) << 48) \
           + ((uint64_t) (self->needle [2]) << 40) \
           + ((uint64_t) (self->needle [3]) << 32) \
           + ((uint64_t) (self->needle [4]) << 24) \
           + ((uint64_t) (self->needle [5]) << 16) \
           + ((uint64_t) (self->needle [6]) << 8) \
           +  (uint64_t) (self->needle [7]); \
    self->needle += 8; \
    }

//  Put a string to the frame
#define PUT_STRING(host) { \
    string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
    }

//  Get a string from the frame
#define GET_STRING(host) { \
    GET_NUMBER1 (string_size); \
    if (self->needle + string_size > (self->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
    }


//  --------------------------------------------------------------------------
//  Create a new xrap_msg

xrap_msg_t *
xrap_msg_new (int id)
{
    xrap_msg_t *self = (xrap_msg_t *) zmalloc (sizeof (xrap_msg_t));
    self->id = id;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the xrap_msg

void
xrap_msg_destroy (xrap_msg_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        xrap_msg_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->address);
        free (self->whatever);
        zhash_destroy (&self->headers);
        zframe_destroy (&self->content);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Receive and parse a xrap_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

xrap_msg_t *
xrap_msg_recv (void *input)
{
    assert (input);
    xrap_msg_t *self = xrap_msg_new (0);
    zframe_t *frame = NULL;
    size_t string_size;
    size_t hash_size;

    //  Read valid message frame from socket; we loop over any
    //  garbage data we might receive from badly-connected peers
    while (true) {
        //  If we're reading from a ROUTER socket, get address
        if (zsocket_type (input) == ZMQ_ROUTER) {
            zframe_destroy (&self->address);
            self->address = zframe_recv (input);
            if (!self->address)
                goto empty;         //  Interrupted
            if (!zsocket_rcvmore (input))
                goto malformed;
        }
        //  Read and parse command in frame
        frame = zframe_recv (input);
        if (!frame)
            goto empty;             //  Interrupted

        //  Get and check protocol signature
        self->needle = zframe_data (frame);
        self->ceiling = self->needle + zframe_size (frame);
        uint16_t signature;
        GET_NUMBER2 (signature);
        if (signature == (0xAAA0 | 1))
            break;                  //  Valid signature

        //  Protocol assertion, drop message
        while (zsocket_rcvmore (input)) {
            zframe_destroy (&frame);
            frame = zframe_recv (input);
        }
        zframe_destroy (&frame);
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case XRAP_MSG_METHOD:
            free (self->whatever);
            GET_STRING (self->whatever);
            GET_NUMBER1 (hash_size);
            self->headers = zhash_new ();
            zhash_autofree (self->headers);
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->headers, string, value);
                free (string);
            }
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->content = zframe_recv (input);
            break;

        default:
            goto malformed;
    }
    //  Successful return
    zframe_destroy (&frame);
    return self;

    //  Error returns
    malformed:
        printf ("E: malformed message '%d'\n", self->id);
    empty:
        zframe_destroy (&frame);
        xrap_msg_destroy (&self);
        return (NULL);
}

//  Count size of key=value pair
static int
s_headers_count (const char *key, void *item, void *argument)
{
    xrap_msg_t *self = (xrap_msg_t *) argument;
    self->headers_bytes += strlen (key) + 1 + strlen ((char *) item) + 1;
    return 0;
}

//  Serialize headers key=value pair
static int
s_headers_write (const char *key, void *item, void *argument)
{
    xrap_msg_t *self = (xrap_msg_t *) argument;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the xrap_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
xrap_msg_send (xrap_msg_t **self_p, void *output)
{
    assert (output);
    assert (self_p);
    assert (*self_p);

    //  Calculate size of serialized data
    xrap_msg_t *self = *self_p;
    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case XRAP_MSG_METHOD:
            //  whatever is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->whatever)
                frame_size += strlen (self->whatever);
            //  headers is an array of key=value strings
            frame_size++;       //  Size is one octet
            if (self->headers) {
                self->headers_bytes = 0;
                //  Add up size of dictionary contents
                zhash_foreach (self->headers, s_headers_count, self);
            }
            frame_size += self->headers_bytes;
            break;
            
        default:
            printf ("E: bad message type '%d', not sent\n", self->id);
            //  No recovery, this is a fatal application error
            assert (false);
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size);
    self->needle = zframe_data (frame);
    size_t string_size;
    int frame_flags = 0;
    PUT_NUMBER2 (0xAAA0 | 1);
    PUT_NUMBER1 (self->id);

    switch (self->id) {
        case XRAP_MSG_METHOD:
            if (self->whatever) {
                PUT_STRING (self->whatever);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->headers != NULL) {
                PUT_NUMBER1 (zhash_size (self->headers));
                zhash_foreach (self->headers, s_headers_write, self);
            }
            else
                PUT_NUMBER1 (0);    //  Empty dictionary
            frame_flags = ZFRAME_MORE;
            break;
            
    }
    //  If we're sending to a ROUTER, we send the address first
    if (zsocket_type (output) == ZMQ_ROUTER) {
        assert (self->address);
        if (zframe_send (&self->address, output, ZFRAME_MORE)) {
            zframe_destroy (&frame);
            xrap_msg_destroy (self_p);
            return -1;
        }
    }
    //  Now send the data frame
    if (zframe_send (&frame, output, frame_flags)) {
        zframe_destroy (&frame);
        xrap_msg_destroy (self_p);
        return -1;
    }
    //  Now send any frame fields, in order
    if (self->id == XRAP_MSG_METHOD) {
        //  If content isn't set, send an empty frame
        if (!self->content)
            self->content = zframe_new (NULL, 0);
        if (zframe_send (&self->content, output, 0)) {
            zframe_destroy (&frame);
            xrap_msg_destroy (self_p);
            return -1;
        }
    }
    //  Destroy xrap_msg object
    xrap_msg_destroy (self_p);
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the METHOD to the socket in one step

int
xrap_msg_send_method (
    void *output,
    char *whatever,
    zhash_t *headers,
    zframe_t *content)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_METHOD);
    xrap_msg_set_whatever (self, whatever);
    xrap_msg_set_headers (self, zhash_dup (headers));
    xrap_msg_set_content (self, zframe_dup (content));
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Duplicate the xrap_msg message

xrap_msg_t *
xrap_msg_dup (xrap_msg_t *self)
{
    if (!self)
        return NULL;
        
    xrap_msg_t *copy = xrap_msg_new (self->id);
    if (self->address)
        copy->address = zframe_dup (self->address);
    switch (self->id) {
        case XRAP_MSG_METHOD:
            copy->whatever = strdup (self->whatever);
            copy->headers = zhash_dup (self->headers);
            copy->content = zframe_dup (self->content);
            break;

    }
    return copy;
}


//  Dump headers key=value pair to stdout
static int
s_headers_dump (const char *key, void *item, void *argument)
{
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
xrap_msg_dump (xrap_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case XRAP_MSG_METHOD:
            puts ("METHOD:");
            if (self->whatever)
                printf ("    whatever='%s'\n", self->whatever);
            else
                printf ("    whatever=\n");
            printf ("    headers={\n");
            if (self->headers)
                zhash_foreach (self->headers, s_headers_dump, self);
            printf ("    }\n");
            printf ("    content={\n");
            if (self->content)
                zframe_print (self->content, NULL);
            printf ("    }\n");
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message address

zframe_t *
xrap_msg_address (xrap_msg_t *self)
{
    assert (self);
    return self->address;
}

void
xrap_msg_set_address (xrap_msg_t *self, zframe_t *address)
{
    if (self->address)
        zframe_destroy (&self->address);
    self->address = zframe_dup (address);
}


//  --------------------------------------------------------------------------
//  Get/set the xrap_msg id

int
xrap_msg_id (xrap_msg_t *self)
{
    assert (self);
    return self->id;
}

void
xrap_msg_set_id (xrap_msg_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

char *
xrap_msg_command (xrap_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case XRAP_MSG_METHOD:
            return ("METHOD");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the whatever field

char *
xrap_msg_whatever (xrap_msg_t *self)
{
    assert (self);
    return self->whatever;
}

void
xrap_msg_set_whatever (xrap_msg_t *self, char *format, ...)
{
    //  Format whatever from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->whatever);
    self->whatever = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the headers field

zhash_t *
xrap_msg_headers (xrap_msg_t *self)
{
    assert (self);
    return self->headers;
}

//  Greedy function, takes ownership of headers; if you don't want that
//  then use zhash_dup() to pass a copy of headers

void
xrap_msg_set_headers (xrap_msg_t *self, zhash_t *headers)
{
    assert (self);
    zhash_destroy (&self->headers);
    self->headers = headers;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the headers dictionary

char *
xrap_msg_headers_string (xrap_msg_t *self, char *key, char *default_value)
{
    assert (self);
    char *value = NULL;
    if (self->headers)
        value = (char *) (zhash_lookup (self->headers, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
xrap_msg_headers_number (xrap_msg_t *self, char *key, uint64_t default_value)
{
    assert (self);
    uint64_t value = default_value;
    char *string = NULL;
    if (self->headers)
        string = (char *) (zhash_lookup (self->headers, key));
    if (string)
        value = atol (string);

    return value;
}

void
xrap_msg_headers_insert (xrap_msg_t *self, char *key, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = zsys_vprintf (format, argptr);
    va_end (argptr);

    //  Store string in hash table
    if (!self->headers) {
        self->headers = zhash_new ();
        zhash_autofree (self->headers);
    }
    zhash_update (self->headers, key, string);
    free (string);
}

size_t
xrap_msg_headers_size (xrap_msg_t *self)
{
    return zhash_size (self->headers);
}


//  --------------------------------------------------------------------------
//  Get/set the content field

zframe_t *
xrap_msg_content (xrap_msg_t *self)
{
    assert (self);
    return self->content;
}

//  Takes ownership of supplied frame
void
xrap_msg_set_content (xrap_msg_t *self, zframe_t *frame)
{
    assert (self);
    if (self->content)
        zframe_destroy (&self->content);
    self->content = frame;
}



//  --------------------------------------------------------------------------
//  Selftest

int
xrap_msg_test (bool verbose)
{
    printf (" * xrap_msg: ");

    //  @selftest
    //  Simple create/destroy test
    xrap_msg_t *self = xrap_msg_new (0);
    assert (self);
    xrap_msg_destroy (&self);

    //  Create pair of sockets we can send through
    zctx_t *ctx = zctx_new ();
    assert (ctx);

    void *output = zsocket_new (ctx, ZMQ_DEALER);
    assert (output);
    zsocket_bind (output, "inproc://selftest");
    void *input = zsocket_new (ctx, ZMQ_ROUTER);
    assert (input);
    zsocket_connect (input, "inproc://selftest");
    
    //  Encode/send/decode and verify each message type

    self = xrap_msg_new (XRAP_MSG_METHOD);
    xrap_msg_set_whatever (self, "Life is short but Now lasts for ever");
    xrap_msg_headers_insert (self, "Name", "Brutus");
    xrap_msg_headers_insert (self, "Age", "%d", 43);
    xrap_msg_set_content (self, zframe_new ("Captcha Diem", 12));
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (streq (xrap_msg_whatever (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_headers_size (self) == 2);
    assert (streq (xrap_msg_headers_string (self, "Name", "?"), "Brutus"));
    assert (xrap_msg_headers_number (self, "Age", 0) == 43);
    assert (zframe_streq (xrap_msg_content (self), "Captcha Diem"));
    xrap_msg_destroy (&self);

    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
