#include "stdafx.h"
#include "uistaticitem.h"
#include "hudmanager.h"

ref_geom		hGeom_fan = NULL;	

#pragma optimize("gyts", off)

void CreateUIGeom()
{
	hGeom_fan.create(FVF::F_TL, RCache.Vertex.Buffer(), 0);
}

void DestroyUIGeom()
{
	hGeom_fan = NULL;
}

ref_geom	GetUIGeom()
{
	return hGeom_fan;
}

CUIStaticItem::CUIStaticItem()
{    
	dwColor			= 0xffffffff;
	iTileX			= 1;
	iTileY			= 1;
	iRemX			= 0.0f;
	iRemY			= 0.0f;
	alpha_ref		= -1;
	hShader			= NULL;
#ifdef DEBUG
	dbg_tex_name = NULL;
#endif
}

CUIStaticItem::~CUIStaticItem()
{
}

void CUIStaticItem::CreateShader(LPCSTR tex, LPCSTR sh)
{	
	hShader.create(sh,tex);
	
#ifdef DEBUG
	dbg_tex_name = tex;
#endif
	uFlags.set(flValidRect, FALSE);
}

void CUIStaticItem::SetShader(const ref_shader& sh)
{
	hShader = sh;
}

void CUIStaticItem::Init(LPCSTR tex, LPCSTR sh, float left, float top, u32 align)
{
	uFlags.set(flValidRect, FALSE);

	CreateShader	(tex,sh);
	SetPos			(left,top);
	SetAlign		(align);
}

