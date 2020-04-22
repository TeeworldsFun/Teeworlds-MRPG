/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/render.h>

#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/effects.h>
#include <game/client/components/sounds.h>
#include <game/client/components/controls.h>

#include "players.h"

inline float NormalizeAngular(float f)
{
	return fmod(f+pi*2, pi*2);
}

inline float AngularMixDirection (float Src, float Dst) { return sinf(Dst-Src) >0?1:-1; }
inline float AngularDistance(float Src, float Dst) { return asinf(sinf(Dst-Src)); }

inline float AngularApproach(float Src, float Dst, float Amount)
{
	float d = AngularMixDirection (Src, Dst);
	float n = Src + Amount*d;
	if(AngularMixDirection (n, Dst) != d)
		return Dst;
	return n;
}

void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPrevInfo,
	const CNetObj_PlayerInfo *pPlayerInfo,
	int ClientID
	)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = m_aRenderInfo[ClientID];

	float IntraTick = Client()->IntraGameTick();

	// set size
	RenderInfo.m_Size = 64.0f;


	// use preditect players if needed
	if(m_pClient->m_LocalClientID == ClientID && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(!m_pClient->m_Snap.m_pLocalCharacter ||
			(m_pClient->m_Snap.m_pGameData && m_pClient->m_Snap.m_pGameData->m_GameStateFlags&(GAMESTATEFLAG_PAUSED|GAMESTATEFLAG_ROUNDOVER|GAMESTATEFLAG_GAMEOVER)))
		{
		}
		else
		{
			// apply predicted results
			m_pClient->m_PredictedChar.Write(&Player);
			m_pClient->m_PredictedPrevChar.Write(&Prev);
			IntraTick = Client()->PredIntraGameTick();
		}
	}

	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	// draw hook
	if (Prev.m_HookState>0 && Player.m_HookState>0)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		//Graphics()->QuadsBegin();

		vec2 Pos = Position;
		vec2 HookPos;

		if(pPlayerChar->m_HookedPlayer != -1)
		{
			if(m_pClient->m_LocalClientID != -1 && pPlayerChar->m_HookedPlayer == m_pClient->m_LocalClientID)
			{
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK) // only use prediction if needed
					HookPos = vec2(m_pClient->m_LocalCharacterPos.x, m_pClient->m_LocalCharacterPos.y);
				else
					HookPos = mix(vec2(m_pClient->m_PredictedPrevChar.m_Pos.x, m_pClient->m_PredictedPrevChar.m_Pos.y),
						vec2(m_pClient->m_PredictedChar.m_Pos.x, m_pClient->m_PredictedChar.m_Pos.y), Client()->PredIntraGameTick());
			}
			else if(m_pClient->m_LocalClientID == ClientID)
			{
				HookPos = mix(vec2(m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Prev.m_X,
					m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Prev.m_Y),
					vec2(m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Cur.m_X,
					m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Cur.m_Y),
					Client()->IntraGameTick());
			}
			else
				HookPos = mix(vec2(pPrevChar->m_HookX, pPrevChar->m_HookY), vec2(pPlayerChar->m_HookX, pPlayerChar->m_HookY), Client()->IntraGameTick());
		}
		else
			HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);

		float d = distance(Pos, HookPos);
		vec2 Dir = normalize(Pos-HookPos);

		Graphics()->QuadsSetRotation(angle(Dir)+pi);

		// render head
		RenderTools()->SelectSprite(SPRITE_HOOK_HEAD);
		IGraphics::CQuadItem QuadItem(HookPos.x, HookPos.y, 24,16);
		Graphics()->QuadsDraw(&QuadItem, 1);

		// render chain
		RenderTools()->SelectSprite(SPRITE_HOOK_CHAIN);
		IGraphics::CQuadItem Array[1024];
		int i = 0;
		for(float f = 16; f < d && i < 1024; f += 16, i++)
		{
			vec2 p = HookPos + Dir*f;
			Array[i] = IGraphics::CQuadItem(p.x, p.y,16,16);
		}

		Graphics()->QuadsDraw(Array, i);
		Graphics()->QuadsSetRotation(0);
		Graphics()->QuadsEnd();

		RenderTools()->RenderTeeHand(&RenderInfo, Position, normalize(HookPos-Pos), -pi/2, vec2(20, 0));
	}
}

