#include "PathingMap.h"
#include <iostream>
#include <fstream>
#include <math.h>
#include <algorithm>

using namespace std;

#pragma region HelperFuncs
void PrintProgressbar( int G,int W,int parts )
{
	int p;
	float percent = ((float)W/(float)G) * 100;
	float progress = ((float)parts / 100) * percent;
	printf("[");
	for(p = 0;p<(int)progress;p++)
		printf("#");
	for(;p<parts;p++)
		printf(" ");
	printf("] %.01f%%",percent);
}

struct Point2D
{
	float x;
	float y;
};

__forceinline Point2D GetCenterOfTrapezoid(GWPathingTrapezoid* t)
{
	Point2D c;

	c.y = (t->YB + t->YT) / 2;
	c.x = (t->XBL + t->XBR + t->XTL + t->XTR) / 4;

	return c;
}
#pragma endregion

PathingMap::PathingMap(unsigned long maphash )
:Hash(maphash)
{
}


bool PathingMap::Save( TCHAR* filename )
{
	CompileAdjacentList();

	ofstream pmap;
	pmap.open(filename,ios::binary);
	if(!pmap)
		return false;

	PathingMapHeader head;
	head.Magic = PATHING_MAP_MAGIC;
	head.MapHash = Hash;
	head.TrapezoidCount = PathingData.size();
	head.FirstTrapezoid = sizeof(PathingMapHeader);
	head.AdjacentCount = Adjacents.size();
	head.FirstAdjacent = (PathingData.size() * sizeof(PathingMapTrapezoid)) + sizeof(PathingMapHeader);
	Boundaries = GetBoundaries();
	head.startX = Boundaries.startX;
	head.startY = Boundaries.startY;
	head.Width = Boundaries.Width;
	head.Heigth = Boundaries.Heigth;
	pmap.write((char*)&head,sizeof(PathingMapHeader));

	for(unsigned int i=0;i<PathingData.size();i++)
		pmap.write((char*)&PathingData[i],sizeof(PathingMapTrapezoid));

	for(unsigned int i=0;i<Adjacents.size();i++)
		pmap.write((char*)&Adjacents[i],sizeof(Adjacent));

	pmap.close();

	printf("\nSaved map %s\n\n\n",filename);

	return true;
}

Bounds PathingMap::GetBoundaries()
{
	float minX = 2500000,minY = 2500000,maxX = 0,maxY = 0;
	for(unsigned int i=0;i<PathingData.size();i++)
	{
		if (PathingData[i].YT < minY) minY = PathingData[i].YT;
		if (PathingData[i].YB < minY) minY = PathingData[i].YB;

		if (PathingData[i].YT > maxY) maxY = PathingData[i].YT;
		if (PathingData[i].YB > maxY) maxY = PathingData[i].YB;

		if (PathingData[i].XTL < minX) minX = PathingData[i].XTL;
		if (PathingData[i].XTR < minX) minX = PathingData[i].XTR;
		if (PathingData[i].XBL < minX) minX = PathingData[i].XBL;
		if (PathingData[i].XBR < minX) minX = PathingData[i].XBR;

		if (PathingData[i].XTL > maxX) maxX = PathingData[i].XTL;
		if (PathingData[i].XTR > maxX) maxX = PathingData[i].XTR;
		if (PathingData[i].XBL > maxX) maxX = PathingData[i].XBL;
		if (PathingData[i].XBR > maxX) maxX = PathingData[i].XBR;
	}
	Bounds b = {minX,minY,maxX-minX,maxY-minY};
	return b;
}


