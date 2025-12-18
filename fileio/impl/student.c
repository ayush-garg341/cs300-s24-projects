#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../io300.h"

/*
    student.c
    Fill in the following stencils
*/

/*
    When starting, you might want to change this for testing on small files.
*/
#ifndef CACHE_SIZE
#define CACHE_SIZE 8
#endif

#if (CACHE_SIZE < 4)
#error "internal cache size should not be below 4."
#error "if you changed this during testing, that is fine."
#error "when handing in, make sure it is reset to the provided value"
#error "if this is not done, the autograder will not run"
#endif

/*
   This macro enables/disables the dbg() function. Use it to silence your
   debugging info.
   Use the dbg() function instead of printf debugging if you don't want to
   hunt down 30 printfs when you want to hand in
*/
#define DEBUG_PRINT 0
#define DEBUG_STATISTICS 1

struct io300_file {
    /* read,write,seek all take a file descriptor as a parameter */
    int fd;
    /* this will serve as our cache */
    char* cache;

    // TODO: Your properties go here
    uint buff_start;
    uint buff_end;
    uint buff_pos;
    uint logical_file_pos;
    unsigned short int  is_dirty;

    /* Used for debugging, keep track of which io300_file is which */
    char* description;
    /* To tell if we are getting the performance we are expecting */
    struct io300_statistics {
        int read_calls;
        int write_calls;
        int seeks;
    } stats;
};

/*
    Assert the properties that you would like your file to have at all times.
    Call this function frequently (like at the beginning of each function) to
    catch logical errors early on in development.
*/
static void check_invariants(struct io300_file* f) {
    assert(f != NULL);
    assert(f->cache != NULL);
    assert(f->fd >= 0);

    // TODO: Add more invariants
    assert(f->buff_pos <= CACHE_SIZE);
}

/*
    Wrapper around printf that provides information about the
    given file. You can silence this function with the DEBUG_PRINT macro.
*/
static void dbg(struct io300_file* f, char* fmt, ...) {
    (void)f;
    (void)fmt;
#if (DEBUG_PRINT == 1)
    static char buff[300];
    size_t const size = sizeof(buff);
    int n = snprintf(buff, size,
                     // TODO: Add the fields you want to print when debugging
                     "{desc:%s, } -- ", f->description);
    int const bytes_left = size - n;
    va_list args;
    va_start(args, fmt);
    vsnprintf(&buff[n], bytes_left, fmt, args);
    va_end(args);
    printf("%s", buff);
#endif
}

struct io300_file* io300_open(const char* const path, char* description) {
    if (path == NULL) {
        fprintf(stderr, "error: null file path\n");
        return NULL;
    }

    int const fd = open(path, O_RDWR | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        fprintf(stderr, "error: could not open file: `%s`: %s\n", path,
                strerror(errno));
        return NULL;
    }

    struct io300_file* const ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        fprintf(stderr, "error: could not allocate io300_file\n");
        close(fd);
        return NULL;
    }

    ret->fd = fd;
    ret->cache = malloc(CACHE_SIZE);
    if (ret->cache == NULL) {
        fprintf(stderr, "error: could not allocate file cache\n");
        close(ret->fd);
        free(ret);
        return NULL;
    }
    ret->description = description;
    // TODO: Initialize your file
    ret->buff_start = 0;
    ret->buff_end = 0;
    ret->buff_pos = 0;
    ret->is_dirty = 0;
    ret->logical_file_pos = 0;

    check_invariants(ret);
    dbg(ret, "Just finished initializing file from path: %s\n", path);
    return ret;
}

int io300_seek(struct io300_file* const f, off_t const pos) {
    check_invariants(f);
    f->stats.seeks++;

    // If offset pos is out of valid cache range
    if(pos < f->buff_start || pos >= f->buff_end)
    {
        // if dirty, flush it
        if(f->is_dirty == 1)
        {
            int n = io300_flush(f);
            if(n == -1)
            {
                return -1;
            }

        }
        // Invalidate the cache
        f->buff_start = pos;
        f->buff_end = pos;
        f->buff_pos = 0;
        f->logical_file_pos = pos;
    }
    else {
        f->buff_pos = pos - f->buff_start;
        f->logical_file_pos = pos;
    }
    return lseek(f->fd, pos, SEEK_SET);
}