void CPlayers::RenderPlayer(const CNetObj_Character *pPrevChar, const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPrevInfo, const CNetObj_PlayerInfo *pPlayerInfo, int ClientID)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CNetObj_PlayerInfo pInfo = *pPlayerInfo;
	CTeeRenderInfo RenderInfo = m_aRenderInfo[ClientID];
	RenderInfo.m_Size = 64.0f;

	float IntraTick = Client()->IntraGameTick();
	if(Prev.m_Angle < pi*-128 && Player.m_Angle > pi*128)
		Prev.m_Angle += 2*pi*256;
	else if(Prev.m_Angle > pi*128 && Player.m_Angle < pi*-128)
		Player.m_Angle += 2*pi*256;
	float Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick)/256.0f;
	if(m_pClient->m_LocalClientID == ClientID && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's local player we are rendering
		Angle = angle(m_pClient->m_pControls->m_MousePos);
	}

	// use preditect players if needed
	if(m_pClient->m_LocalClientID == ClientID && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(!m_pClient->m_Snap.m_pLocalCharacter ||
			(m_pClient->m_Snap.m_pGameData && m_pClient->m_Snap.m_pGameData->m_GameStateFlags&(GAMESTATEFLAG_PAUSED|GAMESTATEFLAG_ROUNDOVER|GAMESTATEFLAG_GAMEOVER)))
		{
		}
		else
		{
			// apply predicted results
			m_pClient->m_PredictedChar.Write(&Player);
			m_pClient->m_PredictedPrevChar.Write(&Prev);
			IntraTick = Client()->PredIntraGameTick();
		}
	}

	vec2 Direction = direction(Angle);
	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	vec2 Vel = mix(vec2(Prev.m_VelX/256.0f, Prev.m_VelY/256.0f), vec2(Player.m_VelX/256.0f, Player.m_VelY/256.0f), IntraTick);
	m_pClient->m_pFlow->Add(Position, Vel*100.0f, 10.0f);
	RenderInfo.m_GotAirJump = Player.m_Jumped&2?0:1;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y+16);
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);

	// evaluate animation
	const float WalkTimeMagic = 100.0f;
	float WalkTime =
		((Position.x >= 0)
			? fmod(Position.x, WalkTimeMagic)
			: WalkTimeMagic - fmod(-Position.x, WalkTimeMagic))
		/ WalkTimeMagic;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f); // TODO: some sort of time here
	else if(Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f); // TODO: some sort of time here
	else if(!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	static float s_LastGameTickTime = Client()->GameTickTime();
	if(m_pClient->m_Snap.m_pGameData && !(m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime();
	if (Player.m_Weapon == WEAPON_HAMMER)
	{
		float ct = (Client()->PrevGameTick()-Player.m_AttackTick)/(float)SERVER_TICK_SPEED + s_LastGameTickTime;
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], clamp(ct*5.0f,0.0f,1.0f), 1.0f);
	}
	if (Player.m_Weapon == WEAPON_NINJA)
	{
		float ct = (Client()->PrevGameTick()-Player.m_AttackTick)/(float)SERVER_TICK_SPEED + s_LastGameTickTime;
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], clamp(ct*2.0f,0.0f,1.0f), 1.0f);
	}

	// Wings animation
	{
		if (RenderInfo.m_GotAirJump && InAir)
		{
			if (m_pClient->m_aClients[ClientID].m_AnimWings >= 1.6f)
				m_pClient->m_aClients[ClientID].m_AnimWings = 1.0f;
		}
		else if (!RenderInfo.m_GotAirJump && InAir)
		{
			if (m_pClient->m_aClients[ClientID].m_AnimWings >= 2.6f)
				m_pClient->m_aClients[ClientID].m_AnimWings = 2.2f;
		}
		else
		{
			if (m_pClient->m_aClients[ClientID].m_AnimWings >= 1.0f)
				m_pClient->m_aClients[ClientID].m_AnimWings = 0.0f;
		}
		State.Add(&g_pData->m_aAnimations[ANIM_WINGS], m_pClient->m_aClients[ClientID].m_AnimWings, 1.0f);
		m_pClient->m_aClients[ClientID].m_AnimWings += 0.59f / Client()->ClientFPS();

		int WingsEquipItem = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_WINGS];
		int WingsEnchantItem = m_pClient->m_aClients[ClientID].m_aEnchantItem[EQUIP_WINGS];
		RenderWings(&State, Position, Direction, WingsEquipItem, WingsEnchantItem);
	}

	// do skidding
	if(!InAir && WantOtherDir && length(Vel*50) > 500.0f)
	{
		static int64 SkidSoundTime = 0;
		if(time_get()-SkidSoundTime > time_freq()/10)
		{
			m_pClient->m_pSounds->PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, 0.25f, Position);
			SkidSoundTime = time_get();
		}

		m_pClient->m_pEffects->SkidTrail(
			Position+vec2(-Player.m_Direction*6,12),
			vec2(-Player.m_Direction*100*length(Vel),-50)
		);
	}

	// draw gun
	{
		if (m_pClient->m_pControls->m_ShowHookColl && m_pClient->m_LocalClientID == ClientID)
		{
			Graphics()->TextureClear();
			vec2 initPos = Position + Direction * 42.0f;

			vec2 finishPos = initPos + Direction * (m_pClient->m_Tuning.m_HookLength - 62.0f);

			Graphics()->LinesBegin();
			Graphics()->SetColor(1.00f, 0.0f, 0.0f, 1.00f);
			if (Collision()->IntersectLine(initPos, finishPos, &finishPos, 0x0))
			{
				vec2 finishPosPost = finishPos + Direction * 1.0f;
				if (!(Collision()->GetCollisionAt(finishPosPost.x, finishPosPost.y)&CCollision::COLFLAG_NOHOOK))
					Graphics()->SetColor(130.0f / 255.0f, 232.0f / 255.0f, 160.0f / 255.0f, 1.0f);
			}

			if (m_pClient->IntersectCharacter(initPos, finishPos, 12.0f, finishPos) != -1)
				Graphics()->SetColor(1.0f, 1.0f, 0.0f, 1.0f);

			IGraphics::CLineItem LineItem(Position.x, Position.y, finishPos.x, finishPos.y);
			Graphics()->LinesDraw(&LineItem, 1);
			Graphics()->LinesEnd();
		}

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);

		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle*pi * 2 + Angle);

		// normal weapons
		int iw = clamp(Player.m_Weapon, 0, NUM_WEAPONS-1);
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[iw].m_pSpriteBody, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);

		vec2 Dir = Direction, p;
		float Recoil = 0.0f;
		if (Player.m_Weapon == WEAPON_HAMMER)
		{
			p = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
	
			if (!RenderWeaponsMRPG(Player, &State, Angle, p, ClientID))
			{
				if (Direction.x < 0)
				{
					Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle*pi * 2);
					p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
				}
				else
				{
					Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle*pi * 2);
				}
				RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);
			}
	
		}
		else if (Player.m_Weapon == WEAPON_NINJA)
		{
			p = Position;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;

			if(Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi/2-State.GetAttach()->m_Angle*pi*2);
				p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
				m_pClient->m_pEffects->PowerupShine(p+vec2(32,0), vec2(32,12));
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi/2+State.GetAttach()->m_Angle*pi*2);
				m_pClient->m_pEffects->PowerupShine(p-vec2(32,0), vec2(32,12));
			}
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);

			// HADOKEN
			if ((Client()->GameTick()-Player.m_AttackTick) <= (SERVER_TICK_SPEED / 6) && g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles)
			{
				int IteX = random_int() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
				{
					const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
					if(pInfo->m_Paused)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				else
				{
					if(m_pClient->m_Snap.m_pGameData && m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				if(g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
				{
					vec2 Dir = vec2(pPlayerChar->m_X,pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y);
					Dir = normalize(Dir);
					float HadokenAngle = angle(Dir);
					Graphics()->QuadsSetRotation(HadokenAngle );
					//float offsety = -data->weapons[iw].muzzleoffsety;
					RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX], 0);
					vec2 DirY(-Dir.y,Dir.x);
					p = Position;
					float OffsetX = g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx;
					p -= Dir * OffsetX;
					RenderTools()->DrawSprite(p.x, p.y, 160.0f);
				}
			}
		}
		else
		{
			// TODO: should be an animation
			Recoil = 0;
			static float s_LastIntraTick = IntraTick;
			if(m_pClient->m_Snap.m_pGameData && !(m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
				s_LastIntraTick = IntraTick;

			float a = (Client()->GameTick()-Player.m_AttackTick+s_LastIntraTick)/5.0f;
			if(a < 1)
				Recoil = sinf(a*pi);
			p = Position + Dir * g_pData->m_Weapons.m_aId[iw].m_Offsetx - Dir*Recoil*10.0f;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;

			if (!RenderWeaponsMRPG(Player, &State, Angle, p, ClientID))
			{
				RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);

				if ((Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN) && g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles)
				{
					float Alpha = 0.0f;
					int Phase1Tick = (Client()->GameTick() - Player.m_AttackTick);
					if (Phase1Tick < (g_pData->m_Weapons.m_aId[iw].m_Muzzleduration + 3))
					{
						float t = ((((float)Phase1Tick) + IntraTick) / (float)g_pData->m_Weapons.m_aId[iw].m_Muzzleduration);
						Alpha = mix(2.0f, 0.0f, min(1.0f, max(0.0f, t)));
					}

					int IteX = random_int() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
					static int s_LastIteX = IteX;
					if (Client()->State() == IClient::STATE_DEMOPLAYBACK)
					{
						const IDemoPlayer::CInfo* pInfo = DemoPlayer()->BaseInfo();
						if (pInfo->m_Paused)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					else
					{
						if (m_pClient->m_Snap.m_pGameData && m_pClient->m_Snap.m_pGameData->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					if (Alpha > 0.0f && g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
					{
						float OffsetY = -g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsety;
						RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX], Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
						if (Direction.x < 0)
							OffsetY = -OffsetY;

						vec2 DirY(-Dir.y, Dir.x);
						vec2 MuzzlePos = p + Dir * g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx + DirY * OffsetY;
						RenderTools()->DrawSprite(MuzzlePos.x, MuzzlePos.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);
					}
				}
			}
		}
		Graphics()->QuadsEnd();

		switch (Player.m_Weapon)
		{
			case WEAPON_GUN: RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -3*pi/4, vec2(-15, 4)); break;
			case WEAPON_SHOTGUN: RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -pi/2, vec2(-5, 4)); break;
			case WEAPON_GRENADE: RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -pi/2, vec2(-4, 7)); break;
		}

	}

	// render the "shadow" tee
	if(m_pClient->m_LocalClientID == ClientID && g_Config.m_Debug)
	{
		vec2 GhostPosition = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), Client()->IntraGameTick());
		CTeeRenderInfo Ghost = RenderInfo;
		for(int p = 0; p < NUM_SKINPARTS; p++)
			Ghost.m_aColors[p].a *= 0.5f;
		RenderTools()->RenderTee(&State, &Ghost, Player.m_Emote, Direction, GhostPosition); // render ghost
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position);

	if(pInfo.m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_DOTDOT);
		IGraphics::CQuadItem QuadItem(Position.x + 24, Position.y - 40, 64,64);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	if (m_pClient->m_aClients[ClientID].m_EmoticonStart != -1 && m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() > Client()->GameTick())
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
		Graphics()->QuadsBegin();

		int SinceStart = Client()->GameTick() - m_pClient->m_aClients[ClientID].m_EmoticonStart;
		int FromEnd = m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() - Client()->GameTick();

		float a = 1;

		if (FromEnd < Client()->GameTickSpeed() / 5)
			a = FromEnd / (Client()->GameTickSpeed() / 5.0);

		float h = 1;
		if (SinceStart < Client()->GameTickSpeed() / 10)
			h = SinceStart / (Client()->GameTickSpeed() / 10.0);

		float Wiggle = 0;
		if (SinceStart < Client()->GameTickSpeed() / 5)
			Wiggle = SinceStart / (Client()->GameTickSpeed() / 5.0);

		float WiggleAngle = sinf(5*Wiggle);

		Graphics()->QuadsSetRotation(pi/6*WiggleAngle);

		Graphics()->SetColor(1.0f * a, 1.0f * a, 1.0f * a, a);
		// client_datas::emoticon is an offset from the first emoticon
		RenderTools()->SelectSprite(SPRITE_OOP + m_pClient->m_aClients[ClientID].m_Emoticon);
		IGraphics::CQuadItem QuadItem(Position.x, Position.y - 23 - 32*h, 64, 64*h);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
}

