/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include "ItemJob.h"

using namespace sqlstr;
std::map < int , std::map < int , ItemJob::InventoryItem > > ItemJob::Items;
std::map < int , ItemJob::ItemInformation > ItemJob::ItemsInfo;

void ItemJob::OnInit()
{
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_items_list", "WHERE ItemID > '0'"));
	while(RES->next())
	{
		int ItemID = (int)RES->getInt("ItemID");
		str_copy(ItemsInfo[ItemID].Name, RES->getString("Name").c_str(), sizeof(ItemsInfo[ItemID].Name));
		str_copy(ItemsInfo[ItemID].Desc, RES->getString("Description").c_str(), sizeof(ItemsInfo[ItemID].Desc));
		str_copy(ItemsInfo[ItemID].Icon, RES->getString("Icon").c_str(), sizeof(ItemsInfo[ItemID].Icon));
		ItemsInfo[ItemID].Type = (int)RES->getInt("Type");
		ItemsInfo[ItemID].Function = (int)RES->getInt("Function");
		ItemsInfo[ItemID].Notify = (bool)RES->getBoolean("Message");
		ItemsInfo[ItemID].Dysenthis = (int)RES->getInt("Desynthesis");
		ItemsInfo[ItemID].MinimalPrice = (int)RES->getInt("AuctionPrice");
		for (int i = 0; i < STATS_MAX_FOR_ITEM; i++)
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "Attribute_%d", i);
			ItemsInfo[ItemID].Attribute[i] = (int)RES->getInt(aBuf);
			str_format(aBuf, sizeof(aBuf), "AttributeCount_%d", i);
			ItemsInfo[ItemID].AttributeCount[i] = (int)RES->getInt(aBuf);
		}
		ItemsInfo[ItemID].MaximalEnchant = (int)RES->getInt("EnchantMax");
		ItemsInfo[ItemID].EnchantPrice = (int)RES->getInt("EnchantPrice");
		ItemsInfo[ItemID].ProjID = (int)RES->getInt("ProjectileID");
	}

	// загрузить аттрибуты
	boost::scoped_ptr<ResultSet> AttributsLoadRES(SJK.SD("*", "tw_attributs", "WHERE ID > '0'"));
	while(AttributsLoadRES->next())
	{
		int AttID = AttributsLoadRES->getInt("ID");
		str_copy(CGS::AttributInfo[AttID].Name, AttributsLoadRES->getString("name").c_str(), sizeof(CGS::AttributInfo[AttID].Name));
		str_copy(CGS::AttributInfo[AttID].FieldName, AttributsLoadRES->getString("field_name").c_str(), sizeof(CGS::AttributInfo[AttID].FieldName));
		CGS::AttributInfo[AttID].UpgradePrice = AttributsLoadRES->getInt("price");
		CGS::AttributInfo[AttID].AtType = AttributsLoadRES->getInt("at_type");
	}
}

// Загрузка данных игрока
void ItemJob::OnInitAccount(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	boost::scoped_ptr<ResultSet> RES(SJK.SD("ItemID, Count, Settings, Enchant, Durability", "tw_accounts_items", "WHERE OwnerID = '%d'", pPlayer->Acc().AuthID));
	while(RES->next())
	{
		int ItemID = (int)RES->getInt("ItemID");
		Items[ClientID][ItemID] = InventoryItem(pPlayer, ItemID);
		Items[ClientID][ItemID].Count = (int)RES->getInt("Count");
		Items[ClientID][ItemID].Settings = (int)RES->getInt("Settings");
		Items[ClientID][ItemID].Enchant = (int)RES->getInt("Enchant");
		Items[ClientID][ItemID].Durability = (int)RES->getInt("Durability");
	}		
}

void ItemJob::OnResetClient(int ClientID)
{
	if (Items.find(ClientID) != Items.end())
		Items.erase(ClientID);
}

void ItemJob::RepairDurabilityFull(CPlayer *pPlayer)
{ 
	int ClientID = pPlayer->GetCID();
	SJK.UD("tw_accounts_items", "Durability = '100' WHERE OwnerID = '%d'", pPlayer->Acc().AuthID);
	for(auto& it : Items[ClientID])
		it.second.Durability = 100;
}