int io300_close(struct io300_file* const f) {
    check_invariants(f);

#if (DEBUG_STATISTICS == 1)
    printf("stats: {desc: %s, read_calls: %d, write_calls: %d, seeks: %d}\n",
           f->description, f->stats.read_calls, f->stats.write_calls,
           f->stats.seeks);
#endif

    if(f->is_dirty == 1)
    {
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }

    }
    close(f->fd);
    free(f->cache);
    free(f);
    return 0;
}

off_t io300_filesize(struct io300_file* const f) {
    check_invariants(f);
    struct stat s;
    int const r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}

int io300_readc(struct io300_file* const f) {
    check_invariants(f);

    // When you first start writing into cache, not read
    if(f->buff_start == f->buff_end && f->is_dirty == 1)
    {
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }
    else if(f->buff_pos == CACHE_SIZE && f->is_dirty == 1)
    {
        // Cache is full and dirty
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }

    // when cache is either empty or cache is fully consumed
    if(f->buff_pos == 0 || f->buff_pos == CACHE_SIZE)
    {
        // fetch the cache from disk
        int n = io300_fetch(f);
        if(n == -1 || n == 0)
        {
            return n;
        }
    }

    // Case: cache is not full, have fewer bytes and we keep reading from cache while the cache is not valid at current position.
    if(f->logical_file_pos >= f->buff_end)
    {
        return -1;
    }

    unsigned char c = (unsigned char)f->cache[f->buff_pos];
    f->buff_pos += 1;
    f->logical_file_pos += 1;
    return c;
}

int io300_writec(struct io300_file* f, int ch) {
    check_invariants(f);

    // when cache is full and dirty
    if(f->buff_pos == CACHE_SIZE && f->is_dirty == 1)
    {
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }
    else if(f->buff_pos == CACHE_SIZE)
    {
        // when cache is fully consumed and not dirty

        f->buff_start = f->logical_file_pos;
        f->buff_end = f->logical_file_pos;
        f->buff_pos = 0;
    }

    f->cache[f->buff_pos] = (char)ch;
    f->buff_pos += 1;
    f->logical_file_pos += 1;
    f->is_dirty = 1;
    return ch;
}

ssize_t io300_read(struct io300_file* const f, char* const buff,
                   size_t const sz) {
    check_invariants(f);

    // When you first start writing into cache, not read
    if(f->buff_start == f->buff_end && f->is_dirty == 1)
    {
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }
    else if(f->buff_pos == CACHE_SIZE && f->is_dirty == 1)
    {
        // Cache is full and dirty
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }

    uint valid_last_byte_index = f->buff_end > f->buff_start ? f->buff_end - f->buff_start: 0;
    // meaning number of bytes read successfully

    // Checking bytes that are in the valid cache
    uint valid_bytes_cache = valid_last_byte_index > f->buff_pos ? valid_last_byte_index - f->buff_pos : 0;
    if(sz > valid_bytes_cache)
    {
        // Cache is dirty
        if(f->is_dirty)
        {
            // flush the cache
            int n = io300_flush(f);
            if(n == -1)
            {
                return -1;
            }

        }

        // Reading more than or equal to cache size.
        if(sz >= CACHE_SIZE)
        {
            // Change the kernel file pointer to correct offset.
            off_t seek_pos = (off_t)f->logical_file_pos;
            off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
            if(pos == -1)
            {
                return -1;
            }
            int n = read(f->fd, buff, sz);
            if(n == 0 || n == -1)
            {
                return n;
            }
            f->logical_file_pos += n;
            f->buff_start = f->logical_file_pos;
            f->buff_end = 0;
            f->buff_pos = 0;
            return n;
        }
        else {
            // First populate the cache and then return from it.
            // Change the kernel file pointer to correct offset.
            off_t seek_pos = (off_t)f->logical_file_pos;
            off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
            if(pos == -1)
            {
                return -1;
            }
            int n = io300_fetch(f);
            if(n == 0 || n == -1)
            {
                return 0;
            }
            valid_last_byte_index = n;
            // In this case setting this variable to the number of bytes read.
        }
    }


    // It might be possible that cache has only 3 valid bytes left with buff end = 3 and buff pos = 0, reading more than 3 bytes will give garbage data, so need to have a check of valid bytes return. It's not always possible to return sz bytes correctly from cache.
    int valid_read = sz;
    if (sz > valid_last_byte_index - f->buff_pos) {
        valid_read = valid_last_byte_index - f->buff_pos;
    }
    memcpy(buff, &f->cache[f->buff_pos], valid_read);
    f->buff_pos += valid_read;
    f->logical_file_pos += valid_read;
    return (ssize_t)valid_read;
}

