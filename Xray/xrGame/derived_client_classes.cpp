////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 07.12.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "base_client_classes.h"
#include "derived_client_classes.h"
#include "HUDManager.h"
#include "WeaponHUD.h"
#include "exported_classes_def.h"
#include "script_game_object.h"
#include "ui/UIDialogWnd.h"
#include "ui/UIInventoryWnd.h"
#include "../xr_3da/lua_tools.h"
#include "camerarecoil.h"
/* ���������� � ����� �������� ������� � �������:
     * �������� �������� �������������� �� ����������� ���, ��� ��� �������� � ������ ������������ (*.ltx), � �� ��� ��� ��� ������� � ���������� ������
	 * ������ �������� �������������� �������� ����� �������� ��� game_object, �.� ��� ������������� ��������� ����. 
	    ��� ��������� ��������� ������ ����� ���������������� � �������� � �������� ����� ������ �� ������� ��������� ������ 1.0006.
   Alexander Petrov
*/


using namespace luabind;
#pragma optimize("s", on)

extern LPCSTR get_lua_class_name(luabind::object O);
extern CGameObject *lua_togameobject(lua_State *L, int index);

// ================================ ANOMALY ZONE SCRIPT EXPORT =================== //
Fvector get_restrictor_center(CSpaceRestrictor *SR)
{
	Fvector result;
	SR->Center(result);
	return result;
}

u32 get_zone_state(CCustomZone *obj)  { return (u32)obj->ZoneState(); }
void CAnomalyZoneScript::set_zone_state(CCustomZone *obj, u32 new_state)
{ 
	obj->SwitchZoneState ( (CCustomZone::EZoneState) new_state); 
}


void CAnomalyZoneScript::script_register(lua_State *L)
{
	module(L)
		[	
			class_<CSpaceRestrictor, CGameObject>("CSpaceRestrictor")
			.property("restrictor_center"				,				&get_restrictor_center)
			.property("restrictor_type"					,				&CSpaceRestrictor::restrictor_type)
			.property("radius"							,				&CSpaceRestrictor::Radius)						
			,
			class_<CCustomZone, CSpaceRestrictor>("CustomZone")
			.def  ("get_state_time"						,				&CCustomZone::GetStateTime) 
			.def  ("power"								,				&CCustomZone::Power)
			.def  ("relative_power"						,				&CCustomZone::RelativePower)


			.def_readwrite("attenuation"				,				&CCustomZone::m_fAttenuation) 
			.def_readwrite("effective_radius"			,				&CCustomZone::m_fEffectiveRadius) 
			.def_readwrite("hit_impulse_scale"			,				&CCustomZone::m_fHitImpulseScale) 
			.def_readwrite("max_power"					,				&CCustomZone::m_fMaxPower) 
			.def_readwrite("state_time"					,				&CCustomZone::m_iStateTime) 
			.def_readwrite("start_time"					,				&CCustomZone::m_StartTime) 
			.def_readwrite("time_to_live"				,				&CCustomZone::m_ttl) 
			.def_readwrite("zone_active"				,				&CCustomZone::m_bZoneActive) 			
			.property("zone_state"						,				&get_zone_state, &CAnomalyZoneScript::set_zone_state)

		];
}

IC void alive_entity_set_radiation(CEntityAlive *E, float value)
{
	E->SetfRadiation(value);
}

void CEntityScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEntity, CGameObject>("CEntity")
			,
			class_<CEntityAlive, CEntity>("CEntityAlive")
			.property("radiation"						,			&CEntityAlive::g_Radiation, &alive_entity_set_radiation) // ���� � %
			.property("condition"						,           &CEntityAlive::conditions)
		];
}

void CEatableItemScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEatableItem, CInventoryItem>("CEatableItem")
			.def_readwrite("eat_health"					,			&CEatableItem::m_fHealthInfluence)
			.def_readwrite("eat_power"					,			&CEatableItem::m_fPowerInfluence)
			.def_readwrite("eat_satiety"				,			&CEatableItem::m_fSatietyInfluence)
			.def_readwrite("eat_radiation"				,			&CEatableItem::m_fRadiationInfluence)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_fMaxPowerUpInfluence)
			.def_readwrite("wounds_heal_perc"			,			&CEatableItem::m_fWoundsHealPerc)
			.def_readwrite("eat_portions_num"			,			&CEatableItem::m_iPortionsNum)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_iStartPortionsNum)			
			,
			class_<CEatableItemObject, bases<CEatableItem, CGameObject>>("CEatableItemObject")
		];
}

void set_io_money(CInventoryOwner *IO, u32 money) { IO->set_money(money, true); }

CScriptGameObject  *item_lua_object(PIItem itm)
{	
	if (itm)
	{
		CGameObject *obj = smart_cast<CGameObject *>(itm);
		if (obj) return obj->lua_game_object();
	}
	return NULL;
}


CScriptGameObject  *inventory_active_item(CInventory *I)	{ return item_lua_object (I->ActiveItem()); }
CScriptGameObject  *inventory_selected_item(CInventory *I)
{
	CUIDialogWnd *IR = HUD().GetUI()->MainInputReceiver();
	if (!IR) return NULL;
	CUIInventoryWnd *wnd = smart_cast<CUIInventoryWnd *>(IR);
	if (!wnd) return NULL;
	if (wnd->GetInventory() != I) return NULL;		
	return item_lua_object( wnd->CurrentIItem() );	
}

CScriptGameObject  *get_inventory_target(CInventory *I)		{ return item_lua_object(I->m_pTarget); }

LPCSTR get_item_name				(CInventoryItem *I) { return I->Name(); }
LPCSTR get_item_name_short			(CInventoryItem *I) { return I->NameShort(); }


void item_to_belt(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Belt (itm);
}

void item_to_slot(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Slot(itm, !!lua_toboolean(L, 3));
}

void item_to_ruck(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Ruck(itm);
}

#ifdef INV_NEW_SLOTS_SYSTEM
void get_slots(luabind::object O)
{
	lua_State *L = O.lua_state();
	CInventoryItem *itm = luabind::object_cast<CInventoryItem*> (O);
	lua_createtable (L, 0, 0);
	int tidx = lua_gettop(L);
	if (itm)
	{
		for (u32 i = 0; i < itm->GetSlotsCount(); i++)
		{
			lua_pushinteger(L, i + 1); // key
			lua_pushinteger(L, itm->GetSlots()[i]);
			lua_settable(L, tidx);
		}
	}

}

void fake_set_slots(CInventoryItem *I, luabind::object T) { } // �������������� ����� �����, ���� GetSlots �� ����� ���������� ���������
#endif

bool is_quest_item(const CInventoryItem *I)  { return !!I->IsQuestItem(); }
void set_is_quest_item(CInventoryItem *I, bool yep)
{
	I->m_flags.set(CInventoryItem::FIsQuestItem, yep);
}