void ItemJob::FormatAttributes(InventoryItem& pItem, int size, char* pformat)
{
	dynamic_string Buffer;
	for (int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		int BonusID = pItem.Info().Attribute[i];
		int BonusCount = pItem.Info().AttributeCount[i] * (pItem.Enchant + 1);
		if (BonusID <= 0 || BonusCount <= 0)
			continue;

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s +%d ", GS()->AtributeName(BonusID), BonusCount);
		Buffer.append_at(Buffer.length(), aBuf);
	}
	str_copy(pformat, Buffer.buffer(), size);
	Buffer.clear();
}

void ItemJob::FormatAttributes(ItemInformation& pInfoItem, int Enchant, int size, char* pformat)
{
	dynamic_string Buffer;
	for (int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		int BonusID = pInfoItem.Attribute[i];
		int BonusCount = pInfoItem.AttributeCount[i] * (Enchant + 1);
		if (BonusID <= 0 || BonusCount <= 0)
			continue;

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s +%d ", GS()->AtributeName(BonusID), BonusCount);
		Buffer.append_at(Buffer.length(), aBuf);
	}
	str_copy(pformat, Buffer.buffer(), size);
	Buffer.clear();
}

void ItemJob::ListInventory(CPlayer *pPlayer, int TypeList, bool SortedFunction)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AV(ClientID, "null", "");

	// показываем лист предметов игроку 
	bool Found = false;	
	for(const auto& it : Items[ClientID])
	{
		if(!it.second.Count || ((SortedFunction && it.second.Info().Function != TypeList) || (!SortedFunction && it.second.Info().Type != TypeList)))
			continue;

		ItemSelected(pPlayer, it.second);
		Found = true;
	}
	if(!Found) GS()->AVL(ClientID, "null", "There are no items in this tab");
}

// Выдаем предмет обработка повторная
void ItemJob::GiveItem(short *SecureCode, CPlayer *pPlayer, int ItemID, int Count, int Settings, int Enchant)
{
	// проверяем выдан ли и вправду предмет
	const int ClientID = pPlayer->GetCID();
	*SecureCode = SecureCheck(pPlayer, ItemID, Count, Settings, Enchant);
	if(*SecureCode == 1)
	{
		SJK.UD("tw_accounts_items", "Count = '%d', Settings = '%d', Enchant = '%d' WHERE ItemID = '%d' AND OwnerID = '%d'",
			Items[ClientID][ItemID].Count, Items[ClientID][ItemID].Settings, Items[ClientID][ItemID].Enchant, ItemID, pPlayer->Acc().AuthID);
	}
}

