#pragma once
#include "build.h"
#include "common_game.h"

struct HITINFO {
    short hitsect;
    short hitwall;
    short hitsprite;
    int hitx;
    int hity;
    int hitz;
};

extern POINT2D baseWall[kMaxWalls];
extern POINT3D baseSprite[kMaxSprites];
extern long baseFloor[kMaxSectors];
extern long baseCeil[kMaxSectors];
extern long velFloor[kMaxSectors];
extern long velCeil[kMaxSectors];
extern short gUpperLink[kMaxSectors];
extern short gLowerLink[kMaxSectors];
extern HITINFO gHitInfo;

bool AreSectorsNeighbors(int sect1, int sect2);
bool FindSector(int nX, int nY, int nZ, int *nSector);
bool FindSector(int nX, int nY, int *nSector);
void CalcFrameRate(void);
bool CheckProximity(SPRITE *pSprite, int nX, int nY, int nZ, int nSector, int nDist);
bool CheckProximityPoint(int nX1, int nY1, int nZ1, int nX2, int nY2, int nZ2, int nDist);
bool CheckProximityWall(int nWall, int x, int y, int nDist);
int GetWallAngle(int nWall);
void GetWallNormal(int nWall, int *pX, int *pY);
bool IntersectRay(long wx, long wy, long wdx, long wdy, long x1, long y1, long z1, long x2, long y2, long z2, long *ix, long *iy, long *iz);
int HitScan(SPRITE *pSprite, int z, int dx, int dy, int dz, unsigned long nMask, int a8);
int VectorScan(SPRITE *pSprite, int nOffset, int nZOffset, int dx, int dy, int dz, int nRange, int ac);
void GetZRange(SPRITE *pSprite, long *ceilZ, long *ceilHit, long *floorZ, long *floorHit, int nDist, unsigned long nMask);
void GetZRangeAtXYZ(long x, long y, long z, int nSector, long *ceilZ, long *ceilHit, long *floorZ, long *floorHit, int nDist, unsigned long nMask);
int GetDistToLine(int x1, int y1, int x2, int y2, int x3, int y3);
unsigned int ClipMove(long *x, long *y, long *z, int *nSector, long xv, long yv, int wd, int cd, int fd, unsigned long nMask);
int GetClosestSectors(int nSector, int x, int y, int nDist, short *pSectors, char *pSectBit);
int GetClosestSpriteSectors(int nSector, int x, int y, int nDist, short *pSectors, char *pSectBit, short *a8);
