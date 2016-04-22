#include "response.h"
#include <stdlib.h>
#include <strings.h>


// The key of (extension, mime-type) pair should be kept in lexicographical order.
// As we will use simple binary search on this array to get the specifical mime-type.
// A little more attention when add more (extension, mime-type) items.
static const char* mime_map[][2] = {
    {"css",     "text/css"},
    {"htm",     "text/html"},
    {"html",    "text/html"},
    {"gif",     "image/gif"},
    {"ico",     "image/x-icon"},
    {"jpeg",    "image/jpeg"},
    {"jpg",     "image/jpeg"},
    {"svg",     "image/svg+xml"},
    {"txt",     "text/plain"},
    {"zip",     "application/zip"},
};
static const size_t mime_num = sizeof(mime_map) / sizeof(mime_map[0]);

/*
static const char* get_mime_type(const char* ext);;
static const char* status_repr(int status);

// Binary search to get corresponding mime-type,
// return NULL when the extension is not supported.
static const char* get_mime_type(const char* ext)
{
    int begin = 0, end = mime_num;
    while (begin <= end) {
        int mid = (end - begin) / 2 + begin;
        int res = strcasecmp(ext, mime_map[mid][0]);
        if (res == 0)
            return mime_map[mid][1];
        else if (res > 0)
            begin = mid + 1;
        else
            end = mid - 1;
    }
    return NULL;
}

static const char* status_repr(int status)
{
    switch(status) {
#       include "status.inc"
    }
    return "(null)";
}
*/

void response_init(Response* response)
{

}

void response_clear(Response* response)
{
    response_init(response);
}