// Выдаем предмет обработка первичная
int ItemJob::SecureCheck(CPlayer *pPlayer, int ItemID, int Count, int Settings, int Enchant)
{
	// проверяем инициализируем и добавляем предмет
	const int ClientID = pPlayer->GetCID();
	boost::scoped_ptr<ResultSet> RES(SJK.SD("Count, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		Items[ClientID][ItemID].Count = RES->getInt("Count")+Count;
		Items[ClientID][ItemID].Settings = RES->getInt("Settings")+Settings;
		Items[ClientID][ItemID].Enchant = Enchant;
		return 1;	
	}
	// создаем предмет если не найден
	Items[ClientID][ItemID].Count = Count;
	Items[ClientID][ItemID].Settings = Settings;
	Items[ClientID][ItemID].Enchant = Enchant;
	Items[ClientID][ItemID].Durability = 100;
	SJK.ID("tw_accounts_items", "(ItemID, OwnerID, Count, Settings, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')",
		ItemID, pPlayer->Acc().AuthID, Count, Settings, Enchant);
	return 2;
}

// удаляем предмет второстепенная обработка
void ItemJob::RemoveItem(short *SecureCode, CPlayer *pPlayer, int ItemID, int Count, int Settings)
{
	*SecureCode = DeSecureCheck(pPlayer, ItemID, Count, Settings);
	if(*SecureCode == 1)
		SJK.UD("tw_accounts_items", "Count = Count - '%d', Settings = Settings - '%d' WHERE ItemID = '%d' AND OwnerID = '%d'", Count, Settings, ItemID, pPlayer->Acc().AuthID);
}

// удаление предмета первостепенная обработка
int ItemJob::DeSecureCheck(CPlayer *pPlayer, int ItemID, int Count, int Settings)
{
	// проверяем в базе данных и проверяем 
	const int ClientID = pPlayer->GetCID();
	boost::scoped_ptr<ResultSet> RES(SJK.SD("Count, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		// обновляем если количество больше
		if(RES->getInt("Count") > Count)
		{
			Items[ClientID][ItemID].Count = RES->getInt("Count")-Count;
			Items[ClientID][ItemID].Settings = RES->getInt("Settings")-Settings;
			return 1;		
		}
		// удаляем предмет если кол-во меньше положенного
		Items[ClientID][ItemID].Count = 0;
		Items[ClientID][ItemID].Settings = 0;
		Items[ClientID][ItemID].Enchant = 0;
		SJK.DD("tw_accounts_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID);
		return 2;		
	}
	// суда мы заходим если предметов нет и нечего удалять
	Items[ClientID][ItemID].Count = 0;
	Items[ClientID][ItemID].Settings = 0;
	Items[ClientID][ItemID].Enchant = 0;	
	return 0;		
}

int ItemJob::ActionItemCountAllowed(CPlayer *pPlayer, int ItemID)
{
	const int ClientID = pPlayer->GetCID();
	const int AvailableCount = Job()->Quest()->QuestingAllowedItemsCount(pPlayer, ItemID);
	if (AvailableCount <= 0)
	{
		GS()->Chat(ClientID, "This count of items that you have, iced for the quest!");
		GS()->Chat(ClientID, "Can see in which quest they are required in Adventure journal!");
		return -1;
	}
	return AvailableCount;
}

void ItemJob::ItemSelected(CPlayer* pPlayer, const InventoryItem& pPlayerItem, bool Dress)
{
	const int ClientID = pPlayer->GetCID();
	const int ItemID = pPlayerItem.GetID();
	const int HideID = NUM_TAB_MENU + ItemID;
	const char* NameItem = pPlayerItem.Info().GetName(pPlayer);

	// зачеровыванный или нет
	if (pPlayerItem.Info().IsEnchantable())
	{
		char aEnchantSize[16];
		str_format(aEnchantSize, sizeof(aEnchantSize), " [+%d]", pPlayerItem.Enchant);
		GS()->AVHI(ClientID, pPlayerItem.Info().GetIcon(), HideID, LIGHT_GRAY_COLOR, "{STR}{STR} {STR}",
			NameItem, (pPlayerItem.Enchant > 0 ? aEnchantSize : "\0"), (pPlayerItem.Settings ? " ✔" : "\0"));
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pPlayerItem.Info().GetDesc(pPlayer));

		char aAttributes[128];
		FormatAttributes(pPlayerItem.Info(), pPlayerItem.Enchant, sizeof(aAttributes), aAttributes);
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
	}
	else
	{
		GS()->AVHI(ClientID, pPlayerItem.Info().GetIcon(), HideID, LIGHT_GRAY_COLOR, "{STR}{STR} x{INT}",
			(pPlayerItem.Settings ? "Dressed - " : "\0"), NameItem, &pPlayerItem.Count);
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pPlayerItem.Info().GetDesc(pPlayer));
	}

	if (pPlayerItem.Info().Function == FUNCTION_ONE_USED || pPlayerItem.Info().Function == FUNCTION_USED)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Bind command \"/useitem %d'\"", ItemID);
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aBuf);
		GS()->AVM(ClientID, "IUSE", ItemID, HideID, "Use {STR}", NameItem);
	}

	if (pPlayerItem.Info().Type == ItemType::TYPE_POTION)
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "Auto use {STR} - {STR}", NameItem, (pPlayerItem.Settings ? "Enable" : "Disable"));

	if (pPlayerItem.Info().Type == ItemType::TYPE_DECORATION)
	{
		GS()->AVM(ClientID, "DECOSTART", ItemID, HideID, "Added {STR} to your house", NameItem);
		GS()->AVM(ClientID, "DECOGUILDSTART", ItemID, HideID, "Added {STR} to your guild house", NameItem);
	}

	if (pPlayerItem.Info().Function == FUNCTION_PLANTS)
	{
		const int HouseID = Job()->House()->OwnerHouseID(pPlayer->Acc().AuthID);
		const int PlantItemID = Job()->House()->GetPlantsID(HouseID);
		if(HouseID > 0)
		{
			if(PlantItemID != ItemID)
			{
				const int random_change = random_int() % 250;
				GS()->AVD(ClientID, "HOMEPLANTSET", ItemID, random_change, HideID, "To plant {STR}, to house (chance 0.25%)", NameItem);
			}
		}
	}

	if (pPlayerItem.Info().Type == ItemType::TYPE_EQUIP || pPlayerItem.Info().Function == FUNCTION_SETTINGS)
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "{STR} {STR}", (pPlayerItem.Settings ? "Undress" : "Equip"), NameItem);

	if (pPlayerItem.Info().IsEnchantable())
	{
		int Price = pPlayerItem.EnchantPrice();
		GS()->AVM(ClientID, "IENCHANT", ItemID, HideID, "Enchant {STR} ({INT} material)", NameItem, &Price);
	}

	if (ItemID == itHammer)
		return;

	if (pPlayerItem.Info().Dysenthis > 0)
		GS()->AVM(ClientID, "IDESYNTHESIS", ItemID, HideID, "Disassemble {STR} (+{INT}mat - 1 item)", NameItem, &pPlayerItem.Info().Dysenthis);

	GS()->AVM(ClientID, "IDROP", ItemID, HideID, "Drop {STR}", NameItem);

	if (pPlayerItem.Info().MinimalPrice)
		GS()->AVM(ClientID, "AUCTIONSLOT", ItemID, HideID, "Create Slot Auction {STR}", NameItem);
}

