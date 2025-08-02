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
#include <stdbool.h>

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
#define DEBUG_PRINT 1
#define DEBUG_STATISTICS 1

struct io300_file {
    /* read,write,seek all take a file descriptor as a parameter */
    int fd;
    /* this will serve as our cache */
    char* cache;

    // TODO: Your properties go here
    size_t file_offset;
    size_t buff_pos;
    size_t buff_end;
    bool is_dirty;
    size_t cache_start_file_offset;
    size_t cache_dirty_chars;
    bool write_mode;


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
    assert(f->buff_end <= CACHE_SIZE);
    assert(f->cache_dirty_chars <= CACHE_SIZE);
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
    ret->file_offset = 0;
    ret->buff_pos = 0;
    ret->buff_end = 0;
    ret->cache_start_file_offset = 0;
    ret->cache_dirty_chars = 0;
    ret->is_dirty = false;
    ret->write_mode = false;

    ret->stats.read_calls = 0;
    ret->stats.write_calls = 0;
    ret->stats.seeks = 0;

    check_invariants(ret);
    dbg(ret, "Just finished initializing file from path: %s\n", path);
    return ret;
}

int io300_seek(struct io300_file* const f, off_t const pos) {
    check_invariants(f);
    f->stats.seeks++;

    // TODO: Implement this
    return lseek(f->fd, pos, SEEK_SET);
}

int io300_close(struct io300_file* const f) {
    check_invariants(f);

    // TODO: Implement this
    // Flush the cache if it's dirty
    if(f->is_dirty && f->write_mode == true)
    {
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
    }

#if (DEBUG_STATISTICS == 1)
    printf("stats: {desc: %s, read_calls: %d, write_calls: %d, seeks: %d}\n",
           f->description, f->stats.read_calls, f->stats.write_calls,
           f->stats.seeks);
#endif
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

    // If cache is dirty, flush it and reload the cache, so that appropriate value is read.
    // Switching from writing to reading, flushing cache
    if(f->is_dirty && f->write_mode == true)
    {
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
        f->write_mode = false;
    }

    // Identifying that cache is empty or full and need reloading with new data
    if(f->buff_end == 0 || (f->buff_pos >= f->buff_end))
    {
        int fetched = io300_fetch(f);
        if(fetched == -1)
        {
            return -1;
        }
    }

    return (unsigned char)f->cache[f->buff_pos++];
}


int io300_writec(struct io300_file* f, int ch) {
    check_invariants(f);

    char const c = (char)ch;

    // Switching from read to write
    if(!f->is_dirty && f->buff_pos == CACHE_SIZE && f->write_mode == false)
    {
        f->buff_pos = 0;
    }


    // If cache is full and dirty, flush it
    if(f->is_dirty && f->buff_pos == CACHE_SIZE)
    {
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
    }

    // Else going down the happy path

    f->cache[f->buff_pos++] = c;
    f->is_dirty = true;
    f->cache_dirty_chars += 1;
    f->write_mode = true;

    return ch;
}

ssize_t io300_read(struct io300_file* const f, char* const buff,
                   size_t const sz) {
    check_invariants(f);
    // TODO: Implement this
    return read(f->fd, buff, sz);
}
ssize_t io300_write(struct io300_file* const f, const char* buff,
                    size_t const sz) {
    check_invariants(f);
    // TODO: Implement this
    return write(f->fd, buff, sz);
}

int io300_flush(struct io300_file* const f) {
    check_invariants(f);

    // Flush the cache in a file
    // Increment the user maintained file offset by those dirty characters, it should be matching with internal file offset maintained by kernel.

    // Finding the index where dirty characters begin from
    size_t start_dirty = f->buff_pos - f->cache_dirty_chars;

    // Find seek position
    off_t seek_pos = f->cache_start_file_offset + start_dirty;
    if(f->file_offset != (size_t)seek_pos)
    {
        int res = io300_seek(f, seek_pos);
        if(res == -1)
        {
            return -1;
        }

        f->file_offset = res;
    }

    f->stats.write_calls += 1;
    ssize_t bytes_written = write(f->fd, &f->cache[start_dirty], f->cache_dirty_chars);
    if(bytes_written == -1)
    {
        return -1;
    }
    if(bytes_written > 0 && (size_t)bytes_written != f->cache_dirty_chars)
    {
        return -1;
    }
    f->file_offset += f->cache_dirty_chars;
    f->buff_pos = 0;
    f->cache_dirty_chars = 0;
    f->cache_start_file_offset = f->file_offset;
    return 0;
}

int io300_fetch(struct io300_file* const f) {
    check_invariants(f);
    // TODO: Implement this
    /* This helper should contain the logic for fetching data from the file into the cache. */
    /* Think about how you can use this helper to refactor out some of the logic in your read, write, and seek functions! */
    /* Feel free to add arguments if needed. */

    f->stats.read_calls += 1;
    ssize_t bytes_read = read(f->fd, f->cache, CACHE_SIZE);
    if(bytes_read == -1 || bytes_read == 0)
    {
        return -1;
    }
    f->buff_pos = 0;
    f->buff_end = bytes_read;
    f->cache_start_file_offset = f->file_offset;
    f->file_offset += bytes_read;

    return 0;
}
