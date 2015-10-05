#pragma once
#include <vector>
#include <tchar.h>

#define PATHING_MAP_MAGIC 'PMAP'

#pragma region GWStuff
#pragma pack(1)
struct GWPathingTrapezoid//Coords are floats!
{
	//index to Adjacent Trapezoid or -1
	int Adjacent1; /*Y1 X1*/
	int Adjacent2; /*Y1 X2*/
	int Adjacent3; /*Y2 X3*/
	int Adjacent4; /*Y2 X4*/

	short Transition1;//index of transitions
	short Transition2;

	float YT;
	float YB;

	float XTL;
	float XTR;
	float XBL;
	float XBR;

};

#pragma pack(1)
struct Transition
{
	float X1;
	float Y1;
	float X2;
	float Y2;
};
#pragma endregion

#pragma pack(1)
struct PathingMapHeader
{
	int Magic; // MAGIC Bytes (PMAP)
	unsigned long MapHash;  // hash of mapfile
	int TrapezoidCount;
	int FirstTrapezoid;//Offset to Trapezoid Chunk
	int AdjacentCount;
	int FirstAdjacent;//Offset to Adjacent Chunk
	float startX;
	float startY;
	float Width;
	float Heigth;
};

#pragma pack(1)
struct PathingMapTrapezoid
{
	int AdjacentsCount;
	int Adjacents;		//start index in adjacent array

	short Plane;		//plane the trapezoid is in

	float YT;
	float YB;

	float XTL;
	float XTR;
	float XBL;
	float XBR;
};

#pragma pack(1)
struct Adjacent
{
	unsigned int Trapezoid; //index of trapezoid
	float XL;		//left/right relative to walking direction
	float YL;		//			L /\ R
	float XR;		//			  || <-- walking direction
	float YR;		//			R \/ L
};

#pragma pack()
struct Bounds
{
	float startX;
	float startY;
	float Width;
	float Heigth;
};
/**
* Header
* TrapezoidCunk
* ...
* AdjacentChunk
**/

class PathingMap
{
public:
	PathingMap(unsigned long maphash);
	~PathingMap();

	bool Open(TCHAR* filename);

	__forceinline std::vector<PathingMapTrapezoid> GetPathingData(){return PathingData;}
	__forceinline Bounds GetMapBoundaries(){return Boundaries;}
	__forceinline unsigned long GetMapHash(){return Hash;}


	//for map generation only
	__forceinline void AddPathingTrapazoid(unsigned int Plane,GWPathingTrapezoid trapez)
	{
		PathingMapTrapezoid pmt;
		pmt.Plane = (short)Plane;
		pmt.XBL = trapez.XBL;
		pmt.XBR = trapez.XBR;
		pmt.XTL = trapez.XTL;
		pmt.XTR = trapez.XTR;
		pmt.YB = trapez.YB;
		pmt.YT = trapez.YT;
		pmt.Adjacents = 0;
		pmt.AdjacentsCount = 0;
		PathingData.push_back(pmt);
		GWTrapezoids.push_back(trapez);
	}

	__forceinline void AddTransition(unsigned int index,Transition t)
	{
		if(index >= GWTransitions.size())
			GWTransitions.resize(index + 1);
		GWTransitions[index] = t;
	}
	bool Save(TCHAR* filename);
private:
	Bounds GetBoundaries();
	void CompileAdjacentList();
private:
	Bounds Boundaries;
	unsigned long Hash;
	std::vector<PathingMapTrapezoid> PathingData;
	std::vector<Adjacent> Adjacents;
	
	//Needed for map generation
	std::vector<GWPathingTrapezoid> GWTrapezoids;
	std::vector<Transition> GWTransitions;
};
