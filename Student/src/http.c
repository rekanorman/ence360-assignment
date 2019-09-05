#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "http.h"

#define BUF_SIZE  1024
#define HTTP_PORT 80


/**
 * Attempts to create a new stream socket and connect it to the server with
 * the given host name and port number. Returns the new socket file
 * descriptor on success, -1 otherwise.
 *
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param port - The port number e.g. 80
 * @return File descriptor of the new socket, or -1 on failure.
 */
int connect_to_server(char* host, int port) {
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
        fprintf(stderr, "getaddrinfo %s: %s\n", host, gai_strerror(status));
        return -1;
    }

    // Create a stream socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    // Connect to the server
    if (connect(sock, server_addr->ai_addr, server_addr->ai_addrlen) == -1) {
        perror("connect");
        return -1;
    }

    freeaddrinfo(server_addr);

    return sock;
}


/**
 * Constructs an HTTP GET request for the given page and byte range, and
 * sends this request via the given socket. Returns 0 on success, -1 on
 * failure.
 *
 * @param sock - File descriptor of the socket to send the request through.
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param page - The page to request e.g. index.html
 * @param range - Byte range e.g. 0-500.
 * @return 0 on success, -1 on failure.
 */
int send_http_request(int sock, char* host, char* page, const char* range) {
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
        return -1;
    }

    return 0;
}


/**
 * Receives an HTTP response from the given socket, reading data until EOF
 * is received. On success, returns a pointer to a buffer holding the response
 * data, otherwise returns NULL.
 *
 * @param sock - File descriptor of the socket to receive data from.
 * @return Pointer to a buffer holding the response data from the query,
 *         or NULL on failure.
 */
Buffer* receive_response(int sock) {
    // Create a Buffer to hold the response
    Buffer* response = malloc(sizeof(Buffer));
    response->data = (char*)malloc(BUF_SIZE);
    response->length = 0;

    // Receive and write data to the buffer, calling realloc as needed.
    char temp[BUF_SIZE];  // temporary buffer to read data into
    int bytes_read;
    int buffer_size = BUF_SIZE;   // current size of response->data

    do {
        bytes_read = read(sock, temp, BUF_SIZE);
        if (bytes_read == -1) {
            perror("read");
            return NULL;
        }

        // Check if we have enough space left in response->data
        if (buffer_size - response->length < bytes_read) {
            response->data = (char*)realloc(response->data, buffer_size * 2);
            buffer_size *= 2;
        }

        memcpy(&response->data[response->length], temp, bytes_read);
        response->length += bytes_read;

    } while (bytes_read > 0);

    return response;
}


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
    int sock = connect_to_server(host, port);
    if (sock == -1) {
        return NULL;
    }

    if (send_http_request(sock, host, page, range) != 0) {
        return NULL;
    }

    return receive_response(sock);
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
        return http_query(host, page, range, HTTP_PORT);
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
    // Extract the hostname and page from the given url
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);
    char* page = strstr(host, "/");

    if (!page) {
        fprintf(stderr, "could not split url into host/page %s\n", url);
        exit(1);
    } else {
        page[0] = 0;
        page++;
    }

    // Create a socket and connect to the server
    int sock = connect_to_server(host, HTTP_PORT);
    if (sock == -1) {
        fprintf(stderr, "failed to connect to server\n");
        exit(1);
    }

    // Construct and send a HEAD request
    char request[BUF_SIZE];
    snprintf(request, BUF_SIZE,
             "HEAD /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: getter\r\n\r\n",
             page, host);

    if (write(sock, request, strlen(request)) == -1) {
        perror("write");
        exit(1);
    }

    // Receive the response from the server
    int max_header_size = 8192;  // 8K is a common HTTP header size limit
    char response[max_header_size];

    int bytes_read = read(sock, response, max_header_size);
    if (bytes_read == -1) {
        perror("read");
        exit(1);
    }

    // Extract the content length
    char* content_length_field = strstr(response, "Content-Length:");
    if (!content_length_field) {
        fprintf(stderr, "No Content-Length field in response from: %s\n", url);
        exit(1);
    }

    int content_length = atoi(content_length_field + strlen("Content-Length: "));
    printf("%s\n\n", response);
    printf("%d\n", content_length);

    // To get the chunk size, divide total length by number of threads
    // and round up.
    max_chunk_size = (content_length + threads - 1) / threads;
    printf("%d\n", max_chunk_size);

    return threads;
}

int get_max_chunk_size() {
    return max_chunk_size;
}