bool PathingMap::Open( TCHAR* filename )
{
	ifstream pmap;
	pmap.open(filename,ios::binary);
	if(!pmap)
		return false;

	int length;
	pmap.seekg (0, ios::end);
	length = pmap.tellg();
	pmap.seekg (0, ios::beg);

	char* buffer = new char[length];
	pmap.read(buffer,length);
	pmap.close();

	PathingMapHeader* pHead = reinterpret_cast<PathingMapHeader*>(buffer);

	if(!(pHead->Magic == PATHING_MAP_MAGIC))
		throw "UNKNOWN_FORMAT";

	Hash = pHead->MapHash;
	Boundaries.startX = pHead->startX;
	Boundaries.startY = pHead->startY;
	Boundaries.Heigth = pHead->Heigth;
	Boundaries.Width = pHead->Width;
	
	PathingMapTrapezoid* pTrapez = reinterpret_cast<PathingMapTrapezoid*>(buffer + pHead->FirstTrapezoid);
	for(int i=0;i<pHead->TrapezoidCount;i++)
		PathingData.push_back(pTrapez[i]);

	Adjacent* pAdjacent = reinterpret_cast<Adjacent*>(buffer + pHead->FirstAdjacent);
	for (int i=0;i<pHead->AdjacentCount;i++)
		Adjacents.push_back(pAdjacent[i]);

	delete[] buffer;
	return true;
}

PathingMap::~PathingMap()
{
	PathingData.clear();	
	GWTransitions.clear();
	Adjacents.clear();
	GWTrapezoids.clear();
}



