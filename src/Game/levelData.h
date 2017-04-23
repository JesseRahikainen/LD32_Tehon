#ifndef LEVEL_DATA_H
#define LEVEL_DATA_H

/*
Levels are 16x8 grids of characters. Primarily used to store state of level so you can move backwards
through them.
This was poorly planned.
*/

#define LEVEL_WIDTH 16
#define LEVEL_HEIGHT 8

typedef struct {
	int entrance, exit; // HACKS!
	int floorImg;
	char data[LEVEL_WIDTH * LEVEL_HEIGHT];
} Level;

Level forest;
Level cave;

// called to set the initial values for all the levels
void levels_Setup( void );

char levels_GetData( Level* level, int x, int y );
void levels_MoveData( Level* level, int prevX, int prevY, int nextX, int nextY, int* outX, int* outY );
void levels_ReplaceData( Level* level, int x, int y, char data );
void levels_ClearData( Level* level, int x, int y );

void levels_MarkData( Level* level, int x, int y );
void levels_UnMarkAllData( Level* level );

void levels_MoveToward( Level* level, int ourX, int ourY, int targetX, int targetY, int* outDX, int* outDY );

#endif /* inclusion guard */