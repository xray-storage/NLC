// hit_immunity.cpp:	����� ��� ��� ��������, ������� ������������
//						������������ ���������� ��� ������ ����� �����
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "hit_immunity.h"
#include "GameObject.h"
#include "pch_script.h"

#pragma optimize("gyts", off)

LPCSTR _imm_names [11] = {
	"burn_immunity",
	"strike_immunity",
	"shock_immunity",
	"wound_immunity",		
	"radiation_immunity",
	"telepatic_immunity",
	"chemical_burn_immunity",
	"explosion_immunity",
	"fire_wound_immunity",
	"wound_2_immunity",
	"physic_strike_immunity"
};



CHitImmunity::CHitImmunity()
{
	m_HitImmunityKoefs.resize(ALife::eHitTypeMax);
	for(int i=0; i<ALife::eHitTypeMax; i++)
		m_HitImmunityKoefs[i] = 1.0f;
}

CHitImmunity::~CHitImmunity()
{
}


void CHitImmunity::LoadImmunities(LPCSTR imm_sect,CInifile* ini)
{
	R_ASSERT2	(ini->section_exist(imm_sect), imm_sect);
	if (strstr(imm_sect, "af_dummy_spring_absorbation"))
		__asm nop;

	for (int i = ALife::eHitTypeBurn; i <= ALife::eHitTypePhysicStrike; i ++)
		m_HitImmunityKoefs[i] = READ_IF_EXISTS(ini, r_float, imm_sect, _imm_names[i], 1.0f);

	/*
	m_HitTypeK[ALife::eHitTypeBurn]			= ini->r_float(imm_sect,"burn_immunity");
	m_HitTypeK[ALife::eHitTypeStrike]		= ini->r_float(imm_sect,"strike_immunity");
	m_HitTypeK[ALife::eHitTypeShock]		= ini->r_float(imm_sect,"shock_immunity");
	m_HitTypeK[ALife::eHitTypeWound]		= ini->r_float(imm_sect,"wound_immunity");
	m_HitTypeK[ALife::eHitTypeRadiation]	= ini->r_float(imm_sect,"radiation_immunity");
	m_HitTypeK[ALife::eHitTypeTelepatic]	= ini->r_float(imm_sect,"telepatic_immunity");
	m_HitTypeK[ALife::eHitTypeChemicalBurn] = ini->r_float(imm_sect,"chemical_burn_immunity");
	m_HitTypeK[ALife::eHitTypeExplosion]	= ini->r_float(imm_sect,"explosion_immunity");
	m_HitTypeK[ALife::eHitTypeFireWound]	= ini->r_float(imm_sect,"fire_wound_immunity");	
	m_HitTypeK[ALife::eHitTypeWound_2]		= READ_IF_EXISTS(ini, r_float, imm_sect, "wound_2_immunity", 1.0f);
	m_HitTypeK[ALife::eHitTypePhysicStrike]	= READ_IF_EXISTS(ini, r_float, imm_sect, "physic_strike_immunity", 1.0f);
	*/
}

