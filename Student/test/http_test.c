#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "http.h"

/*
./http_test www.thomas-bayer.com sqlrest/CUSTOMER/3/
Header:
HTTP/1.1 200 OK
Server: Apache-Coyote/1.1
Content-Type: application/xml
Date: Tue, 02 Sep 2014 04:47:16 GMT
Connection: close
Content-Length: 235

Content:
<?xml version="1.0"?><CUSTOMER xmlns:xlink="http://www.w3.org/1999/xlink">
    <ID>3</ID>
    <FIRSTNAME>Michael</FIRSTNAME>
    <LASTNAME>Clancy</LASTNAME>
    <STREET>542 Upland Pl.</STREET>
    <CITY>San Francisco</CITY>
</CUSTOMER>
*/

int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "usage: ./http_test host page\n");
        exit(1);
    }

    Buffer *response = http_query(argv[1], argv[2], "", 80);
    if (response) {
        char *content = http_get_content(response);
        int header_length = content - response->data;

        printf("header length: %d\n", header_length);

        printf("Header:\n%.*s\n\n", header_length - 4, response->data);
        printf("Content:\n%s\n", content);

        // Print binary data
//        printf("Content:\n");
//        int content_length = response->length - header_length;
//        printf("%d\n", content_length);
//        for (int i = 0; i < content_length; i++) {
//            printf("%08x", content[i]);
//        }

        // Write data to local file
//        int fd = open("downloaded", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
//        int content_length = response->length - header_length;
//        write(fd, content, content_length);
//
//        buffer_free(response);
    }

    return 0;
}
