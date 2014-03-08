/*  =========================================================================
    xrap_msg - xrap serialization
    
    Generated codec header for xrap_msg
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

#ifndef __XRAP_MSG_H_INCLUDED__
#define __XRAP_MSG_H_INCLUDED__

/*  These are the xrap_msg messages
    METHOD - Some description.
        whatever      string
        headers       dictionary
        content       frame
*/


#define XRAP_MSG_METHOD                     1

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

//  Send the METHOD to the output in one step
int
    xrap_msg_send_method (void *output,
        char *whatever,
        zhash_t *headers,
        zframe_t *content);
    
//  Duplicate the xrap_msg message
xrap_msg_t *
    xrap_msg_dup (xrap_msg_t *self);

//  Print contents of message to stdout
void
    xrap_msg_dump (xrap_msg_t *self);

//  Get/set the message address
zframe_t *
    xrap_msg_address (xrap_msg_t *self);
void
    xrap_msg_set_address (xrap_msg_t *self, zframe_t *address);

//  Get the xrap_msg id and printable command
int
    xrap_msg_id (xrap_msg_t *self);
void
    xrap_msg_set_id (xrap_msg_t *self, int id);
char *
    xrap_msg_command (xrap_msg_t *self);

//  Get/set the whatever field
char *
    xrap_msg_whatever (xrap_msg_t *self);
void
    xrap_msg_set_whatever (xrap_msg_t *self, char *format, ...);

//  Get/set the headers field
zhash_t *
    xrap_msg_headers (xrap_msg_t *self);
void
    xrap_msg_set_headers (xrap_msg_t *self, zhash_t *headers);
    
//  Get/set a value in the headers dictionary
char *
    xrap_msg_headers_string (xrap_msg_t *self, char *key, char *default_value);
uint64_t
    xrap_msg_headers_number (xrap_msg_t *self, char *key, uint64_t default_value);
void
    xrap_msg_headers_insert (xrap_msg_t *self, char *key, char *format, ...);
size_t
    xrap_msg_headers_size (xrap_msg_t *self);

//  Get/set the content field
zframe_t *
    xrap_msg_content (xrap_msg_t *self);
void
    xrap_msg_set_content (xrap_msg_t *self, zframe_t *frame);

//  Self test of this class
int
    xrap_msg_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