void CPlayers::OnRender()
{
	// update RenderInfo for ninja
	bool IsTeamplay = (m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS) != 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aRenderInfo[i] = m_pClient->m_aClients[i].m_RenderInfo;
		if(m_pClient->m_Snap.m_aCharacters[i].m_Cur.m_Weapon == WEAPON_NINJA)
		{
			// change the skin for the player to the ninja
			int Skin = m_pClient->m_pSkins->Find("x_ninja", true);
			if(Skin != -1)
			{
				const CSkins::CSkin *pNinja = m_pClient->m_pSkins->Get(Skin);
				for(int p = 0; p < NUM_SKINPARTS; p++)
				{
					if(IsTeamplay)
					{
						m_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_ColorTexture;
						int ColorVal = m_pClient->m_pSkins->GetTeamColor(true, pNinja->m_aPartColors[p], m_pClient->m_aClients[i].m_Team, p);
						m_aRenderInfo[i].m_aColors[p] = m_pClient->m_pSkins->GetColorV4(ColorVal, p==SKINPART_MARKING);
					}
					else if(pNinja->m_aUseCustomColors[p])
					{
						m_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_ColorTexture;
						m_aRenderInfo[i].m_aColors[p] = m_pClient->m_pSkins->GetColorV4(pNinja->m_aPartColors[p], p==SKINPART_MARKING);
					}
					else
					{
						m_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_OrgTexture;
						m_aRenderInfo[i].m_aColors[p] = vec4(1.0f, 1.0f, 1.0f, 1.0f);
					}
				}
			}
		}
	}

	// render other players in two passes, first pass we render the other, second pass we render our self
	for(int p = 0; p < 4; p++)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			// only render active characters
			if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
				continue;

			const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, i);
			const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

			if(pPrevInfo && pInfo)
			{
				//
				bool Local = m_pClient->m_LocalClientID == i;
				if((p % 2) == 0 && Local) continue;
				if((p % 2) == 1 && !Local) continue;

				CNetObj_Character PrevChar = m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				CNetObj_Character CurChar = m_pClient->m_Snap.m_aCharacters[i].m_Cur;

				if(p<2)
					RenderHook(
							&PrevChar,
							&CurChar,
							(const CNetObj_PlayerInfo *)pPrevInfo,
							(const CNetObj_PlayerInfo *)pInfo,
							i
						);
				else
					RenderPlayer(
							&PrevChar,
							&CurChar,
							(const CNetObj_PlayerInfo *)pPrevInfo,
							(const CNetObj_PlayerInfo *)pInfo,
							i
						);
			}
		}
	}
}