void CUIStaticItem::Render()
{
/*	VERIFY(g_bRendering);
	// ���������� ����������� ����� ������� CustomItem::Render() !!!
	FORCE_VERIFY(hShader);
	__try
	{
		RCache.set_Shader(hShader);
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!#EXCEPTION: in CUIStatic %p, shader assigned = %s ", (void*)this, hShader ? "yep" : "no" );
	}
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(alpha_ref);
	// convert&set pos
	Fvector2		bp;
	UI()->ClientToScreenScaled	(bp,float(iPos.x),float(iPos.y));
	bp.x						= (float)iFloor(bp.x);
	bp.y						= (float)iFloor(bp.y);

	// actual rendering
	u32							vOffset;
	Fvector2					pos;
	Fvector2					f_len;
	UI()->ClientToScreenScaled	(f_len, iVisRect.x2, iVisRect.y2 );

	int tile_x					= fis_zero(iRemX)?iTileX:iTileX+1;
	int tile_y					= fis_zero(iRemY)?iTileY:iTileY+1;
	int							x,y;
	if (!(tile_x&&tile_y))		return;
	// render
	FVF::TL* start_pv			= (FVF::TL*)RCache.Vertex.Lock	(8*tile_x*tile_y,hGeom_fan.stride(),vOffset);
	FVF::TL* pv					= start_pv;
	for (x=0; x<tile_x; ++x){
		for (y=0; y<tile_y; ++y){
			pos.set				(bp.x+f_len.x*x,bp.y+f_len.y*y);
			inherited::Render	(pv,pos,dwColor);
		}
	}
	std::ptrdiff_t p_cnt		= (pv-start_pv)/3;						VERIFY((pv-start_pv)<=8*tile_x*tile_y);
	RCache.Vertex.Unlock		(u32(pv-start_pv),hGeom_fan.stride());
	// set scissor
	Frect clip_rect				= {iPos.x,iPos.y,iPos.x+iVisRect.x2*iTileX+iRemX,iPos.y+iVisRect.y2*iTileY+iRemY};
	UI()->PushScissor			(clip_rect);
	// set geom
	RCache.set_Geometry			(hGeom_fan);
	if (p_cnt!=0)RCache.Render	(D3DPT_TRIANGLELIST,vOffset,u32(p_cnt));
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(0);
	UI()->PopScissor			();*/



	VERIFY						(g_bRendering);
	RCache.set_Shader			(hShader);
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(alpha_ref);

	u32		vOffset;
	FVF::TL* start_pv			= (FVF::TL*)RCache.Vertex.Lock	(8,hGeom_fan.stride(),vOffset);
	FVF::TL* pv					= start_pv;

	Fvector2					pos;
	UI()->ClientToScreenScaled	(pos, iPos.x, iPos.y);
	pos.x						= (float)iFloor(pos.x);
	pos.y						= (float)iFloor(pos.y);

	Fvector2		ts;
	CTexture* T		= RCache.get_ActiveTexture(0);
	ts.set			(float(T->get_Width()),float(T->get_Height()));

	if (!uFlags.test(flValidRect))	SetRect		(0,0,ts.x,ts.y);

	if (!uFlags.test(flValidOriginalRect) )
	{
		iOriginalRect.set(0,0,ts.x,ts.y);
		uFlags.set (flValidOriginalRect, TRUE);
	}

	Fvector2 LTp,RBp;
	Fvector2 LTt,RBt;
	//���������� �� ������ � ��������
	LTp.set						(pos);

	UI()->ClientToScreenScaled	(RBp, iVisRect.x2, iVisRect.y2);
	RBp.add						(pos);

	//���������� ����������
	LTt.set			( iOriginalRect.x1/ts.x, iOriginalRect.y1/ts.y);
	RBt.set			( iOriginalRect.x2/ts.x, iOriginalRect.y2/ts.y);

	float offset	= -0.5f;
	
	// clip poly
	sPoly2D			S; 
	S.resize		(4);

	LTp.x			+=offset;
	LTp.y			+=offset;
	RBp.x			+=offset;
	RBp.y			+=offset;

	S[0].set		(LTp.x,	LTp.y,	LTt.x,	LTt.y);	// LT
	S[1].set		(RBp.x,	LTp.y,	RBt.x,	LTt.y);	// RT
	S[2].set		(RBp.x,	RBp.y,	RBt.x,	RBt.y);	// RB
	S[3].set		(LTp.x,	RBp.y,	LTt.x,	RBt.y);	// LB
	
	sPoly2D D;
	sPoly2D* R		= NULL;
	R				= UI()->ScreenFrustum().ClipPoly(S,D);

	if (R && R->size())
	{
		for (u32 k=0; k<R->size()-2; ++k)
		{
			pv->set((*R)[0+0].pt.x, (*R)[0+0].pt.y, dwColor, (*R)[0+0].uv.x, (*R)[0+0].uv.y); pv++;
			pv->set((*R)[k+1].pt.x, (*R)[k+1].pt.y, dwColor, (*R)[k+1].uv.x, (*R)[k+1].uv.y); pv++;
			pv->set((*R)[k+2].pt.x, (*R)[k+2].pt.y, dwColor, (*R)[k+2].uv.x, (*R)[k+2].uv.y); pv++;
		}
	}


	u32 primCount = 0;
	_D3DPRIMITIVETYPE d3dPrimType = D3DPT_FORCE_DWORD;
	std::ptrdiff_t p_cnt = 0;

	p_cnt = pv - start_pv;
	VERIFY(u32(p_cnt) <= 8);

	RCache.Vertex.Unlock(u32(p_cnt), hGeom_fan.stride());
	RCache.set_Geometry(hGeom_fan);

	primCount = (u32)(p_cnt / 3);
	d3dPrimType = D3DPT_TRIANGLELIST;

	if (primCount > 0)
		RCache.Render(d3dPrimType, vOffset, primCount);

	if(alpha_ref!=-1)
		RCache.set_AlphaRef(0);
}

