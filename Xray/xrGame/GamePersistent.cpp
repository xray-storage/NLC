#include "pch_script.h"
#include "gamepersistent.h"
#include "../xr_3da/fmesh.h"
#include "../xr_3da/xr_ioconsole.h"
#include "../xr_3da/GameMtlLib.h"
#include "../xr_3da/skeletoncustom.h"
#include "profiler.h"
#include "MainMenu.h"
#include "UICursor.h"
#include "game_base_space.h"
#include "level.h"
#include "ParticlesObject.h"
#include "actor.h"
#include "game_base_space.h"
#include "weaponhud.h"
#include "stalker_animation_data_storage.h"
#include "stalker_velocity_holder.h"
#include "../xr_3da/x_ray.h"
#include "../xr_3da/resourcemanager.h"
#include "ActorEffector.h"
#include "actor.h"
#include "script_engine.h"
#include "string_table.h"
#include "../xr_3da/xrSASH.h"

#pragma optimize("gyts", on)

#ifndef MASTER_GOLD
#	include "custommonster.h"
#endif // MASTER_GOLD

#ifndef _EDITOR
#	include "ai_debug.h"
#endif // _EDITOR

#ifdef DEBUG_MEMORY_MANAGER
	static	void *	ode_alloc	(size_t size)								{ return Memory.mem_alloc(size,"ODE");			}
	static	void *	ode_realloc	(void *ptr, size_t oldsize, size_t newsize)	{ return Memory.mem_realloc(ptr,newsize,"ODE");	}
	static	void	ode_free	(void *ptr, size_t size)					{ return xr_free(ptr);							}
#else // DEBUG_MEMORY_MANAGER
	static	void *	ode_alloc	(size_t size)								{ return xr_malloc(size);			}
	static	void *	ode_realloc	(void *ptr, size_t oldsize, size_t newsize)	{ return xr_realloc(ptr,newsize);	}
	static	void	ode_free	(void *ptr, size_t size)					{ return xr_free(ptr);				}
#endif // DEBUG_MEMORY_MANAGER

CGamePersistent::CGamePersistent(void)
{
	m_bPickableDOF				= false;
	m_game_params.m_e_game_type	= GAME_ANY;
	ambient_sound_next_time		= 0;
	ambient_effect_next_time	= 0;
	ambient_effect_stop_time	= 0;
	ambient_particles			= 0;

	m_pUI_core					= NULL;
	m_pMainMenu					= NULL;
	m_intro						= NULL;
	m_intro_event.bind			(this, &CGamePersistent::start_logo_intro);
#ifdef DEBUG
	m_frame_counter				= 0;
	m_last_stats_frame			= u32(-2);
#endif
	// 
	dSetAllocHandler			(ode_alloc		);
	dSetReallocHandler			(ode_realloc	);
	dSetFreeHandler				(ode_free		);

	// 
	BOOL	bDemoMode	= (0!=strstr(Core.Params,"-demomode "));
	if (bDemoMode)
	{
		string256	fname;
		LPCSTR		name	=	strstr(Core.Params,"-demomode ") + 10;
		sscanf				(name,"%s",fname);
		R_ASSERT2			(fname[0],"Missing filename for 'demomode'");
		Msg					("- playing in demo mode '%s'",fname);
		pDemoFile			=	FS.r_open	(fname);
		Device.seqFrame.Add	(this);
		eDemoStart			=	Engine.Event.Handler_Attach("GAME:demo",this);	
		uTime2Change		=	0;
	} else {
		pDemoFile			=	NULL;
		eDemoStart			=	NULL;
	}

	CWeaponHUD::CreateSharedContainer();

	eQuickLoad					= Engine.Event.Handler_Attach("Game:QuickLoad",this);
	Fvector3* DofValue		= Console->GetFVectorPtr("r2_dof");
	SetBaseDof				(*DofValue);

}

CGamePersistent::~CGamePersistent(void)
{	
	CWeaponHUD::DestroySharedContainer();
	FS.r_close					(pDemoFile);
	Device.seqFrame.Remove		(this);
	Engine.Event.Handler_Detach	(eDemoStart,this);
	Engine.Event.Handler_Detach	(eQuickLoad,this);
}

