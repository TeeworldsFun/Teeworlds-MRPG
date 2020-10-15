/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CRAFTJOB_H
#define GAME_SERVER_CRAFTJOB_H

#include "../MmoComponent.h"
 
class CraftJob : public MmoComponent
{
 	// create an instance of the class so that you can use it globally
	struct CraftStruct
	{
		int m_aItemNeedID[3];
		int	m_aItemNeedCount[3];
		int m_ReceivedItemID;
		int m_ReceivedItemCount;
		int m_Price;
		int m_WorldID;
	};
	static std::map <int, CraftStruct> ms_aCraft;

	int GetFinalPrice(CPlayer* pPlayer, int CraftID) const;
	void CraftItem(CPlayer* pPlayer, int CraftID);
	void ShowCraftList(CPlayer* pPlayer, const char* TypeName, int SelectType);

public:
	virtual void OnInit();
	virtual bool OnHandleTile(CCharacter* pChr, int IndexCollision);
	virtual bool OnParsingVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText);
	virtual bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu);

};

#endif