bool ItemJob::OnVotingMenu(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();

	// сортировка предметов
	if(PPSTR(CMD, "SORTEDINVENTORY") == 0)
	{
		pPlayer->m_SortTabs[SORTINVENTORY] = VoteID;
		GS()->VResetVotes(ClientID, MenuList::MENU_INVENTORY);
		return true;
	}

	// выброс предмета
	if(PPSTR(CMD, "IDROP") == 0)
	{
		if (!pPlayer->GetCharacter())
			return true;

		int AvailableCount = ActionItemCountAllowed(pPlayer, VoteID);
		if (AvailableCount <= 0)
			return true;

		if (Get > AvailableCount)
			Get = AvailableCount;

		InventoryItem& pPlayerDropItem = pPlayer->GetItem(VoteID);
		vec2 Force(pPlayer->GetCharacter()->m_Core.m_Input.m_TargetX, pPlayer->GetCharacter()->m_Core.m_Input.m_TargetY);
		if(length(Force) > 8.0f)
			Force = normalize(Force) * 8.0f;

		GS()->CreateDropItem(pPlayer->GetCharacter()->m_Core.m_Pos, -1, pPlayerDropItem, Get, Force);

		GS()->SBL(ClientID, BroadcastPriority::BROADCAST_GAME_WARNING, 100, "You drop {STR}x{INT}", pPlayerDropItem.Info().GetName(pPlayer), &Get);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	// использование предмета
	if(PPSTR(CMD, "IUSE") == 0)
	{
		int AvailableCount = ActionItemCountAllowed(pPlayer, VoteID);
		if (AvailableCount <= 0)
			return true;

		if (Get > AvailableCount)
			Get = AvailableCount;

		ItemInformation &pInformationItem = GS()->GetItemInfo(VoteID);
		if(pInformationItem.Function == FUNCTION_ONE_USED)
			Get = 1;

		UseItem(ClientID, VoteID, Get);
		return true;
	}

	// десинтез предмета
	if(PPSTR(CMD, "IDESYNTHESIS") == 0)
	{
		int AvailableCount = ActionItemCountAllowed(pPlayer, VoteID);
		if (AvailableCount <= 0)
			return true;

		if (Get > AvailableCount)
			Get = AvailableCount;

		InventoryItem &pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		InventoryItem &pPlayerMaterialItem = pPlayer->GetItem(itMaterial);
		const int DesCount = pPlayerSelectedItem.Info().Dysenthis * Get;
		if(pPlayerSelectedItem.Remove(Get) && pPlayerMaterialItem.Add(DesCount))
		{
			GS()->Chat(ClientID, "Disassemble {STR}x{INT}, you receive {INT} {STR}(s)", 
				pPlayerSelectedItem.Info().GetName(pPlayer), &Get, &DesCount, pPlayerMaterialItem.Info().GetName(pPlayer));
			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	// настройка предмета
	if(PPSTR(CMD, "ISETTINGS") == 0)
	{
		InventoryItem& pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		pPlayerSelectedItem.Equip();
		if(pPlayerSelectedItem.Info().Function == EQUIP_DISCORD)
			GS()->Mmo()->SaveAccount(pPlayer, SaveType::SAVE_STATS);
		else if(pPlayerSelectedItem.Info().Type == ItemType::TYPE_EQUIP)
			GS()->ChangeEquipSkin(ClientID, VoteID);

		GS()->CreatePlayerSound(ClientID, SOUND_ITEM_EQUIP);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	// зачарование
	if(PPSTR(CMD, "IENCHANT") == 0)
	{
		InventoryItem &pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		if(pPlayerSelectedItem.Enchant >= pPlayerSelectedItem.Info().MaximalEnchant)
		{
			GS()->Chat(ClientID, "You enchant max level for this item!");
			return true;			
		}

		const int Price = pPlayerSelectedItem.EnchantPrice();
		InventoryItem &pPlayerMaterialItem = pPlayer->GetItem(itMaterial);
		if(Price > pPlayerMaterialItem.Count)
		{
			GS()->Chat(ClientID, "You need {INT} Your {INT} materials!", &Price, &pPlayerMaterialItem.Count);
			return true;
		}

		if(pPlayerMaterialItem.Remove(Price, 0))
		{
			const int EnchantLevel = pPlayerSelectedItem.Enchant+1;
			pPlayerSelectedItem.SetEnchant(EnchantLevel);
			if (EnchantLevel >= EFFECTENCHANT)
				GS()->SendEquipItem(ClientID, -1);

			char aAttributes[128];
			FormatAttributes(pPlayerSelectedItem, sizeof(aAttributes), aAttributes);
			GS()->Chat(-1, "{STR} enchant {STR}+{INT} {STR}", GS()->Server()->ClientName(ClientID), pPlayerSelectedItem.Info().GetName(), &EnchantLevel, aAttributes);
			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	// сортировка надевания
	if(PPSTR(CMD, "SORTEDEQUIP") == 0)
	{
		pPlayer->m_SortTabs[SORTEQUIP] = VoteID;
		GS()->VResetVotes(ClientID, MenuList::MENU_EQUIPMENT);
		return true;				
	}

	return false;
}

bool ItemJob::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if (ReplaceMenu)
	{
		return false;
	}

	if (Menulist == MenuList::MENU_SETTINGS)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;

		// Настройки
		bool FoundSettings = false;
		GS()->AVH(ClientID, TAB_SETTINGS, RED_COLOR, "Some of the settings becomes valid after death");
		for (const auto& it : Items[ClientID])
		{
			const InventoryItem ItemData = it.second;
			if (ItemData.Info().Type != ItemType::TYPE_SETTINGS || ItemData.Count <= 0)
				continue;

			GS()->AVM(ClientID, "ISETTINGS", it.first, TAB_SETTINGS, "[{STR}] {STR}", (ItemData.Settings ? "Enable" : "Disable"), ItemData.Info().GetName(pPlayer));
			FoundSettings = true;
		}
		if (!FoundSettings)
		{
			GS()->AVM(ClientID, "null", NOPE, TAB_SETTINGS, "The list of settings is empty");
		}

		// Снаряжение
		FoundSettings = false;
		GS()->AV(ClientID, "null", "");
		GS()->AVH(ClientID, TAB_SETTINGS_MODULES, GREEN_COLOR, "Sub items settings.");
		for (const auto& it : Items[ClientID])
		{
			InventoryItem ItemData = it.second;
			if (ItemData.Count <= 0 || ItemData.Info().Type != ItemType::TYPE_MODULE)
				continue;

			char aAttributes[128];
			FormatAttributes(ItemData, sizeof(aAttributes), aAttributes);
			GS()->AVMI(ClientID, ItemData.Info().GetIcon(), "ISETTINGS", it.first, TAB_SETTINGS_MODULES, "{STR}{STR}{STR}",
				ItemData.Info().GetName(pPlayer), aAttributes, (ItemData.Settings ? "✔ " : "\0"));
			FoundSettings = true;
		}
		if (!FoundSettings)
		{
			GS()->AVM(ClientID, "null", NOPE, TAB_SETTINGS_MODULES, "The list of equipment sub upgrades is empty");
		}
		GS()->AddBack(ClientID);
		return true;
	}

	if (Menulist == MenuList::MENU_INVENTORY)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;
		GS()->AVH(ClientID, TAB_INFO_INVENTORY, GREEN_COLOR, "Inventory Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "Choose the type of items you want to show");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "After, need select item to interact");
		GS()->AV(ClientID, "null", "");

		int SizeItems;
		GS()->AVH(ClientID, TAB_INVENTORY_SELECT, RED_COLOR, "Inventory Select List");
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_USED); 
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_USED, TAB_INVENTORY_SELECT, "Used Items ({INT})", &SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_CRAFT);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_CRAFT, TAB_INVENTORY_SELECT, "Craft Items ({INT})", &SizeItems);
		
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_MODULE);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_MODULE, TAB_INVENTORY_SELECT, "Modules Items ({INT})", &SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_POTION);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_POTION, TAB_INVENTORY_SELECT, "Potion Items ({INT})", &SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_OTHER);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_OTHER, TAB_INVENTORY_SELECT, "Other Items ({INT})", &SizeItems);
		if (pPlayer->m_SortTabs[SORTINVENTORY])	
			ListInventory(pPlayer, pPlayer->m_SortTabs[SORTINVENTORY]);

		GS()->AddBack(ClientID);
		return true;
	}

	if (Menulist == MenuList::MENU_EQUIPMENT)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;
		GS()->AVH(ClientID, TAB_EQUIP_INFO, GREEN_COLOR, "Equip / Armor Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_EQUIP_INFO, "Select tab and select armor.");
		GS()->AV(ClientID, "null", "");

		GS()->AVH(ClientID, TAB_EQUIP_SELECT, RED_COLOR, "Equip Select List");
		const char* pType[NUM_EQUIPS] = { "Wings", "Hammer", "Gun", "Shotgun", "Grenade", "Rifle", "Discord", "Pickaxe" };
		for (int i = EQUIP_WINGS; i < NUM_EQUIPS; i++)
		{
			const int ItemID = pPlayer->GetEquippedItem(i);
			ItemJob::InventoryItem& pPlayerItem = pPlayer->GetItem(ItemID);
			if (ItemID <= 0 || !pPlayerItem.IsEquipped())
			{
				GS()->AVM(ClientID, "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} Not equipped", pType[i]);
				continue;
			}

			char aAttributes[128];
			FormatAttributes(pPlayerItem, sizeof(aAttributes), aAttributes);
			GS()->AVMI(ClientID, pPlayerItem.Info().GetIcon(), "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} {STR} | {STR}", pType[i], GS()->GetItemInfo(ItemID).GetName(pPlayer), aAttributes);
		}

		// все Equip слоты предемтов
		GS()->AV(ClientID, "null", "");
		bool FindItem = false;
		for (const auto& it : ItemJob::Items[ClientID])
		{
			if (!it.second.Count || it.second.Info().Function != pPlayer->m_SortTabs[SORTEQUIP])
				continue;

			ItemSelected(pPlayer, it.second, true);
			FindItem = true;
		}

		if (!FindItem)
			GS()->AVL(ClientID, "null", "There are no items in this tab");

		GS()->AddBack(ClientID);
		return true;
	}
	return false;
}

