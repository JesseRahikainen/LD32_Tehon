#include "titleScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../sound.h"

#include "../UI/text.h"
#include "resources.h"

#include "gameScreen.h"

static float timeAlive;

static int titleScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	timeAlive = 0.0f;

	snd_PlayStreaming( titleMusic, 1.0f, 0.0f );

	return 1;
}

static int titleScreen_Exit( void )
{
	snd_StopStreaming( titleMusic );

	return 1;
}

static void titleScreen_ProcessEvents( SDL_Event* e )
{
	if( timeAlive < 1.0f ) return;

	if( e->type == SDL_KEYUP ) {
		gsmEnterState( &globalFSM, &gameScreenState );
	}
}

static void titleScreen_Process( void )
{
}

static void titleScreen_Draw( void )
{
	txt_DisplayTextArea( (const uint8_t*)"The Mines of\nTehon", vec2( 128.0f, 32.0f ), vec2( 256.0f, 256.0f ), CLR_CYAN,
		HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, titleFont, 0, NULL, 1, 0 );

	txt_DisplayTextArea( (const uint8_t*)"There has been a increase in <R>Orc raids <L>recently. Your hamlet's letters to the <W>capital <L>have been ignored. If something isn't done soon the <R>orcs<L> will swarm the countryside.\n"
		"As a retired adventurer you have some knowledge of orcs. They're too organized, and must have a new <Y>leader<L> keeping them from fighting each other. "
		"You volunteer to go <R>kill<L> this ord leader before your <G>home<L> is destroyed.",
		vec2( 64.0f, 128.0f ), vec2( 256.0f + 128.0f, 256.0f ), CLR_LIGHT_GREY,
		HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, inGameFont, 0, NULL, 1, 0 );

	if( timeAlive >= 1.0f ) {
		txt_DisplayString( "Press any key to begin",
			vec2( 256.0f, 256.0f ), CLR_YELLOW, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, inGameFont, 1, 0 );
	}
}

static void titleScreen_PhysicsTick( float dt )
{
	timeAlive += dt;
}

struct GameState titleScreenState = { titleScreen_Enter, titleScreen_Exit, titleScreen_ProcessEvents,
	titleScreen_Process, titleScreen_Draw, titleScreen_PhysicsTick };