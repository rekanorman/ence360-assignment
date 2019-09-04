#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "http.h"

#define BUF_SIZE 1024


/**
 * Perform an HTTP 1.0 query to a given host and page and port number.
 * host is a hostname and page is a path on the remote server. The query
 * will attempt to retrieve content in the given byte range.
 * User is responsible for freeing the memory.
 * 
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param page - e.g. /index.html
 * @param range - Byte range e.g. 0-500. NOTE: A server may not respect this
 * @param port - e.g. 80
 * @return Buffer - Pointer to a buffer holding response data from query
 *                  NULL is returned on failure.
 */
Buffer* http_query(char *host, char *page, const char *range, int port) {
    // Convert port number to a string
    char port_string[BUF_SIZE];
    snprintf(port_string, BUF_SIZE, "%d", port);

    // Use getaddrinfo to get the server address from host and port.
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* server_addr;
    int status;
    if ((status = getaddrinfo(host, port_string, &hints, &server_addr)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // Create a stream socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(1);
    }

    // Connect to the server
    if (connect(sock, server_addr->ai_addr, server_addr->ai_addrlen) == -1) {
        perror("connect");
        exit(1);
    }

    freeaddrinfo(server_addr);

    // Construct a GET request
    char request[BUF_SIZE];

    char range_string[BUF_SIZE] = {0};  // empty string
    if (range && strlen(range) > 0) {
        snprintf(range_string, BUF_SIZE, "Range: bytes=%s\r\n", range);
    }

    snprintf(request, BUF_SIZE,
            "GET /%s HTTP/1.0\r\nHost: %s\r\n%sUser-Agent: getter\r\n\r\n",
            page, host, range_string);

    // Write the request to the socket
    if (write(sock, request, strlen(request)) == -1) {
        perror("write");
        exit(1);
    }

    // Create a Buffer to hold the response
    Buffer* response = malloc(sizeof(Buffer));
    response->data = (char*)malloc(BUF_SIZE);
    response->length = 0;

    // Receive and write data to the buffer, calling realloc as needed.
    char buffer[BUF_SIZE];
    int bytes_read;
    int buffer_size = BUF_SIZE;   // current size of response->data

    do {
        bytes_read = read(sock, buffer, BUF_SIZE);
        if (bytes_read == -1) {
            perror("read");
            exit(1);
        }

        // Check if we have enough space left in response->data
        if (buffer_size - response->length < bytes_read) {
            response->data = (char*)realloc(response->data, buffer_size * 2);
            buffer_size *= 2;
            printf("%d\n", buffer_size);
        }

        memcpy(&response->data[response->length], buffer, bytes_read);
        response->length += bytes_read;

    } while (bytes_read > 0);

    printf("%ld\n", response->length);

    return response;
}


/**
 * Separate the content from the header of an http request.
 * NOTE: returned string is an offset into the response, so
 * should not be freed by the user. Do not copy the data.
 * @param response - Buffer containing the HTTP response to separate 
 *                   content from
 * @return string response or NULL on failure (buffer is not HTTP response)
 */
char* http_get_content(Buffer *response) {

    char* header_end = strstr(response->data, "\r\n\r\n");

    if (header_end) {
        return header_end + 4;
    }
    else {
        return response->data;
    }
}


/**
 * Splits an HTTP url into host, page. On success, calls http_query
 * to execute the query against the url. 
 * @param url - Webpage url e.g. learn.canterbury.ac.nz/profile
 * @param range - The desired byte range of data to retrieve from the page
 * @return Buffer pointer holding raw string data or NULL on failure
 */
Buffer *http_url(const char *url, const char *range) {
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);

    char *page = strstr(host, "/");
    
    if (page) {
        page[0] = '\0';

        ++page;
        return http_query(host, page, range, 80);
    }
    else {

        fprintf(stderr, "could not split url into host/page %s\n", url);
        return NULL;
    }
}


/**
 * Makes a HEAD request to a given URL and gets the content length
 * Then determines max_chunk_size and number of split downloads needed
 * @param url   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int get_num_tasks(char *url, int threads) {
   assert(0 && "not implemented yet!");
}


int get_max_chunk_size() {
    return max_chunk_size;
}
