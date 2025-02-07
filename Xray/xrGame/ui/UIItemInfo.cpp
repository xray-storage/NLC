#include "stdafx.h"

#include "uiiteminfo.h"
#include "uistatic.h"
#include "UIXmlInit.h"

#include "UIListWnd.h"
#include "UIProgressBar.h"
#include "UIScrollView.h"
#include "UI3dStatic.h"
#include "../string_table.h"
#include "../Inventory_Item.h"
#include "UIInventoryUtilities.h"
#include "../PhysicsShellHolder.h"
#include "UIWpnParams.h"
#include "ui_af_params.h"
#include "../Artifact.h"

CUIItemInfo::CUIItemInfo()
{
	//UIItemImageSize.set			(0.0f,0.0f);
	UIItem3dPos.set				(0.0f,0.0f);
	UICondProgresBar			= NULL;
	UICondition					= NULL;
	UICost						= NULL;
	UIWeight					= NULL;
	//UIItemImage					= NULL;
	UIItem3d					= NULL;
	UIDesc						= NULL;
	UIWpnParams					= NULL;
	UIArtefactParams			= NULL;
	UIName						= NULL;
	m_pInvItem					= NULL;
	m_b_force_drawing			= false;
	f_3d_static_time			= 0.f;
	f_3d_static_time_delta		= 0.f;
	b_3d_static_mouse_down		= false;
	b_3d_static_db_click		= false;
}

CUIItemInfo::~CUIItemInfo()
{
	xr_delete					(UIWpnParams);
	xr_delete					(UIArtefactParams);
}

void CUIItemInfo::Init(LPCSTR xml_name){

	CUIXml						uiXml;
	bool xml_result				= uiXml.Init(CONFIG_PATH, UI_PATH, xml_name);
	R_ASSERT2					(xml_result, "xml file not found");

	CUIXmlInit					xml_init;

	if(uiXml.NavigateToNode("main_frame",0))
	{
		Frect wnd_rect;
		wnd_rect.x1		= uiXml.ReadAttribFlt("main_frame", 0, "x", 0);
		wnd_rect.y1		= uiXml.ReadAttribFlt("main_frame", 0, "y", 0);

		wnd_rect.x2		= uiXml.ReadAttribFlt("main_frame", 0, "width", 0);
		wnd_rect.y2		= uiXml.ReadAttribFlt("main_frame", 0, "height", 0);
		
		inherited::Init(wnd_rect.x1, wnd_rect.y1, wnd_rect.x2, wnd_rect.y2);
	}

	if(uiXml.NavigateToNode("static_name",0))
	{
		UIName						= xr_new<CUIStatic>();	 
		AttachChild					(UIName);		
		UIName->SetAutoDelete		(true);
		xml_init.InitStatic			(uiXml, "static_name", 0,	UIName);
	}
	if(uiXml.NavigateToNode("static_weight",0))
	{
		UIWeight				= xr_new<CUIStatic>();	 
		AttachChild				(UIWeight);		
		UIWeight->SetAutoDelete(true);
		xml_init.InitStatic		(uiXml, "static_weight", 0,			UIWeight);
	}

	if(uiXml.NavigateToNode("static_cost",0))
	{
		UICost					= xr_new<CUIStatic>();	 
		AttachChild				(UICost);
		UICost->SetAutoDelete	(true);
		xml_init.InitStatic		(uiXml, "static_cost", 0,			UICost);
	}

	if(uiXml.NavigateToNode("static_condition",0))
	{
		UICondition					= xr_new<CUIStatic>();	 
		AttachChild					(UICondition);
		UICondition->SetAutoDelete	(true);
		xml_init.InitStatic			(uiXml, "static_condition", 0,		UICondition);
	}

	if(uiXml.NavigateToNode("condition_progress",0))
	{
		UICondProgresBar			= xr_new<CUIProgressBar>(); AttachChild(UICondProgresBar);UICondProgresBar->SetAutoDelete(true);
		xml_init.InitProgressBar	(uiXml, "condition_progress", 0, UICondProgresBar);
	}

	if(uiXml.NavigateToNode("descr_list",0))
	{
		UIWpnParams						= xr_new<CUIWpnParams>();
		UIArtefactParams				= xr_new<CUIArtefactParams>();
		UIWpnParams->InitFromXml		(uiXml);
		UIArtefactParams->InitFromXml	(uiXml);
		UIDesc							= xr_new<CUIScrollView>(); 
		AttachChild						(UIDesc);		
		UIDesc->SetAutoDelete			(true);
		m_desc_info.bShowDescrText		= !!uiXml.ReadAttribInt("descr_list",0,"only_text_info", 1);
		xml_init.InitScrollView			(uiXml, "descr_list", 0, UIDesc);
		xml_init.InitFont				(uiXml, "descr_list:font", 0, m_desc_info.uDescClr, m_desc_info.pDescFont);
	}	

	if (uiXml.NavigateToNode("image_static", 0))
	{	
/*		UIItemImage					= xr_new<CUIStatic>();	 
		AttachChild					(UIItemImage);	
		UIItemImage->SetAutoDelete	(true);
		xml_init.InitStatic			(uiXml, "image_static", 0, UIItemImage);
		UIItemImage->TextureAvailable(true);

		UIItemImage->TextureOff			();
		UIItemImage->ClipperOn			();
		UIItemImageSize.set				(UIItemImage->GetWidth(),UIItemImage->GetHeight());*/

		UIItem3d					= xr_new<CUI3dStatic>();
		AttachChild					(UIItem3d);
		UIItem3d->SetAutoDelete	(true);

		float x = uiXml.ReadAttribFlt("image_static", 0, "x");
		float y = uiXml.ReadAttribFlt("image_static", 0, "y");
		float width = uiXml.ReadAttribFlt("image_static", 0, "width");
		float height = uiXml.ReadAttribFlt("image_static", 0, "height");
		UIItem3d->Init(x, y, width, height);

		UIItem3dPos.set				(UIItem3d->GetWndPos());
	}

	xml_init.InitAutoStaticGroup	(uiXml, "auto", 0, this);
}