void CInventoryScript::script_register(lua_State *L)
{
	module(L)
		[

			class_<CInventoryItem>("CInventoryItem")						
			.def_readonly("item_place"					,			&CInventoryItem::m_eItemPlace)
			.def_readwrite("item_condition"				,			&CInventoryItem::m_fCondition)
			.def_readwrite("inv_weight"					,			&CInventoryItem::m_weight)
			.property("class_name"						,			&get_lua_class_name)
			.property("item_name"						,			&get_item_name)
			.property("item_name_short"					,			&get_item_name_short)
			.property("quest_item"						,			&is_quest_item, &set_is_quest_item)
			.property("cost"							,			&CInventoryItem::Cost,  &CInventoryItem::SetCost)
			.property("slot"							,			&CInventoryItem::GetSlot, &CInventoryItem::SetSlot)
#ifdef INV_NEW_SLOTS_SYSTEM
			.property("slots"							,			&get_slots,    &fake_set_slots, raw(_2))	
#endif
			.def("visible_in_slot"						,			&CInventoryItem::VisibleInSlot)
			.def("set_visible_in_slot"					,			&CInventoryItem::SetVisibleInSlot)
			,
			class_<CInventoryItemObject, bases<CInventoryItem, CGameObject>>("CInventoryItemObject"),

			class_ <CInventory>("CInventory")
			.def_readwrite("max_belt"					,			&CInventory::m_iMaxBelt)
			.def_readwrite("max_weight"					,			&CInventory::m_fMaxWeight)
			.def_readwrite("take_dist"					,			&CInventory::m_fTakeDist)
			.def_readonly ("total_weight"				,			&CInventory::m_fTotalWeight)
			.property	  ("active_item"				,			&inventory_active_item)
			.property	  ("selected_item"				,			&inventory_selected_item)
			.property	  ("target"						,			&get_inventory_target)
			.property	  ("class_name"					,			&get_lua_class_name)
			.def		  ("to_belt"					,			&item_to_slot,   raw(_2))
			.def		  ("to_slot"					,			&item_to_slot,   raw(_2))
			.def		  ("to_ruck"					,			&item_to_ruck,   raw(_2))
			,
			class_<IInventoryBox>("IInventoryBox")			
			.def	  	 ("object"						,			&IInventoryBox::GetObjectByIndex)
			.def	  	 ("object"						,			&IInventoryBox::GetObjectByName)
			.def	  	 ("object_count"				,			&IInventoryBox::GetSize)
			.def		 ("empty"						,			&IInventoryBox::IsEmpty)
			,
			class_<CInventoryBox, bases<IInventoryBox, CGameObject>>("CInventoryBox")
			,
			class_<CInventoryContainer, bases<IInventoryBox, CInventoryItemObject>>("CInventoryContainer")
			.property	 ("cost"						,			&CInventoryContainer::Cost)
			.property	 ("weight"						,			&CInventoryContainer::Weight)
			.property	 ("is_opened"					,			&CInventoryContainer::IsOpened)
			.def		  ("open"						,			&CInventoryContainer::open)
			.def		  ("close"						,			&CInventoryContainer::close)
			
			
			,
			class_<CInventoryOwner>("CInventoryOwner")
			.def_readonly ("inventory"					,			&CInventoryOwner::m_inventory)
			.def_readonly ("talking"					,			&CInventoryOwner::m_bTalking)
			.def_readwrite("allow_talk"					,			&CInventoryOwner::m_bAllowTalk)
			.def_readwrite("allow_trade"				,			&CInventoryOwner::m_bAllowTrade)
			.def_readwrite("raw_money"					,			&CInventoryOwner::m_money)
			.property	  ("money"						,			&CInventoryOwner::get_money,				&set_io_money)			
			.property	  ("class_name"					,			&get_lua_class_name)			
			
		];
}

CParticlesObject* monster_play_particles(CBaseMonster *monster, LPCSTR name, const Fvector &position, const Fvector &dir, BOOL auto_remove, BOOL xformed)
{
	return monster->PlayParticles(name, position, dir, auto_remove, xformed);
}

void CMonsterScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CBaseMonster, bases<CInventoryOwner, CEntityAlive>>("CBaseMonster")
			.def_readwrite("agressive"					,			&CBaseMonster::m_bAggressive)
			.def_readwrite("angry"						,			&CBaseMonster::m_bAngry)
			.def_readwrite("damaged"					,			&CBaseMonster::m_bDamaged)
			.def_readwrite("grownlig"					,			&CBaseMonster::m_bGrowling)						
			.def_readwrite("run_turn_left"				,			&CBaseMonster::m_bRunTurnLeft)
			.def_readwrite("run_turn_right"				,			&CBaseMonster::m_bRunTurnRight)
			.def_readwrite("sleep"						,			&CBaseMonster::m_bSleep)
			.def_readwrite("state_invisible"			,			&CBaseMonster::state_invisible)			
		];
}


