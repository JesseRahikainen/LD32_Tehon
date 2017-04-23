#include "gameScreen.h"

#include <stdbool.h>
#include <math.h>

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../Input/input.h"
#include "../System/platformLog.h"
#include "../Utils/helpers.h"
#include "../Utils/stretchyBuffer.h"

#include "../UI/text.h"

#include "../tween.h"

#include "levelData.h"
#include "resources.h"

#include "../System/random.h"

#include "gameOverScreen.h"

#include "../Utils/cfgFile.h"

#include "../sound.h"

/*
Sub states
 Explore
 Fight
 Help
*/

static struct GameState* postHelpState = NULL;
struct GameState gameScreenExploreState;
struct GameState gameScreenFightState;
struct GameState gameScreenHelpState;

static Level* currentLevel = NULL;
static bool killedBoss;

// for the fight screen
#define FIGHT_CENTER_X 256.0f
#define FIGHT_CENTER_Y 128.0f
const static Vector2 PLAYER_BASE =			{ ( FIGHT_CENTER_X - 48.0f ), FIGHT_CENTER_Y };
const static Vector2 OPPONENT_BASE =		{ ( FIGHT_CENTER_X + 48.0f ), FIGHT_CENTER_Y };
const static Vector2 PLAYER_BOTH_ATT =		{ ( FIGHT_CENTER_X - 16.0f ), FIGHT_CENTER_Y };
const static Vector2 OPPONENT_BOTH_ATT =	{ ( FIGHT_CENTER_X + 16.0f ), FIGHT_CENTER_Y };
const static Vector2 PLAYER_ATT_ONLY =		{ ( FIGHT_CENTER_X + 16.0f ), FIGHT_CENTER_Y };
const static Vector2 OPPONENT_ATT_ONLY =	{ ( FIGHT_CENTER_X - 16.0f ), FIGHT_CENTER_Y };
const static Vector2 PLAYER_DEF =			{ ( FIGHT_CENTER_X - 56.0f ), FIGHT_CENTER_Y };
const static Vector2 OPPONENT_DEF =			{ ( FIGHT_CENTER_X + 56.0f ), FIGHT_CENTER_Y };

const static Vector2 PLAYER_DAMAGE_TXT_POS = { ( FIGHT_CENTER_X - 48.0f ), ( FIGHT_CENTER_Y - 28.0f ) };
const static Vector2 OPPONENT_DAMAGE_TXT_POS = { ( FIGHT_CENTER_X + 48.0f ), ( FIGHT_CENTER_Y - 28.0f ) };

typedef enum {
	CGA_NONE = -1,
	CGA_STRONG_ATT, // +1 to attack stat
	CGA_STRONG_DEF, // +1 to defense stat
	CGA_WEAK_ATT, // -1 to attack stat
	CGA_WEAK_DEF, // -1 to defense stat
	CGA_MULTI, // extra attack
	CGA_STUN, // defender is stunned next turn if attacker is able to damage them
	CGA_COUNTER, // for every attack you take the damage and deal the same amount to them
	CGA_RESET, // move back to the start position
	CGA_SKIP, // lets you move additional spots
	CGA_DO_NOTHING // skip your turn
} CombatGridAttachment;

typedef struct {
	CombatGridAttachment attachments[4];
} CombatGridSpot;

typedef struct {
	CombatGridSpot* sbGridSpots;
	int start;
	int width;
	int height;
} CombatGridTemplate;

typedef struct {
	CombatGridTemplate* template;
	Color markerClr;

	float posMoveAmt;
	int pos;
	int prevPos;
} CombatGrid;

typedef enum {
	GTT_PLAYER,
	GTT_WOLF,
	GTT_SCOUT,
	GTT_GUARD,
	GTT_WARRIOR,
	GTT_RAIDER,
	GTT_BOSS,
	NUM_GRID_TEMPLATE_TYPES
} GridTemplateType;

CombatGridTemplate combatGridTemplates[NUM_GRID_TEMPLATE_TYPES];

void combatGrid_CenterOffsetToPos( CombatGrid* grid, int pos, Vector2* out )
{
	out->x = -( ( grid->template->width / 2 ) * 32.0f );
	if( ( grid->template->width % 2 ) == 1 ) {
		out->x -= 16.0f;
	}

	out->y = -( ( grid->template->height / 2 ) * 32.0f );
	if( ( grid->template->height % 2 ) == 1 ) {
		out->y -= 16.0f;
	}

	int x = pos % grid->template->width;
	int y = pos / grid->template->width;

	out->x += x * 32.0f;
	out->y += y * 32.0f;
}

void drawCombatGrid( Vector2 center, CombatGrid* grid )
{
	Vector2 upLeft;

	upLeft.x = center.x - ( ( grid->template->width / 2 ) * 32.0f );
	if( ( grid->template->width % 2 ) == 1 ) {
		upLeft.x -= 16.0f;
	}

	upLeft.y = center.y - ( ( grid->template->height / 2 ) * 32.0f );
	if( ( grid->template->height % 2 ) == 1 ) {
		upLeft.y -= 16.0f;
	}

	Vector2 attachOffsets[] = {
		{ -12.0f, -12.0f },
		{  12.0f, -12.0f },
		{ -12.0f,  12.0f },
		{  12.0f,  12.0f }
	};

	for( int y = 0; y < grid->template->height; ++y ) {
		for( int x = 0; x < grid->template->width; ++x ) {
			Vector2 offset = vec2( x * 32.0f, y * 32.0f );
			Vector2 pos;
			vec2_Add( &upLeft, &offset, &pos );

			img_Draw( gridBGImg, 1, pos, pos, 0 );

			int idx = COORD_TO_IDX( x, y, grid->template->width );

			// draw attachments
			for( int x = 0; x < 4; ++x ) {
				if( grid->template->sbGridSpots[idx].attachments[x] == CGA_NONE ) continue;
				Vector2 attachPos;
				vec2_Add( &pos, &( attachOffsets[x] ), &attachPos );
				img_Draw( sbGridIconImgs[ grid->template->sbGridSpots[idx].attachments[x] ], 1, attachPos, attachPos, 2 );
			}
			
		}
	}

	Vector2 offset, startPos, endPos, pos;
	combatGrid_CenterOffsetToPos( grid, grid->pos, &offset );
	vec2_Add( &center, &offset, &endPos );

	combatGrid_CenterOffsetToPos( grid, grid->prevPos, &offset );
	vec2_Add( &center, &offset, &startPos );

	vec2_Lerp( &startPos, &endPos, grid->posMoveAmt, &pos );

	img_Draw_c( gridMarkerImg, 1, pos, pos, grid->markerClr, grid->markerClr, 1 );
}

