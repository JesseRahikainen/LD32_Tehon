#include "levelData.h"

#include <string.h>
#include <float.h>
#include <assert.h>

#include "../Utils/stretchyBuffer.h"
#include "../Utils/helpers.h"
#include "../System/platformLog.h"
#include "resources.h"

#define LVL_COORD_TO_IDX( x, y ) ( ( x ) + ( LEVEL_WIDTH * ( y ) ) )

Level forestTemplate = { -1, -1, -1,
	{
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '#', 
		' ', ' ', '*', 'S', ' ', ' ', '*', ' ', '*', ' ', '*', ' ', '*', ' ', ' ', '#', 
		' ', ' ', ' ', ' ', ' ', '*', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'G', 'X', 
		' ', ' ', ' ', ' ', '*', ' ', ' ', ' ', 'W', ' ', ' ', ' ', ' ', ' ', ' ', '#', 
		' ', ' ', ' ', ' ', ' ', ' ', '*', ' ', ' ', 'S', ' ', ' ', ' ', ' ', ' ', '#', 
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '#', 
		' ', ' ', '*', ' ', ' ', ' ', ' ', '*', ' ', ' ', '*', '*', ' ', 'W', ' ', '#', 
		'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '#', 
	} };

Level caveTemplate = {  -1, -1, -1,
	{
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'A', ' ', ' ', ' ', ' ', ' ', 
		' ', '#', ' ', ' ', ' ', ' ', ' ', 'R', ' ', ' ', ' ', ' ', ' ', '#', '#', ' ', 
		' ', '#', '#', ' ', '#', '#', '#', '#', '#', ' ', ' ', ' ', ' ', '#', ' ', ' ', 
		'E', '#', '#', ' ', 'A', ' ', ' ', ' ', '#', '#', '#', '#', '#', '#', ' ', 'B', 
		' ', '#', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'A', ' ', '#', ' ', ' ', 
		' ', '#', ' ', ' ', ' ', ' ', '#', ' ', ' ', ' ', ' ', '#', ' ', '#', '#', ' ', 
		' ', ' ', ' ', '#', ' ', ' ', ' ', 'R', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 
		' ', '#', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '#', ' ', ' ', ' ', ' ', ' ', ' ', 
	} };

static void setupHacks( Level* level )
{
	level->exit = -1;
	level->entrance = -1;
	for( int i = 0; i < ARRAY_SIZE( level->data ); ++i ) {
		if( level->data[i] == 'E' ) {
			level->entrance = i;
		}
		if( level->data[i] == 'X' ) {
			level->exit = i;
		}
	}
}

void levels_Setup( void )
{
	// create the forest and caves
	memcpy( &forest, &forestTemplate, sizeof( Level ) );
	memcpy( &cave, &caveTemplate, sizeof( Level ) );

	setupHacks( &forest );
	setupHacks( &cave );

	forest.floorImg = forestFloorImg;
	cave.floorImg = caveFloorImg;
}

char levels_GetData( Level* level, int x, int y )
{
	assert( level );
	assert( x >= 0 );
	assert( y >= 0 );
	assert( x < LEVEL_WIDTH );
	assert( y < LEVEL_HEIGHT );

	return level->data[LVL_COORD_TO_IDX(x,y)];
}

void levels_MoveData( Level* level, int prevX, int prevY, int nextX, int nextY, int* outX, int* outY )
{
	assert( level );
	assert( prevX >= 0 );
	assert( prevY >= 0 );
	assert( prevX < LEVEL_WIDTH );
	assert( prevY < LEVEL_HEIGHT );
	if( nextX < 0 ) return;
	if( nextY < 0 ) return;
	if( nextX >= LEVEL_WIDTH ) return;
	if( nextY >= LEVEL_HEIGHT ) return;

	int nextPos = LVL_COORD_TO_IDX(nextX,nextY);
	int prevPos = LVL_COORD_TO_IDX(prevX,prevY);
	
	char nextData = level->data[nextPos];
	if( ( nextData != ' ' ) && ( nextData != 'X' ) && ( nextData != 'E' ) ) {
		(*outX) = prevX;
		(*outY) = prevY;
		return;
	}

	char data = level->data[prevPos];
	char result = ' ';
	if( prevPos == level->entrance ) {
   		result = 'E';
	} else if( prevPos == level->exit ) {
		result = 'X';
	}
	level->data[prevPos] = result;
	level->data[nextPos] = data;

	(*outX) = nextX;
	(*outY) = nextY;
}

void levels_ReplaceData( Level* level, int x, int y, char data )
{
	assert( level );
	assert( x >= 0 );
	assert( y >= 0 );
	assert( x < LEVEL_WIDTH );
	assert( y < LEVEL_HEIGHT );

	level->data[LVL_COORD_TO_IDX(x,y)] = data;
}

void levels_ClearData( Level* level, int x, int y )
{
	assert( level );
	assert( x >= 0 );
	assert( y >= 0 );
	assert( x < LEVEL_WIDTH );
	assert( y < LEVEL_HEIGHT );

	int pos = LVL_COORD_TO_IDX( x, y );
	char data = ' ';
	if( pos == level->entrance ) {
   		data = 'E';
	} else if( pos == level->exit ) {
		data = 'X';
	}
	level->data[LVL_COORD_TO_IDX(x,y)] = data;
}

