/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/vmath.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>

#include "drop_items.h"

CDropItem::CDropItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, InventoryItem DropItem, int OwnerID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPITEM, Pos, 28.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Angle = 0.0f;
	m_AngleForce = AngleForce;

	m_OwnerID = OwnerID;
	m_DropItem = DropItem;
	m_DropItem.m_Settings = 0;
	m_Flashing = false;
	m_LifeSpan = Server()->TickSpeed() * 20;
	
	GameWorld()->InsertEntity(this);
	for(int i=0; i<NUM_IDS; i++)
	{
		m_IDs[i] = Server()->SnapNewID();
	}
}

CDropItem::~CDropItem()
{
	for(int i=0; i<NUM_IDS; i++)
	{
		Server()->SnapFreeID(m_IDs[i]);
	}
}

bool CDropItem::TakeItem(int ClientID)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || (m_OwnerID >= 0 && m_OwnerID != ClientID))
		return false;

	// change of enchanted objects
	GS()->CreatePlayerSound(ClientID, SOUND_ITEM_EQUIP);
	InventoryItem &pPlayerDroppedItem = pPlayer->GetItem(m_DropItem.GetID());
	if(pPlayerDroppedItem.m_Count > 0 && pPlayerDroppedItem.Info().IsEnchantable())
	{
		tl_swap(pPlayerDroppedItem, m_DropItem);
		GS()->Chat(ClientID, "You now own {STR}(+{INT})", pPlayerDroppedItem.Info().GetName(pPlayer), &pPlayerDroppedItem.m_Enchant);
		GS()->UpdateVotes(ClientID, MenuList::MENU_INVENTORY);
		GS()->UpdateVotes(ClientID, MenuList::MENU_EQUIPMENT);
		return true;
	}
	
	// simple subject delivery
	pPlayerDroppedItem.Add(m_DropItem.m_Count, 0, m_DropItem.m_Enchant);
	GS()->SBL(ClientID, BroadcastPriority::BROADCAST_GAME_WARNING, 10, "\0");
	GS()->UpdateVotes(ClientID, MenuList::MENU_INVENTORY);
	GS()->UpdateVotes(ClientID, MenuList::MENU_EQUIPMENT);
	GS()->m_World.DestroyEntity(this);
	return true;
}

void CDropItem::Tick()
{
	// lifetime dk
	m_LifeSpan--;
	if(m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GS()->m_World.DestroyEntity(this);
		return;
	}

	if(m_LifeSpan < 150)
	{
		m_FlashTimer--;
		if (m_FlashTimer > 5)
			m_Flashing = true;
		else
		{
			m_Flashing = false;
			if (m_FlashTimer <= 0)
				m_FlashTimer = 10;
		}
	}


	// physic
	m_Vel.y += 0.5f;
	static const float CheckSize = (GetProximityRadius()/2.0f);
	const bool Grounded = (bool)GS()->Collision()->CheckPoint(m_Pos.x - CheckSize, m_Pos.y + CheckSize + 5) 
		|| GS()->Collision()->CheckPoint(m_Pos.x + CheckSize, m_Pos.y + CheckSize + 5);
	if (Grounded)
	{
		m_AngleForce += (m_Vel.x - 0.74f * 6.0f - m_AngleForce) / 2.0f;
		m_Vel.x *= 0.8f;
	}
	else
	{
		m_Angle += clamp(m_AngleForce * 0.04f, -0.6f, 0.6f);
		m_Vel.x *= 0.99f;
	}

	GS()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.5f);
	if(length(m_Vel) < 0.3f)
		m_Angle = 0.0f;


	// interactive
	if(m_OwnerID != -1 && !GS()->GetPlayer(m_OwnerID, true, true))
		m_OwnerID = -1;

	CCharacter *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 64, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(!pChar || pChar->GetPlayer()->IsBot())
		return;

	// if not an enchanted object
	const InventoryItem pPlayerDroppedItem = pChar->GetPlayer()->GetItem(m_DropItem.GetID());
	if(!pPlayerDroppedItem.Info().IsEnchantable())
	{
		GS()->SBL(pChar->GetPlayer()->GetCID(), BroadcastPriority::BROADCAST_GAME_INFORMATION, 100, "{STR}x{INT} {STR}",
			m_DropItem.Info().GetName(pChar->GetPlayer()), &m_DropItem.m_Count, (m_OwnerID != -1 ? Server()->ClientName(m_OwnerID) : "\0"));
		return;
	}

	if (pPlayerDroppedItem.m_Count > 0)
	{
		GS()->SBL(pChar->GetPlayer()->GetCID(), BroadcastPriority::BROADCAST_GAME_INFORMATION, 100, "{STR}(+{INT}) -> (+{INT}) {STR}", 
			m_DropItem.Info().GetName(pChar->GetPlayer()),
			&pPlayerDroppedItem.m_Enchant, &m_DropItem.m_Enchant,
			(m_OwnerID != -1 ? Server()->ClientName(m_OwnerID) : "\0"));
		return;
	}

	GS()->SBL(pChar->GetPlayer()->GetCID(), BroadcastPriority::BROADCAST_GAME_INFORMATION, 100, "{STR}(+{INT}) {STR}",
		pPlayerDroppedItem.Info().GetName(pChar->GetPlayer()), &m_DropItem.m_Enchant, (m_OwnerID != -1 ? Server()->ClientName(m_OwnerID) : "\0"));
}

void CDropItem::Snap(int SnappingClient)
{
	if(m_Flashing || NetworkClipped(SnappingClient))
		return;

	// mrpg
	if(GS()->IsMmoClient(SnappingClient))
	{
		CNetObj_MmoPickup *pMmoPickup = static_cast<CNetObj_MmoPickup*>(Server()->SnapNewItem(NETOBJTYPE_MMOPICKUP, GetID(), sizeof(CNetObj_MmoPickup)));
		if(!pMmoPickup)
			return;

		pMmoPickup->m_X = (int)m_Pos.x;
		pMmoPickup->m_Y = (int)m_Pos.y;
		pMmoPickup->m_Type = MMO_PICKUP_BOX;
		pMmoPickup->m_Angle = (int)(m_Angle * 256.0f);
		return;
	}

	// vanilla
	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = PICKUP_GUN;
	}

	static const float Radius = 24.0f;
	const float AngleStep = 2.0f * pi / CDropItem::NUM_IDS;
	const float AngleStart = (pi / CDropItem::NUM_IDS) + (2.0f * pi * m_Angle) / 5.0f;
	for(int i = 0; i < CDropItem::NUM_IDS; i++)
	{
		CNetObj_Laser *pRifleObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs[i], sizeof(CNetObj_Laser)));
		if(!pRifleObj)
			return;

		vec2 Pos = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
		vec2 PosTo = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * (i+1)), Radius * sin(AngleStart + AngleStep * (i+1)));
		pRifleObj->m_X = (int)Pos.x;
		pRifleObj->m_Y = (int)Pos.y;
		pRifleObj->m_FromX = (int)PosTo.x;
		pRifleObj->m_FromY = (int)PosTo.y;
		pRifleObj->m_StartTick = Server()->Tick() - 3;
	}
} 