void CUIItemInfo::Init(float x, float y, float width, float height, LPCSTR xml_name)
{
	inherited::Init	(x, y, width, height);
    Init			(xml_name);
}

bool				IsGameTypeSingle();

void CUIItemInfo::InitItem(CInventoryItem* pInvItem)
{
	m_pInvItem				= pInvItem;
	if(!m_pInvItem)			return;

	string256				str;
	if(UIName)
	{
		UIName->SetText		(pInvItem->Name());
	}
	if(UIWeight)
	{
		sprintf_s				(str, "%3.2f kg", pInvItem->Weight());
		UIWeight->SetText	(str);
	}
	if( UICost && IsGameTypeSingle() )
	{
		sprintf_s				(str, "%d %s", pInvItem->Cost(),*CStringTable().translate("ui_st_money_regional"));		// will be owerwritten in multiplayer
		UICost->SetText		(str);
	}

	if(UICondProgresBar)
	{
		float cond							= pInvItem->GetConditionToShow();
		UICondProgresBar->Show				(true);
		UICondProgresBar->SetProgressPos	( cond*100.0f+1.0f-EPS );
	}

	if(UIDesc)
	{
		UIDesc->Clear						();
		VERIFY								(0==UIDesc->GetSize());
		TryAddWpnInfo						(pInvItem->object().cNameSect());
		TryAddArtefactInfo					(pInvItem->object().cNameSect());
		if(m_desc_info.bShowDescrText)
		{
			CUIStatic* pItem					= xr_new<CUIStatic>();
			pItem->SetTextColor					(m_desc_info.uDescClr);
			pItem->SetFont						(m_desc_info.pDescFont);
			pItem->SetWidth						(UIDesc->GetDesiredChildWidth());
			pItem->SetTextComplexMode			(true);
			pItem->SetText						(*pInvItem->ItemDescription());
			pItem->AdjustHeightToText			();
			UIDesc->AddWindow					(pItem, true);
		}
		UIDesc->ScrollToBegin				();
	}
/*	if(UIItemImage)
	{
		// Загружаем картинку
		UIItemImage->SetShader				(pInvItem->m_icon_params.get_shader());

		Frect rect = pInvItem->m_icon_params.original_rect();
		int iGridWidth						= pInvItem->GetGridWidth();
		int iGridHeight						= pInvItem->GetGridHeight();
		UIItemImage->GetUIStaticItem().SetOriginalRect(rect);
		UIItemImage->TextureOn				();
		UIItemImage->ClipperOn				();
		UIItemImage->SetStretchTexture		(true);
		Frect v_r							= {	0.0f, 0.0f, 
												rect.width(), rect.height()};
		if(UI()->is_16_9_mode())
			v_r.x2 /= 1.328f;

		UIItemImage->GetUIStaticItem().SetRect	(v_r);
		UIItemImage->SetWidth					(_min(v_r.width(),	UIItemImageSize.x));
		UIItemImage->SetHeight					(_min(v_r.height(),	UIItemImageSize.y));
	}*/

	if( UIItem3d )
	{
		Fvector2 b_p = UIItem3dPos;
		Frect rect = pInvItem->m_icon_params.original_rect();
		Frect v_r;
		if ( rect.width() > rect.height() )
		{
			v_r = { 0.f, 0.f, rect.width(), rect.width() };
			b_p.x = b_p.x - rect.width() / 2.f;
			b_p.y = b_p.y - rect.width() / 2.f;
		}
		else
		{
			v_r = { 0.f, 0.f, rect.height(), rect.height() };
			b_p.x = b_p.x - rect.height() / 2.f;
			b_p.y = b_p.y - rect.height() / 2.f;
		}

		UIItem3d->SetWndPos(b_p);
		UIItem3d->SetWndSize( Fvector2().set(v_r.width(), v_r.height()) );
		UIItem3d->SetGameObject(pInvItem->object().Visual(), false);

		b_3d_static_db_click	= false;
		Fvector4 item_size		= pInvItem->m_icon_3d_static_size;
		float scale				= item_size.w * 1.15f;
		CArtefact* pArtefact	= smart_cast<CArtefact*>(pInvItem);
		if ( pArtefact )		scale *= 2.f;
		UIItem3d->SetScale		( scale );
		UIItem3d->SetRotate		( item_size.x, item_size.y, item_size.z );
	}
}

