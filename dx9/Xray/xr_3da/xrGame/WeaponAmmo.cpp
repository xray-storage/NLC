#include "stdafx.h"
#include "weaponammo.h"
#include "PhysicsShell.h"
#include "xrserver_objects_alife_items.h"
#include "Actor_Flags.h"
#include "inventory.h"
#include "weapon.h"
#include "level_bullet_manager.h"
#include "ai_space.h"
#include "../GameMtlLib.h"
#include "level.h"
#include "string_table.h"

#define BULLET_MANAGER_SECTION "bullet_manager"

IC void verify_ptr(const void *ptr)
{
	R_ASSERT((UINT_PTR) ptr > 0x10000);
}

CCartridge::CCartridge() 
{
	m_flags.assign			(cfTracer | cfRicochet);
	m_ammoSect = NULL;
	m_kDist = m_kDisp = m_kHit = m_kImpulse = m_kPierce = 1.f;
	m_kAP = 0.0f;
	m_kAirRes = 0.0f;
	m_buckShot = 1;
	m_impair = 1.f;

	bullet_material_idx = u16(-1);
}

void CCartridge::Load(LPCSTR section, u8 LocalAmmoType) 
{
	m_ammoSect				= section;
	m_LocalAmmoType			= LocalAmmoType;
	m_kDist					= pSettings->r_float(section, "k_dist");
	m_kDisp					= pSettings->r_float(section, "k_disp");
	m_kHit					= pSettings->r_float(section, "k_hit");
	m_kImpulse				= pSettings->r_float(section, "k_impulse");
	m_kPierce				= pSettings->r_float(section, "k_pierce");
	m_kAP					= READ_IF_EXISTS(pSettings, r_float, section, "k_ap", 0.0f);
	m_u8ColorID				= READ_IF_EXISTS(pSettings, r_u8, section, "tracer_color_ID", 0);
	
	if (pSettings->line_exist(section, "k_air_resistance"))
		m_kAirRes				=  pSettings->r_float(section, "k_air_resistance");
	else
		m_kAirRes				= pSettings->r_float(BULLET_MANAGER_SECTION, "air_resistance_k");

	m_flags.set				(cfTracer, pSettings->r_bool(section, "tracer"));
	m_buckShot				= pSettings->r_s32(section, "buck_shot");
	m_impair				= pSettings->r_float(section, "impair");
	fWallmarkSize			= pSettings->r_float(section, "wm_size");

	m_flags.set				(cfCanBeUnlimited | cfRicochet, TRUE);
	if(pSettings->line_exist(section,"can_be_unlimited"))
		m_flags.set(cfCanBeUnlimited, pSettings->r_bool(section, "can_be_unlimited"));

	if(pSettings->line_exist(section,"explosive"))
		m_flags.set			(cfExplosive, pSettings->r_bool(section, "explosive"));

	bullet_material_idx		=  GMLib.GetMaterialIdx(WEAPON_MATERIAL_NAME);
	VERIFY	(u16(-1)!=bullet_material_idx);
	VERIFY	(fWallmarkSize>0);

	m_InvShortName			= CStringTable().translate( pSettings->r_string(section, "inv_name_short"));
}

CWeaponAmmo::CWeaponAmmo(void)
{
	m_boxSize				= 0;
	m_pBoxCurr				= xr_new<CComplexVarInt>();
	set_box_curr			(0);
	m_weight				= .2f;
	m_flags.set				(Fbelt, TRUE);	
}

CWeaponAmmo::~CWeaponAmmo(void)
{
	xr_delete(m_pBoxCurr);
}

void CWeaponAmmo::Load(LPCSTR section) 
{
	inherited::Load			(section);

	m_kDist					= pSettings->r_float(section, "k_dist");
	m_kDisp					= pSettings->r_float(section, "k_disp");
	m_kHit					= pSettings->r_float(section, "k_hit");
	m_kImpulse				= pSettings->r_float(section, "k_impulse");
	m_kPierce				= pSettings->r_float(section, "k_pierce");
	m_kAP					= READ_IF_EXISTS(pSettings, r_float, section, "k_ap", 0.0f);
	m_u8ColorID				= READ_IF_EXISTS(pSettings, r_u8, section, "tracer_color_ID", 0);

	if (pSettings->line_exist(section, "k_air_resistance"))
		m_kAirRes				=  pSettings->r_float(section, "k_air_resistance");
	else
		m_kAirRes				= pSettings->r_float(BULLET_MANAGER_SECTION, "air_resistance_k");
	m_tracer				= !!pSettings->r_bool(section, "tracer");
	m_buckShot				= pSettings->r_s32(section, "buck_shot");
	m_impair				= pSettings->r_float(section, "impair");
	fWallmarkSize			= pSettings->r_float(section,"wm_size");
	R_ASSERT				(fWallmarkSize>0);

	m_boxSize				= (u16)pSettings->r_s32(section, "box_size");
	set_box_curr			(m_boxSize);	
}

BOOL CWeaponAmmo::net_Spawn(CSE_Abstract* DC) 
{
	BOOL bResult			= inherited::net_Spawn	(DC);
	CSE_Abstract	*e		= (CSE_Abstract*)(DC);
	CSE_ALifeItemAmmo* l_pW	= smart_cast<CSE_ALifeItemAmmo*>(e);
	u16  curr				= l_pW->a_elapsed;	
	if(curr > m_boxSize)
		l_pW->a_elapsed		= curr = m_boxSize;
	set_box_curr			(curr);	
	return					bResult;
}