void levels_MarkData( Level* level, int x, int y )
{
	assert( level );
	assert( x >= 0 );
	assert( y >= 0 );
	assert( x < LEVEL_WIDTH );
	assert( y < LEVEL_HEIGHT );

	level->data[LVL_COORD_TO_IDX(x,y)] = TURN_ON_BITS( level->data[LVL_COORD_TO_IDX(x,y)], 0x80 );
}

void levels_UnMarkAllData( Level* level )
{
	assert( level );

	for( int y = 0; y < LEVEL_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_WIDTH; ++x ) {
			level->data[LVL_COORD_TO_IDX(x,y)] = TURN_OFF_BITS( level->data[LVL_COORD_TO_IDX(x,y)], 0x80 );
		}
	}
}

// gives us a path towards a position, use A*
typedef struct {
	int loc;
	float cost;
} AStarFrontierData;

typedef struct {
	int from;
	int to;
	float cost;
} AStarPathData;

static float aStarMoveCost( Level* level, int from, int to )
{
	// to spot is blocked
	if( ( level->data[to] == '#' ) || ( level->data[to] == '*' ) ) {
		return FLT_MAX;
	}

	// movement would be a wrap around
	int min = from > to ? to : from;
	int max = from > to ? from : to;
	if( ( ( min % LEVEL_WIDTH ) == ( LEVEL_WIDTH - 1 ) ) && ( max - min ) == 1 ) {
		return FLT_MAX;
	}

	return 1.0f;
}

static float aStarHeuristic( int from, int to )
{
	return 1.0f;
}

void levels_MoveToward( Level* level, int ourX, int ourY, int targetX, int targetY, int* outDX, int* outDY )
{
	int target = LVL_COORD_TO_IDX( targetX, targetY );
	int ourPos = LVL_COORD_TO_IDX( ourX, ourY );
	(*outDX) = 0;
	(*outDY) = 0;

	AStarPathData* sbPathData = NULL;
	AStarFrontierData* sbFrontier = NULL;

	size_t arraySize = ARRAY_SIZE( level->data );
	sb_Add( sbPathData, arraySize );
	for( size_t i = 0; i < sb_Count( sbPathData ); ++i ) {
		sbPathData[i].from = -1;
		sbPathData[i].cost = FLT_MAX;
	}

	AStarFrontierData initASD;
	initASD.loc = ourPos;
	initASD.cost = 0.0f;
	sb_Reserve( sbFrontier, arraySize ); // just reserve some data
	sb_Push( sbFrontier, initASD );

	sbPathData[ourPos].from = ourPos;
	sbPathData[ourPos].cost = 0.0f;

	while( sb_Count( sbFrontier ) > 0 ) {
		AStarFrontierData front = sb_Pop( sbFrontier );

		if( front.loc == target ) {
			break;
		}

		for( int i = 0; i < 4; ++i ) {

			int next = front.loc;
			switch( i ) {
			case 0: // left
				next -= 1;
				break;
			case 1: // right
				next += 1;
				break;
			case 2: // up
				next -= LEVEL_WIDTH;
				break;
			case 3: // down
				next += LEVEL_WIDTH;
				break;
			}

			// if it's outside the level or a blocked spot then don't bother checking
			if( ( next < 0 ) || ( next >= (int)arraySize ) ) continue;

			float newCost = sbPathData[front.loc].cost + aStarMoveCost( level, front.loc, next );
			// doing not set check with this as well because we initialize cost to FLT_MAX
			if( newCost < sbPathData[next].cost ) {
				sbPathData[next].cost = newCost;
				sbPathData[next].from = front.loc;

				// insertion sort into frontier, simulate a priority queue
				AStarFrontierData asfd;
				asfd.loc = next;
				asfd.cost = newCost + aStarHeuristic( next, target );
				size_t idx = 0;
				while( ( idx < sb_Count( sbFrontier ) ) && ( asfd.cost < sbFrontier[idx].cost ) ) {
					++idx;
				}
				sb_Insert( sbFrontier, idx, asfd );
			}
		}
	}

	// see which way we should go
	// generate the path
	int current = target;
	int diff = 0;
	//llog( LOG_INFO, "Path: " );
	//llog( LOG_INFO, " %i, %i   %i", ( current % LEVEL_WIDTH ), ( current / LEVEL_WIDTH ), diff );
	while( current != ourPos ) {
		//sb_Push( sbPathOut->sbPath, current );
		//sb_Insert( sbPathOut->sbPath, 0, current );
		diff = current - sbPathData[current].from;
		current = sbPathData[current].from;
		//llog( LOG_INFO, " %i, %i   %i", ( current % LEVEL_WIDTH ), ( current / LEVEL_WIDTH ), diff );
	}//*/

	//llog( LOG_INFO, "diff: %i", diff );
	if( diff == 0 ) {
		return;
	} else if( diff == -1 ) {
		(*outDX) = -1;
		(*outDY) = 0;
	} else if( diff == 1 ) {
		(*outDX) = 1;
		(*outDY) = 0;
	} else if( diff < 0 ) {
		(*outDX) = 0;
		(*outDY) = -1;
	} else if( diff > 0 ) {
		(*outDX) = 0;
		(*outDY) = 1;
	}
}