void CUIItemInfo::TryAddWpnInfo (const shared_str& wpn_section){
	if (UIWpnParams->Check(wpn_section))
	{
		UIWpnParams->SetInfo(&m_pInvItem->object());
		UIDesc->AddWindow(UIWpnParams,false);
	}
}

void CUIItemInfo::TryAddArtefactInfo	(const shared_str& af_section)
{
	if (UIArtefactParams->Check(af_section))
	{
		UIArtefactParams->SetInfo(&m_pInvItem->object());
		UIDesc->AddWindow(UIArtefactParams, false);
	}
}

void CUIItemInfo::Draw()
{
	if (m_pInvItem || m_b_force_drawing)
	{
		if ( UIItem3d && !b_3d_static_mouse_down )
			UIItem3d->SetRotateY( 0.35f * f_3d_static_time );

		inherited::Draw();
	}
}

void CUIItemInfo::Update	()
{
	if ( !b_3d_static_mouse_down )
		f_3d_static_time = Device.fTimeGlobal - f_3d_static_time_delta;
	else
		f_3d_static_time_delta = Device.fTimeGlobal - f_3d_static_time;

	inherited::Update();
}

bool CUIItemInfo::OnDbClick		()
{
	if ( !b_3d_static_db_click )
	{
		UIItem3d->SetScale( UIItem3d->GetScale() * 2.f );
		b_3d_static_db_click = true;
	}
	else
	{
		UIItem3d->SetScale( UIItem3d->GetScale() / 2.f );
		b_3d_static_db_click = false;
	}
	return inherited::OnDbClick();
}

bool CUIItemInfo::OnMouseDown	(int mouse_btn)
{
	if ( mouse_btn == MOUSE_2 )
	{
		if ( !b_3d_static_mouse_down )
			b_3d_static_mouse_down = true;
		else
			b_3d_static_mouse_down = false;
	}
	return inherited::OnMouseDown(mouse_btn);
}
