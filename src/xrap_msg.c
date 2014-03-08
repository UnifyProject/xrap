/*  =========================================================================
    xrap_msg - xrap serialization over zmtp

    Generated codec implementation for xrap_msg
    -------------------------------------------------------------------------
    Copyright (C) 2014 the Authors                                         
                                                                           
    Permission is hereby granted, free of charge, to any person obtaining  
    a copy of this software and associated documentation files (the        
    "Software"), to deal in the Software without restriction, including    
    without limitation the rights to use, copy, modify, merge, publish,    
    distribute, sublicense, and/or sell copies of the Software, and to     
    permit persons to whom the Software is furnished to do so, subject to  
    the following conditions:                                              
                                                                           
    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.                 
                                                                           
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF             
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 
    =========================================================================
*/

/*
@header
    xrap_msg - xrap serialization over zmtp
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
    char *parent;               //  Schema/type/name
    char *content_type;         //  Content type
    zframe_t *content_body;     //  New resource specification
    uint16_t status_code;       //  Response status code 2xx
    char *location;             //  Schema/type/name
    char *etag;                 //  Opaque hash tag
    uint64_t date_modified;     //  Date and time modified
    char *resource;             //  Schema/type/name
    uint64_t if_modified_since;  //  GET if more recent
    char *if_none_match;        //  GET if changed
    uint64_t if_unmodified_since;  //  Update if same date
    char *if_match;             //  Update if same ETag
    char *status_text;          //  Response status text
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
        free (self->parent);
        free (self->content_type);
        zframe_destroy (&self->content_body);
        free (self->location);
        free (self->etag);
        free (self->resource);
        free (self->if_none_match);
        free (self->if_match);
        free (self->status_text);

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
        case XRAP_MSG_POST:
            free (self->parent);
            GET_STRING (self->parent);
            free (self->content_type);
            GET_STRING (self->content_type);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->content_body = zframe_recv (input);
            break;

        case XRAP_MSG_POST_OK:
            GET_NUMBER2 (self->status_code);
            free (self->location);
            GET_STRING (self->location);
            free (self->etag);
            GET_STRING (self->etag);
            GET_NUMBER8 (self->date_modified);
            free (self->content_type);
            GET_STRING (self->content_type);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->content_body = zframe_recv (input);
            break;

        case XRAP_MSG_GET:
            free (self->resource);
            GET_STRING (self->resource);
            GET_NUMBER8 (self->if_modified_since);
            free (self->if_none_match);
            GET_STRING (self->if_none_match);
            free (self->content_type);
            GET_STRING (self->content_type);
            break;

        case XRAP_MSG_GET_OK:
            GET_NUMBER2 (self->status_code);
            free (self->content_type);
            GET_STRING (self->content_type);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->content_body = zframe_recv (input);
            break;

        case XRAP_MSG_GET_EMPTY:
            GET_NUMBER2 (self->status_code);
            break;

        case XRAP_MSG_PUT:
            free (self->resource);
            GET_STRING (self->resource);
            GET_NUMBER8 (self->if_unmodified_since);
            free (self->if_match);
            GET_STRING (self->if_match);
            free (self->content_type);
            GET_STRING (self->content_type);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->content_body = zframe_recv (input);
            break;

        case XRAP_MSG_PUT_OK:
            GET_NUMBER2 (self->status_code);
            free (self->location);
            GET_STRING (self->location);
            free (self->etag);
            GET_STRING (self->etag);
            GET_NUMBER8 (self->date_modified);
            break;

        case XRAP_MSG_DELETE:
            free (self->resource);
            GET_STRING (self->resource);
            GET_NUMBER8 (self->if_unmodified_since);
            free (self->if_match);
            GET_STRING (self->if_match);
            break;

        case XRAP_MSG_DELETE_OK:
            GET_NUMBER2 (self->status_code);
            break;

        case XRAP_MSG_ERROR:
            GET_NUMBER2 (self->status_code);
            free (self->status_text);
            GET_STRING (self->status_text);
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
        case XRAP_MSG_POST:
            //  parent is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->parent)
                frame_size += strlen (self->parent);
            //  content_type is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->content_type)
                frame_size += strlen (self->content_type);
            break;
            
        case XRAP_MSG_POST_OK:
            //  status_code is a 2-byte integer
            frame_size += 2;
            //  location is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->location)
                frame_size += strlen (self->location);
            //  etag is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->etag)
                frame_size += strlen (self->etag);
            //  date_modified is a 8-byte integer
            frame_size += 8;
            //  content_type is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->content_type)
                frame_size += strlen (self->content_type);
            break;
            
        case XRAP_MSG_GET:
            //  resource is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->resource)
                frame_size += strlen (self->resource);
            //  if_modified_since is a 8-byte integer
            frame_size += 8;
            //  if_none_match is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->if_none_match)
                frame_size += strlen (self->if_none_match);
            //  content_type is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->content_type)
                frame_size += strlen (self->content_type);
            break;
            
        case XRAP_MSG_GET_OK:
            //  status_code is a 2-byte integer
            frame_size += 2;
            //  content_type is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->content_type)
                frame_size += strlen (self->content_type);
            break;
            
        case XRAP_MSG_GET_EMPTY:
            //  status_code is a 2-byte integer
            frame_size += 2;
            break;
            
        case XRAP_MSG_PUT:
            //  resource is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->resource)
                frame_size += strlen (self->resource);
            //  if_unmodified_since is a 8-byte integer
            frame_size += 8;
            //  if_match is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->if_match)
                frame_size += strlen (self->if_match);
            //  content_type is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->content_type)
                frame_size += strlen (self->content_type);
            break;
            
        case XRAP_MSG_PUT_OK:
            //  status_code is a 2-byte integer
            frame_size += 2;
            //  location is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->location)
                frame_size += strlen (self->location);
            //  etag is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->etag)
                frame_size += strlen (self->etag);
            //  date_modified is a 8-byte integer
            frame_size += 8;
            break;
            
        case XRAP_MSG_DELETE:
            //  resource is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->resource)
                frame_size += strlen (self->resource);
            //  if_unmodified_since is a 8-byte integer
            frame_size += 8;
            //  if_match is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->if_match)
                frame_size += strlen (self->if_match);
            break;
            
        case XRAP_MSG_DELETE_OK:
            //  status_code is a 2-byte integer
            frame_size += 2;
            break;
            
        case XRAP_MSG_ERROR:
            //  status_code is a 2-byte integer
            frame_size += 2;
            //  status_text is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->status_text)
                frame_size += strlen (self->status_text);
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
        case XRAP_MSG_POST:
            if (self->parent) {
                PUT_STRING (self->parent);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->content_type) {
                PUT_STRING (self->content_type);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case XRAP_MSG_POST_OK:
            PUT_NUMBER2 (self->status_code);
            if (self->location) {
                PUT_STRING (self->location);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->etag) {
                PUT_STRING (self->etag);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->date_modified);
            if (self->content_type) {
                PUT_STRING (self->content_type);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case XRAP_MSG_GET:
            if (self->resource) {
                PUT_STRING (self->resource);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->if_modified_since);
            if (self->if_none_match) {
                PUT_STRING (self->if_none_match);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->content_type) {
                PUT_STRING (self->content_type);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;
            
        case XRAP_MSG_GET_OK:
            PUT_NUMBER2 (self->status_code);
            if (self->content_type) {
                PUT_STRING (self->content_type);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case XRAP_MSG_GET_EMPTY:
            PUT_NUMBER2 (self->status_code);
            break;
            
        case XRAP_MSG_PUT:
            if (self->resource) {
                PUT_STRING (self->resource);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->if_unmodified_since);
            if (self->if_match) {
                PUT_STRING (self->if_match);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->content_type) {
                PUT_STRING (self->content_type);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case XRAP_MSG_PUT_OK:
            PUT_NUMBER2 (self->status_code);
            if (self->location) {
                PUT_STRING (self->location);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->etag) {
                PUT_STRING (self->etag);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->date_modified);
            break;
            
        case XRAP_MSG_DELETE:
            if (self->resource) {
                PUT_STRING (self->resource);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->if_unmodified_since);
            if (self->if_match) {
                PUT_STRING (self->if_match);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;
            
        case XRAP_MSG_DELETE_OK:
            PUT_NUMBER2 (self->status_code);
            break;
            
        case XRAP_MSG_ERROR:
            PUT_NUMBER2 (self->status_code);
            if (self->status_text) {
                PUT_STRING (self->status_text);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
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
    if (self->id == XRAP_MSG_POST) {
        //  If content_body isn't set, send an empty frame
        if (!self->content_body)
            self->content_body = zframe_new (NULL, 0);
        if (zframe_send (&self->content_body, output, 0)) {
            zframe_destroy (&frame);
            xrap_msg_destroy (self_p);
            return -1;
        }
    }
    //  Now send any frame fields, in order
    if (self->id == XRAP_MSG_POST_OK) {
        //  If content_body isn't set, send an empty frame
        if (!self->content_body)
            self->content_body = zframe_new (NULL, 0);
        if (zframe_send (&self->content_body, output, 0)) {
            zframe_destroy (&frame);
            xrap_msg_destroy (self_p);
            return -1;
        }
    }
    //  Now send any frame fields, in order
    if (self->id == XRAP_MSG_GET_OK) {
        //  If content_body isn't set, send an empty frame
        if (!self->content_body)
            self->content_body = zframe_new (NULL, 0);
        if (zframe_send (&self->content_body, output, 0)) {
            zframe_destroy (&frame);
            xrap_msg_destroy (self_p);
            return -1;
        }
    }
    //  Now send any frame fields, in order
    if (self->id == XRAP_MSG_PUT) {
        //  If content_body isn't set, send an empty frame
        if (!self->content_body)
            self->content_body = zframe_new (NULL, 0);
        if (zframe_send (&self->content_body, output, 0)) {
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
//  Send the POST to the socket in one step

int
xrap_msg_send_post (
    void *output,
    char *parent,
    char *content_type,
    zframe_t *content_body)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_POST);
    xrap_msg_set_parent (self, parent);
    xrap_msg_set_content_type (self, content_type);
    xrap_msg_set_content_body (self, zframe_dup (content_body));
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the POST_OK to the socket in one step

int
xrap_msg_send_post_ok (
    void *output,
    uint16_t status_code,
    char *location,
    char *etag,
    uint64_t date_modified,
    char *content_type,
    zframe_t *content_body)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_POST_OK);
    xrap_msg_set_status_code (self, status_code);
    xrap_msg_set_location (self, location);
    xrap_msg_set_etag (self, etag);
    xrap_msg_set_date_modified (self, date_modified);
    xrap_msg_set_content_type (self, content_type);
    xrap_msg_set_content_body (self, zframe_dup (content_body));
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the GET to the socket in one step

int
xrap_msg_send_get (
    void *output,
    char *resource,
    uint64_t if_modified_since,
    char *if_none_match,
    char *content_type)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_GET);
    xrap_msg_set_resource (self, resource);
    xrap_msg_set_if_modified_since (self, if_modified_since);
    xrap_msg_set_if_none_match (self, if_none_match);
    xrap_msg_set_content_type (self, content_type);
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the GET_OK to the socket in one step

int
xrap_msg_send_get_ok (
    void *output,
    uint16_t status_code,
    char *content_type,
    zframe_t *content_body)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_GET_OK);
    xrap_msg_set_status_code (self, status_code);
    xrap_msg_set_content_type (self, content_type);
    xrap_msg_set_content_body (self, zframe_dup (content_body));
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the GET_EMPTY to the socket in one step

int
xrap_msg_send_get_empty (
    void *output,
    uint16_t status_code)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_GET_EMPTY);
    xrap_msg_set_status_code (self, status_code);
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the PUT to the socket in one step

int
xrap_msg_send_put (
    void *output,
    char *resource,
    uint64_t if_unmodified_since,
    char *if_match,
    char *content_type,
    zframe_t *content_body)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_PUT);
    xrap_msg_set_resource (self, resource);
    xrap_msg_set_if_unmodified_since (self, if_unmodified_since);
    xrap_msg_set_if_match (self, if_match);
    xrap_msg_set_content_type (self, content_type);
    xrap_msg_set_content_body (self, zframe_dup (content_body));
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the PUT_OK to the socket in one step

int
xrap_msg_send_put_ok (
    void *output,
    uint16_t status_code,
    char *location,
    char *etag,
    uint64_t date_modified)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_PUT_OK);
    xrap_msg_set_status_code (self, status_code);
    xrap_msg_set_location (self, location);
    xrap_msg_set_etag (self, etag);
    xrap_msg_set_date_modified (self, date_modified);
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the DELETE to the socket in one step

int
xrap_msg_send_delete (
    void *output,
    char *resource,
    uint64_t if_unmodified_since,
    char *if_match)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_DELETE);
    xrap_msg_set_resource (self, resource);
    xrap_msg_set_if_unmodified_since (self, if_unmodified_since);
    xrap_msg_set_if_match (self, if_match);
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the DELETE_OK to the socket in one step

int
xrap_msg_send_delete_ok (
    void *output,
    uint16_t status_code)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_DELETE_OK);
    xrap_msg_set_status_code (self, status_code);
    return xrap_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the ERROR to the socket in one step

int
xrap_msg_send_error (
    void *output,
    uint16_t status_code,
    char *status_text)
{
    xrap_msg_t *self = xrap_msg_new (XRAP_MSG_ERROR);
    xrap_msg_set_status_code (self, status_code);
    xrap_msg_set_status_text (self, status_text);
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
        case XRAP_MSG_POST:
            copy->parent = strdup (self->parent);
            copy->content_type = strdup (self->content_type);
            copy->content_body = zframe_dup (self->content_body);
            break;

        case XRAP_MSG_POST_OK:
            copy->status_code = self->status_code;
            copy->location = strdup (self->location);
            copy->etag = strdup (self->etag);
            copy->date_modified = self->date_modified;
            copy->content_type = strdup (self->content_type);
            copy->content_body = zframe_dup (self->content_body);
            break;

        case XRAP_MSG_GET:
            copy->resource = strdup (self->resource);
            copy->if_modified_since = self->if_modified_since;
            copy->if_none_match = strdup (self->if_none_match);
            copy->content_type = strdup (self->content_type);
            break;

        case XRAP_MSG_GET_OK:
            copy->status_code = self->status_code;
            copy->content_type = strdup (self->content_type);
            copy->content_body = zframe_dup (self->content_body);
            break;

        case XRAP_MSG_GET_EMPTY:
            copy->status_code = self->status_code;
            break;

        case XRAP_MSG_PUT:
            copy->resource = strdup (self->resource);
            copy->if_unmodified_since = self->if_unmodified_since;
            copy->if_match = strdup (self->if_match);
            copy->content_type = strdup (self->content_type);
            copy->content_body = zframe_dup (self->content_body);
            break;

        case XRAP_MSG_PUT_OK:
            copy->status_code = self->status_code;
            copy->location = strdup (self->location);
            copy->etag = strdup (self->etag);
            copy->date_modified = self->date_modified;
            break;

        case XRAP_MSG_DELETE:
            copy->resource = strdup (self->resource);
            copy->if_unmodified_since = self->if_unmodified_since;
            copy->if_match = strdup (self->if_match);
            break;

        case XRAP_MSG_DELETE_OK:
            copy->status_code = self->status_code;
            break;

        case XRAP_MSG_ERROR:
            copy->status_code = self->status_code;
            copy->status_text = strdup (self->status_text);
            break;

    }
    return copy;
}



//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
xrap_msg_dump (xrap_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case XRAP_MSG_POST:
            puts ("POST:");
            if (self->parent)
                printf ("    parent='%s'\n", self->parent);
            else
                printf ("    parent=\n");
            if (self->content_type)
                printf ("    content_type='%s'\n", self->content_type);
            else
                printf ("    content_type=\n");
            printf ("    content_body={\n");
            if (self->content_body)
                zframe_print (self->content_body, NULL);
            printf ("    }\n");
            break;
            
        case XRAP_MSG_POST_OK:
            puts ("POST_OK:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            if (self->location)
                printf ("    location='%s'\n", self->location);
            else
                printf ("    location=\n");
            if (self->etag)
                printf ("    etag='%s'\n", self->etag);
            else
                printf ("    etag=\n");
            printf ("    date_modified=%ld\n", (long) self->date_modified);
            if (self->content_type)
                printf ("    content_type='%s'\n", self->content_type);
            else
                printf ("    content_type=\n");
            printf ("    content_body={\n");
            if (self->content_body)
                zframe_print (self->content_body, NULL);
            printf ("    }\n");
            break;
            
        case XRAP_MSG_GET:
            puts ("GET:");
            if (self->resource)
                printf ("    resource='%s'\n", self->resource);
            else
                printf ("    resource=\n");
            printf ("    if_modified_since=%ld\n", (long) self->if_modified_since);
            if (self->if_none_match)
                printf ("    if_none_match='%s'\n", self->if_none_match);
            else
                printf ("    if_none_match=\n");
            if (self->content_type)
                printf ("    content_type='%s'\n", self->content_type);
            else
                printf ("    content_type=\n");
            break;
            
        case XRAP_MSG_GET_OK:
            puts ("GET_OK:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            if (self->content_type)
                printf ("    content_type='%s'\n", self->content_type);
            else
                printf ("    content_type=\n");
            printf ("    content_body={\n");
            if (self->content_body)
                zframe_print (self->content_body, NULL);
            printf ("    }\n");
            break;
            
        case XRAP_MSG_GET_EMPTY:
            puts ("GET_EMPTY:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            break;
            
        case XRAP_MSG_PUT:
            puts ("PUT:");
            if (self->resource)
                printf ("    resource='%s'\n", self->resource);
            else
                printf ("    resource=\n");
            printf ("    if_unmodified_since=%ld\n", (long) self->if_unmodified_since);
            if (self->if_match)
                printf ("    if_match='%s'\n", self->if_match);
            else
                printf ("    if_match=\n");
            if (self->content_type)
                printf ("    content_type='%s'\n", self->content_type);
            else
                printf ("    content_type=\n");
            printf ("    content_body={\n");
            if (self->content_body)
                zframe_print (self->content_body, NULL);
            printf ("    }\n");
            break;
            
        case XRAP_MSG_PUT_OK:
            puts ("PUT_OK:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            if (self->location)
                printf ("    location='%s'\n", self->location);
            else
                printf ("    location=\n");
            if (self->etag)
                printf ("    etag='%s'\n", self->etag);
            else
                printf ("    etag=\n");
            printf ("    date_modified=%ld\n", (long) self->date_modified);
            break;
            
        case XRAP_MSG_DELETE:
            puts ("DELETE:");
            if (self->resource)
                printf ("    resource='%s'\n", self->resource);
            else
                printf ("    resource=\n");
            printf ("    if_unmodified_since=%ld\n", (long) self->if_unmodified_since);
            if (self->if_match)
                printf ("    if_match='%s'\n", self->if_match);
            else
                printf ("    if_match=\n");
            break;
            
        case XRAP_MSG_DELETE_OK:
            puts ("DELETE_OK:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            break;
            
        case XRAP_MSG_ERROR:
            puts ("ERROR:");
            printf ("    status_code=%ld\n", (long) self->status_code);
            if (self->status_text)
                printf ("    status_text='%s'\n", self->status_text);
            else
                printf ("    status_text=\n");
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
        case XRAP_MSG_POST:
            return ("POST");
            break;
        case XRAP_MSG_POST_OK:
            return ("POST_OK");
            break;
        case XRAP_MSG_GET:
            return ("GET");
            break;
        case XRAP_MSG_GET_OK:
            return ("GET_OK");
            break;
        case XRAP_MSG_GET_EMPTY:
            return ("GET_EMPTY");
            break;
        case XRAP_MSG_PUT:
            return ("PUT");
            break;
        case XRAP_MSG_PUT_OK:
            return ("PUT_OK");
            break;
        case XRAP_MSG_DELETE:
            return ("DELETE");
            break;
        case XRAP_MSG_DELETE_OK:
            return ("DELETE_OK");
            break;
        case XRAP_MSG_ERROR:
            return ("ERROR");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the parent field

char *
xrap_msg_parent (xrap_msg_t *self)
{
    assert (self);
    return self->parent;
}

void
xrap_msg_set_parent (xrap_msg_t *self, char *format, ...)
{
    //  Format parent from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->parent);
    self->parent = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the content_type field

char *
xrap_msg_content_type (xrap_msg_t *self)
{
    assert (self);
    return self->content_type;
}

void
xrap_msg_set_content_type (xrap_msg_t *self, char *format, ...)
{
    //  Format content_type from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->content_type);
    self->content_type = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the content_body field

zframe_t *
xrap_msg_content_body (xrap_msg_t *self)
{
    assert (self);
    return self->content_body;
}

//  Takes ownership of supplied frame
void
xrap_msg_set_content_body (xrap_msg_t *self, zframe_t *frame)
{
    assert (self);
    if (self->content_body)
        zframe_destroy (&self->content_body);
    self->content_body = frame;
}


//  --------------------------------------------------------------------------
//  Get/set the status_code field

uint16_t
xrap_msg_status_code (xrap_msg_t *self)
{
    assert (self);
    return self->status_code;
}

void
xrap_msg_set_status_code (xrap_msg_t *self, uint16_t status_code)
{
    assert (self);
    self->status_code = status_code;
}


//  --------------------------------------------------------------------------
//  Get/set the location field

char *
xrap_msg_location (xrap_msg_t *self)
{
    assert (self);
    return self->location;
}

void
xrap_msg_set_location (xrap_msg_t *self, char *format, ...)
{
    //  Format location from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->location);
    self->location = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the etag field

char *
xrap_msg_etag (xrap_msg_t *self)
{
    assert (self);
    return self->etag;
}

void
xrap_msg_set_etag (xrap_msg_t *self, char *format, ...)
{
    //  Format etag from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->etag);
    self->etag = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the date_modified field

uint64_t
xrap_msg_date_modified (xrap_msg_t *self)
{
    assert (self);
    return self->date_modified;
}

void
xrap_msg_set_date_modified (xrap_msg_t *self, uint64_t date_modified)
{
    assert (self);
    self->date_modified = date_modified;
}


//  --------------------------------------------------------------------------
//  Get/set the resource field

char *
xrap_msg_resource (xrap_msg_t *self)
{
    assert (self);
    return self->resource;
}

void
xrap_msg_set_resource (xrap_msg_t *self, char *format, ...)
{
    //  Format resource from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->resource);
    self->resource = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the if_modified_since field

uint64_t
xrap_msg_if_modified_since (xrap_msg_t *self)
{
    assert (self);
    return self->if_modified_since;
}

void
xrap_msg_set_if_modified_since (xrap_msg_t *self, uint64_t if_modified_since)
{
    assert (self);
    self->if_modified_since = if_modified_since;
}


//  --------------------------------------------------------------------------
//  Get/set the if_none_match field

char *
xrap_msg_if_none_match (xrap_msg_t *self)
{
    assert (self);
    return self->if_none_match;
}

void
xrap_msg_set_if_none_match (xrap_msg_t *self, char *format, ...)
{
    //  Format if_none_match from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->if_none_match);
    self->if_none_match = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the if_unmodified_since field

uint64_t
xrap_msg_if_unmodified_since (xrap_msg_t *self)
{
    assert (self);
    return self->if_unmodified_since;
}

void
xrap_msg_set_if_unmodified_since (xrap_msg_t *self, uint64_t if_unmodified_since)
{
    assert (self);
    self->if_unmodified_since = if_unmodified_since;
}


//  --------------------------------------------------------------------------
//  Get/set the if_match field

char *
xrap_msg_if_match (xrap_msg_t *self)
{
    assert (self);
    return self->if_match;
}

void
xrap_msg_set_if_match (xrap_msg_t *self, char *format, ...)
{
    //  Format if_match from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->if_match);
    self->if_match = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the status_text field

char *
xrap_msg_status_text (xrap_msg_t *self)
{
    assert (self);
    return self->status_text;
}

void
xrap_msg_set_status_text (xrap_msg_t *self, char *format, ...)
{
    //  Format status_text from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->status_text);
    self->status_text = zsys_vprintf (format, argptr);
    va_end (argptr);
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

    self = xrap_msg_new (XRAP_MSG_POST);
    xrap_msg_set_parent (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_type (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_body (self, zframe_new ("Captcha Diem", 12));
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (streq (xrap_msg_parent (self), "Life is short but Now lasts for ever"));
    assert (streq (xrap_msg_content_type (self), "Life is short but Now lasts for ever"));
    assert (zframe_streq (xrap_msg_content_body (self), "Captcha Diem"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_POST_OK);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_set_location (self, "Life is short but Now lasts for ever");
    xrap_msg_set_etag (self, "Life is short but Now lasts for ever");
    xrap_msg_set_date_modified (self, 123);
    xrap_msg_set_content_type (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_body (self, zframe_new ("Captcha Diem", 12));
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    assert (streq (xrap_msg_location (self), "Life is short but Now lasts for ever"));
    assert (streq (xrap_msg_etag (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_date_modified (self) == 123);
    assert (streq (xrap_msg_content_type (self), "Life is short but Now lasts for ever"));
    assert (zframe_streq (xrap_msg_content_body (self), "Captcha Diem"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_GET);
    xrap_msg_set_resource (self, "Life is short but Now lasts for ever");
    xrap_msg_set_if_modified_since (self, 123);
    xrap_msg_set_if_none_match (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_type (self, "Life is short but Now lasts for ever");
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (streq (xrap_msg_resource (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_if_modified_since (self) == 123);
    assert (streq (xrap_msg_if_none_match (self), "Life is short but Now lasts for ever"));
    assert (streq (xrap_msg_content_type (self), "Life is short but Now lasts for ever"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_GET_OK);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_set_content_type (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_body (self, zframe_new ("Captcha Diem", 12));
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    assert (streq (xrap_msg_content_type (self), "Life is short but Now lasts for ever"));
    assert (zframe_streq (xrap_msg_content_body (self), "Captcha Diem"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_GET_EMPTY);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_PUT);
    xrap_msg_set_resource (self, "Life is short but Now lasts for ever");
    xrap_msg_set_if_unmodified_since (self, 123);
    xrap_msg_set_if_match (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_type (self, "Life is short but Now lasts for ever");
    xrap_msg_set_content_body (self, zframe_new ("Captcha Diem", 12));
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (streq (xrap_msg_resource (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_if_unmodified_since (self) == 123);
    assert (streq (xrap_msg_if_match (self), "Life is short but Now lasts for ever"));
    assert (streq (xrap_msg_content_type (self), "Life is short but Now lasts for ever"));
    assert (zframe_streq (xrap_msg_content_body (self), "Captcha Diem"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_PUT_OK);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_set_location (self, "Life is short but Now lasts for ever");
    xrap_msg_set_etag (self, "Life is short but Now lasts for ever");
    xrap_msg_set_date_modified (self, 123);
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    assert (streq (xrap_msg_location (self), "Life is short but Now lasts for ever"));
    assert (streq (xrap_msg_etag (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_date_modified (self) == 123);
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_DELETE);
    xrap_msg_set_resource (self, "Life is short but Now lasts for ever");
    xrap_msg_set_if_unmodified_since (self, 123);
    xrap_msg_set_if_match (self, "Life is short but Now lasts for ever");
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (streq (xrap_msg_resource (self), "Life is short but Now lasts for ever"));
    assert (xrap_msg_if_unmodified_since (self) == 123);
    assert (streq (xrap_msg_if_match (self), "Life is short but Now lasts for ever"));
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_DELETE_OK);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    xrap_msg_destroy (&self);

    self = xrap_msg_new (XRAP_MSG_ERROR);
    xrap_msg_set_status_code (self, 123);
    xrap_msg_set_status_text (self, "Life is short but Now lasts for ever");
    xrap_msg_send (&self, output);
    
    self = xrap_msg_recv (input);
    assert (self);
    assert (xrap_msg_status_code (self) == 123);
    assert (streq (xrap_msg_status_text (self), "Life is short but Now lasts for ever"));
    xrap_msg_destroy (&self);

    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
