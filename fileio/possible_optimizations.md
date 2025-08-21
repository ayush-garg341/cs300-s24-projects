### Possible optimizations:-

- Think of ways to reduce cache invalidation because invalidating and reloading is expensive.
    - Try to write only complete block and keep reading from it even if it's dirty as long as read is correct.
    - When cache is full and dirty only flush then, that way no need to invalidate and re-read.

- Check if seeking pos or current file position already exist in the prefetch buffer and if exist think of some numbers crunching so that we have valid cache, file pos and prefetch buffer.

- Optimize/handle the case of backward reading and do some number crunching only.