void CHitImmunity::AddImmunities(LPCSTR imm_sect, CInifile * ini)
{
	R_ASSERT2	(ini->section_exist(imm_sect), imm_sect);

	m_HitImmunityKoefs[ALife::eHitTypeBurn]			+= READ_IF_EXISTS(ini, r_float, imm_sect,"burn_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeStrike]		+= READ_IF_EXISTS(ini, r_float, imm_sect,"strike_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeShock]		+= READ_IF_EXISTS(ini, r_float, imm_sect,"shock_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeWound]		+= READ_IF_EXISTS(ini, r_float, imm_sect,"wound_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeRadiation]	+= READ_IF_EXISTS(ini, r_float, imm_sect,"radiation_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeTelepatic]	+= READ_IF_EXISTS(ini, r_float, imm_sect,"telepatic_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeChemicalBurn] += READ_IF_EXISTS(ini, r_float, imm_sect,"chemical_burn_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeExplosion]	+= READ_IF_EXISTS(ini, r_float, imm_sect,"explosion_immunity", 0.0f);
	m_HitImmunityKoefs[ALife::eHitTypeFireWound]	+= READ_IF_EXISTS(ini, r_float, imm_sect,"fire_wound_immunity", 0.0f);
//	m_HitImmunityKoefs[ALife::eHitTypePhysicStrike]	+= READ_IF_EXISTS(ini, r_float, imm_sect,"physic_strike_wound_immunity", 0.0f);
//	m_HitImmunityKoefs[ALife::eHitTypeLightBurn]	= m_HitImmunityKoefs[ALife::eHitTypeBurn];
}

float CHitImmunity::AffectHit (float power, ALife::EHitType hit_type)
{	
	float  k = m_HitImmunityKoefs[hit_type];
	return power * k;
}

using namespace luabind;


float get_burn_immunity(CHitImmunity *I)					{ return I->immunities()[ALife::eHitTypeBurn]; }
void  set_burn_immunity(CHitImmunity *I, float i)			{ I->immunities()[ALife::eHitTypeBurn] = i; }

float get_strike_immunity(CHitImmunity *I)					{ return I->immunities()[ALife::eHitTypeStrike]; }
void  set_strike_immunity(CHitImmunity *I, float i)			{ I->immunities()[ALife::eHitTypeStrike] = i; }

float get_shock_immunity(CHitImmunity *I)					{ return I->immunities()[ALife::eHitTypeShock]; }
void  set_shock_immunity(CHitImmunity *I, float i)			{ I->immunities()[ALife::eHitTypeShock] = i; }

float get_wound_immunity(CHitImmunity *I)					{ return I->immunities()[ALife::eHitTypeWound]; }
void  set_wound_immunity(CHitImmunity *I, float i)			{ I->immunities()[ALife::eHitTypeWound] = i; }

float get_radiation_immunity(CHitImmunity *I)				{ return I->immunities()[ALife::eHitTypeRadiation]; }
void  set_radiation_immunity(CHitImmunity *I, float i)		{ I->immunities()[ALife::eHitTypeRadiation] = i; }

float get_telepatic_immunity(CHitImmunity *I)				{ return I->immunities()[ALife::eHitTypeTelepatic]; }
void  set_telepatic_immunity(CHitImmunity *I, float i)		{ I->immunities()[ALife::eHitTypeTelepatic] = i; }

float get_chemical_burn_immunity(CHitImmunity *I)			{ return I->immunities()[ALife::eHitTypeChemicalBurn]; }
void  set_chemical_burn_immunity(CHitImmunity *I, float i)	{ I->immunities()[ALife::eHitTypeChemicalBurn] = i; }

float get_explosion_immunity(CHitImmunity *I)				{ return I->immunities()[ALife::eHitTypeExplosion]; }
void  set_explosion_immunity(CHitImmunity *I, float i)		{ I->immunities()[ALife::eHitTypeExplosion] = i; }

float get_fire_wound_immunity(CHitImmunity *I)				{ return I->immunities()[ALife::eHitTypeFireWound]; }
void  set_fire_wound_immunity(CHitImmunity *I, float i)		{ I->immunities()[ALife::eHitTypeFireWound] = i; }

float get_wound_2_immunity(CHitImmunity *I)					{ return I->immunities()[ALife::eHitTypeWound_2]; }
void  set_wound_2_immunity(CHitImmunity *I, float i)		{ I->immunities()[ALife::eHitTypeWound_2] = i; }

float get_physic_strike_immunity(CHitImmunity *I)			{ return I->immunities()[ALife::eHitTypePhysicStrike]; }
void  set_physic_strike_immunity(CHitImmunity *I, float i)  { I->immunities()[ALife::eHitTypePhysicStrike] = i; }

extern LPCSTR get_lua_class_name(luabind::object O);

void CHitImmunity::script_register(lua_State *L)
{
	module(L)
		[
			class_<CHitImmunity>("CHitImmunity")			
			.property("burn_immunity"			,			&get_burn_immunity,				&set_burn_immunity)
			.property("strike_immunity"			,			&get_strike_immunity,			&set_strike_immunity)
			.property("shock_immunity"			,			&get_shock_immunity,			&set_shock_immunity)
			.property("wound_immunity"			,			&get_wound_immunity,			&set_wound_immunity)
			.property("radiation_immunity"		,			&get_radiation_immunity,		&set_radiation_immunity)
			.property("telepatic_immunity"		,			&get_telepatic_immunity,		&set_telepatic_immunity)
			.property("chemical_burn_immunity"	,			&get_chemical_burn_immunity,	&set_chemical_burn_immunity)
			.property("explosion_immunity"		,			&get_explosion_immunity,		&set_explosion_immunity)
			.property("fire_wound_immunity"		,			&get_fire_wound_immunity,		&set_fire_wound_immunity)
			.property("wound_2_immunity"		,			&get_wound_2_immunity,			&set_wound_2_immunity)
			.property("physic_strike_immunity"	,			&get_physic_strike_immunity,	&set_physic_strike_immunity)
			.property("class_name"				,			&get_lua_class_name)
		];
}