void CGamePersistent::RegisterModel(IRender_Visual* V)
{
	// Check types
	switch (V->Type){
	case MT_SKELETON_ANIM:
	case MT_SKELETON_RIGID:{
		u16 def_idx		= GMLib.GetMaterialIdx("default_object");
		R_ASSERT2		(GMLib.GetMaterialByIdx(def_idx)->Flags.is(SGameMtl::flDynamic),"'default_object' - must be dynamic");
		CKinematics* K	= smart_cast<CKinematics*>(V); VERIFY(K);
		int cnt = K->LL_BoneCount();
		for (u16 k=0; k<cnt; k++){
			CBoneData& bd	= K->LL_GetData(k); 
			if (*(bd.game_mtl_name)){
				bd.game_mtl_idx	= GMLib.GetMaterialIdx(*bd.game_mtl_name);
				R_ASSERT2(GMLib.GetMaterialByIdx(bd.game_mtl_idx)->Flags.is(SGameMtl::flDynamic),"Required dynamic game material");
			}else{
				bd.game_mtl_idx	= def_idx;
			}
		}
	}break;
	}
}

extern void clean_game_globals	();
extern void init_game_globals	();

volatile ULONG g_threads = 0;

void mtParallel_1(void *)
{
  InterlockedIncrement(&g_threads);  
  Log(*CStringTable().translate("Hello world!"));
  InterlockedDecrement(&g_threads);
}

void mtParallel_2(void *)
{
  InterlockedIncrement(&g_threads);
  Log("* Init game globals...");
  init_game_globals			();
  InterlockedDecrement(&g_threads);
}

void CGamePersistent::OnAppStart()
{
	thread_spawn(mtParallel_1, "mtParallelInit_1", 0, 0);
	thread_spawn(mtParallel_2, "mtParallelInit_2", 0, 0);
	Device.Resources->DeferredLoad(TRUE);
	// load game materials
	GMLib.Load					();
	Log("* __super::OnAppStart...");
	__super::OnAppStart			();
	m_pUI_core					= xr_new<ui_core>();
	Log("* creating CMainMenu object");
	m_pMainMenu					= xr_new<CMainMenu>();
	while (g_threads > 0) Sleep(10);
}


void CGamePersistent::OnAppEnd	()
{
	if(m_pMainMenu->IsActive())
		m_pMainMenu->Activate(false);

	xr_delete					(m_pMainMenu);
	xr_delete					(m_pUI_core);

	__super::OnAppEnd			();

	clean_game_globals			();

	GMLib.Unload				();

}


#pragma optimize("gyts", off)

void CGamePersistent::Start		(LPCSTR op)
{
	__super::Start				(op);
	m_intro_event.bind			(this,&CGamePersistent::start_game_intro);
}

void CGamePersistent::Disconnect()
{
	CWeaponHUD::CleanSharedContainer();

	// destroy ambient particles
	CParticlesObject::Destroy(ambient_particles);

	__super::Disconnect			();
	// stop all played emitters
	::Sound->stop_emitters		();
	m_game_params.m_e_game_type	= GAME_ANY;
}

#include "xr_level_controller.h"

void CGamePersistent::OnGameStart()
{
	__super::OnGameStart		();
	
	UpdateGameType				();

}

void CGamePersistent::UpdateGameType			()
{
	__super::UpdateGameType		();
	//  [7/11/2005]
	if (!xr_strcmp(m_game_params.m_game_type, "single")) m_game_params.m_e_game_type = GAME_SINGLE;
	else
		if (!xr_strcmp(m_game_params.m_game_type, "deathmatch")) m_game_params.m_e_game_type = GAME_DEATHMATCH;
		else
			if (!xr_strcmp(m_game_params.m_game_type, "teamdeathmatch")) m_game_params.m_e_game_type = GAME_TEAMDEATHMATCH;
			else
				if (!xr_strcmp(m_game_params.m_game_type, "artefacthunt")) m_game_params.m_e_game_type = GAME_ARTEFACTHUNT;
				else m_game_params.m_e_game_type = GAME_ANY;
	//  [7/11/2005]

	if(	m_game_params.m_e_game_type == GAME_DEATHMATCH ||
		m_game_params.m_e_game_type == GAME_TEAMDEATHMATCH ||
		m_game_params.m_e_game_type == GAME_ARTEFACTHUNT
		)
	g_current_keygroup = _mp;
	else
	g_current_keygroup = _sp;
}