void CUIStaticItem::Render(float angle)
{
/*	VERIFY						(g_bRendering);
	// ���������� ����������� ����� ������� CustomItem::Render() !!!
	VERIFY						(hShader);
	RCache.set_Shader			(hShader);
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(alpha_ref);
	// convert&set pos
	Fvector2		bp_ns;
	bp_ns.set		(iPos);


	// actual rendering
	u32		vOffset;
	FVF::TL* start_pv			= (FVF::TL*)RCache.Vertex.Lock	(32,hGeom_fan.stride(),vOffset);
	FVF::TL* pv					= start_pv;
	inherited::Render			(pv,bp_ns,dwColor,angle);
	// unlock VB and Render it as triangle LIST
	std::ptrdiff_t p_cnt		= pv-start_pv;
	RCache.Vertex.Unlock		(u32(p_cnt),hGeom_fan.stride());
	RCache.set_Geometry	 		(hGeom_fan);
	if (p_cnt>2) RCache.Render	(D3DPT_TRIANGLEFAN,vOffset,u32(p_cnt-2));
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(0);*/



	VERIFY						(g_bRendering);

	RCache.set_Shader			(hShader);
	if(alpha_ref!=-1)
		RCache.set_AlphaRef(alpha_ref);

	u32		vOffset;
	FVF::TL* start_pv			= (FVF::TL*)RCache.Vertex.Lock	(32,hGeom_fan.stride(),vOffset);
	FVF::TL* pv					= start_pv;

	Fvector2		ts;
	Fvector2		hp;
	CTexture* T		= RCache.get_ActiveTexture(0);
	ts.set			(float(T->get_Width()),float(T->get_Height()));
	hp.set			(0.5f/ts.x,0.5f/ts.y);

	if (!uFlags.test(flValidRect))	SetRect		(0,0,ts.x,ts.y);

	if (!uFlags.test(flValidOriginalRect) )
	{
		iOriginalRect.set(0,0,ts.x,ts.y);
		uFlags.set (flValidOriginalRect, TRUE);
	}

	Fvector2							pivot,offset,SZ;
	SZ.set								(iVisRect.rb);


	float cosA							= _cos(angle);
	float sinA							= _sin(angle);

	// Rotation
	if( !uFlags.test(flValidHeadingPivot) )	
		pivot.set(iVisRect.x2/2.f, iVisRect.y2/2.f);
	else								
		pivot.set(iHeadingPivot.x, iHeadingPivot.y);

	offset.set							(iPos);
	offset.add							(iHeadingOffset);

	Fvector2							LTt,RBt;
	LTt.set								(iOriginalRect.x1/ts.x+hp.x, iOriginalRect.y1/ts.y+hp.y);
	RBt.set								(iOriginalRect.x2/ts.x+hp.x, iOriginalRect.y2/ts.y+hp.y);

	float kx =	UI()->get_current_kx();

	// clip poly
	sPoly2D								S; 
	S.resize							(4);
	// LT
	S[0].set		(0.f,0.f,LTt.x,LTt.y);
	S[0].rotate_pt	(pivot,cosA,sinA,kx);
	S[0].pt.add		(offset);

	// RT
	S[1].set		(SZ.x,0.f,RBt.x,LTt.y);
	S[1].rotate_pt	(pivot,cosA,sinA,kx);
	S[1].pt.add		(offset);
	// RB
	S[2].set		(SZ.x,SZ.y,RBt.x,RBt.y);
	S[2].rotate_pt	(pivot,cosA,sinA,kx);
	S[2].pt.add		(offset);
	// LB
	S[3].set		(0.f,SZ.y,LTt.x,RBt.y);
	S[3].rotate_pt	(pivot,cosA,sinA,kx);
	S[3].pt.add		(offset);

	for(int i=0; i<4;++i)
		UI()->ClientToScreenScaled		(S[i].pt);

	sPoly2D D;
	sPoly2D* R		= UI()->ScreenFrustum().ClipPoly(S,D);
	if (R&&R->size()){
		for (u32 k=0; k<R->size()-2; k++)
		{
			pv->set((*R)[0+0].pt.x, (*R)[0+0].pt.y, dwColor, (*R)[0+0].uv.x, (*R)[0+0].uv.y); pv++;
			pv->set((*R)[k+1].pt.x, (*R)[k+1].pt.y, dwColor, (*R)[k+1].uv.x, (*R)[k+1].uv.y); pv++;
			pv->set((*R)[k+2].pt.x, (*R)[k+2].pt.y, dwColor, (*R)[k+2].uv.x, (*R)[k+2].uv.y); pv++;
		}
	}

	u32 primCount = 0;
	_D3DPRIMITIVETYPE d3dPrimType = D3DPT_FORCE_DWORD;
	std::ptrdiff_t p_cnt = 0;

	p_cnt = pv - start_pv;
	VERIFY(u32(p_cnt) <= 32);

	RCache.Vertex.Unlock(u32(p_cnt), hGeom_fan.stride());
	RCache.set_Geometry(hGeom_fan);

	primCount = (u32)(p_cnt / 3);
	d3dPrimType = D3DPT_TRIANGLELIST;

	if (primCount > 0)
		RCache.Render(d3dPrimType, vOffset, primCount);

	if(alpha_ref!=-1)
		RCache.set_AlphaRef(alpha_ref);
}
