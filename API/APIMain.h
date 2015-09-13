#ifndef TOOLBOX_API_H // Did a classic guard here so we can detect if the api is included, might be useful i dunno.
#define TOOLBOX_API_H

#include <Windows.h>

/**********************************************************************************\

		 _____ _    _ _____   ___              
		|  __ \ |  | /  __ \ / _ \   _     _   
		| |  \/ |  | | /  \// /_\ \_| |_ _| |_ 
		| | __| |/\| | |    |  _  |_   _|_   _|
		| |_\ \  /\  / \__/\| | | | |_|   |_|  
		 \____/\/  \/ \____/\_| |_/            
                                

		Created by KAOS (a.k.a. 4D 1) for use in GWToolbox++
		
		Credits to:
		
		Sune C (a.k.a. Harboe) and ACB - 
			Most low level ground work of API via the GWCA and GWLP:R 
			projects as well as getting the gamerevision community rolling.
			Much help from following these two gentlemen's breadcrumbs.
			
		Miracle444 -
			GWA2, as well as helping me really understand how the game handles
			its memory information.
			
		d8rken - 
			Zraw menu/api which acted as a gateway to get a grasp of cpp as well
			as directx.
			
		Has - 
			For getting me to not make everything singletons :P
		
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, and/or merge copies of the Software, and to permit 
   persons to whom the Software is furnished to do so, subject to the 
   following conditions:
  
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
  
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

\**********************************************************************************/

// Comment out this #define to make the directx shit not compile into the project.
#define GWAPI_USEDIRECTX
#include "Hooker.h"

#include "GwConstants.h"
#include "GWAPIMgr.h"



#include "MemoryMgr.h"
#include "GameThreadMgr.h"

#include "GWStructures.h"

#include "CtoSMgr.h"
#include "AgentMgr.h"
#include "ItemMgr.h"

#include "MerchantMgr.h"

#ifdef GWAPI_USEDIRECTX
#include "DirectXMgr.h"
#endif

#include "SkillbarMgr.h"
#include "EffectMgr.h"

#include "GuildMgr.h"
#include "MapMgr.h"

#include "ChatMgr.h"

#endif