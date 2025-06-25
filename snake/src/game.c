#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>

#include "linked_list.h"
#include "mbstrings.h"
#include "common.h"

/** Updates the game by a single step, and modifies the game information
 * accordingly. Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - width: width of the board.
 *  - height: height of the board.
 *  - snake_p: pointer to your snake struct (not used until part 3!)
 *  - input: the next input.
 *  - growing: 0 if the snake does not grow on eating, 1 if it does.
 */
void update(int* cells, size_t width, size_t height, snake_t* snake_p,
            enum input_key user_input, int growing) {

    if(g_game_over) {
        return;
    }
    enum input_key dir = user_input;
    int orig_pos = *(int *)get_first(snake_p->head);
    int next_pos = orig_pos;

    if(dir == INPUT_NONE)
    {
        dir = snake_p->dir;
        if(dir == INPUT_NONE)
        {
            dir = INPUT_RIGHT;
            snake_p->dir = dir;
        }
    }
    else {
        // Snake cannot double back on itself
        if(g_score > 0 && growing == 1)
        {
            if((dir == INPUT_RIGHT && snake_p->dir == INPUT_LEFT) || (dir == INPUT_LEFT && snake_p->dir == INPUT_RIGHT) || (dir == INPUT_UP && snake_p->dir == INPUT_DOWN) || (dir == INPUT_DOWN && snake_p->dir == INPUT_UP))
            {
                dir = snake_p->dir;
            }
            else {
                snake_p->dir = dir;
            }
        }
        else {
            snake_p->dir = dir;
        }
    }

    if(dir == INPUT_RIGHT)
    {
        next_pos += 1;
    }
    if(dir == INPUT_LEFT)
    {
        next_pos -= 1;
    }
    if(dir == INPUT_UP) {
        next_pos -= width;
    }
    if(dir == INPUT_DOWN) {
        next_pos += width;
    }

    if(dir == INPUT_NONE) {
        next_pos += 1;
    }

    int has_snake = cells[orig_pos] & FLAG_SNAKE;
    int has_food = cells[orig_pos] & FLAG_FOOD;

    // If the cell has snake, removing from existing cell.
    if(has_snake && cells[next_pos] != FLAG_WALL && (growing == 0 || g_score == 0))
    {
        // Not removing the snake if next cell is Wall.
        cells[orig_pos] = cells[orig_pos] ^ FLAG_SNAKE;
    }

    // If the cell has food, removing the food as it's eaten
    if(has_food)
    {
        cells[orig_pos] = cells[orig_pos] ^ FLAG_FOOD;
    }

    if(cells[next_pos] == FLAG_WALL)
    {
        g_game_over = 1;
        return;
    }
    else {
        // if cell has food, increasing score and placing new food. ( Meaning snake is eating the food )
        has_food = cells[next_pos] & FLAG_FOOD;
        // printf("Next pos = %d\n", next_pos);
        if(has_food)
        {
            // Updating the next cell with snake positioion
            cells[next_pos] = cells[next_pos] | FLAG_SNAKE;

            if(growing == 1)
            {
                cells[orig_pos] = cells[orig_pos] | FLAG_SNAKE;
            }
            g_score += 1;
            // printf("food eaten\n");

            // Do optimization here if snake is not growing..
            insert_first(&snake_p->head, &next_pos, sizeof(int));

            place_food(cells, width, height);
        }
        else {
            if(g_score > 0 && growing == 1)
            {


                // removing last node from list
                void* data = remove_last(&snake_p->head);
                int last_pos = *(int *)data;
                free(data);
                // printf("last pos = %d\n", last_pos);

                // Growing snake colliding with itself causing game over
                // We have to check if linked list already has element at that position or it may be inside the grass.
                if(next_pos != last_pos && cells[next_pos] & FLAG_SNAKE) {
                    g_game_over = 1;
                    return;
                }

                // Remove snake from last pos of cells
                cells[last_pos] = cells[last_pos] ^ FLAG_SNAKE;

                // Update the next pos with snake
                cells[next_pos] = cells[next_pos] | FLAG_SNAKE;

                // inserting node at the head new position
                insert_first(&snake_p->head, &next_pos, sizeof(int));

            }
            else {
                cells[next_pos] = cells[next_pos] | FLAG_SNAKE;
                *(int *)(snake_p->head->data) = next_pos;
            }
        }
    }

}

/** Sets a random space on the given board to food.
 * Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - width: the width of the board
 *  - height: the height of the board
 */
void place_food(int* cells, size_t width, size_t height) {
    /* DO NOT MODIFY THIS FUNCTION */
    unsigned food_index = generate_index(width * height);
    // check that the cell is empty or only contains grass
    if ((*(cells + food_index) == PLAIN_CELL) || (*(cells + food_index) == FLAG_GRASS)) {
        *(cells + food_index) |= FLAG_FOOD;
    } else {
        place_food(cells, width, height);
    }
    /* DO NOT MODIFY THIS FUNCTION */
}

/** Prompts the user for their name and saves it in the given buffer.
 * Arguments:
 *  - `write_into`: a pointer to the buffer to be written into.
 */
void read_name(char* write_into) {
    // TODO: implement! (remove the call to strcpy once you begin your
    // implementation)
    strcpy(write_into, "placeholder");
}

/** Cleans up on game over — should free any allocated memory so that the
 * LeakSanitizer doesn't complain.
 * Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - snake_p: a pointer to your snake struct. (not needed until part 3)
 */
void teardown(int* cells, snake_t* snake_p) {
    // TODO: implement!
    free(cells);

    if (snake_p && snake_p->head) {
        node_t* head = snake_p->head;
        node_t *next = NULL;
        while(head != NULL)
        {
            next = head->next;
            free(head->data);
            free(head);
            head = next;
        }
    }
}