void CGamePersistent::OnGameEnd	()
{
	__super::OnGameEnd					();

	xr_delete							(g_stalker_animation_data_storage);
	xr_delete							(g_stalker_velocity_holder);

	CWeaponHUD::CleanSharedContainer	();
}

void CGamePersistent::WeathersUpdate()
{
	if (g_pGameLevel && !g_dedicated_server)
	{
		CActor* actor				= smart_cast<CActor*>(Level().CurrentViewEntity());
		BOOL bIndoor				= TRUE;
		if (actor) bIndoor			= actor->renderable_ROS()->get_luminocity_hemi()<0.05f;

		int data_set				= (Random.randF()<(1.f-Environment().CurrentEnv.weight))?0:1; 
		CEnvDescriptor* _env		= Environment().Current[data_set]; VERIFY(_env);
		// if (!_env) Msg("!ERROR: Environment().Current[%d] == NULL", data_set); // alpet: ��� �������� ���������, ��� ������ level.on_frame � ��������
		CEnvAmbient* env_amb		= _env ? _env->env_ambient : NULL; // _env->env_ambient; // 
		if (env_amb){
			// start sound
			if (Device.dwTimeGlobal > ambient_sound_next_time){
				ref_sound* snd			= env_amb->get_rnd_sound();
				ambient_sound_next_time	= Device.dwTimeGlobal + env_amb->get_rnd_sound_time();
				if (snd){
					Fvector	pos;
					float	angle		= ::Random.randF(PI_MUL_2);
					pos.x				= _cos(angle);
					pos.y				= 0;
					pos.z				= _sin(angle);
					pos.normalize		().mul(env_amb->get_rnd_sound_dist()).add(Device.vCameraPosition);
					pos.y				+= 10.f;
					snd->play_at_pos	(0,pos);
				}
			}

			// start effect
			if ((FALSE==bIndoor) && (0==ambient_particles) && Device.dwTimeGlobal>ambient_effect_next_time){
				CEnvAmbient::SEffect* eff			= env_amb->get_rnd_effect(); 
				if (eff){
					Environment().wind_gust_factor	= eff->wind_gust_factor;
					ambient_effect_next_time		= Device.dwTimeGlobal + env_amb->get_rnd_effect_time();
					ambient_effect_stop_time		= Device.dwTimeGlobal + eff->life_time;
					ambient_particles				= CParticlesObject::Create(eff->particles.c_str(),FALSE,false);
					Fvector pos; pos.add			(Device.vCameraPosition,eff->offset); 
					ambient_particles->play_at_pos	(pos);
					if (eff->sound._handle())		eff->sound.play_at_pos(0,pos);
				}
			}
		}
		// stop if time exceed or indoor
		if (bIndoor || Device.dwTimeGlobal>=ambient_effect_stop_time){
			if (ambient_particles)					ambient_particles->Stop();
			Environment().wind_gust_factor			= 0.f;
		}
		// if particles not playing - destroy
		if (ambient_particles&&!ambient_particles->IsPlaying())
			CParticlesObject::Destroy(ambient_particles);
	}
}

#include "UI/UIGameTutorial.h"

void CGamePersistent::start_logo_intro		()
{
	if ((0!=strstr(Core.Params,"-nointro")) || g_SASH.IsRunning())
	{
		m_intro_event			= 0;
		Console->Show			();
		Console->Execute		("main_menu on");
		return;
	}
	if (Device.dwPrecacheFrame==0)
	{
		m_intro_event.bind		(this,&CGamePersistent::update_logo_intro);
		if (!g_dedicated_server && 0==xr_strlen(m_game_params.m_game_or_spawn) && NULL==g_pGameLevel)
		{
			VERIFY				(NULL==m_intro);
			m_intro				= xr_new<CUISequencer>();
			m_intro->Start		("intro_logo");
			Console->Hide		();
		}
	}
}
void CGamePersistent::update_logo_intro			()
{
	if(m_intro && (false==m_intro->IsActive())){
		m_intro_event			= 0;
		xr_delete				(m_intro);
		Console->Execute		("main_menu on");
	}
}

