#ifndef GAME_SETUP_H
#define GAME_SETUP_H

#include <stddef.h>
#include <stdbool.h>

#include "common.h"
#include "game.h"


/**
  * Creating an enum of possible values while processing each row
  */

typedef struct{
  size_t cols;
  bool has_bad_character;
  int snake_count;
  int snake_pos;
} RowResult;

/** Enum to communicate board initialization status.
 * Values include INIT_SUCCESS, INIT_ERR_INCORRECT_DIMENSIONS,
 * INIT_ERR_WRONG_SNAKE_NUM, and INIT_ERR_BAD_CHAR.
 */
enum board_init_status {
    INIT_SUCCESS,                   // no errors were thrown
    INIT_ERR_INCORRECT_DIMENSIONS,  // dimensions description was not formatted
                                    // correctly, or too many rows/columns are
                                    // specified anywhere in the string for the
                                    // given dimensions
    INIT_ERR_WRONG_SNAKE_NUM,  // no snake or multiple snakes are on the board
    INIT_ERR_BAD_CHAR,  // any other part of the compressed string was formatted
                        // incorrectly
    INIT_UNIMPLEMENTED,  // only used in stencil, no need to handle this
    INIT_ERR_BOARD_DIMENSION_PARSING // error in parsing board dimension
};

enum board_init_status initialize_game(int** cells_p, size_t* width_p,
                                       size_t* height_p, snake_t* snake_p,
                                       char* board_rep);

enum board_init_status decompress_board_str(int** cells_p, size_t* width_p,
                                            size_t* height_p, snake_t* snake_p,
                                            char* compressed);
enum board_init_status initialize_default_board(int** cells_p, size_t* width_p,
                                                size_t* height_p);

enum board_init_status set_dimensions(size_t* width_p, size_t* height_p, char *board_dimension);

int get_num_from_string(char *string);

RowResult process_row(char *string, int *cells, int width, int height);

char *substr(const char *string, int start, int length);

#endif