void CWeaponAmmo::net_Destroy() 
{
	inherited::net_Destroy	();
}

void CWeaponAmmo::OnH_B_Chield() 
{
	inherited::OnH_B_Chield	();
}

void CWeaponAmmo::OnH_B_Independent(bool just_before_destroy) 
{
	if(!Useful()) {
		
		if (Local()){
			DestroyObject	();
		}
		m_ready_to_destroy	= true;
	}
	inherited::OnH_B_Independent(just_before_destroy);
}


bool CWeaponAmmo::Useful() const
{
	// ���� IItem ��� �� ��������� �������������, ������� true
	return !!get_box_curr();
}
/*
s32 CWeaponAmmo::Sort(PIItem pIItem) 
{
	// ���� ����� ���������� IItem ����� this - ������� 1, ����
	// ����� - -1. ���� ����� �� 0.
	CWeaponAmmo *l_pA = smart_cast<CWeaponAmmo*>(pIItem);
	if(!l_pA) return 0;
	if(xr_strcmp(cNameSect(), l_pA->cNameSect())) return 0;
	if(m_boxCurr <= l_pA->m_boxCurr) return 1;
	else return -1;
}
*/
bool CWeaponAmmo::Get(CCartridge &cartridge) 
{
	if(!get_box_curr()) return false;
	cartridge.m_ammoSect = cNameSect();
	cartridge.m_kDist = m_kDist;
	cartridge.m_kDisp = m_kDisp;
	cartridge.m_kHit = m_kHit;
	cartridge.m_kImpulse = m_kImpulse;
	cartridge.m_kPierce = m_kPierce;
	cartridge.m_kAP = m_kAP;
	cartridge.m_kAirRes = m_kAirRes;
	cartridge.m_u8ColorID = m_u8ColorID;
	cartridge.m_flags.set(CCartridge::cfTracer ,m_tracer);
	cartridge.m_buckShot = m_buckShot;
	cartridge.m_impair = m_impair;
	cartridge.fWallmarkSize = fWallmarkSize;
	cartridge.bullet_material_idx = GMLib.GetMaterialIdx(WEAPON_MATERIAL_NAME);
	cartridge.m_InvShortName = NameShort();
	set_box_curr (get_box_curr() - 1);
	if(m_pCurrentInventory)
		m_pCurrentInventory->InvalidateState();
	return true;
}

void CWeaponAmmo::renderable_Render() 
{
	if(!m_ready_to_destroy)
		inherited::renderable_Render();
}

void CWeaponAmmo::UpdateCL() 
{
	VERIFY2								(_valid(renderable.xform),*cName());
	inherited::UpdateCL	();
	VERIFY2								(_valid(renderable.xform),*cName());
	
	if(!IsGameTypeSingle())
		make_Interpolation	();

	VERIFY2								(_valid(renderable.xform),*cName());

}

void CWeaponAmmo::net_Export(NET_Packet& P) 
{
	inherited::net_Export	(P);	
	P.w_u16					(get_box_curr());
}

void CWeaponAmmo::net_Import(NET_Packet& P) 
{
	inherited::net_Import	(P);
	u16  v;
	P.r_u16					(v);
	set_box_curr			(v);
}

CInventoryItem *CWeaponAmmo::can_make_killing	(const CInventory *inventory) const
{
	VERIFY					(inventory);

	TIItemContainer::const_iterator	I = inventory->m_all.begin();
	TIItemContainer::const_iterator	E = inventory->m_all.end();
	for ( ; I != E; ++I) {
		CWeapon		*weapon = smart_cast<CWeapon*>(*I);
		if (!weapon)
			continue;
		xr_vector<shared_str>::const_iterator	i = std::find(weapon->m_ammoTypes.begin(),weapon->m_ammoTypes.end(),cNameSect());
		if (i != weapon->m_ammoTypes.end())
			return			(weapon);
	}

	return					(0);
}

float CWeaponAmmo::Weight() const
{
	float res = inherited::Weight();

	res *= (float)get_box_curr()/(float)m_boxSize;

	return res;
}

u32 CWeaponAmmo::Cost() const
{
	float res = (float) m_cost;		
	res *= (float)get_box_curr()/(float)m_boxSize;
	// return (u32)roundf(res); // VC18 only
	return (u32)ceil(res + 0.5);
}

u16 CWeaponAmmo::get_box_curr() const
{	
	verify_ptr(this);
	int curr = 0;
	if ( (UINT_PTR)m_pBoxCurr > 0x40000 )
		__try {
		curr = *m_pBoxCurr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)	{
		Msg("!#EXCEPTION: in CWeaponAmmo::get_box_curr, m_pBoxCurr = 0x%p", (void*)m_pBoxCurr);
		curr = 0;
	}
	else
		__asm nop;
	clamp<int>(curr, 0, m_boxSize);
	return (u16)curr;
}

void CWeaponAmmo::set_box_curr(u16 value)
{
	verify_ptr(this);
	clamp<u16>(value, 0, m_boxSize);
	if (m_pBoxCurr)
		*m_pBoxCurr = value;

	CSE_ALifeDynamicObject *sobj = alife_object();
	CSE_ALifeItemAmmo *ammo = sobj ? sobj->cast_item_ammo() : NULL;  
	if (ammo) 
		ammo->a_elapsed = value;
}
