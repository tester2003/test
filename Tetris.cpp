#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#include "colors.h"

//Define board sizes
#define WIDTH 10
#define HEIGHT 22
#define VISIBLE_HEIGHT 20

#define GRID_SIZE 30

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

const int FPS=60;
const float frame_delay=1000/FPS;

const u8 FRAMES_PER_DROP[] = {
    48,
    43,
    38,
    33,
    28,
    23,
    18,
    13,
    8,
    6,
    5,
    5,
    5,
    4,
    4,
    4,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1
};

    Mix_Music *music;
    Mix_Chunk *increaseLVL;
    Mix_Chunk *decreaseLVL;
    Mix_Chunk *start;
    Mix_Chunk *softDrop;
    Mix_Chunk *move_sfx;
    Mix_Chunk *rotate_sfx;
    Mix_Chunk *landing;
    Mix_Chunk *hardDrop;
    Mix_Chunk *lvlUp;
    Mix_Chunk *pause_sfx;
    Mix_Chunk *gameover;




//Convert from frames to seconds
const float TARGET_SECONDS_PER_FRAME = 1.f / 60.f;

struct Tetromino
{
    const u8 *data;
    const int side;
};

Tetromino tetromino(const u8 *data, int side)
{
    return { data, side };
}

const u8 TETROMINO_DEFAULT[] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};

//I-shaped tetromino
const u8 TETROMINO_1[] = {
    0, 0, 0, 0,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0
};

//O-shaped tetromino
const u8 TETROMINO_2[] = {
    2, 2,
    2, 2
};

//T-shaped tetromino
const u8 TETROMINO_3[] = {
    0, 3, 0,
    3, 3, 3,
    0, 0, 0
};

//S-shaped tetromino
const u8 TETROMINO_4[] = {
    0, 4, 4,
    4, 4, 0,
    0, 0, 0
};

//Z-shaped tetromino
const u8 TETROMINO_5[] = {
    5, 5, 0,
    0, 5, 5,
    0, 0, 0
};

//L-shaped tetromino
const u8 TETROMINO_6[] = {
    6, 0, 0,
    6, 6, 6,
    0, 0, 0
};
 //J-shaped tetromino
const u8 TETROMINO_7[] = {
    0, 0, 7,
    7, 7, 7,
    0, 0, 0
};


const Tetromino TETROMINOS[] = {
    tetromino(TETROMINO_DEFAULT, 4),
    tetromino(TETROMINO_1, 4),
    tetromino(TETROMINO_2, 2),
    tetromino(TETROMINO_3, 3),
    tetromino(TETROMINO_4, 3),
    tetromino(TETROMINO_5, 3),
    tetromino(TETROMINO_6, 3),
    tetromino(TETROMINO_7, 3),
};

enum Game_Phase
{
    GAME_PHASE_START,
    GAME_PHASE_PLAY,
    GAME_PHASE_LINE,
    GAME_PHASE_GAMEOVER
};

struct Piece_State
{
    u8 tetromino_index;
    int offset_row;
    int offset_col;
    int rotation;
};

//Represents the board with zero is an empty cell and the other values represent different colors
struct Game_State
{
    u8 board[WIDTH * HEIGHT];
    u8 lines[HEIGHT];
    int pending_line_count;

    bool muted=false;
    bool holdPlace=false;

    Piece_State piece;
    Piece_State nextPiece;
    Piece_State holdPiece;

    Game_Phase phase;

    int start_level;
    int level;
    int line_count;
    int points;
    u8 pause;

    float next_drop_time;
    float highlight_end_time;
    float time;
};

struct Input_State
{
    u8 left;
    u8 right;
    u8 up;
    u8 down;
    u8 space;
    u8 p;
    u8 m;
    u8 g;
    u8 h;

    s8 dleft;
    s8 dright;
    s8 dup;
    s8 ddown;
    s8 dspace;
    s8 dp;
    s8 dm;
    s8 dg;
    s8 dh;
};

enum Text_Align
{
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT,
    TEXT_ALIGN_HUD
};

//Get the value at a coordinate
u8 matrix_get(const u8 *values, int width, int row, int col)
{
    int index = row * width + col;
    return values[index];
}

//Set the value at a coordinate
void matrix_set(u8 *values, int width, int row, int col, u8 value)
{
    int index = row * width + col;
    values[index] = value;
}

//Rotates the tetromino
u8 tetromino_rotate(const Tetromino *tetromino, int row, int col, int rotation)
{
    int side = tetromino->side;
    switch (rotation)
    {
    case 0:
        return tetromino->data[row * side + col];
    case 1:
        return tetromino->data[(side - col - 1) * side + row];
    case 2:
        return tetromino->data[(side - row - 1) * side + (side - col - 1)];
    case 3:
        return tetromino->data[col * side + (side - row - 1)];
    }
    return 0;
}

u8 check_row_filled(const u8 *values, int width, int row)
{
    for (int col = 0;
         col < width;
         ++col)
    {
        if (!matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

u8 check_row_empty(const u8 *values, int width, int row)
{
    for (int col = 0;
         col < width;
         ++col)
    {
        if (matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

int find_lines(const u8 *values, int width, int height, u8 *lines_out)
{
    int count = 0;
    for (int row = 0;
         row < height;
         ++row)
    {
        u8 filled = check_row_filled(values, width, row);
        lines_out[row] = filled;
        count += filled;
    }
    return count;
}

void clear_lines(u8 *values, int width, int height, const u8 *lines)
{
    int src_row = height - 1;
    for (int dst_row = height - 1;
         dst_row >= 0;
         --dst_row)
    {
        while (src_row >= 0 && lines[src_row])
        {
            --src_row;
        }

        if (src_row < 0)
        {
            memset(values + dst_row * width, 0, width);
        }
        else
        {
            if (src_row != dst_row)
            {
                memcpy(values + dst_row * width,
                       values + src_row * width,
                       width);
            }
            --src_row;
        }
    }
}

//Checks if the tetromino collides with anything
bool check_piece_valid(const Piece_State *piece,
                  const u8 *board, int width, int height)
{
    const Tetromino *tetromino = TETROMINOS + piece->tetromino_index;
    // assert(tetromino);

    for (int row = 0;
         row < tetromino->side;
         ++row)
    {
        for (int col = 0;
             col < tetromino->side;
             ++col)
        {
            u8 value = tetromino_rotate(tetromino, row, col, piece->rotation);
            if (value > 0)
            {
                int board_row = piece->offset_row + row;
                int board_col = piece->offset_col + col;
                if (board_row < 0)
                {
                    return false;
                }
                if (board_row >= height)
                {
                    return false;
                }
                if (board_col < 0)
                {
                    return false;
                }
                if (board_col >= width)
                {
                    return false;
                }
                if (matrix_get(board, width, board_row, board_col))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void merge_piece(Game_State *game)
{
    const Tetromino *tetromino = TETROMINOS + game->piece.tetromino_index;
    for (int row = 0;
         row < tetromino->side;
         ++row)
    {
        for (int col = 0;
             col < tetromino->side;
             ++col)
        {
            u8 value = tetromino_rotate(tetromino, row, col, game->piece.rotation);
            if (value)
            {
                int board_row = game->piece.offset_row + row;
                int board_col = game->piece.offset_col + col;
                matrix_set(game->board, WIDTH, board_row, board_col, value);
            }
        }
    }
}

int random_int(int min, int max)
{
    int range = max - min;
    return min + rand() % range;
}

float get_time_to_next_drop(int level)
{
    if (level > 29)
    {
        level = 29;
    }
    return FRAMES_PER_DROP[level] * TARGET_SECONDS_PER_FRAME;
}


void spawn_piece(Game_State *game, bool start=false)
{
    game->piece = {};
    if(start)
    {
        game->piece.tetromino_index = (u8)random_int(1, ARRAY_COUNT(TETROMINOS));
        game->piece.offset_col = WIDTH / 2;

        game->nextPiece = {};
        game->nextPiece.tetromino_index = (u8)random_int(1, ARRAY_COUNT(TETROMINOS));
        start=false;
    }
    else
    {
        game->piece=game->nextPiece;
        game->piece.offset_col = WIDTH / 2;
        game->nextPiece.tetromino_index = (u8)random_int(1, ARRAY_COUNT(TETROMINOS));
    }
    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
}

void hold_piece(Game_State *game)
{
    if(!game->holdPlace)
    {
        game->holdPiece.tetromino_index = game->piece.tetromino_index;
        spawn_piece(game);
        game->holdPlace = true;
    }
    else
    {
        Piece_State piece=game->piece;
        piece.tetromino_index=game->holdPiece.tetromino_index;
        if (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
        {
            u8 temp = game->piece.tetromino_index;
            game->piece.tetromino_index = game->holdPiece.tetromino_index;
            game->holdPiece.tetromino_index = temp;
        }
    }
}

void pushHold(Game_State* game)
{
    if(game->holdPlace)
    {
        game->nextPiece.tetromino_index=game->holdPiece.tetromino_index;
        game->holdPiece = {};
        game->holdPlace = false;
    }

}

bool soft_drop(Game_State *game)
{
    ++game->piece.offset_row;
    if (!check_piece_valid(&game->piece, game->board, WIDTH, HEIGHT))
    {
        Mix_PlayChannel( -1, landing, 0 );
        --game->piece.offset_row;

        merge_piece(game);
        spawn_piece(game);
        return false;
    }

    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
    return true;
}

int compute_points(int level, int line_count)
{
    switch (line_count)
    {
    case 1:
        return 40 * (level + 1);
    case 2:
        return 100 * (level + 1);
    case 3:
        return 300 * (level + 1);
    case 4:
        return 1200 * (level + 1);
    }
    return 0;
}

int min(int x, int y)
{
    return x < y ? x : y;
}
int max(int x, int y)
{
    return x > y ? x : y;
}

int get_lines_for_next_level(int start_level, int level)
{
    int first_level_up_limit = min((start_level * 10 + 10),
        max(100, (start_level * 10 - 50)));
    if (level == start_level)
    {
        return first_level_up_limit;
    }
    int diff = level - start_level;
    return first_level_up_limit + diff * 10;
}

void update_game_start(Game_State *game, const Input_State *input)
{
    if (input->dup > 0)
    {
        Mix_PlayChannel( -1, increaseLVL, 0 );
        ++game->start_level;
    }

    if (input->ddown > 0 && game->start_level > 0)
    {
        Mix_PlayChannel( -1, decreaseLVL, 0 );
        --game->start_level;
    }

    if (input->dspace > 0)
    {
        Mix_PlayChannel( -1, start, 0 );
        memset(game->board, 0, WIDTH * HEIGHT);
        game->level = game->start_level;
        game->line_count = 0;
        game->points = 0;
        spawn_piece(game, true);
        game->phase = GAME_PHASE_PLAY;
    }
}

void update_game_gameover(Game_State *game, const Input_State *input)
{
    if (input->dspace > 0)
    {
        game->phase = GAME_PHASE_START;
    }
}

void update_game_line(Game_State *game)
{
    if (game->time >= game->highlight_end_time)
    {
        clear_lines(game->board, WIDTH, HEIGHT, game->lines);
        game->line_count += game->pending_line_count;
        game->points += compute_points(game->level, game->pending_line_count);

        int lines_for_next_level = get_lines_for_next_level(game->start_level,
                                                            game->level);
        if (game->line_count >= lines_for_next_level)
        {
            ++game->level;
        }

        game->phase = GAME_PHASE_PLAY;
    }
}

void update_game_play(Game_State *game, const Input_State *input)
{
    if (input->dp > 0) {
        Mix_PlayChannel( -1, pause_sfx, 0 );
        game->pause = (game->pause+1) % 2;
    }
    Piece_State piece = game->piece;

    if (input->dleft > 0 && game->pause == 0)
    {
       Mix_PlayChannel( -1, move_sfx, 0 );
        --piece.offset_col;
    }
    if (input->dright> 0 && game->pause == 0)
    {
        Mix_PlayChannel( -1, move_sfx, 0 );
        ++piece.offset_col;
    }
    if (input->dup > 0 && game->pause == 0)
    {
        Mix_PlayChannel( -1, rotate_sfx, 0 );
        piece.rotation = (piece.rotation + 1) % 4;
    }

    if (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
    {
        game->piece = piece;
    }

    if (input->ddown > 0 && game->pause == 0)
    {
        Mix_PlayChannel( -1, move_sfx, 0 );
        soft_drop(game);
    }

    if (input->dspace > 0 && game->pause == 0)
    {
        Mix_PlayChannel( -1, hardDrop, 0 );
        while(soft_drop(game));
    }

    while (game->time >= game->next_drop_time && game->pause == 0)
    {
        Mix_PlayChannel( -1, softDrop, 0 );
        soft_drop(game);
    }

    if (input->dg > 0 && game->pause == 0)
    {
        hold_piece(game);
    }

    if(input->dh > 0 && game->pause == 0)
    {
        pushHold(game);
    }

    game->pending_line_count = find_lines(game->board, WIDTH, HEIGHT, game->lines);
    if (game->pending_line_count > 0)
    {
        game->phase = GAME_PHASE_LINE;
        game->highlight_end_time = game->time + 0.5f;
    }

    int game_over_row = 0;
    if (!check_row_empty(game->board, WIDTH, game_over_row))
    {
        Mix_PlayChannel( -1, gameover, 0 );
        game->phase = GAME_PHASE_GAMEOVER;
    }
}

//Switches between game phases
void update_game(Game_State *game, const Input_State *input)
{
    switch(game->phase)
    {
    case GAME_PHASE_START:
        update_game_start(game, input);
        break;
    case GAME_PHASE_PLAY:
        update_game_play(game, input);
        break;
    case GAME_PHASE_LINE:
        update_game_line(game);
        break;
    case GAME_PHASE_GAMEOVER:
        update_game_gameover(game, input);
        break;
    }
}

void fill_rect(SDL_Renderer *renderer, int x, int y, int width, int height, Color color)
{
    SDL_Rect rect = {};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}


void draw_rect (SDL_Renderer *renderer, int x, int y, int width, int height, Color color)
{
    SDL_Rect rect = {};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void draw_string(SDL_Renderer *renderer,
            TTF_Font *font,
            const char *text,
            int x, int y,
            Text_Align alignment,
            Color color)
{
    SDL_Color sdl_color = SDL_Color { color.r, color.g, color.b, color.a };
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, sdl_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect rect;
    rect.y = y;
    rect.w = surface->w;
    rect.h = surface->h;
    switch (alignment)
    {
    case TEXT_ALIGN_LEFT:
        rect.x = x;
        break;
    case TEXT_ALIGN_CENTER:
        rect.x = x - surface->w / 2;
        break;
    case TEXT_ALIGN_RIGHT:
        rect.x = x - surface->w;
        break;
    case TEXT_ALIGN_HUD:
        rect.x = x - surface->w / 3;
    }

    SDL_RenderCopy(renderer, texture, 0, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_cell(SDL_Renderer *renderer,
          int row, int col, u8 value,
          int offset_x, int offset_y,
          bool outline = false)
{
    Color base_color = BASE_COLORS[value];
    Color light_color = LIGHT_COLORS[value];
    Color dark_color = DARK_COLORS[value];


    int edge = GRID_SIZE / 8;

    int x = col * GRID_SIZE + offset_x;
    int y = row * GRID_SIZE + offset_y;

    if (outline)
    {
        draw_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, base_color);
        return;
    }

    fill_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, dark_color);
    fill_rect(renderer, x + edge, y,
              GRID_SIZE - edge, GRID_SIZE - edge, light_color);
    fill_rect(renderer, x + edge, y + edge,
              GRID_SIZE - edge * 2, GRID_SIZE - edge * 2, base_color);
}

void draw_piece(SDL_Renderer *renderer,
           const Piece_State *piece,
           int offset_x, int offset_y,
           bool outline = false)
{
    const Tetromino *tetromino = TETROMINOS + piece->tetromino_index;
    for (int row = 0;
         row < tetromino->side;
         ++row)
    {
        for (int col = 0;
             col < tetromino->side;
             ++col)
        {
            u8 value = tetromino_rotate(tetromino, row, col, piece->rotation);
            if (value)
            {
                draw_cell(renderer,
                          row + piece->offset_row,
                          col + piece->offset_col,
                          value,
                          offset_x, offset_y,
                          outline);
            }
        }
    }
}

void draw_board(SDL_Renderer *renderer,
           const u8 *board, int width, int height,
           int offset_x, int offset_y)
{
    fill_rect(renderer, offset_x, offset_y,
              width * GRID_SIZE, height * GRID_SIZE,
              BASE_COLORS[0]);
    for (int row = 0;
         row < height;
         ++row)
    {
        for (int col = 0;
             col < width;
             ++col)
        {
            u8 value = matrix_get(board, width, row, col);
            if (value)
            {
                draw_cell(renderer, row, col, value, offset_x, offset_y);
            }
        }
    }
}

SDL_Texture* loadTexture(SDL_Renderer *renderer, std::string path )
{
	//The final texture
	SDL_Texture* newTexture = NULL;

	//Load image at specified path
	SDL_Surface* loadedSurface = IMG_Load( path.c_str() );
	if( loadedSurface == NULL )
	{
		printf( "Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError() );
	}
	else
	{
		//Create texture from surface pixels
        newTexture = SDL_CreateTextureFromSurface( renderer, loadedSurface );
		if( newTexture == NULL )
		{
			printf( "Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError() );
		}

		//Get rid of old loaded surface
		SDL_FreeSurface( loadedSurface );
	}

	return newTexture;
}


void render_game(Game_State *game,
            SDL_Renderer *renderer,
            TTF_Font *font)
{

    char buffer[4096];

    Color highlight_color = color(0xFF, 0xFF, 0xFF, 0xFF);

    int margin_y = 75;

    draw_board(renderer, game->board, WIDTH, HEIGHT, 60, margin_y);


    if (game->phase == GAME_PHASE_PLAY)
    {
        draw_piece(renderer, &game->piece, 60, margin_y);

        Piece_State piece = game->piece;
        while (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
        {
            piece.offset_row++;
        }
        --piece.offset_row;

        draw_piece(renderer, &piece, 60, margin_y, true);

        draw_piece(renderer, &game->nextPiece, 545, 495);
        draw_piece(renderer, &game->holdPiece, 545, 630);

    }

    if (game->phase == GAME_PHASE_LINE)
    {
        for (int row = 0;
             row < HEIGHT;
             ++row)
        {
            if (game->lines[row])
            {
                int x = 60;
                int y = row * GRID_SIZE + margin_y;

                fill_rect(renderer, x, y,
                          WIDTH * GRID_SIZE, GRID_SIZE, highlight_color);
            }
        }
        draw_piece(renderer, &game->nextPiece, 545, 495);
        draw_piece(renderer, &game->holdPiece, 545, 630);
    }
    else if (game->phase == GAME_PHASE_GAMEOVER)
    {
        int x = 60 + WIDTH * GRID_SIZE / 2;
        int y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        fill_rect(renderer, 95, y - 25, 230, 70, color(0x00, 0x00, 0x00, 0x00));
        draw_string(renderer, font, "GAME OVER",
                    x, y, TEXT_ALIGN_CENTER, highlight_color);
    }
    else if (game->phase == GAME_PHASE_START)
    {
        int x = 60 + WIDTH * GRID_SIZE / 2;
        int y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        fill_rect(renderer, 85, y - 25, 250, 100, color(0x00, 0x00, 0x00, 0x00));

        draw_string(renderer, font, "PRESS SPACE TO START",
                    x, y, TEXT_ALIGN_CENTER, highlight_color);

        snprintf(buffer, sizeof(buffer), "STARTING LEVEL: %d", game->start_level);
        draw_string(renderer, font, buffer,
                    x, y + 30, TEXT_ALIGN_CENTER, highlight_color);
    }

    fill_rect(renderer,
              60, margin_y,
              WIDTH * GRID_SIZE, (HEIGHT - VISIBLE_HEIGHT) * GRID_SIZE,
              color(0x00, 0x00, 0x00, 0x00));


    snprintf(buffer, sizeof(buffer), "LEVEL: %d", game->level);
    draw_string(renderer, font, buffer, 505, 190, TEXT_ALIGN_LEFT, highlight_color);

    snprintf(buffer, sizeof(buffer), "LINES: %d", game->line_count);
    draw_string(renderer, font, buffer, 505, 200 + 120 - 35, TEXT_ALIGN_LEFT, highlight_color);

    snprintf(buffer, sizeof(buffer), "POINTS: %d", game->points);
    draw_string(renderer, font, buffer, 505, 200 + 240 - 55, TEXT_ALIGN_LEFT, highlight_color);
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        return 1;
    }

    if (TTF_Init() < 0)
    {
        return 2;
    }
    int imgFlags = IMG_INIT_PNG;
    if( !( IMG_Init( imgFlags ) & imgFlags ) )
    {
        printf( "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError() );
        return 3;
    }
    if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 ) < 0 )
    {
        printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
        return 4;
    }


    music = Mix_LoadMUS( "sound/main_theme.wav" );
    increaseLVL = Mix_LoadWAV( "sound/se_sys_cursor1.wav" );
    decreaseLVL = Mix_LoadWAV( "sound/se_sys_cursor2.wav" );
    start = Mix_LoadWAV( "sound/me_game_start2.wav" );
    softDrop = Mix_LoadWAV( "sound/se_game_softdrop.wav" );
    move_sfx = Mix_LoadWAV( "sound/se_game_move.wav" );
    rotate_sfx = Mix_LoadWAV( "sound/se_game_rotate.wav" );
    landing = Mix_LoadWAV( "sound/se_game_landing.wav" );
    hardDrop = Mix_LoadWAV( "sound/se_game_harddrop.wav" );
    lvlUp = Mix_LoadWAV( "sound/se_game_lvlup.wav" );
    pause_sfx = Mix_LoadWAV( "sound/se_game_pause.wav" );
    gameover = Mix_LoadWAV( "sound/me_game_gameover.wav" );

    SDL_Window *window = SDL_CreateWindow(
        "Tetris",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800,
        780,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Texture *gTexture = NULL;
    SDL_Texture *nTexture = NULL;
    gTexture = loadTexture(renderer, "image/Back1.png");
    nTexture = loadTexture(renderer, "image/Fix.png");



    const char *font_name = "font/novem___.ttf";
    TTF_Font *font = TTF_OpenFont(font_name, 24);

    Game_State game = {};
    Input_State input = {};

    game.pause = 0;


    Mix_Volume(-1, 64);
    Mix_VolumeMusic(64);

    bool quit = false;
    while (!quit)
    {
        game.time = SDL_GetTicks() / 1000.0f;

        int key_count;
        const u8 *key_states = SDL_GetKeyboardState(&key_count);

        if (key_states[SDL_SCANCODE_ESCAPE])
        {
            quit = true;
        }

        Input_State prev_input = input;

        input.left = key_states[SDL_SCANCODE_LEFT];
        input.right = key_states[SDL_SCANCODE_RIGHT];
        input.up = key_states[SDL_SCANCODE_UP];
        input.down = key_states[SDL_SCANCODE_DOWN];
        input.space = key_states[SDL_SCANCODE_SPACE];
        input.p = key_states[SDL_SCANCODE_P];
        input.m = key_states[SDL_SCANCODE_M];
        input.g = key_states[SDL_SCANCODE_G];
        input.h = key_states[SDL_SCANCODE_H];

        input.dleft = input.left - prev_input.left;
        input.dright = input.right - prev_input.right;
        input.dup = input.up - prev_input.up;
        input.ddown = input.down - prev_input.down;
        input.dspace = input.space - prev_input.space;
        input.dp = input.p - prev_input.p;
        input.dm = input.m - prev_input.m;
        input.dg = input.g - prev_input.g;
        input.dh = input.h - prev_input.h;

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
        }

        if (input.dm > 0)
            {
                    if(game.muted==true)
                    {
                        Mix_Volume(-1, 64);
                        Mix_VolumeMusic(64);
                        game.muted=false;
                    }
                    else
                    {
                        Mix_Volume(-1, 0);
                        Mix_VolumeMusic(0);
                        game.muted=true;
                    }
        }




//        if( Mix_PlayingMusic() == 0 )
//        {
//            Mix_PlayMusic( music, -1 );
//        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        SDL_Rect fullScreenViewport;
        fullScreenViewport.x = 0;
        fullScreenViewport.y = 0;
        fullScreenViewport.w = 800;
        fullScreenViewport.h = 780;
        SDL_RenderSetViewport( renderer, &fullScreenViewport );

        update_game(&game, &input);
        SDL_RenderCopy( renderer, gTexture, NULL, NULL );
        render_game(&game, renderer, font);
        SDL_Rect topLeftViewport;
        topLeftViewport.x = 60;
        topLeftViewport.y = 60 + 15;
        topLeftViewport.w = 300;
        topLeftViewport.h = 60;
        SDL_RenderSetViewport( renderer, &topLeftViewport );

        SDL_RenderCopy( renderer, nTexture, NULL, NULL );

        SDL_RenderPresent(renderer);

//        float frame_time=SDL_GetTicks() / 1000.0f - game.time;
//        if(frame_time < frame_delay) SDL_Delay(frame_delay - frame_time);
    }
    SDL_DestroyTexture( gTexture );
    TTF_CloseFont(font);

    Mix_FreeChunk(increaseLVL);
    Mix_FreeChunk(decreaseLVL);
    Mix_FreeChunk(start);
    Mix_FreeChunk(softDrop);
    Mix_FreeChunk(move_sfx);
    Mix_FreeChunk(rotate_sfx);
    Mix_FreeChunk(landing);
    Mix_FreeChunk(hardDrop);
    Mix_FreeChunk(lvlUp);
    Mix_FreeChunk(pause_sfx);
    Mix_FreeChunk(gameover);

    Mix_FreeMusic(music);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow( window );
    IMG_Quit();
    Mix_Quit();
    SDL_Quit();

    return 0;
}