void createCombatGridTemplates( void )
{
#define INIT_GRID( tmplt, w, h ) \
	{ \
		( tmplt ).width = (w); \
		( tmplt ).height = (h); \
		( tmplt ).sbGridSpots = NULL; \
		sb_Add( ( tmplt ).sbGridSpots, ( tmplt ).width * ( tmplt ).height ); \
	} while( 0 )
#define SET_ATTACHMENT( tmplt, x, y, v0, v1, v2, v3 ) \
	{ \
		( tmplt ).sbGridSpots[ (x) + ( ( tmplt ).width * (y) ) ].attachments[0] = (v0); \
		( tmplt ).sbGridSpots[ (x) + ( ( tmplt ).width * (y) ) ].attachments[1] = (v1); \
		( tmplt ).sbGridSpots[ (x) + ( ( tmplt ).width * (y) ) ].attachments[2] = (v2); \
		( tmplt ).sbGridSpots[ (x) + ( ( tmplt ).width * (y) ) ].attachments[3] = (v3); \
	} while( 0 )

	// player grid
	INIT_GRID( combatGridTemplates[GTT_PLAYER], 5, 5 );
	combatGridTemplates[GTT_PLAYER].start = 12;
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 0, 0, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_RESET );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 1, 0, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 2, 0, CGA_STRONG_ATT, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 3, 0, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 4, 0, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_STUN, CGA_RESET );

	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 0, 1, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 1, 1, CGA_STRONG_ATT, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 2, 1, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 3, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 4, 1, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 0, 2, CGA_WEAK_DEF, CGA_MULTI, CGA_STRONG_ATT, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 1, 2, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 2, 2, CGA_DO_NOTHING, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 3, 2, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 4, 2, CGA_COUNTER, CGA_STRONG_DEF, CGA_WEAK_ATT, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 0, 3, CGA_MULTI, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 1, 3, CGA_MULTI, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 2, 3, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 3, 3, CGA_WEAK_ATT, CGA_STRONG_DEF, CGA_COUNTER, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 4, 3, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 0, 4, CGA_MULTI, CGA_MULTI, CGA_STRONG_ATT, CGA_RESET );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 1, 4, CGA_MULTI, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 2, 4, CGA_MULTI, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 3, 4, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_PLAYER], 4, 4, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_COUNTER, CGA_RESET );

	// wolf grid
	INIT_GRID( combatGridTemplates[GTT_WOLF], 2, 2 );
	combatGridTemplates[GTT_WOLF].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_WOLF], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WOLF], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_WOLF], 1, 0, CGA_STRONG_ATT, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WOLF], 1, 1, CGA_MULTI, CGA_NONE, CGA_NONE, CGA_NONE );

	// scout grid
	INIT_GRID( combatGridTemplates[GTT_SCOUT], 2, 2 );
	combatGridTemplates[GTT_SCOUT].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_SCOUT], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_SCOUT], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_SCOUT], 1, 0, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_SCOUT], 1, 1, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_NONE, CGA_NONE );

	// guard grid
	INIT_GRID( combatGridTemplates[GTT_GUARD], 3, 3 );
	combatGridTemplates[GTT_GUARD].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 0, 2, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 1, 0, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 1, 1, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 1, 2, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 2, 0, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 2, 1, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_GUARD], 2, 2, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_COUNTER, CGA_NONE );

	// warrior grid
	INIT_GRID( combatGridTemplates[GTT_WARRIOR], 3, 3 );
	combatGridTemplates[GTT_WARRIOR].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 0, 2, CGA_STUN, CGA_NONE, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 1, 0, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 1, 1, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 1, 2, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 2, 0, CGA_STUN, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 2, 1, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_WARRIOR], 2, 2, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_STUN, CGA_NONE );

	// raider grid
	INIT_GRID( combatGridTemplates[GTT_RAIDER], 3, 3 );
	combatGridTemplates[GTT_RAIDER].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 0, 2, CGA_COUNTER, CGA_NONE, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 1, 0, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 1, 1, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 1, 2, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 2, 0, CGA_COUNTER, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 2, 1, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_RAIDER], 2, 2, CGA_STRONG_ATT, CGA_MULTI, CGA_MULTI, CGA_WEAK_DEF );

	// boss grid
	INIT_GRID( combatGridTemplates[GTT_BOSS], 4, 4 );
	combatGridTemplates[GTT_BOSS].start = 0;
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 0, 0, CGA_NONE, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 0, 1, CGA_STRONG_DEF, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 0, 2, CGA_STRONG_DEF, CGA_COUNTER, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 0, 3, CGA_STRONG_ATT, CGA_MULTI, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 1, 0, CGA_STRONG_ATT, CGA_NONE, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 1, 1, CGA_STRONG_DEF, CGA_STRONG_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 1, 2, CGA_STRONG_DEF, CGA_MULTI, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 1, 3, CGA_STRONG_ATT, CGA_COUNTER, CGA_NONE, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 2, 0, CGA_STUN, CGA_WEAK_ATT, CGA_NONE, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 2, 1, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_WEAK_DEF, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 2, 2, CGA_MULTI, CGA_STUN, CGA_WEAK_ATT, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 2, 3, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_NONE );

	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 3, 0, CGA_STRONG_ATT, CGA_STRONG_ATT, CGA_WEAK_DEF, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 3, 1, CGA_STUN, CGA_STRONG_DEF, CGA_WEAK_ATT, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 3, 2, CGA_STRONG_ATT, CGA_STRONG_DEF, CGA_STRONG_DEF, CGA_NONE );
	SET_ATTACHMENT( combatGridTemplates[GTT_BOSS], 3, 3, CGA_STRONG_ATT, CGA_MULTI, CGA_COUNTER, CGA_RESET );

#undef INIT_GRID
#undef SET_ATTACHMENT
}

void destroyCombatGridTemplates( void )
{
	for( int i = 0; i < NUM_GRID_TEMPLATE_TYPES; ++i ) {
		sb_Clear( combatGridTemplates[i].sbGridSpots );
	}
}

static int combatGrid_CountCurrentAttachmentTypes( CombatGrid* grid, CombatGridAttachment type )
{
	int v = 0;
	for( int i = 0; i < ARRAY_SIZE( grid->template->sbGridSpots[ grid->pos ].attachments ); ++i ) {
		if( grid->template->sbGridSpots[ grid->pos ].attachments[i] == type ) ++v;
	}
	return v;
}

// Explore state, update the level, do enemy processing, and player movement
static struct GameStateMachine gameScreenStateMachine;

static Vector2 wldToScr( int x, int y )
{
	Vector2 pos;
	pos.x = 16.0f + ( x * 32.0f );
	pos.y = 16.0f + ( y * 32.0f );
	return pos;
}

typedef struct {
	char data; // what level data to tie to
	int attack;
	int defense;
	int health;
	GridTemplateType combatGridTemplate;
	int* imgs;
} EnemyTemplate;

EnemyTemplate enemies[] = {
	{ 'W', 2, 2, 6, GTT_WOLF, NULL },
	{ 'S', 2, 1, 6, GTT_SCOUT, NULL },
	{ 'G', 3, 2, 11, GTT_GUARD, NULL },
	{ 'A', 2, 2, 10, GTT_WARRIOR, NULL },
	{ 'R', 3, 1, 9, GTT_RAIDER, NULL },
	{ 'B', 3, 3, 12, GTT_BOSS, NULL },
};


typedef struct {
	int attack;
	int defense;

	int maxHealth;
	int maxStamina;

	int currHealth;
	int currStamina;

	int staminaRegen;

	CombatGrid grid;

	int* images;

	int currImgIdx;
	Vector2 imgPos;
	Color imgClr;

	Vector2 combatTextPos;

	int attacksTaken;
	bool isStunned;

	bool isBoss;
} Creature;

static Creature player;
static Creature opponent;