void CGamePersistent::start_game_intro		()
{
	if ((0!=strstr(Core.Params,"-nointro")) || g_SASH.IsRunning())
	{
		m_intro_event			= 0;
		return;
	}

	if (g_pGameLevel && g_pGameLevel->bReady && Device.dwPrecacheFrame<=2){
		m_intro_event.bind		(this,&CGamePersistent::update_game_intro);
		if (0==stricmp(m_game_params.m_new_or_load,"new")){
			VERIFY				(NULL==m_intro);
			m_intro				= xr_new<CUISequencer>();
			m_intro->Start		("intro_game");
#ifdef DEBUG
			Log("Intro start",Device.dwFrame);
#endif // #ifdef DEBUG
		}
	}
}
void CGamePersistent::update_game_intro			()
{
	if(m_intro && (false==m_intro->IsActive())){
		xr_delete				(m_intro);
		m_intro_event			= 0;
	}
}
#include "holder_custom.h"
extern CUISequencer * g_tutorial;
extern CUISequencer * g_tutorial2;

void CGamePersistent::OnFrame	()
{
	if(g_tutorial2){ 
		g_tutorial2->Destroy	();
		xr_delete				(g_tutorial2);
	}

	if(g_tutorial && !g_tutorial->IsActive()){
		xr_delete(g_tutorial);
	}

#ifdef DEBUG
	++m_frame_counter;
#endif
	if (!g_dedicated_server && !m_intro_event.empty())	m_intro_event();

	if( !m_pMainMenu->IsActive() )
		m_pMainMenu->DestroyInternal(false);

	if(!g_pGameLevel)			return;
	if(!g_pGameLevel->bReady)	return;

	if(Device.Paused()){
#ifndef MASTER_GOLD
		if (Level().CurrentViewEntity() && IsGameTypeSingle()) {
			if (!g_actor || (g_actor->ID() != Level().CurrentViewEntity()->ID())) {
				CCustomMonster	*custom_monster = smart_cast<CCustomMonster*>(Level().CurrentViewEntity());
				if (custom_monster) // can be spectator in multiplayer
					custom_monster->UpdateCamera();
			}
			else 
			{
				CCameraBase* C = NULL;
				if (g_actor)
				{
					if(!Actor()->Holder())
						C = Actor()->cam_Active();
					else
						C = Actor()->Holder()->Camera();

				Actor()->Cameras().UpdateFromCamera		(C);
				Actor()->Cameras().ApplyDevice	(VIEWPORT_NEAR);
				}
			}
		}
#else // MASTER_GOLD
		if (g_actor && IsGameTypeSingle())
		{
			CCameraBase* C = NULL;
			if(!Actor()->Holder())
				C = Actor()->cam_Active();
			else
				C = Actor()->Holder()->Camera();

			Actor()->Cameras().UpdateFromCamera		(C);
			Actor()->Cameras().ApplyDevice	(VIEWPORT_NEAR);
		}
#endif // MASTER_GOLD
	}
	busy_warn(DEBUG_INFO, 3);
	__super::OnFrame			();
	busy_warn(DEBUG_INFO, 3);

	if	(0 != pDemoFile) {
		if	(Device.dwTimeGlobal > uTime2Change){
			// Change level + play demo
			if			(pDemoFile->elapsed()<3)	pDemoFile->seek(0);		// cycle

			// Read params
			string512			params;
			pDemoFile->r_string	(params,sizeof(params));
			string256			o_server, o_client, o_demo;	u32 o_time;
			sscanf				(params,"%[^,],%[^,],%[^,],%d",o_server,o_client,o_demo,&o_time);

			// Start _new level + demo
			Engine.Event.Defer	("KERNEL:disconnect");
			Engine.Event.Defer	("KERNEL:start",size_t(xr_strdup(_Trim(o_server))),size_t(xr_strdup(_Trim(o_client))));
			Engine.Event.Defer	("GAME:demo",	size_t(xr_strdup(_Trim(o_demo))), u64(o_time));
			uTime2Change		= 0xffffffff;	// Block changer until Event received
		}
	}

	u32 elps = Device.frame_elapsed();
	if (!Device.Paused() && elps <= 10)
	{
		Engine.Sheduler.Update();
		if (!busy_warn(DEBUG_INFO, 3))
		{
			WeathersUpdate();
			busy_warn(DEBUG_INFO, 3);
		}
	}	

#ifdef DEBUG
	if ((m_last_stats_frame + 1) < m_frame_counter)
		profiler().clear		();
#endif
	UpdateDof();
}

