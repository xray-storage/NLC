#include "stdafx.h"
#include "ai_monster_utils.h"
#include "../../entity.h"
#include "../../ai_object_location.h"
#include "../../ai_space.h"
#include "../../level_graph.h"
#include "../../../xr_3da/SkeletonCustom.h"
#include "basemonster/base_monster.h"
// ���������, ��������� �� ������ entity �� ����
// ���������� ������� �������, ���� �� ��������� �� ����, ��� ����� ��� ����
Fvector get_valid_position(const CEntity *entity, const Fvector &actual_position) 
{
	if (
		ai().level_graph().valid_vertex_id(entity->ai_location().level_vertex_id()) &&
		ai().level_graph().valid_vertex_position(entity->Position()) && 
		ai().level_graph().inside(entity->ai_location().level_vertex_id(), entity->Position())
		)
		return			(actual_position);
	else
		return			(ai().level_graph().vertex_position(entity->ai_location().level_vertex()));
}

// ���������� true, ���� ������ entity ��������� �� ����
bool object_position_valid(const CEntity *entity)
{
	return				(
		ai().level_graph().valid_vertex_id(entity->ai_location().level_vertex_id()) &&
		ai().level_graph().valid_vertex_position(entity->Position()) && 
		ai().level_graph().inside(entity->ai_location().level_vertex_id(), entity->Position())
		);
}

Fvector get_bone_position	(CObject *object, LPCSTR bone_name)
{
	u16 bone_id			= smart_cast<CKinematics*>(object->Visual())->LL_BoneID				(bone_name);
	CBoneInstance &bone = smart_cast<CKinematics*>(object->Visual())->LL_GetBoneInstance	(bone_id);

	Fmatrix	global_transform;
	global_transform.mul	(object->XFORM(),bone.mTransform);

	return	(global_transform.c);
}

Fvector get_head_position(CObject *object) 
{
	pcstr bone_name		=	"bip01_head";
	if ( CBaseMonster* monster = smart_cast<CBaseMonster*>(object) )
	{
		bone_name		=	monster->get_head_bone_name();
	}

	return get_bone_position(object, bone_name);
}
