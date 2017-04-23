#ifndef RESOURCES_H
#define RESOURCES_H

extern int forestObstacleImg;
extern int caveObstacleImg;
extern int exitImg;
extern int entranceImg;
extern int forestFloorImg;
extern int caveFloorImg;

extern int gridBGImg;
extern int gridMarkerImg;
extern int* sbGridIconImgs;

extern int* sbPlayerImgs;
extern int* sbWolfImgs;
extern int* sbScoutImgs;
extern int* sbGuardImgs;
extern int* sbWarriorImgs;
extern int* sbRaiderImgs;
extern int* sbBossImgs;

extern int inGameFont;
extern int titleFont;

extern int combatMusic;
extern int deathSuccessMusic;
extern int exploreMusic;
extern int failureMusic;
extern int successMusic;
extern int titleMusic;

extern int clashSFX;
extern int defendSFX;
extern int monsterDeathSFX;
extern int playerDeathSFX;
extern int stepSFX;

extern int ending;

#define IDLE_SPR 0
#define ATTACK_SPR 1
#define DEFEND_SPR 2
#define DEAD_SPR 3

void loadResources( void );

#endif /* inclusion guard */