#include "game_sv_single.h"
#include "xrServer.h"
#include "hudmanager.h"
#include "UIGameCustom.h"

void CGamePersistent::OnEvent(EVENT E, u64 P1, u64 P2)
{
	if(E==eQuickLoad)
	{
		if (Device.Paused())
			Device.Pause		(FALSE, TRUE, TRUE, "eQuickLoad");
		
		if(HUD().GetUI())
			HUD().GetUI()->UIGame()->HideShownDialogs();

		LPSTR		saved_name	= (LPSTR)(P1);

		Level().remove_objects	();
		game_sv_Single			*game = smart_cast<game_sv_Single*>(Level().Server->game);
		R_ASSERT				(game);
		game->restart_simulator	(saved_name);
		xr_free					(saved_name);
		return;
	}else
	if(E==eDemoStart)
	{
		string256			cmd;
		LPCSTR				demo	= LPCSTR(P1);
		sprintf_s				(cmd,"demo_play %s",demo);
		Console->Execute	(cmd);
		xr_free				(demo);
		uTime2Change		= Device.TimerAsync() + u32(P2)*1000;
	}
}

void CGamePersistent::Statistics	(CGameFont* F)
{
#ifdef DEBUG
#	ifndef _EDITOR
		m_last_stats_frame		= m_frame_counter;
		profiler().show_stats	(F,!!psAI_Flags.test(aiStats));
#	endif
#endif
}

float CGamePersistent::MtlTransparent(u32 mtl_idx)
{
	return GMLib.GetMaterialByIdx((u16)mtl_idx)->fVisTransparencyFactor;
}
static BOOL bRestorePause	= FALSE;
static BOOL bEntryFlag		= TRUE;

void CGamePersistent::OnAppActivate		()
{
	bool bIsMP = (g_pGameLevel && Level().game && GameID() != GAME_SINGLE);
	bIsMP		&= !Device.Paused();

	if( !bIsMP )
	{
		Device.Pause			(FALSE, !bRestorePause, TRUE, "CGP::OnAppActivate");
	}else
	{
		Device.Pause			(FALSE, TRUE, TRUE, "CGP::OnAppActivate MP");
	}

	bEntryFlag = TRUE;
}

void CGamePersistent::OnAppDeactivate	()
{
	if(!bEntryFlag) return;

	bool bIsMP = (g_pGameLevel && Level().game && GameID() != GAME_SINGLE);

	bRestorePause = FALSE;

	if ( !bIsMP )
	{
		bRestorePause			= Device.Paused();
		Device.Pause			(TRUE, TRUE, TRUE, "CGP::OnAppDeactivate");
	}else
	{
		Device.Pause			(TRUE, FALSE, TRUE, "CGP::OnAppDeactivate MP");
	}
	bEntryFlag = FALSE;
}

bool CGamePersistent::OnRenderPPUI_query()
{
	return MainMenu()->OnRenderPPUI_query();
	// enable PP or not
}

extern void draw_wnds_rects();
void CGamePersistent::OnRenderPPUI_main()
{
	// always
	MainMenu()->OnRenderPPUI_main();
	draw_wnds_rects();
}

void CGamePersistent::OnRenderPPUI_PP()
{
	MainMenu()->OnRenderPPUI_PP();
}
#include "string_table.h"
#include "../xr_3da/x_ray.h"
void CGamePersistent::LoadTitle(LPCSTR str)
{
	string512			buff;	
	LPCSTR alt_text = TryCallLuafunc("on_load_title", str); // ��������� ������������� ������ ����������� ������
	if (strstr(alt_text, "#ERROR") || strstr(alt_text, "#OK"));
	else
		str = alt_text;
	sprintf_s			(buff, "%s...", CStringTable().translate(str).c_str());
	pApp->LoadTitleInt	(buff);	
}

