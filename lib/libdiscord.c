#include <stdlib.h>
#include <libwebsockets.h>
#include <jansson.h>
#include "libdiscord.h"


struct ld_context *ld_create_context_via_info(struct ld_context_info *info) {
    //assuming the values passed in are good
    struct ld_context *context;
    context = malloc(sizeof(struct ld_context));

    context->log_level = info->log_level;

    context->gateway_state = LD_GATEWAY_UNCONNECTED;

    context->user_callback = info->user_callback;

    /* curl init */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    context->curl_multi_handle=curl_multi_init();

    if(info->bot_token == NULL) {
        return NULL;
    }
    return context;
}

void ld_destroy_context(struct ld_context *context) {
    curl_multi_cleanup(context->curl_multi_handle);
    curl_global_cleanup();
    free(context);
}

void ld_err(struct ld_context *context, const char *message, ...) {
    if((ld_log_error & context->log_level) != 0) {
        va_list myargs;
        va_start(myargs, message);
        _ld_log(ld_log_error, context->log_level, message, myargs);
        va_end(myargs);
    }
}

void ld_warn(struct ld_context *context, const char *message, ...) {
    if((ld_log_warning & context->log_level) != 0) {
        va_list myargs;
        va_start(myargs, message);
        _ld_log(ld_log_warning, context->log_level, message, myargs);
        va_end(myargs);
    }
}
void ld_info(struct ld_context *context, const char *message, ...) {
    if((ld_log_info & context->log_level) != 0) {
        va_list myargs;
        va_start(myargs, message);
        _ld_log(ld_log_info, context->log_level, message, myargs);
        va_end(myargs);
    }
}
void ld_notice(struct ld_context *context, const char *message, ...) {
    if((ld_log_notice & context->log_level) != 0) {
        va_list myargs;
        va_start(myargs, message);
        _ld_log(ld_log_notice, context->log_level, message, myargs);
        va_end(myargs);
    }
}
void ld_debug(struct ld_context *context, const char *message, ...) {\
    if((ld_log_debug & context->log_level) != 0) {\
        va_list myargs;
        va_start(myargs, message);
        _ld_log(ld_log_debug, context->log_level, message, myargs);
        va_end(myargs);
    }
}

int ld_gateway_connection_state(struct ld_context *context) {
    return context->gateway_state;
}

struct _ld_buffer {
    char *string;
    size_t size;
    struct ld_context *context;
};

size_t _ld_curl_response_string(void *contents, size_t size, size_t nmemb, void *userptr){
    size_t recieved_size = size * nmemb;
    struct _ld_buffer *buffer = (struct _ld_buffer *) userptr;

    buffer->string = realloc(buffer->string, buffer->size + recieved_size + 1);
    if(buffer->string == NULL) {
        ld_err(buffer->context,
               "realloc: couldn't allocate memory for curl response string in ld_connect!");
    }

    memcpy(&(buffer->string[buffer->size]), contents, recieved_size);
    buffer->size += recieved_size;
    buffer->string[buffer->size] = '\0';

    return recieved_size;
}

int ld_connect(struct ld_context *context) {
    int ret;

    //check to see if we can even connect to Discord's servers
    CURL *handle;
    struct _ld_buffer buffer;

    buffer.string = malloc(1);
    buffer.size = 0;
    buffer.context = context;
    handle = curl_easy_init();
    if(handle == NULL) {
        //something went wrong trying to create the easy handle.
        ld_err(context, "curl: couldn't init easy handle");
        return 2;
    }

    curl_easy_setopt(handle, CURLOPT_URL, LD_API_URL "/gateway");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _ld_curl_response_string);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&buffer);

    ret = curl_easy_perform(handle);

    if(ret != CURLE_OK) {
        ld_err(context, "curl: couldn't get gateway url from /gateway");
        return 2;
    }
    curl_easy_cleanup(handle);

    ld_debug(context, "received data from /gateway: \n%s", buffer.string);

    //use jansson to extract the JSON data
    json_t *object;
    json_error_t error;

    object = json_loads(buffer.string, 0, &error);
    if(object == NULL) {
        ld_err(context, "jansson: couldn't decode string returned "
                "from /gateway in ld_connext: %s", buffer.string);
        return 3;
    }

    if(json_string_value(object) == NULL) {
        ld_err(context, "jansson: didn't receive string object from "
                "JSON payload received from gateway");
        return 3;
    }
    //we got a valid response from the REST API, which should mean Discord is connectable at basic level

    //check the bot token's validity
        //GET /gateway/bot
    //connect to the websocket
    if((context->gateway_state == LD_GATEWAY_UNCONNECTED)
       || (context->gateway_state == LD_GATEWAY_DISCONNECTED)) {
        //we're not connected, so we should connect
        context->gateway_state = LD_GATEWAY_CONNECTING;
        return 0;
    }
    //we're already connected...
    return 1;
}


