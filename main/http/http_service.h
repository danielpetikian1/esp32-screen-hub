#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP_REQ_GET = 0,
    // later: HTTP_REQ_POST, etc
} http_method_t;

/* Request message sent to the HTTP owner task */
typedef struct {
    uint32_t request_id;
    http_method_t method;
    char url[256];

    /* Optional: if non-NULL, owner sends a response back */
    QueueHandle_t reply_queue;
} http_req_t;

/* Response message sent back to requester (if reply_queue provided) */
typedef struct {
    uint32_t request_id;
    esp_err_t err;
    int http_status;
    int content_length;
} http_resp_t;

/* Start the owner task + create the request queue */
void http_service_start(void);

/* Get the queue to send requests to */
QueueHandle_t http_service_queue(void);

#ifdef __cplusplus
}
#endif
