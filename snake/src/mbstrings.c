#include "mbstrings.h"

#include <stddef.h>
#include <stdbool.h>

/* mbslen - multi-byte string length
 * - Description: returns the number of UTF-8 code points ("characters")
 * in a multibyte string. If the argument is NULL or an invalid UTF-8
 * string is passed, returns -1.
 *
 * - Arguments: A pointer to a character array (`bytes`), consisting of UTF-8
 * variable-length encoded multibyte code points.
 *
 * - Return: returns the actual number of UTF-8 code points in `src`. If an
 * invalid sequence of bytes is encountered, return -1.
 *
 * - Hints:
 * UTF-8 characters are encoded in 1 to 4 bytes. The number of leading 1s in the
 * highest order byte indicates the length (in bytes) of the character. For
 * example, a character with the encoding 1111.... is 4 bytes long, a character
 * with the encoding 1110.... is 3 bytes long, and a character with the encoding
 * 1100.... is 2 bytes long. Single-byte UTF-8 characters were designed to be
 * compatible with ASCII. As such, the first bit of a 1-byte UTF-8 character is
 * 0.......
 *
 * You will need bitwise operations for this part of the assignment!
 */

bool is_valid_utf8(const char *s) {
    while (*s) {
        unsigned char c = *s;

        if (c <= 0x7F) {
            // 1-byte (ASCII)
            s++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if ((s[1] & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false;  // overlong encoding
            s += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return false;
            if (c == 0xE0 && (s[1] & 0xE0) == 0x80) return false; // overlong
            if (c == 0xED && (s[1] & 0xE0) == 0xA0) return false; // UTF-16 surrogates
            s += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return false;
            if (c == 0xF0 && (s[1] & 0xF0) == 0x80) return false; // overlong
            if (c > 0xF4) return false; // out of Unicode range
            s += 4;
        } else {
            // Invalid first byte
            return false;
        }
    }
    return true;
}

size_t mbslen(const char* bytes) {
    int code_points = 0;
    if(bytes == NULL) {
        return -1;
    }
    if(!is_valid_utf8(bytes)) {
        return -1;
    }
    while(*bytes)
    {
        if((unsigned char)*bytes >> 6 != 0b10)
        {
            code_points++;
        }
        bytes++; // go to the next byte, not next character
    }
    return code_points;
}

