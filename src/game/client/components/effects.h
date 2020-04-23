/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_EFFECTS_H
#include <game/client/component.h>

class CEffects : public CComponent
{
	bool m_Add50hz;
	bool m_Add100hz;

	int m_DamageTaken;
	float m_DamageTakenTick;
public:
	CEffects();

	virtual void OnRender();

	// mmotee
	void MmoEffects(vec2 Pos, int EffectID);
	void MmoEffectPotion(vec2 Pos, const char* Potion, bool Added);
	void DamageMmoInd(vec2 Pos, const char* pText, int Type = -1);
	void WingsEffect(vec2 Pos, vec2 Vel, vec4 Color);
	void BubbleEffect(vec2 Pos, vec2 Vel);

	void BulletTrail(vec2 Pos);
	void SmokeTrail(vec2 Pos, vec2 Vel);
	void SkidTrail(vec2 Pos, vec2 Vel);
	void Explosion(vec2 Pos);
	void HammerHit(vec2 Pos);
	void AirJump(vec2 Pos);
	void DamageIndicator(vec2 Pos, int Amount);
	void PlayerSpawn(vec2 Pos);
	void PlayerDeath(vec2 Pos, int ClientID);
	void PowerupShine(vec2 Pos, vec2 Size);
};
#endif