int curr_fire_mode(CWeaponMagazined *wpn) { return wpn->GetCurrentFireMode(); }



void COutfitScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CCustomOutfit, CInventoryItemObject>("CCustomOutfit")
			.def_readwrite("additional_inventory_weight"		,		&CCustomOutfit::m_additional_weight)
			.def_readwrite("additional_inventory_weight2"		,		&CCustomOutfit::m_additional_weight2)
			.def_readwrite("power_loss"							,		&CCustomOutfit::m_fPowerLoss)			
			.property("battery_charge"							,		&CCustomOutfit::GetBatteryCharge						,   &CCustomOutfit::SetBatteryCharge)
			.property("burn_protection"							,		&get_protection<ALife::eHitTypeBurn>					, 	&set_protection<ALife::eHitTypeBurn>)			
			.property("strike_protection"						,		&get_protection<ALife::eHitTypeStrike>					,	&set_protection<ALife::eHitTypeStrike >)
			.property("shock_protection"						,		&get_protection<ALife::eHitTypeShock>					,	&set_protection<ALife::eHitTypeShock>)
			.property("wound_protection"						,		&get_protection<ALife::eHitTypeWound>					,	&set_protection<ALife::eHitTypeWound>)
			.property("radiation_protection"					,		&get_protection<ALife::eHitTypeRadiation>				,	&set_protection<ALife::eHitTypeRadiation>)
			.property("telepatic_protection"					,		&get_protection<ALife::eHitTypeTelepatic>				,	&set_protection<ALife::eHitTypeTelepatic>)
			.property("chemical_burn_protection"				,		&get_protection<ALife::eHitTypeChemicalBurn>			,	&set_protection<ALife::eHitTypeChemicalBurn>)
			.property("explosion_protection"					,		&get_protection<ALife::eHitTypeExplosion>				,	&set_protection<ALife::eHitTypeExplosion>)
			.property("fire_wound_protection"					,		&get_protection<ALife::eHitTypeFireWound>				,	&set_protection<ALife::eHitTypeFireWound>)
			.property("wound_2_protection"						,		&get_protection<ALife::eHitTypeWound_2>					,	&set_protection<ALife::eHitTypeWound_2>)			
			.property("physic_strike_protection"				,		&get_protection<ALife::eHitTypePhysicStrike>			,	&set_protection<ALife::eHitTypePhysicStrike>)			
			.def("condition_ex"									,		&CCustomOutfit::GetConditionEx)
		];

}

int			get_fire_bone(CWeaponHUD *hud)  { return hud->FireBone();  }
const Fvector&	get_fire_point1 (CWeaponHUD *hud) { return hud->FirePoint(); }
const Fvector&	get_fire_point2 (CWeaponHUD *hud) { return hud->FirePoint2(); }
IRender_Visual* get_hud_visual(CWeaponHUD *hud)   { return hud->Visual(); }


SRotation& CWeaponScript::FireDeviation(CWeapon *wpn)
{
	return wpn->constDeviation;
}

luabind::object CWeaponScript::get_fire_modes(CWeaponMagazined *wpn)
{
   lua_State *L = wpn->lua_game_object()->lua_state();   
   luabind::object t = newtable(L);   
   auto &vector = wpn->m_aFireModes;
   int index = 1;
   for (auto it = vector.begin(); it != vector.end(); ++it, ++index)
	   t[index] = *it;

   return t;
}

void CWeaponScript::set_fire_modes(CWeaponMagazined *wpn, luabind::object const& t)
{
	if (LUA_TTABLE != t.type()) return;
	auto &vector = wpn->m_aFireModes;
	vector.clear();
	for (auto it = t.begin(); it != t.end(); ++it)
	{
		int m = object_cast<int>(*it);
		vector.push_back(m);
	}	
}