int randomRangecount(int startrandom, int endrandom, int count)
{
	int result = 0;
	for(int i = 0; i < count; i++)
	{
		int random = startrandom + random_int() % (endrandom - startrandom);
		result += random;
	}
	return result;
}
void ItemJob::UseItem(int ClientID, int ItemID, int Count)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || pPlayer->GetItem(ItemID).Count < Count) 
		return;

	InventoryItem &PlItem = pPlayer->GetItem(ItemID);
	if(ItemID == itPotionHealthRegen && PlItem.Remove(Count, 0))
	{
		pPlayer->GiveEffect("RegenHealth", 15);
		GS()->ChatFollow(ClientID, "You used {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Count);
	}
	
	if(ItemID == itPotionManaRegen && PlItem.Remove(Count, 0))
	{
		pPlayer->GiveEffect("RegenMana", 15);
		GS()->ChatFollow(ClientID, "You used {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Count);
	}

	if(ItemID == itPotionResurrection && PlItem.Remove(Count, 0))
	{
		pPlayer->GetTempData().TempActiveSafeSpawn = false;
		pPlayer->GetTempData().TempHealth = pPlayer->GetStartHealth();
		GS()->ChatFollow(ClientID, "You used {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Count);
	}

	if(ItemID == itCapsuleSurvivalExperience && PlItem.Remove(Count, 0)) 
	{
		int Getting = randomRangecount(10, 50, Count);
		GS()->Chat(-1, "{STR} used {STR}x{INT} and got {INT} Survival Experience.", GS()->Server()->ClientName(ClientID), PlItem.Info().GetName(), &Count, &Getting);
		pPlayer->AddExp(Getting);
	}

	if(ItemID == itLittleBagGold && PlItem.Remove(Count, 0))
	{
		int Getting = randomRangecount(10, 50, Count);
		GS()->Chat(-1, "{STR} used {STR}x{INT} and got {INT} gold.", GS()->Server()->ClientName(ClientID), PlItem.Info().GetName(), &Count, &Getting);
		pPlayer->AddMoney(Getting);
	}
	GS()->VResetVotes(ClientID, MenuList::MENU_INVENTORY);
	return;
}

int ItemJob::GetCountItemsType(CPlayer *pPlayer, int Type) const
{
	int SizeItems = 0;
	const int ClientID = pPlayer->GetCID();
	for(const auto& item : Items[ClientID])
	{
		if(item.second.Count > 0 && item.second.Info().Type == Type)
			SizeItems++;
	}
	return SizeItems;
}

const char *ItemJob::ClassItemInformation::GetName(CPlayer *pPlayer) const
{
	if(!pPlayer) return Name;
	return pPlayer->GS()->Server()->Localization()->Localize(pPlayer->GetLanguage(), _(Name));	
}

const char *ItemJob::ClassItemInformation::GetDesc(CPlayer *pPlayer) const
{
	if(!pPlayer) return Desc;
	return pPlayer->GS()->Server()->Localization()->Localize(pPlayer->GetLanguage(), _(Desc));	
}

bool ItemJob::ClassItemInformation::IsEnchantable() const
{
	for (int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if (CGS::AttributInfo.find(Attribute[i]) != CGS::AttributInfo.end() && Attribute[i] > 0 && AttributeCount[i] > 0 && EnchantPrice && MaximalEnchant > 0)
			return true;
	}
	return false;
}

bool ItemJob::ClassItems::SetEnchant(int arg_enchantlevel)
{
	if (Count < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	Enchant = arg_enchantlevel;
	bool Successful = Save();
	return Successful;
}

bool ItemJob::ClassItems::Add(int arg_count, int arg_settings, int arg_enchant, bool arg_message)
{
	if(arg_count < 1 || !m_pPlayer || !m_pPlayer->IsAuthed()) 
		return false;

	CGS* GameServer = m_pPlayer->GS();
	const int ClientID = m_pPlayer->GetCID();
	if(Info().IsEnchantable())
	{
		if(Count > 0) 
		{
			m_pPlayer->GS()->Chat(ClientID, "This item cannot have more than 1 item");
			return false;
		}
		arg_count = 1;
	}

	// проверить пустой слот если да тогда одеть предмет
	const bool AutoEquip = (Info().Type == ItemType::TYPE_EQUIP && m_pPlayer->GetEquippedItem(Info().Function) <= 0) || (Info().Function == FUNCTION_SETTINGS && Info().IsEnchantable());
	if(AutoEquip)
	{
		if(Info().Function == EQUIP_DISCORD)  
			GameServer->Mmo()->SaveAccount(m_pPlayer, SaveType::SAVE_STATS);

		char aAttributes[128];
		GameServer->Mmo()->Item()->FormatAttributes(*this, sizeof(aAttributes), aAttributes);
		GameServer->Chat(ClientID, "Auto equip {STR} - {STR}", Info().GetName(m_pPlayer), aAttributes);
		GameServer->CreatePlayerSound(ClientID, SOUND_ITEM_EQUIP);
	}

	// выдаем предмет
	GameServer->Mmo()->Item()->GiveItem(&m_pPlayer->m_SecurCheckCode, m_pPlayer, itemid_, arg_count, (AutoEquip ? 1 : arg_settings), arg_enchant);
	if(m_pPlayer->m_SecurCheckCode <= 0) 
		return false;

	// отправить смену скина
	if(AutoEquip) 
		GameServer->ChangeEquipSkin(ClientID, itemid_);

	// если тихий режим
	if(!arg_message || Info().Type == ItemType::TYPE_SETTINGS) 
		return true;

	// информация о получении себе предмета
	if(Info().Type != -1 && itemid_ != itGold)
		GameServer->Chat(ClientID, "You got of the {STR}x{INT}!", Info().GetName(m_pPlayer), &arg_count);
	else if(Info().Notify)
		GameServer->Chat(-1, "{STR} got of the {STR}x{INT}!", GameServer->Server()->ClientName(ClientID), Info().GetName(), &arg_count);

	return true;
}

bool ItemJob::ClassItems::Remove(int arg_removecount, int arg_settings)
{
	if(Count <= 0 || arg_removecount < 1 || !m_pPlayer) 
		return false;

	if(Count < arg_removecount)
		arg_removecount = Count;

	if (IsEquipped())
	{
		Settings = 0;
		int ClientID = m_pPlayer->GetCID();
		m_pPlayer->GS()->ChangeEquipSkin(ClientID, itemid_);
	}

	m_pPlayer->GS()->Mmo()->Item()->RemoveItem(&m_pPlayer->m_SecurCheckCode, m_pPlayer, itemid_, arg_removecount, arg_settings);
	return (bool)(m_pPlayer->m_SecurCheckCode > 0);
}

bool ItemJob::ClassItems::SetSettings(int arg_settings)
{
	if (Count < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	Settings = arg_settings;
	return Save();
}

bool ItemJob::ClassItems::SetDurability(int arg_durability)
{
	if(Count < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	Durability = arg_durability;
	return Save();
}

bool ItemJob::ClassItems::Equip()
{
	if (Count < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	if(Info().Type == ItemType::TYPE_EQUIP)
	{
		const int EquipID = Info().Function;
		int EquipItemID = m_pPlayer->GetEquippedItem(EquipID, itemid_);
		while (EquipItemID >= 1)
		{
			ItemJob::InventoryItem &EquipItem = m_pPlayer->GetItem(EquipItemID);
			EquipItem.Settings = 0;
			EquipItem.SetSettings(0);
			EquipItemID = m_pPlayer->GetEquippedItem(EquipID, itemid_);
		}
	}

	Settings ^= true;
	if(Info().GetStatsBonus(Stats::StAmmoRegen) > 0 && m_pPlayer->GetCharacter())
		m_pPlayer->GetCharacter()->m_AmmoRegen = m_pPlayer->GetAttributeCount(Stats::StAmmoRegen, true);

	m_pPlayer->ShowInformationStats();
	return Save();
}

bool ItemJob::ClassItems::Save()
{
	if (m_pPlayer && m_pPlayer->IsAuthed())
	{
		SJK.UD("tw_accounts_items", "Count = '%d', Settings = '%d', Enchant = '%d' WHERE OwnerID = '%d' AND ItemID = '%d'", Count, Settings, Enchant, m_pPlayer->Acc().AuthID, itemid_);
		return true;
	}
	return false;
}

// TODO: FIX IT (lock .. unlock)
static std::map<int, std::mutex > lock_sleep;
void ItemJob::AddItemSleep(int AccountID, int ItemID, int GiveCount, int Milliseconds)
{
	std::thread([this, AccountID, ItemID, GiveCount, Milliseconds]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

			lock_sleep[AccountID].lock();
			const int ClientID = Job()->Account()->CheckOnlineAccount(AccountID);
			CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
			if(pPlayer)
			{
				pPlayer->GetItem(ItemID).Add(GiveCount);
				lock_sleep[AccountID].unlock();
				return;
			}

			boost::scoped_ptr<ResultSet> RES(SJK.SD("Count", "tw_accounts_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, AccountID));
			if(RES->next())
			{
				const int ReallyCount = (int)RES->getInt("Count") + GiveCount;
				SJK.UD("tw_accounts_items", "Count = '%d' WHERE OwnerID = '%d' AND ItemID = '%d'", ReallyCount, AccountID, ItemID);
				lock_sleep[AccountID].unlock();
				return;
			}
			SJK.ID("tw_accounts_items", "(ItemID, OwnerID, Count, Settings, Enchant) VALUES ('%d', '%d', '%d', '0', '0')", ItemID, AccountID, GiveCount);
			lock_sleep[AccountID].unlock();
		}).detach();
}