void drawCreatureStats( Vector2 base, Creature* creature )
{
	char string[256];
	if( creature->currHealth > 0 ) {
		if( creature->maxStamina > 0 ) {
			SDL_snprintf( string, 255, "<G>Stamina: %i/%i    <R>Health: %i/%i", creature->currStamina, creature->maxStamina, creature->currHealth, creature->maxHealth );
		} else {
			SDL_snprintf( string, 255, "<R>Health: %i/%i", creature->currHealth, creature->maxHealth );
		}
	} else {
		SDL_snprintf( string, 255, "<R>DEAD!", creature->currHealth, creature->maxHealth );
	}
	
	txt_DisplayString( string, base, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	base = creature->imgPos;
	base.y += 24.0f;
	if( creature->isStunned ) {
		txt_DisplayString( "Stunned", base, CLR_YELLOW, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 1, 9 );
	}
}

// ****** Fight state
static int rollDie( int size )
{
	return ( rand_GetU8( NULL ) % size ) + 1;
}

static int roll( int stat )
{
	if( stat < 0 ) return 0;

	int value;
	switch( stat ) {
	case 0:
		value = 1;
		break;
	case 1:
		value = rollDie( 4 );
		break;
	case 2:
		value = rollDie( 6 );
		break;
	case 3:
		value = rollDie( 8 );
		break;
	case 4:
		value = rollDie( 10 );
		break;
	default:
		value = rollDie( 12 );
		break;
	}

	stat -= 6;
	return value + roll( stat );
}

static int rollConflict( int attack, int defense )
{
	int attRoll = roll( attack );
	int defRoll = roll( defense );
	int result = MIN( 0, defRoll - attRoll );
	//llog( LOG_INFO, "Roll: att: %i   def: %i   result: %i", attRoll, defRoll, result );
	return result;
}

typedef struct {
	char text[64];
	Color clr;
	Vector2 basePos;
	Vector2 targetPos;
	float timeAlive;
	bool inUse;
	bool isPlayer;
} CombatTextPopUp;
#define POP_UP_MAX_LIFE 1.0f

CombatTextPopUp* sbCombatPopUpQueue = NULL;
static float lastPlayerPopUpActivated = 0.0f;
static float lastOpponentPopUpActivated = 0.0f;

static void clearCombatPopUps( void )
{
	sb_Clear( sbCombatPopUpQueue );
}

static void renderCombatPopUps( void )
{
	for( size_t i = 0; i < sb_Count( sbCombatPopUpQueue ); ++i ) {
		if( !sbCombatPopUpQueue[i].inUse ) continue;

		Vector2 pos;
		vec2_Lerp( &( sbCombatPopUpQueue[i].basePos ), &( sbCombatPopUpQueue[i].targetPos ), easeOutQuad( sbCombatPopUpQueue[i].timeAlive / POP_UP_MAX_LIFE ),
			&pos );

		Color clr = sbCombatPopUpQueue[i].clr;
		clr.a = 1.0f - easeInQuad( sbCombatPopUpQueue[i].timeAlive / POP_UP_MAX_LIFE );

		txt_DisplayString( sbCombatPopUpQueue[i].text, pos, clr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER,
			inGameFont, 1, 10 );
	}
}

static bool checkPopUpActivation( bool isPlayer, float timeSinceLast )
{
	if( timeSinceLast >= ( POP_UP_MAX_LIFE / 3.0f ) ) {
		for( size_t i = 0; i < sb_Count( sbCombatPopUpQueue ); ++i ) {
			if( sbCombatPopUpQueue[i].inUse ) continue;
			if( sbCombatPopUpQueue[i].isPlayer != isPlayer ) continue;

			sbCombatPopUpQueue[i].inUse = true;
			return true;
		}
	}

	return false;
}

static void updateCombatPopUps( float dt )
{
	// check the queue for any that can be used
	//  if the time since the last player pop up is greater than half the lifetime of a pop-up or there are
	//   no other player pop-ups active, then create activate the next one
	lastPlayerPopUpActivated += dt;
	if( checkPopUpActivation( true, lastPlayerPopUpActivated ) ) {
		lastPlayerPopUpActivated = 0.0f;
	}

	lastOpponentPopUpActivated += dt;
	if( checkPopUpActivation( false, lastOpponentPopUpActivated ) ) {
		lastOpponentPopUpActivated = 0.0f;
	}

	for( size_t i = 0; i < sb_Count( sbCombatPopUpQueue ); ++i ) {
		if( !sbCombatPopUpQueue[i].inUse ) continue;

		sbCombatPopUpQueue[i].timeAlive += dt;

		if( sbCombatPopUpQueue[i].timeAlive > POP_UP_MAX_LIFE ){
			sb_Remove( sbCombatPopUpQueue, i );
			--i;
			continue;
		}
	}
}

static void pushCombatPopUp_V( Color clr, Vector2 startPos, bool isPlayer, char* text, va_list ap )
{
	CombatTextPopUp* popUp = sb_Add( sbCombatPopUpQueue, 1 );

	SDL_vsnprintf( popUp->text, ARRAY_SIZE( popUp->text ), text, ap );
	popUp->clr = clr;
	popUp->basePos = startPos;
	Vector2 offset = { 0.0f, -50.0f };
	vec2_Add( &( popUp->basePos ), &offset, &( popUp->targetPos ) );
	popUp->isPlayer = isPlayer;
	popUp->timeAlive = 0.0f;
	popUp->inUse = false;
}

static void pushCombatPopUp( Color clr, Vector2 startPos, bool isPlayer, char* text, ... )
{
	va_list ap;
	va_start( ap, text );
	pushCombatPopUp_V( clr, startPos, isPlayer, text, ap );
	va_end( ap );
}

typedef void (*CombatActionFunction)(void);

typedef struct {
	Vector2 outPos;
	Vector2 inPos;

	int outImg;
	int inImg;

	Color outClr;
	Color inClr;
} CreatureCombatActionState;

typedef struct {

	CreatureCombatActionState playerState;
	CreatureCombatActionState opponentState;

	CombatActionFunction outDoneFunction;
	CombatActionFunction inDoneFunction;

	float outTimeMax;
	float outTimeLeft;

	float inTimeMax;
	float inTimeLeft;

	bool checkForDeath;
} CombatAction;

static CombatAction* sbCombatActions;

static void addBothAttackAction( void );
static void addPlayerAttackAction( void );
static void addOpponentAttackAction( void );
static void addBothDieAction( void );
static void addPlayerDieAction( void );
static void addOpponentDieAction( void );

static void processCombatAction( float dt )
{
	if( sb_Count( sbCombatActions ) <= 0 ) return;

	// grab the first one and process it
	if( sbCombatActions[0].outTimeLeft > 0.0f ) {
		// process go out

		float t = sbCombatActions[0].outTimeLeft / sbCombatActions[0].outTimeMax;

		vec2_Lerp( &( sbCombatActions[0].playerState.outPos ), &( sbCombatActions[0].playerState.inPos ), t, &( player.imgPos ) );
		vec2_Lerp( &( sbCombatActions[0].opponentState.outPos ), &( sbCombatActions[0].opponentState.inPos ), t, &( opponent.imgPos ) );
		player.imgClr = sbCombatActions[0].playerState.inClr;
		opponent.imgClr = sbCombatActions[0].opponentState.inClr;

		player.currImgIdx = sbCombatActions[0].playerState.outImg;
		opponent.currImgIdx = sbCombatActions[0].opponentState.outImg;

		sbCombatActions[0].outTimeLeft -= dt;

		if( sbCombatActions[0].outTimeLeft <= 0.0f ) {
			if( sbCombatActions[0].outDoneFunction != NULL ) {
				sbCombatActions[0].outDoneFunction( );
			}
		}
	} else {
		// process come back in
		float t = sbCombatActions[0].inTimeLeft / sbCombatActions[0].inTimeMax;

		vec2_Lerp( &( sbCombatActions[0].playerState.inPos ), &( sbCombatActions[0].playerState.outPos ), t, &( player.imgPos ) );
		vec2_Lerp( &( sbCombatActions[0].opponentState.inPos ), &( sbCombatActions[0].opponentState.outPos ), t, &( opponent.imgPos ) );
		clr_Lerp( &( sbCombatActions[0].playerState.inClr ), &( sbCombatActions[0].playerState.outClr ), t, &( player.imgClr ) );
		clr_Lerp( &( sbCombatActions[0].opponentState.inClr ), &( sbCombatActions[0].opponentState.outClr ), t, &( opponent.imgClr ) );

		player.currImgIdx = sbCombatActions[0].playerState.inImg;
		opponent.currImgIdx = sbCombatActions[0].opponentState.inImg;

		sbCombatActions[0].inTimeLeft -= dt;
	}

	if( sbCombatActions[0].inTimeLeft <= 0.0f ) {
		// done, return to the default position and data
		player.currImgIdx = IDLE_SPR;
		player.imgPos = PLAYER_BASE;
		player.imgClr = CLR_WHITE;
			
		opponent.currImgIdx = IDLE_SPR;
		opponent.imgPos = OPPONENT_BASE;
		opponent.imgClr = CLR_WHITE;

		sb_Remove( sbCombatActions, 0 );

		// check for death
		bool playerDie = sbCombatActions[0].checkForDeath && ( player.currHealth <= 0 );
		bool opponentDie = sbCombatActions[0].checkForDeath && ( opponent.currHealth <= 0 );
		if( playerDie && opponentDie ) {
			addBothDieAction( );
		} else if( playerDie ) {
			addPlayerDieAction( );
		} else if( opponentDie ) {
			addOpponentDieAction( );
		} else {
			if( sbCombatActions[0].inDoneFunction != NULL ) {
				sbCombatActions[0].inDoneFunction( );
			}
		}
	}
}

static int attacksThisRoundFor( Creature* c )
{
	int attacks = 1;

	CombatGridSpot* gridSpot = &( c->grid.template->sbGridSpots[ c->grid.pos ] );
	for( int i = 0; i < ARRAY_SIZE( gridSpot->attachments ); ++i ) {
		switch( gridSpot->attachments[i] ) {
		case CGA_MULTI:
			++attacks;
			break;
		}
	}

	//llog( LOG_INFO, "attacks %i", attacks );
	return attacks;
}

static void combat_AttackDone( void )
{
	// no beating dead horses
	if( ( player.currHealth <= 0 ) && ( opponent.currHealth <= 0 ) ) {
		return;
	}

	// check to see if any have multi-attacks left
	bool playerAttack = ( attacksThisRoundFor( &player ) > player.attacksTaken ) && !player.isStunned;
	bool opponentAttack = ( attacksThisRoundFor( &opponent ) > opponent.attacksTaken ) && !opponent.isStunned;

	if( playerAttack && opponentAttack ) {
		addBothAttackAction( );
	} else if( playerAttack ) {
		addPlayerAttackAction( );
	} else if( opponentAttack ) {
		addOpponentAttackAction( );
	} // otherwise there's nothing to do
}

static void getCombatStats( Creature* c, int* outAtt, int* outDef )
{
	(*outAtt) = c->attack + combatGrid_CountCurrentAttachmentTypes( &( c->grid ), CGA_STRONG_ATT ) - combatGrid_CountCurrentAttachmentTypes( &( c->grid ), CGA_WEAK_ATT );
	(*outDef) = c->defense + combatGrid_CountCurrentAttachmentTypes( &( c->grid ), CGA_STRONG_DEF ) - combatGrid_CountCurrentAttachmentTypes( &( c->grid ), CGA_WEAK_DEF );
}

static void damageCreature( Creature* defender, int dmg, bool isPlayer, Creature* attacker, bool canCounter, char* extra )
{
	// check for stun
	if( combatGrid_CountCurrentAttachmentTypes( &( attacker->grid ), CGA_STUN ) ) {
		pushCombatPopUp( CLR_YELLOW, defender->combatTextPos, isPlayer, "Stunned!", extra );
		defender->isStunned = true;
	}

	if( dmg >= 0 ) {
		pushCombatPopUp( CLR_BLUE, defender->combatTextPos, isPlayer, "Guard%s!", extra );
	} else if( defender->currStamina > 0 ) {
		defender->currStamina = MAX( 0, defender->currStamina + dmg );
		pushCombatPopUp( CLR_GREEN, defender->combatTextPos, isPlayer, "%i Stamina%s!", dmg, extra );
	} else {
		defender->currHealth = MAX( 0, defender->currHealth + dmg );
		pushCombatPopUp( CLR_RED, defender->combatTextPos, isPlayer, "%i Health%s!", dmg, extra );
	}

	if( canCounter && combatGrid_CountCurrentAttachmentTypes( &( defender->grid ), CGA_COUNTER ) > 1 ) {
		// reflect the damage back to the attacker
		damageCreature( attacker, dmg, !isPlayer, defender, false, " Countered" );
	}
}

static void combat_ProcessPlayerAttack( void )
{
	int playerAtt, playerDef;
	getCombatStats( &player, &playerAtt, &playerDef );

	int opponentAtt, opponentDef;
	getCombatStats( &opponent, &opponentAtt, &opponentDef );

	damageCreature( &opponent, rollConflict( playerAtt, opponentDef ), false, &player, true, "" );

	++player.attacksTaken;
}

static void combat_ProcessOpponentAttack( void )
{
	int playerAtt, playerDef;
	getCombatStats( &player, &playerAtt, &playerDef );

	int opponentAtt, opponentDef;
	getCombatStats( &opponent, &opponentAtt, &opponentDef );

	damageCreature( &player, rollConflict( opponentAtt, playerDef ), true, &opponent, true, "" );

	++opponent.attacksTaken;
}

static void combat_PlayerAttack( void )
{
	snd_Play( defendSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	combat_ProcessPlayerAttack( );
}

static void combat_OpponentAttack( void )
{
	snd_Play( defendSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	combat_ProcessOpponentAttack( );
}

static void combat_BothAttack( void )
{
	snd_Play( clashSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	combat_ProcessPlayerAttack( );
	combat_ProcessOpponentAttack( );
}

static void combat_DeathDone( void )
{
	if( player.currHealth <= 0 ) {
		// end game
		if( killedBoss ) {
			ending = 1;
		} else {
			ending = 0;
		}
		gsmEnterState( &globalFSM, &gameOverScreenState );
	} else {
		// go to explore state
		gsmEnterState( &gameScreenStateMachine, &gameScreenExploreState );
	}
}

static void addBothAttackAction( void )
{
	CombatAction action;

	action.inTimeMax = 0.2f;
	action.inTimeLeft = action.inTimeMax;
	action.outTimeMax = 0.1f;
	action.outTimeLeft = action.outTimeMax;
	action.outDoneFunction = combat_BothAttack;
	action.inDoneFunction = combat_AttackDone;
	action.checkForDeath = true;

	action.playerState.inClr = CLR_WHITE;
	action.playerState.outClr = CLR_RED;
	action.playerState.inImg = ATTACK_SPR;
	action.playerState.outImg = DEFEND_SPR;
	action.playerState.inPos = PLAYER_BASE;
	action.playerState.outPos = PLAYER_BOTH_ATT;

	action.opponentState.inClr = CLR_WHITE;
	action.opponentState.outClr = CLR_RED;
	action.opponentState.inImg = ATTACK_SPR;
	action.opponentState.outImg = DEFEND_SPR;
	action.opponentState.inPos = OPPONENT_BASE;
	action.opponentState.outPos = OPPONENT_BOTH_ATT;

	sb_Push( sbCombatActions, action );
}

static void addPlayerAttackAction( void )
{
	CombatAction* action = sb_Add( sbCombatActions, 1 );

	action->inTimeMax = 0.2f;
	action->inTimeLeft = action->inTimeMax;
	action->outTimeMax = 0.1f;
	action->outTimeLeft = action->outTimeMax;
	action->outDoneFunction = combat_PlayerAttack;
	action->inDoneFunction = combat_AttackDone;
	action->checkForDeath = true;

	action->playerState.inClr = CLR_WHITE;
	action->playerState.outClr = CLR_WHITE;
	action->playerState.inImg = ATTACK_SPR;
	action->playerState.outImg = IDLE_SPR;
	action->playerState.inPos = PLAYER_BASE;
	action->playerState.outPos = PLAYER_ATT_ONLY;

	action->opponentState.inClr = CLR_WHITE;
	action->opponentState.outClr = CLR_RED;
	action->opponentState.inImg = DEFEND_SPR;
	action->opponentState.outImg = DEFEND_SPR;
	action->opponentState.inPos = OPPONENT_BASE;
	action->opponentState.outPos = OPPONENT_DEF;
}

static void addOpponentAttackAction( void )
{
	CombatAction* action = sb_Add( sbCombatActions, 1 );

	action->inTimeMax = 0.2f;
	action->inTimeLeft = action->inTimeMax;
	action->outTimeMax = 0.1f;
	action->outTimeLeft = action->outTimeMax;
	action->outDoneFunction = combat_OpponentAttack;
	action->inDoneFunction = combat_AttackDone;
	action->checkForDeath = true;

	action->playerState.inClr = CLR_WHITE;
	action->playerState.outClr = CLR_RED;
	action->playerState.inImg = DEFEND_SPR;
	action->playerState.outImg = DEFEND_SPR;
	action->playerState.inPos = PLAYER_BASE;
	action->playerState.outPos = PLAYER_DEF;

	action->opponentState.inClr = CLR_WHITE;
	action->opponentState.outClr = CLR_WHITE;
	action->opponentState.inImg = ATTACK_SPR;
	action->opponentState.outImg = IDLE_SPR;
	action->opponentState.inPos = OPPONENT_BASE;
	action->opponentState.outPos = OPPONENT_ATT_ONLY;
}

static void addPlayerDieAction( void )
{
	CombatAction* action = sb_Add( sbCombatActions, 1 );

	snd_Play( playerDeathSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );

	action->inTimeMax = 0.75f;
	action->inTimeLeft = action->inTimeMax;
	action->outTimeMax = 0.0f;
	action->outTimeLeft = action->outTimeMax;
	action->outDoneFunction = NULL;
	action->inDoneFunction = combat_DeathDone;
	action->checkForDeath = false;

	action->playerState.inClr = CLR_WHITE;
	action->playerState.outClr = CLR_WHITE;
	action->playerState.inImg = DEAD_SPR;
	action->playerState.outImg = DEAD_SPR;
	action->playerState.inPos = PLAYER_BASE;
	action->playerState.outPos = PLAYER_BASE;

	action->opponentState.inClr = CLR_WHITE;
	action->opponentState.outClr = 	CLR_WHITE;
	action->opponentState.inImg = IDLE_SPR;
	action->opponentState.outImg = IDLE_SPR;
	action->opponentState.inPos = OPPONENT_BASE;
	action->opponentState.outPos = OPPONENT_BASE;
}

static void addOpponentDieAction( void )
{
	CombatAction* action = sb_Add( sbCombatActions, 1 );

	if( opponent.isBoss ) {
		killedBoss = true;
	}

	snd_Play( monsterDeathSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );

	action->inTimeMax = 0.75f;
	action->inTimeLeft = action->inTimeMax;
	action->outTimeMax = 0.0f;
	action->outTimeLeft = action->outTimeMax;
	action->outDoneFunction = NULL;
	action->inDoneFunction = combat_DeathDone;
	action->checkForDeath = false;

	action->playerState.inClr = CLR_WHITE;
	action->playerState.outClr = CLR_WHITE;
	action->playerState.inImg = IDLE_SPR;
	action->playerState.outImg = IDLE_SPR;
	action->playerState.inPos = PLAYER_BASE;
	action->playerState.outPos = PLAYER_BASE;

	action->opponentState.inClr = CLR_WHITE;
	action->opponentState.outClr = CLR_WHITE;
	action->opponentState.inImg = DEAD_SPR;
	action->opponentState.outImg = DEAD_SPR;
	action->opponentState.inPos = OPPONENT_BASE;
	action->opponentState.outPos = OPPONENT_BASE;
}

static void addBothDieAction( void )
{
	CombatAction* action = sb_Add( sbCombatActions, 1 );

	if( opponent.isBoss ) {
		killedBoss = true;
	}

	snd_Play( playerDeathSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	snd_Play( monsterDeathSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );

	action->inTimeMax = 0.75f;
	action->inTimeLeft = action->inTimeMax;
	action->outTimeMax = 0.0f;
	action->outTimeLeft = action->outTimeMax;
	action->outDoneFunction = NULL;
	action->inDoneFunction = combat_DeathDone;
	action->checkForDeath = false;

	action->playerState.inClr = CLR_WHITE;
	action->playerState.outClr = CLR_WHITE;
	action->playerState.inImg = DEAD_SPR;
	action->playerState.outImg = DEAD_SPR;
	action->playerState.inPos = PLAYER_BASE;
	action->playerState.outPos = PLAYER_BASE;

	action->opponentState.inClr = CLR_WHITE;
	action->opponentState.outClr = CLR_WHITE;
	action->opponentState.inImg = DEAD_SPR;
	action->opponentState.outImg = DEAD_SPR;
	action->opponentState.inPos = OPPONENT_BASE;
	action->opponentState.outPos = OPPONENT_BASE;
}

static bool isValidFightMove( CombatGrid* grid, int prevPos, int newPos )
{
	if( newPos < 0 ) return false;
	if( newPos >= ( grid->template->width * grid->template->height ) ) return false;

	// attempting to wrap around an edge
	if( ( ( newPos % grid->template->width ) == 0 ) && ( prevPos < newPos ) && ( ( newPos - prevPos ) == 1 ) ) return false;
	if( ( ( prevPos % grid->template->width ) == 0 ) && ( newPos < prevPos ) && ( ( prevPos - newPos ) == 1 ) ) return false;

	return true;
}

static void processEnemyAttackMove( void )
{
	if( opponent.isStunned || ( combatGrid_CountCurrentAttachmentTypes( &( opponent.grid ), CGA_DO_NOTHING ) > 0 ) ) return;

	// just choose a random direction (must be valid)
	int newPos;
	do {
		newPos = opponent.grid.pos;
		switch( rand_GetU32( NULL ) % 4 ) {
		case 0:
			newPos += 1;
			break;
		case 1:
			newPos -= 1;
			break;
		case 2:
			newPos += opponent.grid.template->width;
			break;
		case 3:
			newPos -= opponent.grid.template->width;
			break;
		}
	} while( !isValidFightMove( &( opponent.grid ), opponent.grid.pos, newPos ) );

	opponent.grid.pos = newPos;
	opponent.grid.posMoveAmt = 0.0f;
}

static bool acceptingFightInput( void )
{
	// if the pieces haven't stopped moving
	if( player.grid.pos != player.grid.prevPos ) return false;
	if( opponent.grid.pos != opponent.grid.prevPos ) return false;

	// if we're still processing actions
	if( sb_Count( sbCombatActions ) > 0 ) return false;

	return true;
}

static void fight_processPress( int newPos )
{
	if( !isValidFightMove( &( player.grid ), player.grid.pos, newPos ) ) return;
	if( !acceptingFightInput( ) ) return;

	player.grid.pos = newPos;
	player.grid.posMoveAmt = 0.0f;

	// let the enemy choose their spot now
	processEnemyAttackMove( );
}

static void fight_pressUp( void )
{
	fight_processPress( player.grid.pos - player.grid.template->width );
}

static void fight_pressDown( void )
{
	fight_processPress( player.grid.pos + player.grid.template->width );
}

static void fight_pressLeft( void )
{
	fight_processPress( player.grid.pos - 1 );
}

static void fight_pressRight( void )
{
	fight_processPress( player.grid.pos + 1 );
}

static void fight_pressReset( void )
{
	fight_processPress( player.grid.template->start );
}

static void fightHelp( void )
{
	postHelpState = &gameScreenFightState;
	gsmEnterState( &gameScreenStateMachine, &gameScreenHelpState );
}

typedef enum {
	FS_RESET_MARKERS, // special case state if any of the markers have the reset attachment
	FS_CHOOSE_ACTION,
	FS_MOVE_MARKERS,
	FS_COMBAT
} FightState;
static FightState fightState;
static int gameScreen_Fight_Enter( void )
{
	fightState = FS_CHOOSE_ACTION;
	input_BindOnKeyPress( SDLK_w, fight_pressUp );
	input_BindOnKeyPress( SDLK_s, fight_pressDown );
	input_BindOnKeyPress( SDLK_a, fight_pressLeft );
	input_BindOnKeyPress( SDLK_d, fight_pressRight );
	input_BindOnKeyPress( SDLK_SPACE, fight_pressReset );
	input_BindOnKeyPress( SDLK_h, fightHelp );

	clearCombatPopUps( );

	sbCombatActions = NULL;

	snd_PlayStreaming( combatMusic, 1.0f, 0.0f );

	return 1;
}

static int gameScreen_Fight_Exit( void )
{
	input_ClearAllKeyBinds( );

	player.isStunned = false;

	snd_StopStreaming( combatMusic );

	return 1;
}

static void gameScreen_Fight_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Fight_Process( void )
{
}

static void gameScreen_Fight_Draw( void )
{
	drawCreatureStats( vec2( 100.0f, 272.0f ), &player );
	drawCreatureStats( vec2( 400.0f, 272.0f ), &opponent );

	drawCombatGrid( vec2( 100.0f, 128.0f ), &( player.grid ) );
	drawCombatGrid( vec2( 400.0f, 128.0f ), &( opponent.grid ) );

	renderCombatPopUps( );

	img_Draw_c( player.images[player.currImgIdx], 1, player.imgPos, player.imgPos, player.imgClr, player.imgClr, 2 );
	img_Draw_c( opponent.images[opponent.currImgIdx], 1, opponent.imgPos, opponent.imgPos, opponent.imgClr, opponent.imgClr, 2 );
}

static bool updateGridPos( CombatGrid* grid, float dt )
{
	if( grid->pos != grid->prevPos ) {
		grid->posMoveAmt += dt * 4.0f;

		if( grid->posMoveAmt >= 1.0f ) {
			grid->posMoveAmt = 1.0f;
			grid->prevPos = grid->pos;

			return true;
		}

		return false;
	}

	return true;
}

static void gameScreen_Fight_PhysicsTick( float dt )
{
	if( fightState == FS_RESET_MARKERS ) {
		if( updateGridPos( &( player.grid ), dt ) && updateGridPos( &( opponent.grid ), dt ) ) {
			fightState = FS_CHOOSE_ACTION;
		}
	} else if( fightState == FS_CHOOSE_ACTION ) {
		if( player.isStunned ) {
			processEnemyAttackMove( );
		}

		if( ( player.grid.pos != player.grid.prevPos ) || ( opponent.grid.pos != opponent.grid.prevPos ) ) {
			fightState = FS_MOVE_MARKERS;
		}
	} else if( fightState == FS_MOVE_MARKERS ) {
		if( updateGridPos( &( player.grid ), dt ) && updateGridPos( &( opponent.grid ), dt ) ) {
			bool playerAttack = !player.isStunned && ( combatGrid_CountCurrentAttachmentTypes( &( player.grid ), CGA_DO_NOTHING ) <= 0 );
			bool opponentAttack = !opponent.isStunned && ( combatGrid_CountCurrentAttachmentTypes( &( opponent.grid ), CGA_DO_NOTHING ) <= 0 );

			player.attacksTaken = player.isStunned ? 1000 : 0;
			opponent.attacksTaken = opponent.isStunned ? 1000 : 0;

			player.isStunned = false;
			opponent.isStunned = false;

			if( playerAttack && opponentAttack ) {
				addBothAttackAction( );
			} else if( playerAttack ) {
				addPlayerAttackAction( );
			} else {
				addOpponentAttackAction( );
			}

			fightState = FS_COMBAT;
		}
	} else if( fightState == FS_COMBAT ) {
		processCombatAction( dt );
		if( sb_Count( sbCombatActions ) <= 0 ) {

			if( combatGrid_CountCurrentAttachmentTypes( &( player.grid ), CGA_RESET ) ) {
				player.grid.pos = player.grid.template->start;
				player.grid.posMoveAmt = 0.0f;
			}
			if( combatGrid_CountCurrentAttachmentTypes( &( opponent.grid ), CGA_RESET ) ) {
				opponent.grid.pos = opponent.grid.template->start;
				opponent.grid.posMoveAmt = 0.0f;
			}
			fightState = FS_RESET_MARKERS;
		}
	}

	updateCombatPopUps( dt );
}

struct GameState gameScreenFightState = { gameScreen_Fight_Enter, gameScreen_Fight_Exit, gameScreen_Fight_ProcessEvents,
	gameScreen_Fight_Process, gameScreen_Fight_Draw, gameScreen_Fight_PhysicsTick };

// ****** Help state
static void exitHelp( void )
{
	gsmEnterState( &gameScreenStateMachine, postHelpState );
}

static int gameScreen_Help_Enter( void )
{
	input_BindOnKeyRelease( SDLK_x, exitHelp );
	return 1;
}

static int gameScreen_Help_Exit( void )
{
	input_ClearAllKeyBinds( );
	return 1;
}

static void gameScreen_Help_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Help_Process( void )
{
}

static void gameScreen_Help_Draw( void )
{
	txt_DisplayTextArea(
		(const uint8_t*)"<L>In the explore mode you have a map of the area. You recover stamina every turn that passes.\n"
		"Enemies will chase you if you get close enough and they see you.\n"
		"You goal is to defeat the leader of the orcs and then escape.\n\n"
		"<W>Controls\n"
		" <G>W <L>- Move Up\n"
		" <G>S <L>- Move Down\n"
		" <G>A <L>- Move Left\n"
		" <G>D <L>- Move Right\n"
		" <G>Space <L>- Wait\n"
		" <G>H <L>- Help\n",
		vec2( 16.0f, 16.0f ), vec2( 256.0f - 32.0f, 256.0f - 32.0f ), CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, inGameFont, 0, NULL, 1, 0 );

	txt_DisplayTextArea(
		(const uint8_t*)"<L>In combat you and your opponent will face against each other. If you lose all your health you die. "
		"If you have stamina while taking damage that will be decreased first.\n"
		"To fight you and your opponent will move your markers around you combat grid, what you move to will determine what you do during that turn.\n\n"
		"<W>Controls\n"
		" <G>W <L>- Move Marker Up\n"
		" <G>S <L>- Move Marker Down\n"
		" <G>A <L>- Move Marker Left\n"
		" <G>D <L>- Move Marker Right\n"
		" <G>Space <L>- Move Marker To Center\n"
		" <G>H <L>- Help\n",
		vec2( 256.0f + 16.0f, 16.0f ), vec2( 256.0f - 32.0f, 256.0f - 32.0f ), CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, inGameFont, 0, NULL, 1, 0 );

	txt_DisplayString( "Combat Grid Symbols", vec2( 16.0f, 256.0f - 32.0f ), CLR_YELLOW, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, inGameFont, 1, 0 );

	Vector2 pos = { 16.0f, 256.0f - 16.0f };
	img_Draw( sbGridIconImgs[CGA_STRONG_ATT], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Strong Attack", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_STRONG_DEF], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Strong Defense", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_WEAK_ATT], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Weak Attack", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_WEAK_DEF], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Weak Defense", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_MULTI], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Extra Attack", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x = 16.0f;
	pos.y += 16.0f;
	img_Draw( sbGridIconImgs[CGA_STUN], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Stun", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_COUNTER], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Counter Attack", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_DO_NOTHING], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Skip Turn", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	pos.x += 95.0f;
	img_Draw( sbGridIconImgs[CGA_RESET], 1, pos, pos, 0 );
	pos.x += 8.0f;
	txt_DisplayString( "Marker To Starting Position", pos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );

	txt_DisplayString( "<L>Press <G>'X'<L> to exit help.",
		vec2( 256.0f, 270.0f ), CLR_LIGHT_GREY, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, inGameFont, 1, 0 );
}

static void gameScreen_Help_PhysicsTick( float dt )
{
}

struct GameState gameScreenHelpState = { gameScreen_Help_Enter, gameScreen_Help_Exit, gameScreen_Help_ProcessEvents,
	gameScreen_Help_Process, gameScreen_Help_Draw, gameScreen_Help_PhysicsTick };

// ****** Explore state
static int exploreX;
static int exploreY;

static void switchLevel( Level* newLevel )
{
	currentLevel = newLevel;

	// find the appropriate entrance or exit
	char find;
	if( killedBoss ) {
		find = 'X';
	} else {
		find = 'E';
	}

	exploreX = 0;
	exploreY = 0;
	for( int y = 0; y < LEVEL_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_WIDTH; ++x ) {
			if( levels_GetData( currentLevel, x, y ) == find ) {
				exploreX = x;
				exploreY = y;
			}
		}
	}
}

static void hitSwitchLevel( void )
{
	if( currentLevel == &forest ) {
		if( killedBoss ) {
			// won!
			ending = 2;
			gsmEnterState( &globalFSM, &gameOverScreenState );
		} else {
			switchLevel( &cave );
		}
	} else if( currentLevel == &cave ) {
		switchLevel( &forest );
	}
}

static void hitEnemy( void )
{
	// delete the enemy since either the player or it will die
	char data = levels_GetData( currentLevel, exploreX, exploreY );
	levels_ClearData( currentLevel, exploreX, exploreY );

	// set up the enemy we're going to be fighting
	//  find them
	data = (char)toupper( data );
	int idx = 0;
	while( ( idx < ARRAY_SIZE( enemies ) ) && ( enemies[idx].data != data ) ) {
		++idx;
	}

	if( idx >= ARRAY_SIZE( enemies ) ) {
		llog( LOG_WARN, "Unable to find valid enemy template, skipping combat" );
		return;
	}

	//  put data in there
	opponent.attack = enemies[idx].attack;
	opponent.defense = enemies[idx].defense;
	opponent.currHealth = enemies[idx].health;
	opponent.maxHealth = enemies[idx].health;
	opponent.currStamina = 0;
	opponent.maxStamina = 0;
	opponent.staminaRegen = 0;
	opponent.grid.template = &( combatGridTemplates[ enemies[idx].combatGridTemplate ] );
	opponent.grid.pos = opponent.grid.template->start;
	opponent.grid.posMoveAmt = 0.0f;
	opponent.grid.prevPos = opponent.grid.pos;
	opponent.grid.markerClr = CLR_RED;
	opponent.images = enemies[idx].imgs;
	opponent.imgPos = OPPONENT_BASE;
	opponent.currImgIdx = IDLE_SPR;
	opponent.imgClr = CLR_WHITE;
	opponent.combatTextPos = OPPONENT_DAMAGE_TXT_POS;
	opponent.isStunned = false;
	opponent.isBoss = ( (char)toupper( data ) == 'B' );

	// set us up
	player.grid.pos = player.grid.template->start;
	player.grid.prevPos = player.grid.pos;
	player.grid.posMoveAmt = 0.0f;
	player.imgPos = PLAYER_BASE;
	player.currImgIdx = IDLE_SPR;
	player.imgClr = CLR_WHITE;
	player.combatTextPos = PLAYER_DAMAGE_TXT_POS;


	gsmEnterState( &gameScreenStateMachine, &gameScreenFightState );
}

static void checkForTileEvents( void )
{
	char data = levels_GetData( currentLevel, exploreX, exploreY );

	data = (char)toupper( data );

	switch( data ) {
	case 'E':
		if( killedBoss ) hitSwitchLevel( );
		break;
	case 'X':
		if( !killedBoss) hitSwitchLevel( );
		break;
	case 'B':
	case 'W':
	case 'S':
	case 'G':
	case 'A':
	case 'R':
		hitEnemy( );
		break;
	}
}

static int manhattanDistance( int startX, int startY, int targetX, int targetY )
{
	int d = abs( startX - targetX ) + abs( startY - targetY );
	return d;
}

static bool isVisibleFrom( int lookX, int lookY, int targetX, int targetY )
{
	bool steep = false;

	if( abs( lookX - targetX ) < abs( lookY - targetY ) ) {
		SWAP( lookX, lookY, int );
		SWAP( targetX, targetY, int );
		steep = true;
	}

	if( lookX > targetX ) {
		SWAP( lookX, targetX, int );
		SWAP( lookY, targetY, int );
	}

	// we'll go a maximum of 4 steps away from look, as long as we don't hit
	//  an obstacle we'll have seen the look at target
	float deltaX = (float)( targetX - lookX );
	float deltaY = (float)( targetY - lookY );

	float deltaErr = fabsf( deltaY ) * 2.0f;
	float error = 0.0f;

	int y = lookY;
	for( int x = lookX; x <= targetX; ++x ) {
		char data;
		if( steep ) {
			data = levels_GetData( currentLevel, y, x );
		} else {
			data = levels_GetData( currentLevel, x, y );
		}
		
		if( !( ( ( x == targetX ) && ( y == targetY ) ) || ( ( x == lookX ) && ( y == lookY ) ) ) ) {
			if( ( data == '#' ) || ( data == '*' ) ) {
				return false;
			}
		}

		error += deltaErr;
		if( error >= deltaX ) {
			y += ( targetY > lookY ) ? 1 : -1;
			error -= deltaX * 2.0f;
		}
	}

	return true;
}

static void processEnemies( void )
{
	// find all the enemies and process them
	for( int y = 0; y < LEVEL_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_WIDTH; ++x ) {
			char data = levels_GetData( currentLevel, x, y );
			if( ( data == ' ' ) || ( data == '*' ) || ( data == '#' ) || ( data == 'E' ) || ( data == 'X' ) ) continue;
			levels_MarkData( currentLevel, x, y ); // so we don't process them a second time

			switch( data ) {
				case 'B':
				case 'W':
				case 'S':
				case 'A':
				case 'R': {
					// wander
					int cx, cy;
					int nx = x + rand_GetToleranceS32( NULL, 0, 1 );
					int ny = y + rand_GetToleranceS32( NULL, 0, 1 );
					nx = CLAMP( nx, 0, ( LEVEL_WIDTH - 1 ) );
					ny = CLAMP( ny, 0, ( LEVEL_HEIGHT - 1 ) );

					levels_MoveData( currentLevel, x, y, nx, ny, &cx, &cy );

					// see if the player is visible
					if( ( manhattanDistance( cx, cy, exploreX, exploreY ) <= 4 ) && isVisibleFrom( cx, cy, exploreX, exploreY ) ) {
						levels_ReplaceData( currentLevel, cx, cy, (char)tolower( data ) );
					}
				} break;

				case 'b':
				case 'w':
				case 's':
				case 'a':
				case 'r': {
					// move towards the player, if they're no longer visible then stop
					//  this will break everything if they're moving downwards
					int xd, yd;
					int cx, cy;
					cx = x;
					cy = y;
					levels_MoveToward( currentLevel, cx, cy, exploreX, exploreY, &xd, &yd );
					levels_MoveData( currentLevel, cx, cy, cx+xd, cy+yd, &cx, &cy );

					levels_MoveToward( currentLevel, cx, cy, exploreX, exploreY, &xd, &yd );
					levels_MoveData( currentLevel, cx, cy, cx+xd, cy+yd, &cx, &cy );

					if( !isVisibleFrom( cx, cy, exploreX, exploreY ) ) {
						levels_ReplaceData( currentLevel, cx, cy, (char)toupper( data ) );
					}
				} break;
			}
		}
	}

	levels_UnMarkAllData( currentLevel );
}

static void exploreMove( int xd, int yd )
{
	int newX = exploreX + xd;
	int newY = exploreY + yd;

	if( newX < 0 ) return;
	if( newY < 0 ) return;
	if( newX >= LEVEL_WIDTH ) return;
	if( newY >= LEVEL_HEIGHT ) return;
	
	char data = levels_GetData( currentLevel, newX, newY );

	// obstacle, can't move
	if( ( data == '*' ) || ( data == '#' ) ) {
		return;
	}

	// regen stamina
	player.currStamina = MIN( ( player.currStamina + player.staminaRegen ), player.maxStamina );

	exploreX = newX;
	exploreY = newY;

	snd_Play( stepSFX, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );

	processEnemies( );
	checkForTileEvents( );
}

static void exploreUp( void )
{
	exploreMove( 0, -1 );
}

static void exploreDown( void )
{
	exploreMove( 0, 1 );
}

static void exploreLeft( void )
{
	exploreMove( -1, 0 );
}

static void exploreRight( void )
{
	exploreMove( 1, 0 );
}

static void exploreWait( void )
{
	exploreMove( 0, 0 );
}

static void exploreHelp( void )
{
	postHelpState = &gameScreenExploreState;
	gsmEnterState( &gameScreenStateMachine, &gameScreenHelpState );
}

static int gameScreen_Explore_Enter( void )
{
	input_BindOnKeyPress( SDLK_w, exploreUp );
	input_BindOnKeyPress( SDLK_s, exploreDown );
	input_BindOnKeyPress( SDLK_a, exploreLeft );
	input_BindOnKeyPress( SDLK_d, exploreRight );
	input_BindOnKeyPress( SDLK_SPACE, exploreWait );
	input_BindOnKeyPress( SDLK_h, exploreHelp );

	snd_PlayStreaming( exploreMusic, 1.0f, 0.0f );

	return 1;
}

static int gameScreen_Explore_Exit( void )
{
	input_ClearAllKeyBinds( );

	snd_StopStreaming( exploreMusic );

	return 1;
}

static void gameScreen_Explore_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Explore_Process( void )
{
}

static void gameScreen_Explore_Draw( void )
{
	// draw the current level
	for( int y = 0; y < LEVEL_HEIGHT; ++y ) {
		for( int x = 0; x < LEVEL_WIDTH; ++x ) {
			if( !isVisibleFrom( x, y, exploreX, exploreY ) ) continue;
			char data = levels_GetData( currentLevel, x, y );
			int image = -1;

			data = (char)toupper( data );

			switch( data ) {
			case ' ':
				image = currentLevel->floorImg;
				break;
			case '*': // forest obstruction
				image = forestObstacleImg;
				break;
			case '#': // cave obstruction
				image = caveObstacleImg;
				break;
			case 'E': // entrance, only while the boss is alive
				if( killedBoss ) image = entranceImg;
				else image = currentLevel->floorImg;
				break;
			case 'X': // exit, only once the boss has been killed
				if( !killedBoss ) image = exitImg;
				else image = currentLevel->floorImg;
				break;
			case 'W':
				image = sbWolfImgs[IDLE_SPR];
				break;
			case 'S':
				image = sbScoutImgs[IDLE_SPR];
				break;
			case 'G':
				image = sbGuardImgs[IDLE_SPR];
				break;
			case 'A':
				image = sbWarriorImgs[IDLE_SPR];
				break;
			case 'R':
				image = sbRaiderImgs[IDLE_SPR];
				break;
			case 'B':
				image = sbBossImgs[IDLE_SPR];
				break;
			}

			if( image < 0 ) continue;

			Vector2 pos = wldToScr( x, y );

			img_Draw( image, 1, pos, pos, 0 );
		}
	}

	// draw the player
	Vector2 pos = wldToScr( exploreX, exploreY );
	img_Draw( sbPlayerImgs[IDLE_SPR], 1, pos, pos, 1 );

	drawCreatureStats( vec2( 100.0f, 272.0f ), &player );

	if( killedBoss ) {
		txt_DisplayString( "Escape<W>!", vec2( 400.0f, 272.0f ), CLR_GREEN, HORIZ_ALIGN_RIGHT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );
	} else {
		txt_DisplayString( "Kill the <R>orc leader<W>!", vec2( 400.0f, 272.0f ), CLR_WHITE, HORIZ_ALIGN_RIGHT, VERT_ALIGN_CENTER, inGameFont, 1, 0 );
	}
}

static void gameScreen_Explore_PhysicsTick( float dt )
{
}

struct GameState gameScreenExploreState = { gameScreen_Explore_Enter, gameScreen_Explore_Exit,
	gameScreen_Explore_ProcessEvents, gameScreen_Explore_Process, gameScreen_Explore_Draw, gameScreen_Explore_PhysicsTick };

// Primary state
static int gameScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	createCombatGridTemplates( );

	// setup player
	player.attack = 3;
	player.defense = 3;
	player.maxHealth = 10;
	player.maxStamina = 5;
	player.currHealth = player.maxHealth;
	player.currStamina = player.maxStamina;
	player.staminaRegen = 1;
	player.grid.template = &combatGridTemplates[GTT_PLAYER];
	player.grid.markerClr = CLR_GREEN;
	player.images = sbPlayerImgs;

	// enemy setup
	for( int i = 0; i < ARRAY_SIZE( enemies ); ++i ) {
		switch( enemies[i].data ) {
		case 'W':
			enemies[i].imgs = sbWolfImgs;
			break;
		case 'S':
			enemies[i].imgs = sbScoutImgs;
			break;
		case 'G':
			enemies[i].imgs = sbGuardImgs;
			break;
		case 'A':
			enemies[i].imgs = sbWarriorImgs;
			break;
		case 'R':
			enemies[i].imgs = sbRaiderImgs;
			break;
		case 'B':
			enemies[i].imgs = sbBossImgs;
			break;
		}
	}

	levels_Setup( );

	killedBoss = false;

	void* cfgFile = cfg_OpenFile( "game.cfg" );
	int firstPlay;
	cfg_GetInt( cfgFile, "firstPlay", 1, &firstPlay );
	
	snd_PlayStreaming( exploreMusic, 1.0f, 0.0f );

	switchLevel( &forest );
	if( firstPlay ) {
		cfg_SetInt( cfgFile, "firstPlay", 0 );
		cfg_SaveFile( cfgFile );
		postHelpState = &gameScreenExploreState;
		gsmEnterState( &gameScreenStateMachine, &gameScreenHelpState );
	} else {
		cfg_CloseFile( cfgFile );
		gsmEnterState( &gameScreenStateMachine, &gameScreenExploreState );
	}
	

	return 1;
}

static int gameScreen_Exit( void )
{
	gsmEnterState( &gameScreenStateMachine, NULL );
	destroyCombatGridTemplates( );

	snd_StopStreaming( exploreMusic );
	snd_StopStreaming( combatMusic );

	return 1;
}

static void gameScreen_ProcessEvents( SDL_Event* e )
{
	gsmProcessEvents( &gameScreenStateMachine, e );
}

static void gameScreen_Process( void )
{
	gsmProcess( &gameScreenStateMachine );
}

static void gameScreen_Draw( void )
{
	gsmDraw( &gameScreenStateMachine );
}

static void gameScreen_PhysicsTick( float dt )
{
	gsmPhysicsTick( &gameScreenStateMachine, dt );
}

struct GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };