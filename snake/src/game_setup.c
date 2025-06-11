#include "game_setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "game.h"
#include "common.h"

// Some handy dandy macros for decompression
#define E_CAP_HEX 0x45
#define E_LOW_HEX 0x65
#define G_CAP_HEX 0x47
#define G_LOW_HEX 0x67
#define S_CAP_HEX 0x53
#define S_LOW_HEX 0x73
#define W_CAP_HEX 0x57
#define W_LOW_HEX 0x77
#define DIGIT_START 0x30
#define DIGIT_END 0x39

/** Initializes the board with walls around the edge of the board.
 *
 * Modifies values pointed to by cells_p, width_p, and height_p and initializes
 * cells array to reflect this default board.
 *
 * Returns INIT_SUCCESS to indicate that it was successful.
 *
 * Arguments:
 *  - cells_p: a pointer to a memory location where a pointer to the first
 *             element in a newly initialized array of cells should be stored.
 *  - width_p: a pointer to a memory location where the newly initialized
 *             width should be stored.
 *  - height_p: a pointer to a memory location where the newly initialized
 *              height should be stored.
 */
enum board_init_status initialize_default_board(int** cells_p, size_t* width_p,
        size_t* height_p) {
    *width_p = 20;
    *height_p = 10;
    int* cells = malloc(20 * 10 * sizeof(int));
    *cells_p = cells;
    for (int i = 0; i < 20 * 10; i++) {
        cells[i] = PLAIN_CELL;
    }

    // Set edge cells!
    // Top and bottom edges:
    for (int i = 0; i < 20; ++i) {
        cells[i] = FLAG_WALL;
        cells[i + (20 * (10 - 1))] = FLAG_WALL;
    }
    // Left and right edges:
    for (int i = 0; i < 10; ++i) {
        cells[i * 20] = FLAG_WALL;
        cells[i * 20 + 20 - 1] = FLAG_WALL;
    }

    // Set grass cells!
    // Top and bottom edges:
    for (int i = 1; i < 19; ++i) {
        cells[i + 20] = FLAG_GRASS;
        cells[i + (20 * (9 - 1))] = FLAG_GRASS;
    }
    // Left and right edges:
    for (int i = 1; i < 9; ++i) {
        cells[i * 20 + 1] = FLAG_GRASS;
        cells[i * 20 + 19 - 1] = FLAG_GRASS;
    }

    // Add snake
    cells[20 * 2 + 2] = FLAG_SNAKE;

    return INIT_SUCCESS;
}

/** Initialize variables relevant to the game board.
 * Arguments:
 *  - cells_p: a pointer to a memory location where a pointer to the first
 *             element in a newly initialized array of cells should be stored.
 *  - width_p: a pointer to a memory location where the newly initialized
 *             width should be stored.
 *  - height_p: a pointer to a memory location where the newly initialized
 *              height should be stored.
 *  - snake_p: a pointer to your snake struct (not used until part 3!)
 *  - board_rep: a string representing the initial board. May be NULL for
 * default board.
 */
enum board_init_status initialize_game(int** cells_p, size_t* width_p,
                                       size_t* height_p, snake_t* snake_p,
                                       char* board_rep) {

    enum board_init_status status;
    if(board_rep == NULL)
    {
        status = initialize_default_board(cells_p, width_p, height_p);
        place_food(*cells_p, *width_p, *height_p);

        if(status == INIT_SUCCESS)
        {
            g_game_over = 0;
            g_score = 0;
        }

        snake_p->pos = 42;
        snake_p->dir = INPUT_RIGHT;

    }
    else {
        status = decompress_board_str(cells_p, width_p, height_p, snake_p, board_rep);
        if(status == INIT_SUCCESS)
        {
            place_food(*cells_p, *width_p, *height_p);
            g_game_over = 0;
            g_score = 0;
        }
        snake_p->dir = INPUT_RIGHT;
    }

    return status;
}

/** Takes in a string `compressed` and initializes values pointed to by
 * cells_p, width_p, and height_p accordingly. Arguments:
 *      - cells_p: a pointer to the pointer representing the cells array
 *                 that we would like to initialize.
 *      - width_p: a pointer to the width variable we'd like to initialize.
 *      - height_p: a pointer to the height variable we'd like to initialize.
 *      - snake_p: a pointer to your snake struct (not used until part 3!)
 *      - compressed: a string that contains the representation of the board.
 * Note: We assume that the string will be of the following form:
 * B24x80|E5W2E73|E5W2S1E72... To read it, we scan the string row-by-row
 * (delineated by the `|` character), and read out a letter (E, S or W) a number
 * of times dictated by the number that follows the letter.
 */
enum board_init_status decompress_board_str(int** cells_p, size_t* width_p,
        size_t* height_p, snake_t* snake_p,
        char* compressed) {

    int snake_count = 0;
    char *parts_tokenizer = "|";
    char *rows_ptr;

    char *token = strtok_r(compressed, parts_tokenizer, &rows_ptr);
    char *token_copy = substr(token, 0, strlen(token));
    enum board_init_status status = set_dimensions(width_p, height_p, token_copy);
    free(token_copy);
    if(status != INIT_SUCCESS)
    {
        return status;
    }

    int board_size = *width_p * *height_p;

    printf("Width: %ld, height: %ld, board size: %d\n", *width_p, *height_p, board_size);

    int* cells = malloc(board_size * sizeof(int));
    *cells_p = cells;

    size_t rows = 0;

    while(token != NULL)
    {
        token = strtok_r(NULL, parts_tokenizer, &rows_ptr);
        if(token != NULL)
        {
            rows += 1;
            if(rows > *height_p)
            {
                return INIT_ERR_INCORRECT_DIMENSIONS;
            }
            RowResult rr = process_row(token, cells, *width_p, rows);
            if(rr.has_bad_character)
            {
                return INIT_ERR_BAD_CHAR;
            }
            if(rr.cols != *width_p)
            {
                return INIT_ERR_INCORRECT_DIMENSIONS;
            }
            if(rr.snake_count > 0)
            {
                snake_p->pos = rr.snake_pos;
                snake_count += rr.snake_count;
                if(snake_count > 1)
                {
                    return INIT_ERR_WRONG_SNAKE_NUM;
                }
            }
        }
    }

    if(snake_count == 0)
    {
        return INIT_ERR_WRONG_SNAKE_NUM;
    }

    if(rows != *height_p)
    {
        return INIT_ERR_INCORRECT_DIMENSIONS;
    }


    return INIT_SUCCESS;
}

enum board_init_status set_dimensions(size_t* width_p, size_t* height_p, char* board_dimension) {

    char *row_string;
    char *column_string;
    char *dimension_tokenizer = "x";
    char *board_ptr;
    char *token = strtok_r(board_dimension, dimension_tokenizer, &board_ptr);
    row_string = token;

    token = strtok_r(NULL, dimension_tokenizer, &board_ptr);
    column_string = token;
    if(column_string == NULL)
    {
        return INIT_ERR_BOARD_DIMENSION_PARSING;
    }

    if(row_string[0] != 'B')
    {
        return INIT_ERR_BOARD_DIMENSION_PARSING;
    }

    int row_count = get_num_from_string(row_string+1);

    int column_count = get_num_from_string(column_string);

    *height_p = row_count;
    *width_p = column_count;

    return INIT_SUCCESS;
}

int get_num_from_string(char *string)
{
    int count = 0;
    for(int i=0; string[i] != '\0'; i++)
    {
        count = count * 10 + string[i] - '0';
    }

    return count;
}

RowResult process_row(char *string, int *cells, int width, int height)
{
    RowResult rr;
    bool length_found = false;
    int start = 0;
    int length_str = 0;
    int total_cols_processed = 0;
    int snake_count = 0;
    int cell_pos = (height - 1) * width;
    char prev_c = '\0';

    for(int i = 0; string[i] != '\0'; i++)
    {
        char c = string[i];
        if(prev_c == '\0')
        {
            prev_c = c;
        }

        if(c >= '0' && c <= '9') {
            // Process characters 0-9
            length_str += 1;
            length_found = true;
            if(start == -1)
            {
                start = i;
            }
        }
        if(c == 'W' || c == 'E' || c == 'S' || c == 'G' || string[i+1] == '\0')
        {
            // Process walls, empty cells, snake and grass.
            if(length_found)
            {
                char *numeric_str = substr(string, start, length_str);
                int length = get_num_from_string(numeric_str);
                free(numeric_str);
                total_cols_processed += length;
                if(total_cols_processed > width)
                {
                    rr.cols = total_cols_processed;
                    break;
                }
                if(prev_c == 'W')
                {
                    for(int j = cell_pos; j < cell_pos + length; j++)
                    {
                        cells[j] = FLAG_WALL;
                    }

                    cell_pos += length;
                }
                if(prev_c == 'E')
                {
                    for(int j = cell_pos; j < cell_pos + length; j++)
                    {
                        cells[j] = PLAIN_CELL;
                    }
                    cell_pos += length;

                }
                if(prev_c == 'S')
                {
                    snake_count += length;
                    for(int j = cell_pos; j < cell_pos + length; j++)
                    {
                        cells[j] = FLAG_SNAKE;
                        rr.snake_pos = j;
                    }
                    cell_pos += length;
                }
                if(prev_c == 'G')
                {
                    for(int j = cell_pos; j < cell_pos + length; j++)
                    {
                        cells[j] = FLAG_GRASS;
                    }
                    cell_pos += length;
                }
            }
            prev_c = c;
            start = -1;
            length_str = 0;
        }
        else if (c < '0' || c > '9') {
            rr.has_bad_character = true;
            break;
        }
    }
    rr.cols = total_cols_processed;
    rr.snake_count = snake_count;

    return rr;
}

char *substr(const char *str, int start, int length) {
    if (start < 0 || length < 0 || (size_t)(start + length) > strlen(str)) {
        return NULL;  // Handle out-of-bounds
    }

    char *result = malloc(length + 1); // +1 for null terminator
    if (result == NULL) return NULL;

    strncpy(result, str + start, length);
    result[length] = '\0'; // Null-terminate
    return result;
}
