### A file is an ordered sequence of bytes — not text, not ASCII characters, not UTF-8 characters, but just bytes.

- We’ll create a file that contains raw bytes, including:
    - Printable text
    - Null bytes
    - Non-ASCII binary values

```bash

printf '\x41\x42\x00\x43\xFF\xD8' > myfile.bin

0x41  → 'A'
0x42  → 'B'
0x00  → NULL byte
0x43  → 'C'
0xFF  → 255 (non-printable)
0xD8  → 216 (non-ASCII)

[A][B][\0][C][ÿ][Ø]     ← if interpreted visually
```

- View as raw bytes

```bash
xxd myfile.bin

00000000: 4142 0043 ffd8                           AB.C..

Bytes in hex: 41 42 00 43 FF D8
ASCII interpretation on the right: printable where possible
```

- Interpretation
    - This file has no concept of characters unless a program interprets it that way.
    - The OS sees this as: A (0x41), B (0x42), null (0x00), C (0x43), binary 255, binary 216
    - You can't tell whether this is:
        - A UTF-8 string?
        - A PNG header?
        - A compressed ZIP fragment?
    - It depends on how you interpret the bytes.


```bash
vim myfile.bin
```

- You'll likely see:
    - AB printed
    - ^@ for null
    - C
    - Garbled characters or replacement symbols for 0xFF, 0xD8
    - Because editors try to treat the bytes as UTF-8 or ASCII, which fails for raw binary data.
    
- This example shows:
    - A file is literally just a byte sequence.
    - Interpretation (text, image, UTF-8, etc.) is external, done by software.
    - Even the same bytes can represent wildly different things depending on the program.



### Debugging the code with gdb and xxd, diff

- dd:- Copy a file, converting and formatting according to the operands.
- xxd:- convert binary file to hex + ASCII dump to compare and can reverse that as well to binary.
- diff:- compare two hex files or text files and give the difference where it occurs.

1. `byte_cat` program which runs like this `./byte_cat test_files/tiny.txt /tmp/out.txt`

- Set Arguments Inside GDB (Recommended)
```bash

gcc -g -O0 -o byte_cat byte_cat.c

# Start GDB
gdb ./byte_cat

# Set arguments inside GDB
(gdb) set args input_file out_file

# Set breakpoints
(gdb) break main
(gdb) break some_function_name

(gdb) show args

# Run with the arguments
(gdb) run
```

- Method 2: Pass Arguments Directly to GDB
```bash

# Start GDB with arguments in one command
gdb --args ./byte_cat input_file out_file

# Set breakpoints and run
(gdb) break main
(gdb) run
```

- Method 3: Specify Arguments During Run
```bash

gdb ./byte_cat

# Run with arguments directly (without set args)
(gdb) run input_file out_file
```

- Method 4: Advanced: Using GDB Script

```bash

# Create a file debug.gdb:

set args test_input.txt test_output.txt
break main
break fopen
run
print argc
print argv[1]
print argv[2]
continue

# Then run:

gdb -x debug.gdb ./byte_cat

```
