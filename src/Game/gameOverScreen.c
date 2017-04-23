#include "gameOverScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../sound.h"

#include "titleScreen.h"

#include "../UI/text.h"

#include "resources.h"

static float timeAlive;

static int gameOverScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	timeAlive = 0.0f;

	switch( ending ) {
	case 0: // failed
		snd_PlayStreaming( failureMusic, 1.0f, 0.0f );
		break;
	case 1: // killed boss but died;
		snd_PlayStreaming( deathSuccessMusic, 1.0f, 0.0f );
		break;
	case 2: // won
		snd_PlayStreaming( successMusic, 1.0f, 0.0f );
		break;
	}

	return 1;
}

static int gameOverScreen_Exit( void )
{
	snd_StopStreaming( failureMusic );
	snd_StopStreaming( deathSuccessMusic );
	snd_StopStreaming( successMusic );
	return 1;
}

static void gameOverScreen_ProcessEvents( SDL_Event* e )
{
	if( timeAlive < 1.0f ) return;

	if( e->type == SDL_KEYUP ) {
		gsmEnterState( &globalFSM, &titleScreenState );
	}
}

static void gameOverScreen_Process( void )
{
}

#include "../Graphics/debugRendering.h"
static void gameOverScreen_Draw( void )
{
	//debugRenderer_AABB( 1, vec2( 50.0f, 50.0f ), vec2( 512.0f - 100.0f, 256.0f - 100.0f ), CLR_GREEN );
	switch( ending ) {
	case 0: // failed
		txt_DisplayTextArea(
			(const uint8_t*)"You have <R>failed<L> your mission.\nThe orc clans join together and destroy your home while your corpse is left to rot in the wilderness.",
			vec2( 50.0f, 50.0f ), vec2( 512.0f - 100.0f, 256.0f - 100.0f ), CLR_LIGHT_GREY,
			HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 0, NULL, 1, 0 );
		break;
	case 1: // killed boss but died
		txt_DisplayTextArea(
			(const uint8_t*)"You have <G>succeeded<L>, but <R>died<L> while trying to escape.\nWhen the orc clans disperse many pints are raised in the pub, and your sacrifice is remembered.",
			vec2( 50.0f, 50.0f ), vec2( 512.0f - 100.0f, 256.0f - 100.0f ), CLR_LIGHT_GREY,
			HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 0, NULL, 1, 0 );
		break;
	case 2: // killed boss and escaped
		txt_DisplayTextArea(
			(const uint8_t*)"You have <G>succeeded<L> and returned alive!\nThere is a celebration and you get very drunk. You don't remember much the next morning, but apparently you're now married.",
			vec2( 50.0f, 50.0f ), vec2( 512.0f - 100.0f, 256.0f - 100.0f ), CLR_LIGHT_GREY,
			HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 0, NULL, 1, 0 );
		break;
	}

	if( timeAlive >= 1.0f ) {
		txt_DisplayString( "Press any key to return to the title screen",
			vec2( 256.0f, 256.0f ), CLR_YELLOW, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 1, 0 );
	}
}

static void gameOverScreen_PhysicsTick( float dt )
{
	timeAlive += dt;
}

struct GameState gameOverScreenState = { gameOverScreen_Enter, gameOverScreen_Exit, gameOverScreen_ProcessEvents,
	gameOverScreen_Process, gameOverScreen_Draw, gameOverScreen_PhysicsTick };