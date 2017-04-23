#include "resources.h"

#include "../Graphics/Images.h"
#include "../Graphics/ImageSheets.h"
#include "../UI/text.h"
#include "../sound.h"

int forestObstacleImg;
int caveObstacleImg;
int exitImg;
int entranceImg;
int forestFloorImg;
int caveFloorImg;

int gridBGImg;
int gridMarkerImg;
int* sbGridIconImgs;

int* sbPlayerImgs;
int* sbWolfImgs;
int* sbScoutImgs;
int* sbGuardImgs;
int* sbWarriorImgs;
int* sbRaiderImgs;
int* sbBossImgs;

int inGameFont;
int titleFont;

int combatMusic;
int deathSuccessMusic;
int exploreMusic;
int failureMusic;
int successMusic;
int titleMusic;

int clashSFX;
int defendSFX;
int monsterDeathSFX;
int playerDeathSFX;
int stepSFX;

int ending;

#define LOAD_AND_TEST( file, func, id ) \
	id = func( file ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_IMG( file, shaderType, id ) \
	id = img_Load( file, shaderType ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_FNT( file, size, id ) \
	id = txt_LoadFont( file, size ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_SS( file, ids, cnt ) \
	{ ids = NULL; cnt = img_LoadSpriteSheet( file, ST_DEFAULT, &ids ); if( cnt < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); } }

#define LOAD_AND_TEST_MUSIC( file, id ) \
	id = snd_LoadStreaming( file, true, 0 ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

#define LOAD_AND_TEST_SOUND( file, id ) \
	id = snd_LoadSample( file, 1, false ); if( id < 0 ) { \
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error loading resource file %s.", file ); }

void loadResources( void )
{
	int cnt;

	LOAD_AND_TEST_IMG( "Images/forest_obstacle.png", ST_DEFAULT, forestObstacleImg );
	LOAD_AND_TEST_IMG( "Images/cave_obstacle.png", ST_DEFAULT, caveObstacleImg );
	LOAD_AND_TEST_IMG( "Images/exit.png", ST_DEFAULT, exitImg );
	LOAD_AND_TEST_IMG( "Images/entrance.png", ST_DEFAULT, entranceImg );
	LOAD_AND_TEST_IMG( "Images/forest_floor.png", ST_DEFAULT, forestFloorImg );
	LOAD_AND_TEST_IMG( "Images/cave_floor.png", ST_DEFAULT, caveFloorImg );

	LOAD_AND_TEST_IMG( "Images/tile_bg.png", ST_DEFAULT, gridBGImg );
	LOAD_AND_TEST_IMG( "Images/grid_marker.png", ST_DEFAULT, gridMarkerImg );
	LOAD_AND_TEST_SS( "Images/icons.ss", sbGridIconImgs, cnt );

	LOAD_AND_TEST_SS( "Images/player.ss", sbPlayerImgs, cnt );

	LOAD_AND_TEST_SS( "Images/wolf.ss", sbWolfImgs, cnt );
	LOAD_AND_TEST_SS( "Images/scout.ss", sbScoutImgs, cnt );
	LOAD_AND_TEST_SS( "Images/guard.ss", sbGuardImgs, cnt );

	LOAD_AND_TEST_SS( "Images/warrior.ss", sbWarriorImgs, cnt );
	LOAD_AND_TEST_SS( "Images/raider.ss", sbRaiderImgs, cnt );
	LOAD_AND_TEST_SS( "Images/boss.ss", sbBossImgs, cnt );

	LOAD_AND_TEST_FNT( "Fonts/kenpixel.ttf", 12.0f, inGameFont );
	LOAD_AND_TEST_FNT( "Fonts/kenpixel.ttf", 36.0f, titleFont );

	LOAD_AND_TEST_MUSIC( "Music/combat.ogg", combatMusic );
	LOAD_AND_TEST_MUSIC( "Music/deathSuccess.ogg", deathSuccessMusic );
	LOAD_AND_TEST_MUSIC( "Music/explore.ogg", exploreMusic );
	LOAD_AND_TEST_MUSIC( "Music/failure.ogg", failureMusic );
	LOAD_AND_TEST_MUSIC( "Music/success.ogg", successMusic );
	LOAD_AND_TEST_MUSIC( "Music/title.ogg", titleMusic );

	LOAD_AND_TEST_SOUND( "Sounds/clash.ogg", clashSFX );
	LOAD_AND_TEST_SOUND( "Sounds/defend.ogg", defendSFX );
	LOAD_AND_TEST_SOUND( "Sounds/monsterDeath.ogg", monsterDeathSFX );
	LOAD_AND_TEST_SOUND( "Sounds/playerDeath.ogg", playerDeathSFX );
	LOAD_AND_TEST_SOUND( "Sounds/step.ogg", stepSFX );
}