ssize_t io300_write(struct io300_file* const f, const char* buff,
                    size_t const sz) {
    check_invariants(f);
    // when cache is full and dirty
    if(f->buff_pos == CACHE_SIZE && f->is_dirty == 1)
    {
        // flush the cache
        int n = io300_flush(f);
        if(n == -1)
        {
            return -1;
        }
    }
    else if(f->buff_pos == CACHE_SIZE)
    {
        // when cache is fully consumed and not dirty

        f->buff_start = f->logical_file_pos;
        f->buff_end = f->logical_file_pos;
        f->buff_pos = 0;
    }

    // Checking bytes that are in the valid cache
    uint valid_bytes_cache = CACHE_SIZE - f->buff_pos;
    if(sz > valid_bytes_cache)
    {
        // Cache is dirty
        if(f->is_dirty)
        {
            // flush the cache
            int n = io300_flush(f);
            if(n == -1)
            {
                return -1;
            }

        }

        if(sz >= CACHE_SIZE)
        {
            // Write directly into a file and invalidate the cache by seeking to correct pos
            off_t seek_pos = (off_t)f->logical_file_pos;
            off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
            if(pos == -1)
            {
                return -1;
            }
            int n = write(f->fd, buff, sz);
            if(n == 0 || n == -1)
            {
                return n;
            }
            f->buff_pos = 0;
            f->logical_file_pos += n;
            f->buff_start = f->logical_file_pos;
            f->buff_end = f->buff_start;
            return n;
        }
        else {
            // Write into cache by seeking into correct position
            off_t seek_pos = (off_t)f->logical_file_pos;
            off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
            if(pos == -1)
            {
                return -1;
            }
            f->buff_pos = 0;
            f->buff_start = f->logical_file_pos;
            f->buff_end = f->buff_start;
        }
    }

    memcpy(&f->cache[f->buff_pos], buff, sz);
    f->buff_pos += sz;
    f->logical_file_pos += sz;
    f->is_dirty = 1;

    return (ssize_t)sz;
}

int io300_flush(struct io300_file* const f) {
    check_invariants(f);
    off_t seek_pos = (off_t)f->buff_start;
    off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
    if(pos == -1)
    {
        return -1;
    }
    int n = write(f->fd, f->cache, f->buff_pos);
    if(n == -1)
    {
        return -1;
    }
    f->buff_start = f->buff_start + n;
    f->buff_end = f->buff_start;
    f->logical_file_pos = f->buff_start;
    f->buff_pos = 0;
    f->is_dirty = 0;
    return n;
}

int io300_fetch(struct io300_file* const f) {
    check_invariants(f);
    /* This helper should contain the logic for fetching data from the file into the cache. */
    /* Think about how you can use this helper to refactor out some of the logic in your read, write, and seek functions! */
    /* Feel free to add arguments if needed. */
    int n = read(f->fd, f->cache, CACHE_SIZE);

    // if we get 0, it means we have reached the EOF and in this case return -1.
    if(n == -1 || n == 0)
    {
        return -1;
    }
    f->buff_pos = 0;
    f->buff_start = f->logical_file_pos;
    f->buff_end = f->buff_start + n;
    return n;
}