// mmotee
bool CPlayers::RenderWeaponsMRPG(const CNetObj_Character Player, CAnimState* pAnim, float Angle, vec2 Position, int ClientID)
{
	if (Player.m_Weapon == WEAPON_HAMMER)
	{
		int SpriteID = -1;
		float Size = 75.0f;
		int EquipID = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_HAMMER];
		if (EquipID == 10000) // Небесный
		{
			Size = 100.0f;
			SpriteID = SPRITE_MMO_HAMMER_THOOR;
		}
		return RenderHammer(pAnim, Angle, Position, SpriteID, Size);
	}
	else if (Player.m_Weapon == WEAPON_GUN)
	{
		int SpriteID = -1;
		int EquipID = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_GUN];
		if (EquipID == 10001) SpriteID = SPRITE_MMO_GUN_THOOR; // Небесный
		return RenderGun(Player, pAnim, Angle, Position, SpriteID);
	}
	else if (Player.m_Weapon == WEAPON_SHOTGUN)
	{
		int SpriteID = -1;
		int EquipID = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_SHOTGUN];
		if (EquipID == 10002) SpriteID = SPRITE_MMO_SHOTGUN_THOOR; // Небесный
		return RenderShotgun(Player, pAnim, Angle, Position, SpriteID);
	}
	else if (Player.m_Weapon == WEAPON_GRENADE)
	{
		int SpriteID = -1;
		int EquipID = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_GRENADE];
		if (EquipID == 10003) SpriteID = SPRITE_MMO_GRENADE_THOOR; // Небесный
		return RenderGrenade(pAnim, Angle, Position, SpriteID);
	}
	else if (Player.m_Weapon == WEAPON_LASER)
	{
		int SpriteID = -1;
		int EquipID = m_pClient->m_aClients[ClientID].m_aEquipItem[EQUIP_RIFLE];
		if (EquipID == 10004) SpriteID = SPRITE_MMO_RIFLE_THOOR; // Небесный
		return RenderRifle(pAnim, Angle, Position, SpriteID);
	}
	return false;
}
void CPlayers::RenderWings(CAnimState* pAnimWings, vec2 Position, vec2 Direction, int EquipItem, int Enchant)
{
	if (g_Config.m_ClShowMEffects != 2 && Enchant >= EFFECTENCHANT)
	{
		vec4 Color = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		switch(EquipItem)
		{
		case  19: Color = vec4(1.75f, 0.0f, 0.0f, 0.01f); break;
		case  20: Color = vec4(0.2f, 0.2f, 1.75f, 0.01f); break;
		case  21: Color = vec4(0.2f, 0.2f, 1.75f, 0.01f); break;
		case  22: Color = vec4(1.0f, 0.85f, 0.0f, 0.01f); break;
		case  23: Color = vec4(0.1f + frandom() * 0.9f, 0.1f + frandom() * 0.9f, 0.1f + frandom() * 0.9f, 0.01f); break;
		}

		m_pClient->m_pEffects->WingsEffect(vec2(Position.x - 60, Position.y - 20), Direction, Color);
		m_pClient->m_pEffects->WingsEffect(vec2(Position.x + 60, Position.y - 20), Direction, Color);

	}
	RenderTools()->RenderPicItems(pAnimWings, EquipItem, Direction, Position);
}

