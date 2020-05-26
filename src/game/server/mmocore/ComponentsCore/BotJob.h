/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_BOTS_INFO_SQL_H
#define GAME_SERVER_BOTS_INFO_SQL_H

#include "../MmoComponent.h"
 
class BotJob : public MmoComponent
{
	~BotJob()
	{
		DataBot.clear();
		QuestBot.clear();
		NpcBot.clear();
		MobBot.clear();
	};

	// bots talking data
	struct TalkingData
	{
		char m_TalkingText[512];
		int m_Style;
		int m_Emote;
		int m_GivingQuest;
		bool m_RequestComplete;
		bool m_PlayerTalked;
	};

	// main bots information
	struct DescDataBot
	{
		char NameBot[16];
		char SkinNameBot[6][24];
		int UseCustomBot[6];
		int SkinColorBot[6];
		int EquipSlot[EQUIP_MAX_BOTS];
		bool AlreadySnapQuestBot[MAX_PLAYERS];
	};

	// types bots information
	struct ClassNpcBot
	{
		const char* GetName() const
		{
			dbg_assert(DataBot.find(BotID) != DataBot.end(), "Name bot it invalid");
			return DataBot[BotID].NameBot;
		}

		bool Static;
		int PositionX;
		int PositionY;
		int Emote;
		int WorldID;
		int BotID;
		int Function;
		std::vector < TalkingData > m_Talk;
	};

	struct ClassQuestBot
	{
		const char* GetName() const
		{
			dbg_assert(DataBot.find(BotID) != DataBot.end(), "Name bot it invalid");
			return DataBot[BotID].NameBot;
		}

		int PositionX;
		int PositionY;
		int QuestID;
		int Progress;
		int WorldID;
		int BotID;
		int SubBotID;

		int ItemSearch[2];
		int ItemSearchCount[2];

		int ItemGives[2];
		int ItemGivesCount[2];

		int NeedMob[2];
		int NeedMobCount[2];

		int InteractiveType;
		int InteractiveTemp;
		bool GenerateNick;
		bool IdenticalLastProgress
		std::vector < TalkingData > m_Talk;
	};

	struct ClassMobsBot
	{
		const char* GetName() const
		{
			dbg_assert(DataBot.find(BotID) != DataBot.end(), "Name bot it invalid");
			return DataBot[BotID].NameBot;
		}
		bool Boss;
		int Power;
		int Spread;
		int PositionX;
		int PositionY;
		int Level;
		int RespawnTick;
		int DungeonDoorID;
		int WorldID;
		char Effect[16];

		int DropItem[MAX_DROPPED_FROM_MOBS];
		int CountItem[MAX_DROPPED_FROM_MOBS];
		float RandomItem[MAX_DROPPED_FROM_MOBS];
		int BotID;
	};

	void LoadMainInformationBots();
	void LoadQuestBots(const char* pWhereLocalWorld);
	void LoadNpcBots(const char* pWhereLocalWorld);
	void LoadMobsBots(const char* pWhereLocalWorld);
public:
	virtual void OnInitWorld(const char* pWhereLocalWorld);

	typedef DescDataBot DataBotInfo;
	static std::map < int , DataBotInfo > DataBot;

	typedef ClassQuestBot QuestBotInfo;
	static std::map < int , QuestBotInfo > QuestBot;

	typedef ClassNpcBot NpcBotInfo;
	static std::map < int , NpcBotInfo > NpcBot;

	typedef ClassMobsBot MobBotInfo;
	static std::map < int , MobBotInfo > MobBot;

	void ConAddCharacterBot(int ClientID, const char *pCharacter);
	void ProcessingTalkingNPC(int OwnID, int TalkingID, bool PlayerTalked, const char *Message, int Style, int TalkingEmote);
	bool TalkingBotNPC(CPlayer* pPlayer, int MobID, int Progress, int TalkedID, const char *pText = "empty");
	bool TalkingBotQuest(CPlayer* pPlayer, int MobID, int Progress, int TalkedID);
	void ShowBotQuestTaskInfo(CPlayer* pPlayer, int MobID, int Progress);
	int IsGiveQuestNPC(int MobID) const;

	// ------------------ CHECK VALID DATA --------------------
	bool IsDataBotValid(int BotID) const { return (DataBot.find(BotID) != DataBot.end()); }
	bool IsNpcBotValid(int MobID) const 
	{ 
		if(NpcBot.find(MobID) != NpcBot.end() && IsDataBotValid(NpcBot[MobID].BotID))
			return true;
		return false; 
	}
	bool IsMobBotValid(int MobID) const 
	{ 
		if(MobBot.find(MobID) != MobBot.end() && IsDataBotValid(MobBot[MobID].BotID))
			return true;
		return false;
	}
	
	bool IsQuestBotValid(int MobID) const 
	{ 
		if(QuestBot.find(MobID) != QuestBot.end() && IsDataBotValid(QuestBot[MobID].BotID))
			return true;
		return false;
	}

	// threading path finder
	void FindThreadPath(class CPlayerBot* pBotPlayer, vec2 StartPos, vec2 SearchPos);
	void GetThreadRandomWaypointTarget(class CPlayerBot* pBotPlayer);
};

#endif