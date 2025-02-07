#pragma once

#include "../xr_3da/Effector.h"

// ���������� ����� �������
class CEffectorFall : public CEffectorCam
{
	float	fPower;
	float	fPhase;
public:
					CEffectorFall	(float power, float life_time=1);
	virtual BOOL	ProcessCam		(SCamEffectorInfo& info);
};

class CEffectorDOF : public CEffectorCam
{
	float			m_fPhase;
public:
					CEffectorDOF	(const Fvector4& dof);
	virtual BOOL	ProcessCam		(SCamEffectorInfo& info);
};
