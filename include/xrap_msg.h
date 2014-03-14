/*  =========================================================================
    xrap_msg - XRAP serialization over ZMTP
    
    Generated codec header for xrap_msg
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

#ifndef __XRAP_MSG_H_INCLUDED__
#define __XRAP_MSG_H_INCLUDED__

/*  These are the xrap_msg messages:

    POST - Create a new, dynamically named resource in some parent.
        parent              string      Schema/type/name
        content_type        string      Content type
        content_body        longstr     New resource specification

    POST_OK - Success response for POST.
        status_code         number 2    Response status code 2xx
        location            string      Schema/type/name
        etag                string      Opaque hash tag
        date_modified       number 8    Date and time modified
        content_type        string      Content type
        content_body        longstr     Resource contents

    GET - Retrieve a known resource.
        resource            string      Schema/type/name
        if_modified_since   number 8    GET if more recent
        if_none_match       string      GET if changed
        content_type        string      Desired content type

    GET_OK - Success response for GET.
        status_code         number 2    Response status code 2xx
        content_type        string      Actual content type
        content_body        longstr     Resource specification

    GET_EMPTY - Conditional GET returned 304 Not Modified.
        status_code         number 2    Response status code 3xx

    PUT - Update a known resource.
        resource            string      Schema/type/name
        if_unmodified_since  number 8   Update if same date
        if_match            string      Update if same ETag
        content_type        string      Content type
        content_body        longstr     New resource specification

    PUT_OK - Success response for PUT.
        status_code         number 2    Response status code 2xx
        location            string      Schema/type/name
        etag                string      Opaque hash tag
        date_modified       number 8    Date and time modified

    DELETE - Remove a known resource.
        resource            string      schema/type/name
        if_unmodified_since  number 8   DELETE if same date
        if_match            string      DELETE if same ETag

    DELETE_OK - Success response for DELETE.
        status_code         number 2    Response status code 2xx

    ERROR - Error response for any request.
        status_code         number 2    Response status code, 4xx or 5xx
        status_text         string      Response status text
*/


#define XRAP_MSG_POST                       1
#define XRAP_MSG_POST_OK                    2
#define XRAP_MSG_GET                        3
#define XRAP_MSG_GET_OK                     4
#define XRAP_MSG_GET_EMPTY                  5
#define XRAP_MSG_PUT                        6
#define XRAP_MSG_PUT_OK                     7
#define XRAP_MSG_DELETE                     8
#define XRAP_MSG_DELETE_OK                  9
#define XRAP_MSG_ERROR                      10

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _xrap_msg_t xrap_msg_t;

//  @interface
//  Create a new xrap_msg
xrap_msg_t *
    xrap_msg_new (int id);

//  Destroy the xrap_msg
void
    xrap_msg_destroy (xrap_msg_t **self_p);

//  Receive and parse a xrap_msg from the input
xrap_msg_t *
    xrap_msg_recv (void *input);

//  Send the xrap_msg to the output, and destroy it
int
    xrap_msg_send (xrap_msg_t **self_p, void *output);

//  Send the xrap_msg to the output, and do not destroy it
int
    xrap_msg_send_again (xrap_msg_t *self, void *output);

//  Send the POST to the output in one step
int
    xrap_msg_send_post (void *output,
        char *parent,
        char *content_type,
        char *content_body);
    
//  Send the POST_OK to the output in one step
int
    xrap_msg_send_post_ok (void *output,
        uint16_t status_code,
        char *location,
        char *etag,
        uint64_t date_modified,
        char *content_type,
        char *content_body);
    
//  Send the GET to the output in one step
int
    xrap_msg_send_get (void *output,
        char *resource,
        uint64_t if_modified_since,
        char *if_none_match,
        char *content_type);
    
//  Send the GET_OK to the output in one step
int
    xrap_msg_send_get_ok (void *output,
        uint16_t status_code,
        char *content_type,
        char *content_body);
    
//  Send the GET_EMPTY to the output in one step
int
    xrap_msg_send_get_empty (void *output,
        uint16_t status_code);
    
