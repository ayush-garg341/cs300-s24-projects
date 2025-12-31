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
    uint original_file_size;
    unsigned short int is_dir_reverse;
    unsigned short int backward_seek_count;
    uint internal_file_offset;

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
    ret->original_file_size = io300_filesize(ret);
    ret->is_dir_reverse = 0;
    ret->backward_seek_count = 0;
    ret->stats.read_calls = 0;
    ret->stats.write_calls = 0;
    ret->stats.seeks = 0;
    ret->internal_file_offset = 0;

    check_invariants(ret);
    dbg(ret, "Just finished initializing file from path: %s\n", path);
    return ret;
}

int io300_seek(struct io300_file* const f, off_t const pos) {
    check_invariants(f);
    f->stats.seeks++;

    if(pos < f->logical_file_pos)
    {
        f->backward_seek_count += 1;
        if(f->backward_seek_count > 5)
        {
            f->is_dir_reverse = 1;
        }
    }
    else {
        f->backward_seek_count = 0;
        f->is_dir_reverse = 0;
    }

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

    if((pos == f->original_file_size - 1 || pos == f->original_file_size) && (f->stats.seeks == 1))
    {
        f->is_dir_reverse = 1;
    }
    f->internal_file_offset = pos;
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

    if(f->is_dirty == 1)
    {

        // When you first start writing into cache, not read, in that case, buff_start = buff_end
        // When buffer is fully consumed either by reading or writing or both i.e buff_pos = CACHE_SIZE
        if(f->buff_start == f->buff_end || f->buff_pos == CACHE_SIZE)
        {
            // flush the cache
            int n = io300_flush(f);
            if(n == -1)
            {
                return -1;
            }
        }
    }

    // when cache is either empty or cache is fully consumed i.e buff_pos = 0 or buff_pos = CACHE_SIZE
    if(f->is_dir_reverse == 1)
    {
        if(f->logical_file_pos < f->buff_start || f->buff_start == f->buff_end)
        {
            int n = io300_fetch(f);
            if(n == -1 || n == 0)
            {
                return n;
            }

        }
    }
    else {

        if(f->buff_pos % CACHE_SIZE == 0)
        {
            // fetch the cache from disk
            int n = io300_fetch(f);
            if(n == -1 || n == 0)
            {
                return n;
            }
        }
    }

    // Case: cache is not full, have fewer bytes and we keep reading from cache while the cache is not valid at current position.
    if(f->logical_file_pos >= f->buff_end)
    {
        return -1;
    }

    if(f->is_dir_reverse == 1)
    {
        unsigned char c = (unsigned char)f->cache[f->buff_pos];
        return c;
    }

    unsigned char c = (unsigned char)f->cache[f->buff_pos++];
    f->logical_file_pos += 1;
    return c;
}

int io300_writec(struct io300_file* f, int ch) {
    check_invariants(f);

    // When cache is fully consumed either by reading or writing
    if(f->buff_pos == CACHE_SIZE)
    {
        if(f->is_dirty == 1)
        {
            // flush the cache, cache is dirty
            int n = io300_flush(f);
            if(n == -1)
            {
                return -1;
            }
        }
        else {

            // when cache is fully consumed and not dirty, invalidate the cache
            f->buff_start = f->logical_file_pos;
            f->buff_end = f->logical_file_pos;
            f->buff_pos = 0;
        }
    }

    f->cache[f->buff_pos++] = (char)ch;
    f->logical_file_pos += 1;
    f->is_dirty = 1;
    return ch;
}

