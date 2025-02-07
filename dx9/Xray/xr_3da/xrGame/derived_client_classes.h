////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 20.10.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////
#include "script_export_space.h"
#include "CustomOutfit.h"
// alpet : � ���� ����� ��� ���������� ����������� � ������������� ������������, ���������� ��������� ������� - ������� �������������� ������� ������
// NOTE  : ��������� ������ ����� ������ ���������, ����� �������� ��� ������� 

class CAnomalyZoneScript
{
	static void	 set_zone_state(CCustomZone *obj, u32 new_state);
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CAnomalyZoneScript)
#undef script_type_list
#define script_type_list save_type_list(CAnomalyZoneScript)


class CInventoryScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CInventoryScript)
#undef script_type_list
#define script_type_list save_type_list(CInventoryScript)



class	CEatableItemScript 
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEatableItemScript)
#undef script_type_list
#define script_type_list save_type_list(CEatableItemScript)


class	CEntityScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEntityScript)
#undef script_type_list
#define script_type_list save_type_list(CEntityScript)

class	CMonsterScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CMonsterScript)
#undef script_type_list
#define script_type_list save_type_list(CMonsterScript)


class   COutfitScript
{
protected:
	template <ALife::EHitType idx> 
	static	float get_protection(CCustomOutfit *O)
	{	
		// u32 idx = sizeof(T) - 1;
		return O->m_HitTypeProtection[idx]; 
	}

	template <ALife::EHitType idx> 
	static	void set_protection(CCustomOutfit *O, float value)
	{	
		// u32 idx = sizeof(T) - 1;
		O->m_HitTypeProtection[idx] = value; 
	}


	DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(COutfitScript)
#undef script_type_list
#define script_type_list save_type_list(COutfitScript)


class	CWeaponScript
{
public:
	static SRotation&					FireDeviation				(CWeapon *wpn);
 	static luabind::object				get_fire_modes				(CWeaponMagazined *wpn);
	static void							set_fire_modes				(CWeaponMagazined *wpn, luabind::object const& t);
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponScript)
#undef script_type_list
#define script_type_list save_type_list(CWeaponScript)

