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

    size_t file_offset;
    size_t buff_pos;
    size_t buff_end;
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

    ret->file_offset = 0;
    ret->buff_pos = 0;
    ret->buff_end = 0;
    ret->cache_start_file_offset = 0;
    ret->cache_dirty_chars = 0;
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

    // If seeking then we might have to do cache invalidation as seek pos might not be in cache range...
    if(f->write_mode && f->cache_dirty_chars > 0)
    {
        // Flush the cache as it is dirty...
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
        f->write_mode = false;
    }
    else {
        // Invalidate it..
        f->buff_end = 0;
        f->buff_pos = 0;
    }
    f->file_offset = pos;
    f->cache_start_file_offset = pos;

    return lseek(f->fd, pos, SEEK_SET);
}

int io300_close(struct io300_file* const f) {
    check_invariants(f);

    // Flush the cache if it's dirty
    if(f->write_mode == true && f->cache_dirty_chars > 0)
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

    if(f->write_mode == true)
    {
        // Flush the cache
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
        f->write_mode = false;
    }

    // Buffer is empty or full
    if(f->buff_end == 0 || (f->buff_pos >= f->buff_end))
    {
        int bytes_read = io300_fetch(f);
        if(bytes_read == -1 || bytes_read == 0)
        {
            return -1;
        }
    }

    return (unsigned char)f->cache[f->buff_pos++];
}


int io300_writec(struct io300_file* f, int ch) {
    check_invariants(f);
    char const c = (char)ch;
    if(f->write_mode == false && f->buff_pos == CACHE_SIZE)
    {
        // Switching from reading to writing and cache is fully read..., change the position of cache start file offset.
        f->buff_pos = 0;
        f->buff_end = 0;
        f->cache_start_file_offset += CACHE_SIZE;
    }

    if(f->write_mode == true && f->buff_pos == CACHE_SIZE)
    {
        // We are writing and cache is full
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
    }

    f->cache[f->buff_pos++] = c;
    f->write_mode = true;
    f->cache_dirty_chars += 1;

    return ch;
}