bool is_ammo_compatible(CWeapon *wpn, LPCSTR section)
{
	for (int i = 0; i < wpn->m_ammoTypes.size(); i++)
	{
		if (wpn->m_ammoTypes[i] == section) return true;
	}	
	return false;
}

LPCSTR get_ammo_type(CWeapon *wpn, int t)
{
	if (t < 0) 		
		t = (int) wpn->m_ammoType;
	if (t >= 0 && t < wpn->m_ammoTypes.size())
		return *wpn->m_ammoTypes[t];
	else
		return "";
}

LPCSTR charged_ammo(CWeapon *wpn)
{
	if (wpn->m_pAmmo)
		return wpn->m_pAmmo->cNameSect().c_str();
	return "";
}


void CWeaponScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CWeaponHUD>("CWeaponHUD")
			.property("fire_bone"						,			&get_fire_bone)
			.property("fire_point"						,			&get_fire_point1)
			.property("fire_point2"						,			&get_fire_point2)
			
			.property("visual"							,			&get_hud_visual)
			.def_readwrite("zoom_offset"				,			&CWeaponHUD::m_fZoomOffset)   //  ZoomOffset,  &CWeaponHUD::SetZoomOffset)
			.def_readwrite("zoom_rotate_x"				,			&CWeaponHUD::m_fZoomRotateX)
			.def_readwrite("zoom_rotate_y"				,			&CWeaponHUD::m_fZoomRotateY)

			.def_readonly("transform"					,			&CWeaponHUD::m_Transform)
			.def_readonly("visible"						,			&CWeaponHUD::m_bVisible)
			,

			class_<CWeapon,	CInventoryItemObject>		("CWeapon")
			// �� ����������������� ������ CHudItemObject:
			.property("state"							,			&CHudItemObject::GetState)
			.property("next_state"						,			&CHudItemObject::GetNextState)
			.property("ammo_charged"					,			&charged_ammo)
			// ============================================================================= //
			// ��������� ������ �������� �� ������
		//	.def_readwrite("cam_max_angle"				,			&CWeapon::camMaxAngle)
		//	.def_readwrite("cam_relax_speed"			,			&CWeapon::CameraRecoil().RelaxSpeed)
		//	.def_readwrite("cam_relax_speed_ai"			,			&CWeapon::camRelaxSpeed_AI)
		//	.def_readwrite("cam_dispersion"				,			&CWeapon::camDispersion)
		//	.def_readwrite("cam_dispersion_inc"			,			&CWeapon::camDispersionInc)
		//	.def_readwrite("cam_dispertion_frac"		,			&CWeapon::camDispertionFrac)
		//	.def_readwrite("cam_max_angle_horz"			,			&CWeapon::camMaxAngleHorz)
		//	.def_readwrite("cam_step_angle_horz"		,			&CWeapon::camStepAngleHorz)

			.def_readwrite("fire_dispersion_condition_factor"	,	&CWeapon::fireDispersionConditionFactor)			
			.def_readwrite("misfire_probability"		,			&CWeapon::misfireProbability)
			.def_readwrite("misfire_condition_k"		,			&CWeapon::misfireConditionK)	
			.def_readwrite("condition_shot_dec"			,			&CWeapon::conditionDecreasePerShot)

			.def_readwrite("ammo_mag_size"				,			&CWeapon::iMagazineSize)
			.def_readwrite("scope_dynamic_zoom"			,			&CWeapon::m_bScopeDynamicZoom)
			.def_readwrite("zoom_enabled"				,			&CWeapon::m_bZoomEnabled)
			.def_readwrite("zoom_factor"				,			&CWeapon::m_fZoomFactor)
			.def_readwrite("zoom_rotate_time"			,			&CWeapon::m_fZoomRotateTime)
			.def_readwrite("ironsight_zoom_factor"		,			&CWeapon::m_fIronSightZoomFactor)
			.def_readwrite("scope_zoom_factor"			,			&CWeapon::m_fScopeZoomFactor)
			// ���������� ��� ���������� ��������� ������� �� ��������:
			
			.def_readwrite("grenade_launcher_x"			,			&CWeapon::m_iGrenadeLauncherX)
			.def_readwrite("grenade_launcher_y"			,			&CWeapon::m_iGrenadeLauncherY)
			.def_readonly ("scope_mode"					,			&CWeapon::m_eScopeMode)
			.def_readwrite("scope_x"					,			&CWeapon::m_iScopeX)
			.def_readwrite("scope_y"					,			&CWeapon::m_iScopeY)
			.def_readwrite("silencer_x"					,			&CWeapon::m_iSilencerX)
			.def_readwrite("silencer_y"					,			&CWeapon::m_iSilencerY)

			.def_readonly("misfire"						,			&CWeapon::bMisfire)
			.def_readonly("zoom_mode"					,			&CWeapon::m_bZoomMode)
			

			.property("ammo_elapsed"					,			&CWeapon::GetAmmoElapsed, &CWeapon::SetAmmoElapsed)
			.property("const_deviation"					,			&CWeaponScript::FireDeviation)	// ���������� ��� �������� �� ������ (��� ���������������� ������).
			.property("scope_attached"					,			&CWeapon::IsScopeAttached)
			.property("scope_name"						,			&CWeapon::GetScopeNameScript)
			.def("get_ammo_current"						,			&CWeapon::GetAmmoCurrent)
			.def("get_ammo_type"						,			&get_ammo_type)
			.def("is_ammo_compat"						,			&is_ammo_compatible)			
			//.def("load_config"						,			&CWeapon::Load)
			.def("start_fire"							,			&CWeapon::FireStart)
			.def("stop_fire"							,			&CWeapon::FireEnd)
			.def("start_fire2"							,			&CWeapon::Fire2Start)			// ����� ����� - ������ �������? )
			.def("stop_fire2"							,			&CWeapon::Fire2End)
			.def("stop_shoothing"						,			&CWeapon::StopShooting)
			.def("get_particles_xform"					,			&CWeapon::get_ParticlesXFORM)
			.def("get_fire_point"						,			&CWeapon::get_CurrentFirePoint)
			.def("get_fire_point2"						,			&CWeapon::get_CurrentFirePoint2)


			,
			class_<CWeaponMagazined, CWeapon>			("CWeaponMagazined")
			.def_readonly("shot_num"					,			&CWeaponMagazined::m_iShotNum)
			.def_readwrite("queue_size"					,			&CWeaponMagazined::m_iQueueSize)
			.def_readwrite("shoot_effector_start"		,			&CWeaponMagazined::m_iShootEffectorStart)
			.def_readwrite("cur_fire_mode"				,			&CWeaponMagazined::m_iCurFireMode)			
			.property	  ("fire_mode"					,			&curr_fire_mode)
			.property	  ("fire_modes"					,			&get_fire_modes, &set_fire_modes)
			,
			class_<CWeaponMagazinedWGrenade,			CWeaponMagazined>("CWeaponMagazinedWGrenade")
			.def_readwrite("gren_mag_size"				,			&CWeaponMagazinedWGrenade::iMagazineSize2)			
			,
			class_<CMissile, CInventoryItemObject>		("CMissile")
			.def_readwrite("destroy_time"				,			&CMissile::m_dwDestroyTime)
			.def_readwrite("destroy_time_max"			,			&CMissile::m_dwDestroyTimeMax)			
			,
			class_<CWeaponAmmo, CInventoryItemObject>	("CWeaponAmmo")
			.property("current"							,			&CWeaponAmmo::get_box_curr,				&CWeaponAmmo::set_box_curr)
			.def_readwrite("size"						,			&CWeaponAmmo::m_boxSize)
		];
}
