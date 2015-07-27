#include "GWAPIMgr.h"


GWAPI::GWAPIMgr* GWAPI::GWAPIMgr::GetInstance()
{
	static GWAPIMgr* inst = new GWAPIMgr();
	return inst
}