void PathingMap::CompileAdjacentList()
{
	printf("\n Compiling adjacency list:\n");
	PrintProgressbar(PathingData.size(),0,50);
	for(unsigned int i=0;i<PathingData.size();i++)
	{
		PathingData[i].Adjacents = (int)Adjacents.size();
//seems to work without -.-
// 		if(GWTrapezoids[i].Transition1 != -1)
// 		{
// 			for(unsigned int j=0;j<GWTrapezoids.size();j++)//find "adjacent" trapezoid
// 			{
// 				if(i == j)
// 					continue;
// 				if(PathingData[i].Plane == PathingData[j].Plane)
// 					continue;
// 				if (GWTrapezoids[i].Transition1 == GWTrapezoids[j].Transition1 || GWTrapezoids[i].Transition1 == GWTrapezoids[j].Transition2)
// 				{
// 					Adjacent a;
// 					a.Trapezoid = j;
// 					//Get direction
// 					if (GWTrapezoids[i].YB >= GWTrapezoids[j].YT)// down
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition1].X1 > GWTransitions[GWTrapezoids[i].Transition1].X2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 						}
// 					}
// 					else if (GWTrapezoids[i].YT <= GWTrapezoids[j].YB)// up
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition1].X1 > GWTransitions[GWTrapezoids[i].Transition1].X2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 						}
// 					}
// 					else if(GetCenterOfTrapezoid(&GWTrapezoids[i]).x > GetCenterOfTrapezoid(&GWTrapezoids[j]).x)// left
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition1].Y1 > GWTransitions[GWTrapezoids[i].Transition1].Y2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 						}
// 					}
// 					else if(GetCenterOfTrapezoid(&GWTrapezoids[i]).x < GetCenterOfTrapezoid(&GWTrapezoids[j]).x)// right
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition1].Y1 < GWTransitions[GWTrapezoids[i].Transition1].Y2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition1].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition1].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition1].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition1].Y1;
// 						}
// 					}
// 					Adjacents.push_back(a);
// 					PathingData[i].AdjacentsCount++;
// 				}
// 			}
// 		}
// 
// 		if(GWTrapezoids[i].Transition2 != -1)
// 		{
// 			for(unsigned int j=0;j<GWTrapezoids.size();j++)//find "adjacent" trapezoid
// 			{
// 				if(i == j)
// 					continue;
// 				if(PathingData[i].Plane == PathingData[j].Plane)
// 					continue;
// 				if (GWTrapezoids[i].Transition2 == GWTrapezoids[j].Transition2 || GWTrapezoids[i].Transition2 == GWTrapezoids[j].Transition1)
// 				{
// 					Adjacent a;
// 					a.Trapezoid = j;
// 					//Get direction
// 					if (GWTrapezoids[i].YB >= GWTrapezoids[j].YT)// down
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition2].X1 > GWTransitions[GWTrapezoids[i].Transition2].X2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 						}
// 					}
// 					else if (GWTrapezoids[i].YT <= GWTrapezoids[j].YB)// up
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition2].X1 > GWTransitions[GWTrapezoids[i].Transition2].X2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 						}
// 					}
// 					else if(GetCenterOfTrapezoid(&GWTrapezoids[i]).x > GetCenterOfTrapezoid(&GWTrapezoids[j]).x)// left
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition2].Y1 > GWTransitions[GWTrapezoids[i].Transition2].Y2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 						}
// 					}
// 					else if(GetCenterOfTrapezoid(&GWTrapezoids[i]).x < GetCenterOfTrapezoid(&GWTrapezoids[j]).x)// right
// 					{
// 						if(GWTransitions[GWTrapezoids[i].Transition2].Y1 < GWTransitions[GWTrapezoids[i].Transition2].Y2)
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 						}
// 						else
// 						{
// 							a.XR = GWTransitions[GWTrapezoids[i].Transition2].X2;
// 							a.YR = GWTransitions[GWTrapezoids[i].Transition2].Y2;
// 							a.XL = GWTransitions[GWTrapezoids[i].Transition2].X1;
// 							a.YL = GWTransitions[GWTrapezoids[i].Transition2].Y1;
// 						}
// 					}
// 					Adjacents.push_back(a);
// 					PathingData[i].AdjacentsCount++;
// 				}
// 			}
// 		}

		for(unsigned int j=0;j<PathingData.size();j++)
		{
			if(i == j)
				continue;

// 			if(PathingData[i].Plane != PathingData[j].Plane)//added before!
// 				continue;

			if(PathingData[i].YT == PathingData[j].YB)
			{
				vector<float> XCoords;
				Adjacent a;
				a.YL = a.YR = PathingData[i].YT;
				if(PathingData[i].XTL <= PathingData[j].XBR && PathingData[i].XTL >= PathingData[j].XBL)
				{
					XCoords.push_back(PathingData[i].XTL);
				}
				if(PathingData[i].XTR <= PathingData[j].XBR && PathingData[i].XTR >= PathingData[j].XBL)
				{
					XCoords.push_back(PathingData[i].XTR);
				}
				if(PathingData[j].XBR <= PathingData[i].XTR && PathingData[j].XBR >= PathingData[i].XTL)
				{
					XCoords.push_back(PathingData[j].XBR);
				}
				if(PathingData[j].XBL <= PathingData[i].XTR && PathingData[j].XBL >= PathingData[i].XTL)
				{
					XCoords.push_back(PathingData[j].XBL);
				}
				if(XCoords.size() >= 2)
				{
					sort(XCoords.begin(),XCoords.end());
					a.XL = XCoords[0];
					a.XR = XCoords[XCoords.size()-1];
					a.Trapezoid = j;
					Adjacents.push_back(a);
					PathingData[i].AdjacentsCount++;
				}
			}
			else if(PathingData[i].YB == PathingData[j].YT)
			{
				vector<float> XCoords;
				Adjacent a;
				a.YL = a.YR = PathingData[i].YB;
				if(PathingData[i].XBL <= PathingData[j].XTR && PathingData[i].XBL >= PathingData[j].XTL)
				{
					XCoords.push_back(PathingData[i].XBL);
				}
				if(PathingData[i].XBR <= PathingData[j].XTR && PathingData[i].XBR >= PathingData[j].XTL)
				{
					XCoords.push_back(PathingData[i].XBR);
				}
				if(PathingData[j].XTR <= PathingData[i].XBR && PathingData[j].XTR >= PathingData[i].XBL)
				{
					XCoords.push_back(PathingData[j].XTR);
				}
				if(PathingData[j].XTL <= PathingData[i].XBR && PathingData[j].XTL >= PathingData[i].XBL)
				{
					XCoords.push_back(PathingData[j].XTL);
				}
				if(XCoords.size() >= 2)
				{
					sort(XCoords.begin(),XCoords.end());
					a.XL = XCoords[XCoords.size()-1];
					a.XR = XCoords[0];
					a.Trapezoid = j;
					Adjacents.push_back(a);
					PathingData[i].AdjacentsCount++;
				}
			}
		}
		printf("\r");
		PrintProgressbar(PathingData.size(),i,50);
	}
	printf("\r");
	PrintProgressbar(1,1,50);
	printf("\n");
}