bool CGamePersistent::CanBePaused()
{
	return IsGameTypeSingle	() || (g_pGameLevel && Level().IsDemoPlay());
}
void CGamePersistent::SetPickableEffectorDOF(bool bSet)
{
	m_bPickableDOF = bSet;
	if(!bSet)
		RestoreEffectorDOF();
}

void CGamePersistent::GetCurrentDof(Fvector3& dof)
{
	dof = m_dof[1];
}

void CGamePersistent::SetBaseDof(const Fvector3& dof)
{
	m_dof[0]=m_dof[1]=m_dof[2]=m_dof[3]	= dof;
}

void CGamePersistent::SetEffectorDOF(const Fvector& needed_dof)
{
	if(m_bPickableDOF)	return;
	m_dof[0]	= needed_dof;
	m_dof[2]	= m_dof[1]; //current
}

void CGamePersistent::RestoreEffectorDOF()
{
	SetEffectorDOF			(m_dof[3]);
}
#include "hudmanager.h"

//	m_dof		[4];	// 0-dest 1-current 2-from 3-original
void CGamePersistent::UpdateDof()
{
	static float diff_far	= pSettings->r_float("zone_pick_dof","far");//70.0f;
	static float diff_near	= pSettings->r_float("zone_pick_dof","near");//-70.0f;

	if(m_bPickableDOF)
	{
		Fvector pick_dof;
		pick_dof.y	= HUD().GetCurrentRayQuery().range;
		pick_dof.x	= pick_dof.y+diff_near;
		pick_dof.z	= pick_dof.y+diff_far;
		m_dof[0]	= pick_dof;
		m_dof[2]	= m_dof[1]; //current
	}
	if(m_dof[1].similar(m_dof[0]))
						return;

	float td			= Device.fTimeDelta;
	Fvector				diff;
	diff.sub			(m_dof[0], m_dof[2]);
	diff.mul			(td/0.2f); //0.2 sec
	m_dof[1].add		(diff);
	(m_dof[0].x<m_dof[2].x)?clamp(m_dof[1].x,m_dof[0].x,m_dof[2].x):clamp(m_dof[1].x,m_dof[2].x,m_dof[0].x);
	(m_dof[0].y<m_dof[2].y)?clamp(m_dof[1].y,m_dof[0].y,m_dof[2].y):clamp(m_dof[1].y,m_dof[2].y,m_dof[0].y);
	(m_dof[0].z<m_dof[2].z)?clamp(m_dof[1].z,m_dof[0].z,m_dof[2].z):clamp(m_dof[1].z,m_dof[2].z,m_dof[0].z);
}

#include "ui\uimainingamewnd.h"
void CGamePersistent::OnSectorChanged(int sector)
{
	if(HUD().GetUI())
		HUD().GetUI()->UIMainIngameWnd->OnSectorChanged(sector);
}

void CGamePersistent::OnAssetsChanged()
{
	IGame_Persistent::OnAssetsChanged	();
	CStringTable().rescan				();
}

#include "inventory.h"
#include "weapon.h"
float CGamePersistent::ZoomRotationFactor() const
{
	CEntity* pEntity = smart_cast<CEntity*>(Level().CurrentEntity());
	if (pEntity)
	{
		CActor* pActor = smart_cast<CActor*>(pEntity);
		if (pActor)
		{
			PIItem item = Actor()->inventory().ActiveItem();
			if (item)
			{
				CWeapon* wpn = smart_cast<CWeapon*>(item);
				if (wpn)
					return wpn->ZoomRotationFactor();
			}
		}
	}
	return 0.f;
}
CUIWindow* get_inventory_wnd();
bool CGamePersistent::IsInventoryMenu()
{
	CUIWindow* wnd = get_inventory_wnd();
	if (wnd && wnd->IsShown())
		return true;

	return false;
}