//  Send the PUT to the output in one step
int
    xrap_msg_send_put (void *output,
        char *resource,
        uint64_t if_unmodified_since,
        char *if_match,
        char *content_type,
        char *content_body);
    
//  Send the PUT_OK to the output in one step
int
    xrap_msg_send_put_ok (void *output,
        uint16_t status_code,
        char *location,
        char *etag,
        uint64_t date_modified);
    
//  Send the DELETE to the output in one step
int
    xrap_msg_send_delete (void *output,
        char *resource,
        uint64_t if_unmodified_since,
        char *if_match);
    
//  Send the DELETE_OK to the output in one step
int
    xrap_msg_send_delete_ok (void *output,
        uint16_t status_code);
    
//  Send the ERROR to the output in one step
int
    xrap_msg_send_error (void *output,
        uint16_t status_code,
        char *status_text);
    
//  Duplicate the xrap_msg message
xrap_msg_t *
    xrap_msg_dup (xrap_msg_t *self);

//  Print contents of message to stdout
void
    xrap_msg_dump (xrap_msg_t *self);

//  Get/set the message routing id
zframe_t *
    xrap_msg_routing_id (xrap_msg_t *self);
void
    xrap_msg_set_routing_id (xrap_msg_t *self, zframe_t *routing_id);

//  Get the xrap_msg id and printable command
int
    xrap_msg_id (xrap_msg_t *self);
void
    xrap_msg_set_id (xrap_msg_t *self, int id);
char *
    xrap_msg_command (xrap_msg_t *self);

//  Get/set the parent field
char *
    xrap_msg_parent (xrap_msg_t *self);
void
    xrap_msg_set_parent (xrap_msg_t *self, char *format, ...);

//  Get/set the content_type field
char *
    xrap_msg_content_type (xrap_msg_t *self);
void
    xrap_msg_set_content_type (xrap_msg_t *self, char *format, ...);

//  Get/set the content_body field
char *
    xrap_msg_content_body (xrap_msg_t *self);
void
    xrap_msg_set_content_body (xrap_msg_t *self, char *format, ...);

//  Get/set the status_code field
uint16_t
    xrap_msg_status_code (xrap_msg_t *self);
void
    xrap_msg_set_status_code (xrap_msg_t *self, uint16_t status_code);

//  Get/set the location field
char *
    xrap_msg_location (xrap_msg_t *self);
void
    xrap_msg_set_location (xrap_msg_t *self, char *format, ...);

//  Get/set the etag field
char *
    xrap_msg_etag (xrap_msg_t *self);
void
    xrap_msg_set_etag (xrap_msg_t *self, char *format, ...);

//  Get/set the date_modified field
uint64_t
    xrap_msg_date_modified (xrap_msg_t *self);
void
    xrap_msg_set_date_modified (xrap_msg_t *self, uint64_t date_modified);

//  Get/set the resource field
char *
    xrap_msg_resource (xrap_msg_t *self);
void
    xrap_msg_set_resource (xrap_msg_t *self, char *format, ...);

//  Get/set the if_modified_since field
uint64_t
    xrap_msg_if_modified_since (xrap_msg_t *self);
void
    xrap_msg_set_if_modified_since (xrap_msg_t *self, uint64_t if_modified_since);

//  Get/set the if_none_match field
char *
    xrap_msg_if_none_match (xrap_msg_t *self);
void
    xrap_msg_set_if_none_match (xrap_msg_t *self, char *format, ...);

//  Get/set the if_unmodified_since field
uint64_t
    xrap_msg_if_unmodified_since (xrap_msg_t *self);
void
    xrap_msg_set_if_unmodified_since (xrap_msg_t *self, uint64_t if_unmodified_since);

//  Get/set the if_match field
char *
    xrap_msg_if_match (xrap_msg_t *self);
void
    xrap_msg_set_if_match (xrap_msg_t *self, char *format, ...);

//  Get/set the status_text field
char *
    xrap_msg_status_text (xrap_msg_t *self);
void
    xrap_msg_set_status_text (xrap_msg_t *self, char *format, ...);

//  Self test of this class
int
    xrap_msg_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