// - - - - - - - - - - MMOREPLACE GAME GUNS - - - - - - - - - - -
bool CPlayers::RenderHammer(CAnimState* pAnim, float Angle, vec2 Position, int SpriteID, float Size)
{
	if(SpriteID == -1)
		return false;

	Graphics()->QuadsEnd();
	vec2 Direction = direction(Angle);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MMOHAMMER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	RenderTools()->SelectSprite(SpriteID, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);

	if (Direction.x < 0)
	{
		Graphics()->QuadsSetRotation(-pi / 2 - pAnim->GetAttach()->m_Angle * pi * 2);
		Position.x -= g_pData->m_Weapons.m_aId[WEAPON_HAMMER].m_Offsetx;
	}
	else
	{
		Graphics()->QuadsSetRotation(-pi / 2 + pAnim->GetAttach()->m_Angle * pi * 2);
	}

	RenderTools()->DrawSprite(Position.x, Position.y, Size, Size);
	Graphics()->QuadsEnd();

	//
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	return true;
} 

bool CPlayers::RenderGun(const CNetObj_Character Player, CAnimState* pAnim, float Angle, vec2 Position, int SpriteID)
{
	if (SpriteID == -1)
		return false;

	Graphics()->QuadsEnd();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MMOGUN].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);

	vec2 Direction = direction(Angle);
	RenderTools()->SelectSprite(SpriteID, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
	RenderTools()->DrawSprite(Position.x, Position.y, 50.0f, 25.0f);

	// - - - - - - - - MUZZLE - - - - - - - - -
	float IntraTick = Client()->IntraGameTick();
	int SPRITE_MUZZLE = (1 + SpriteID + (random_int() % 2));

	float Alpha = 0.0f;
	int Phase1Tick = (Client()->GameTick() - Player.m_AttackTick);
	if (Phase1Tick < (g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Muzzleduration + 3))
	{
		float t = ((((float)Phase1Tick) + IntraTick) / (float)g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Muzzleduration);
		Alpha = mix(2.0f, 0.0f, min(1.0f, max(0.0f, t)));
	}

	if (Alpha > 0.0f)
	{
		float OffsetY = -g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Muzzleoffsety;
		RenderTools()->SelectSprite(SPRITE_MUZZLE, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
		if (Direction.x < 0)
			OffsetY = -OffsetY;

		vec2 DirY(-Direction.y, Direction.x);
		vec2 MuzzlePos = Position + Direction * g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Muzzleoffsetx + DirY * OffsetY;
		RenderTools()->DrawSprite(MuzzlePos.x, MuzzlePos.y, g_pData->m_Weapons.m_aId[WEAPON_GUN].m_VisualSize);
	}
	Graphics()->QuadsEnd();

	// - - - - - - - - BACK GAME TEEWORLDS - - - - - - - - -
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	return true;
}

bool CPlayers::RenderShotgun(const CNetObj_Character Player, CAnimState* pAnim, float Angle, vec2 Position, int SpriteID)
{
	if (SpriteID == -1)
		return false;

	Graphics()->QuadsEnd();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MMOSHOTGUN].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);

	vec2 Direction = direction(Angle);
	RenderTools()->SelectSprite(SpriteID, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
	RenderTools()->DrawSprite(Position.x, Position.y, 100.0f, 25.0f);

	// - - - - - - - - MUZZLE - - - - - - - - -
	float IntraTick = Client()->IntraGameTick();
	int SPRITE_MUZZLE = (1 + SpriteID + (random_int() % 2));

	float Alpha = 0.0f;
	int Phase1Tick = (Client()->GameTick() - Player.m_AttackTick);
	if (Phase1Tick < (g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Muzzleduration + 3))
	{
		float t = ((((float)Phase1Tick) + IntraTick) / (float)g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Muzzleduration);
		Alpha = mix(2.0f, 0.0f, min(1.0f, max(0.0f, t)));
	}

	if (Alpha > 0.0f)
	{
		float OffsetY = -g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Muzzleoffsety;
		RenderTools()->SelectSprite(SPRITE_MUZZLE, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
		if (Direction.x < 0)
			OffsetY = -OffsetY;

		vec2 DirY(-Direction.y, Direction.x);
		vec2 MuzzlePos = Position + Direction * g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Muzzleoffsetx + DirY * OffsetY;
		RenderTools()->DrawSprite(MuzzlePos.x, MuzzlePos.y, g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_VisualSize);
	}
	Graphics()->QuadsEnd();

	// - - - - - - - - BACK GAME TEEWORLDS - - - - - - - - -
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	return true;
}

bool CPlayers::RenderGrenade(CAnimState* pAnim, float Angle, vec2 Position, int SpriteID)
{
	if (SpriteID == -1)
		return false;

	Graphics()->QuadsEnd();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MMOGRENADE].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);

	vec2 Direction = direction(Angle);
	RenderTools()->SelectSprite(SpriteID, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
	RenderTools()->DrawSprite(Position.x, Position.y, 100.0f, 25.0f);
	Graphics()->QuadsEnd();

	// - - - - - - - - BACK GAME TEEWORLDS - - - - - - - - -
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	return true;
}

bool CPlayers::RenderRifle(CAnimState* pAnim, float Angle, vec2 Position, int SpriteID)
{
	if (SpriteID == -1)
		return false;

	Graphics()->QuadsEnd();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MMORIFLE].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);

	vec2 Direction = direction(Angle);
	RenderTools()->SelectSprite(SpriteID, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
	RenderTools()->DrawSprite(Position.x, Position.y, 100.0f, 50.0f);
	Graphics()->QuadsEnd();

	// - - - - - - - - BACK GAME TEEWORLDS - - - - - - - - -
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(pAnim->GetAttach()->m_Angle * pi * 2 + Angle);
	return true;
}