ssize_t io300_read(struct io300_file* const f, char* const buff,
                   size_t const sz) {
    check_invariants(f);

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
            if(f->internal_file_offset != seek_pos) {
                off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
                if(pos == -1)
                {
                    return -1;
                }
                f->internal_file_offset = pos;
            }
            int n = read(f->fd, buff, sz);
            if(n == 0 || n == -1)
            {
                return n;
            }
            f->logical_file_pos += n;
            f->buff_start = f->logical_file_pos;
            f->buff_end = f->buff_start;
            f->buff_pos = 0;
            f->internal_file_offset = f->logical_file_pos;
            return n;
        }
        else {
            // First populate the cache and then return from it.
            // Change the kernel file pointer to correct offset.
            off_t seek_pos = (off_t)f->logical_file_pos;
            if(f->internal_file_offset != seek_pos) {

                off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
                if(pos == -1)
                {
                    return -1;
                }
                f->internal_file_offset = pos;
            }
            uint original_pos = f->logical_file_pos;
            uint boundary_found = 0;
            if(f->is_dir_reverse == 1 && sz > 1 && (original_pos != f->original_file_size - 1 && original_pos != f->original_file_size))
            {
                f->logical_file_pos += sz;
                boundary_found = 1;
            }

            int n = io300_fetch(f);
            if(n == 0 || n == -1)
            {
                return 0;
            }

            if(boundary_found == 1)
            {
                f->logical_file_pos = original_pos;
                f->buff_pos -= sz;
            }

            valid_last_byte_index = n;
            // In this case setting this variable to the number of bytes read.
        }
    }


    // It might be possible that cache has only 3 valid bytes left with buff end = 3 and buff pos = 0, reading more than 3 bytes will give garbage data, so need to have a check of valid bytes return. It's not always possible to return sz bytes correctly from cache.
    int valid_read = sz;
    if(f->is_dir_reverse == 1)
    {
        valid_last_byte_index -= 1;
        if(f->buff_pos > valid_last_byte_index)
        {
            valid_read = 0;
        }
        memcpy(buff, &f->cache[f->buff_pos], valid_read);
    }
    else {
        if (sz > valid_last_byte_index - f->buff_pos) {
            valid_read = valid_last_byte_index - f->buff_pos;
        }
        memcpy(buff, &f->cache[f->buff_pos], valid_read);
        f->buff_pos += valid_read;
        f->logical_file_pos += valid_read;
    }
    return (ssize_t)valid_read;
}

ssize_t io300_write(struct io300_file* const f, const char* buff,
                    size_t const sz) {
    check_invariants(f);

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
            if(f->internal_file_offset != seek_pos) {
                off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
                if(pos == -1)
                {
                    return -1;
                }
                f->internal_file_offset = pos;
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
            f->internal_file_offset = f->logical_file_pos;
            return n;
        }
        else {
            // Write into cache by seeking into correct position
            off_t seek_pos = (off_t)f->logical_file_pos;
            if(f->internal_file_offset != seek_pos) {
                off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
                if(pos == -1)
                {
                    return -1;
                }
                f->internal_file_offset = pos;
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
    if(f->internal_file_offset != seek_pos) {
        if(pos == -1)
        {
            return -1;
        }
        f->internal_file_offset = pos;
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
    f->internal_file_offset = f->logical_file_pos;
    f->original_file_size = io300_filesize(f);
    return n;
}

int io300_fetch(struct io300_file* const f) {
    check_invariants(f);
    /* This helper should contain the logic for fetching data from the file into the cache. */
    /* Think about how you can use this helper to refactor out some of the logic in your read, write, and seek functions! */
    /* Feel free to add arguments if needed. */

    if(f->is_dir_reverse == 1)
    {
        // It means we are at the end of the file and caching in forward direction does not make sense.
        int valid_chars_left = f->logical_file_pos >= CACHE_SIZE ? CACHE_SIZE : f->logical_file_pos + 1;
        off_t seek_pos = f->logical_file_pos >= CACHE_SIZE ? f->logical_file_pos - CACHE_SIZE + 1: 0;
        if(f->internal_file_offset != seek_pos) {
            off_t pos = lseek(f->fd, seek_pos, SEEK_SET);
            if(pos == -1)
            {
                return -1;
            }
            f->internal_file_offset = pos;
        }

        int n = read(f->fd, f->cache, valid_chars_left);
        if(n == -1 || n == 0)
        {
            return -1;
        }
        // if we get 0, it means we have reached the EOF and in this case return -1.
        f->buff_start = seek_pos;
        f->buff_end = f->buff_start + n;
        f->buff_pos = f->logical_file_pos - f->buff_start;
        f->internal_file_offset = f->buff_end;
        return n;
    }

    int n = read(f->fd, f->cache, CACHE_SIZE);

    // if we get 0, it means we have reached the EOF and in this case return -1.
    if(n == -1 || n == 0)
    {
        return -1;
    }
    f->buff_pos = 0;
    f->buff_start = f->logical_file_pos;
    f->buff_end = f->buff_start + n;
    f->internal_file_offset = f->buff_end;
    return n;
}