ssize_t io300_read(struct io300_file* const f, char* const buff,
                   size_t const sz) {
    check_invariants(f);

    if(f->write_mode == true && f->cache_dirty_chars > 0)
    {
        int flushed = io300_flush(f);
        if(flushed == -1)
        {
            return -1;
        }
        f->write_mode = false;
    }

    size_t valid_cache_left = (f->buff_end > f->buff_pos) ? f->buff_end - f->buff_pos : CACHE_SIZE - f->buff_pos;

    // We can have an edge case, where we first filled cache but read fewer bytes and then reading again more bytes, we might have to shift our file offset in order to make it work correctly.

    if(sz > valid_cache_left && sz < CACHE_SIZE)
    {
        // Reset the internal file offset.
        off_t seek_pos = f->cache_start_file_offset + f->buff_pos;
        if(f->file_offset != (size_t)seek_pos)
        {
            f->stats.seeks++;
            int reset = lseek(f->fd, seek_pos, SEEK_SET);
            if(reset == -1)
            {
                return -1;
            }
            f->file_offset = reset;
            f->cache_start_file_offset = reset;
        }
        f->buff_pos = 0;
        f->buff_end = 0;
    }
    else if(sz > CACHE_SIZE)     // sz is greater than cache size
    {
        // Reset the internal file offset.
        off_t seek_pos = f->cache_start_file_offset + f->buff_pos;
        if(f->file_offset != (size_t)seek_pos) {
            f->stats.seeks++;
            int reset = lseek(f->fd, seek_pos, SEEK_SET);
            if(reset == -1)
            {
                return -1;
            }
            f->file_offset = reset;
            f->cache_start_file_offset = f->file_offset;
        }

        // Invalid cache
        f->buff_pos = 0;
        f->buff_end = 0;

        // return from file directly, changing file offset
        f->stats.read_calls++;
        ssize_t bytes_read = read(f->fd, buff, sz);
        f->file_offset += bytes_read;
        f->cache_start_file_offset = f->file_offset;
        return bytes_read;
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

    // It might be possible that cache has only 3 valid bytes left with buff end = 3 and buff pos = 0, reading more than 3 bytes will give garbage data, so need to have a check of valid bytes return. It's not always possible to return sz bytes correctly from cache.
    int valid_read = sz;
    if (sz > f->buff_end - f->buff_pos) {
        valid_read = f->buff_end - f->buff_pos;
    }

    memcpy(buff, &f->cache[f->buff_pos], valid_read);

    f->buff_pos += valid_read;

    return (ssize_t)valid_read;
}


ssize_t io300_write(struct io300_file* const f, const char* buff,
                    size_t const sz) {
    check_invariants(f);

    if(f->write_mode == false)
    {
        if(f->buff_pos == CACHE_SIZE)
        {
            f->cache_start_file_offset += CACHE_SIZE;
            f->buff_pos = 0;
            f->buff_end = 0;
        }

        size_t valid_cache_left = (f->buff_end > f->buff_pos) ? f->buff_end - f->buff_pos : CACHE_SIZE - f->buff_pos;
        if(sz <= valid_cache_left)
        {
            // write into cache
            memcpy(&f->cache[f->buff_pos], buff, sz);
            f->cache_dirty_chars += sz;
            f->buff_pos += sz;
            f->write_mode = true;
            return (ssize_t)sz;
        }
        else {
            // Seek to correct position
            off_t seek_pos = f->cache_start_file_offset + f->buff_pos;
            if(f->file_offset != (size_t)seek_pos)
            {
                f->stats.seeks++;
                int reset = lseek(f->fd, seek_pos, SEEK_SET);
                if(reset == -1)
                {
                    return -1;
                }
                f->file_offset = reset;
                f->cache_start_file_offset = f->file_offset;
            }

            // Invalidate it
            f->buff_pos = 0;
            f->buff_end = 0;

            // Write into file directly
            f->stats.write_calls++;
            ssize_t bytes_written = write(f->fd, buff, sz);
            f->file_offset += bytes_written;
            f->cache_start_file_offset = f->file_offset;
            return (ssize_t)sz;
        }
    }
    else { // Cache is dirty

        if(f->buff_pos == CACHE_SIZE) // Cache is full
        {
            int flushed = io300_flush(f);
            if(flushed == -1)
            {
                return -1;
            }
            f->write_mode = false;
        }

        size_t valid_cache_left = (f->buff_end > f->buff_pos) ? f->buff_end - f->buff_pos : CACHE_SIZE - f->buff_pos;

        if(sz <= valid_cache_left)
        {
            memcpy(&f->cache[f->buff_pos], buff, sz);
            f->cache_dirty_chars += sz;
            f->buff_pos += sz;
            f->write_mode = true;
            return (ssize_t)sz;
        }
        else {

            if(f->cache_dirty_chars > 0 && f->write_mode)
            {
                int flushed = io300_flush(f);
                if(flushed == -1)
                {
                    return -1;
                }
                f->write_mode = false;
            }

            // Write into file directly
            f->stats.write_calls++;
            ssize_t bytes_written = write(f->fd, buff, sz);
            f->file_offset += bytes_written;
            f->cache_start_file_offset = f->file_offset;
            return (ssize_t)sz;
        }
    }
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
        f->stats.seeks++;
        int res = lseek(f->fd, seek_pos, SEEK_SET);
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
    f->file_offset += bytes_written;
    f->buff_pos = 0;
    f->buff_end = 0;
    f->cache_dirty_chars = 0;
    f->cache_start_file_offset = f->file_offset;
    return bytes_written;
}

int io300_fetch(struct io300_file* const f) {
    check_invariants(f);
    /* This helper should contain the logic for fetching data from the file into the cache. */
    /* Think about how you can use this helper to refactor out some of the logic in your read, write, and seek functions! */
    /* Feel free to add arguments if needed. */

    f->stats.read_calls += 1;
    ssize_t bytes_read = read(f->fd, f->cache, CACHE_SIZE);
    if(bytes_read == -1)
    {
        return -1;
    }
    f->buff_pos = 0;
    f->buff_end = bytes_read;
    f->cache_start_file_offset = f->file_offset;
    f->file_offset += bytes_read;

    return bytes_read;
}
