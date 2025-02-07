//-----------------------------------------------------------------------------
// File: x_ray.cpp
//
// Programmers:
//	Oles		- Oles Shishkovtsov
//	AlexMX		- Alexander Maksimchuk
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"

#include "dedicated_server_only.h"
#include "xr_input.h"
#include "xr_ioconsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "GameFont.h"
#include "resource.h"
#include "LightAnimLibrary.h"
#include "ispatial.h"
#include "CopyProtection.h"
#include "Text_Console.h"
#include "ResourceManager.h"
#include "../../build_config_defines.h"
#include <process.h>

//---------------------------------------------------------------------
ENGINE_API CInifile* pGameIni		= NULL;
BOOL	g_bIntroFinished			= FALSE;
extern	void	Intro				( void* fn );
extern	void	Intro_DSHOW			( void* fn );
extern	int PASCAL IntroDSHOW_wnd	(HINSTANCE hInstC, HINSTANCE hInstP, LPSTR lpCmdLine, int nCmdShow);
int		max_load_stage = 0;

float	   g_fLastSwitchEvent = 0.f;
ENGINE_API CTimer_paused g_GameLoaded;
ENGINE_API bool    g_bGameInteractive = true;
extern     float   g_fTimeInteractive;

ENGINE_API void switch_game_interactive(bool flag, LPCSTR context)
{
	if (!flag) g_fTimeInteractive = 0.f;
	if (flag == g_bGameInteractive) return;
	g_bGameInteractive = flag;
	g_fLastSwitchEvent = Device.fTimeGlobal;

	Msg("* game render status switched to '%s' from '%s'", (flag ? "interactive" : "loading"), context);	
	Device.bWarnFreeze = flag;
}


// computing build id
XRCORE_API	LPCSTR	build_date;
XRCORE_API	u32		build_id;

//#define NO_SINGLE
#define NO_MULTI_INSTANCES

static LPSTR month_id[12] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int days_in_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int start_day	= 31;	// 31
static int start_month	= 1;	// January
static int start_year	= 1999;	// 1999

void compute_build_id	()
{
	build_date			= __DATE__;

	int					days;
	int					months = 0;
	int					years;
	string16			month;
	string256			buffer;
	strcpy_s				(buffer,__DATE__);
	sscanf				(buffer,"%s %d %d",month,&days,&years);

	for (int i=0; i<12; i++) {
		if (_stricmp(month_id[i],month))
			continue;

		months			= i;
		break;
	}

	build_id			= (years - start_year)*365 + days - start_day;

	for (int i=0; i<months; ++i)
		build_id		+= days_in_month[i];

	for (int i=0; i<start_month-1; ++i)
		build_id		-= days_in_month[i];
}

XRCORE_API BOOL GetThreadTimesEx(HANDLE hThread, float &kernel, float &user);

//---------------------------------------------------------------------
// 2446363
// umbt@ukr.net
//////////////////////////////////////////////////////////////////////////
struct _SoundProcessor	: public pureFrame
{
	virtual void OnFrame	( )
	{
		//Msg							("------------- sound: %d [%3.2f,%3.2f,%3.2f]",u32(Device.dwFrame),VPUSH(Device.vCameraPosition));
		Device.Statistic->Sound.Begin();
		::Sound->update				(Device.vCameraPosition,Device.vCameraDirection,Device.vCameraTop);
		Device.Statistic->Sound.End	();
	}
}	SoundProcessor;

//////////////////////////////////////////////////////////////////////////
// global variables
ENGINE_API	CApplication*	pApp			= NULL;
static		HWND			logoWindow		= NULL;

			int				doLauncher		();
			void			doBenchmark		(LPCSTR name);
ENGINE_API	bool			g_bBenchmark	= false;
string512	g_sBenchmarkName;


ENGINE_API	string512		g_sLaunchOnExit_params;
ENGINE_API	string512		g_sLaunchOnExit_app;
XRCORE_API  void		    LogStackTraceEx(struct _EXCEPTION_POINTERS*);


bool XRCORE_API force_flush_log;

// -------------------------------------------
// startup point
void LoadInterceptor()
{	
	CHAR dllName[MAX_PATH];
	GetModuleFileName(0, dllName, MAX_PATH);
	HMODULE hDLL = GetModuleHandle("luaicp.dll");
	if (!hDLL)	
		for (int i = xr_strlen(dllName) - 1; i > 0; i--)
		if (dllName[i] == 0x5C)
		{
			dllName[i + 1] = 0;
			strcat_s(dllName, "luaicp.dll");
			hDLL = LoadLibrary(dllName);
			if (!hDLL) break;
		}	
	if (!hDLL) return;
	typedef void(WINAPI *LUAICP_INITPROC) (LPVOID param);
	LUAICP_INITPROC init = (LUAICP_INITPROC)GetProcAddress(hDLL, "Init");
	if (init)
		init(NULL);
}


void InitEngine()
{
	__try
	{	
		Engine.Initialize();
		while (!g_bIntroFinished)	Sleep(100);
		Device.Initialize();
		CheckCopyProtection();		
		force_flush_log = false;
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!#EXCEPTION: catched in InitEngine");
		MsgCB("$#DUMP_CONTEXT");
	}
}

PROTECT_API void InitSettings	()
{
	string_path					fname; 
	FS.update_path				(fname,"$game_config$","system.ltx");
	pSettings					= xr_new<CInifile>	(fname,TRUE, FALSE);	
	//pSettings->SetIncludeMacro ("$game_balance$", "default");
	pSettings->reload();	

	CHECK_OR_EXIT				(!pSettings->sections().empty(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));

	FS.update_path				(fname,"$game_config$","game.ltx");
	pGameIni					= xr_new<CInifile>	(fname,TRUE);
	CHECK_OR_EXIT				(!pGameIni->sections().empty(),make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));
}
PROTECT_API void InitConsole	()
{
#ifdef DEDICATED_SERVER
	{
		Console						= xr_new<CTextConsole>	();		
	}
#else
	//	else
	{
		Console						= xr_new<CConsole>	();
	}
#endif
	R_ASSERT(pSettings);

	Console->Initialize			( );

	strcpy_s						(Console->ConfigFile, sizeof(Console->ConfigFile), "user.ltx");
	if (strstr(Core.Params,"-ltx ")) {
		string64				c_name;
		sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
		strcpy_s					(Console->ConfigFile, sizeof(Console->ConfigFile), c_name);
	}
}

PROTECT_API void InitInput		()
{
	BOOL bCaptureInput			= !strstr(Core.Params,"-i");
	if(g_dedicated_server)
		bCaptureInput			= FALSE;

	pInput						= xr_new<CInput>		(bCaptureInput);
}
void destroyInput	()
{
	xr_delete					( pInput		);
}

PROTECT_API void InitSound1		()
{
	CSound_manager_interface::_create				(0);
}

PROTECT_API void InitSound2		()
{
	CSound_manager_interface::_create				(1);
}

void destroySound	()
{
	CSound_manager_interface::_destroy				( );
}
void destroySettings()
{
	xr_delete					( pSettings		);
	xr_delete					( pGameIni		);
}
void destroyConsole	()
{
	Console->Destroy			( );
	xr_delete					(Console);
}
void destroyEngine	()
{
	Device.Destroy				( );
	Engine.Destroy				( );
}

void execUserScript				( )
{
// Execute script

	Console->Execute			("unbindall");
	Console->ExecuteScript		(Console->ConfigFile);
}
void slowdownthread	( void* )
{
//	Sleep		(30*1000);
	for (;;)	{
		if (Device.Statistic->fFPS < 30) Sleep(1);
		if (Device.mt_bMustExit)	return;
		if (0==pSettings)			return;
		if (0==Console)				return;
		if (0==pInput)				return;
		if (0==pApp)				return;
	}
}
void CheckPrivilegySlowdown		( )
{
#ifdef DEBUG
	if	(strstr(Core.Params,"-slowdown"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
	if	(strstr(Core.Params,"-slowdown2x"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
#endif // DEBUG
}

void Startup					( )
{

	__try
	{
		//Console->Execute("g_game_balance default"); // defaults 
	InitSound1		();
		execUserScript();
		//.	InitInput		();
	InitSound2		();

		// ...command line for auto start
		{
		LPCSTR	pStartup			= strstr				(Core.Params,"-start ");
		if (pStartup)				Console->Execute		(pStartup+1);
	}
	{
		LPCSTR	pStartup			= strstr				(Core.Params,"-load ");
		if (pStartup)				Console->Execute		(pStartup+1);
		}

		// Initialize APP
		//#ifndef DEDICATED_SERVER
	ShowWindow( Device.m_hWnd , SW_SHOWNORMAL );
	Device.Create				( );
//#endif
	LALib.OnCreate				( );
	pApp						= xr_new<CApplication>	();
	g_pGamePersistent			= (IGame_Persistent*)	NEW_INSTANCE (CLSID_GAME_PERSISTANT);
	g_SpatialSpace				= xr_new<ISpatial_DB>	();
	g_SpatialSpacePhysic		= xr_new<ISpatial_DB>	();

	// Destroy LOGO
	DestroyWindow				(logoWindow);
	logoWindow					= NULL;

	// Main cycle
	CheckCopyProtection			( );
	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("!#EXCEPTION: was Startup()");
		MsgCB("$#DUMP_CONTEXT");
	}

	Memory.mem_usage();
	
	__try
	{
		Device.Run();
	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("!#EXCEPTION: was catched in Device.Run");
		MsgCB("$#DUMP_CONTEXT");
	}

	// Destroy APP
	xr_delete					( g_SpatialSpacePhysic	);
	xr_delete					( g_SpatialSpace		);
	DEL_INSTANCE				( g_pGamePersistent		);
	xr_delete					( pApp					);
	Engine.Event.Dump			( );

	// Destroying
//.	destroySound();
	destroyInput();

	if(!g_bBenchmark)
		destroySettings();

	LALib.OnDestroy				( );
	
	if(!g_bBenchmark)
		destroyConsole();
	else
		Console->Destroy();

	destroySound();

	destroyEngine();
}

static BOOL CALLBACK logDlgProc( HWND hw, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg ){
		case WM_DESTROY:
			break;
		case WM_CLOSE:
			DestroyWindow( hw );
			break;
		case WM_COMMAND:
			if( LOWORD(wp)==IDCANCEL )
				DestroyWindow( hw );
			break;
		default:
			return FALSE;
	}
	return TRUE;
}
/*
void	test_rtc	()
{
	CStatTimer		tMc,tM,tC,tD;
	u32				bytes=0;
	tMc.FrameStart	();
	tM.FrameStart	();
	tC.FrameStart	();
	tD.FrameStart	();
	::Random.seed	(0x12071980);
	for		(u32 test=0; test<10000; test++)
	{
		u32			in_size			= ::Random.randI(1024,256*1024);
		u32			out_size_max	= rtc_csize		(in_size);
		u8*			p_in			= xr_alloc<u8>	(in_size);
		u8*			p_in_tst		= xr_alloc<u8>	(in_size);
		u8*			p_out			= xr_alloc<u8>	(out_size_max);
		for (u32 git=0; git<in_size; git++)			p_in[git] = (u8)::Random.randI	(8);	// garbage
		bytes		+= in_size;

		tMc.Begin	();
		memcpy		(p_in_tst,p_in,in_size);
		tMc.End		();

		tM.Begin	();
		CopyMemory(p_in_tst,p_in,in_size);
		tM.End		();

		tC.Begin	();
		u32			out_size		= rtc_compress	(p_out,out_size_max,p_in,in_size);
		tC.End		();

		tD.Begin	();
		u32			in_size_tst		= rtc_decompress(p_in_tst,in_size,p_out,out_size);
		tD.End		();

		// sanity check
		R_ASSERT	(in_size == in_size_tst);
		for (u32 tit=0; tit<in_size; tit++)			R_ASSERT(p_in[tit] == p_in_tst[tit]);	// garbage

		xr_free		(p_out);
		xr_free		(p_in_tst);
		xr_free		(p_in);
	}
	tMc.FrameEnd	();	float rMc		= 1000.f*(float(bytes)/tMc.result)/(1024.f*1024.f);
	tM.FrameEnd		(); float rM		= 1000.f*(float(bytes)/tM.result)/(1024.f*1024.f);
	tC.FrameEnd		(); float rC		= 1000.f*(float(bytes)/tC.result)/(1024.f*1024.f);
	tD.FrameEnd		(); float rD		= 1000.f*(float(bytes)/tD.result)/(1024.f*1024.f);
	Msg				("* memcpy:        %5.2f M/s (%3.1f%%)",rMc,100.f*rMc/rMc);
	Msg				("* mm-memcpy:     %5.2f M/s (%3.1f%%)",rM,100.f*rM/rMc);
	Msg				("* compression:   %5.2f M/s (%3.1f%%)",rC,100.f*rC/rMc);
	Msg				("* decompression: %5.2f M/s (%3.1f%%)",rD,100.f*rD/rMc);
}
*/
extern void	testbed	(void);

// video
/*
static	HINSTANCE	g_hInstance		;
static	HINSTANCE	g_hPrevInstance	;
static	int			g_nCmdShow		;
void	__cdecl		intro_dshow_x	(void*)
{
	IntroDSHOW_wnd		(g_hInstance,g_hPrevInstance,"GameData\\Stalker_Intro.avi",g_nCmdShow);
	g_bIntroFinished	= TRUE	;
}
*/
#define dwStickyKeysStructSize sizeof( STICKYKEYS )
#define dwFilterKeysStructSize sizeof( FILTERKEYS )
#define dwToggleKeysStructSize sizeof( TOGGLEKEYS )

struct damn_keys_filter {
	BOOL bScreenSaverState;

	// Sticky & Filter & Toggle keys

	STICKYKEYS StickyKeysStruct;
	FILTERKEYS FilterKeysStruct;
	TOGGLEKEYS ToggleKeysStruct;

	DWORD dwStickyKeysFlags;
	DWORD dwFilterKeysFlags;
	DWORD dwToggleKeysFlags;

	damn_keys_filter	()
	{
		// Screen saver stuff

		bScreenSaverState = FALSE;

		// Saveing current state
		SystemParametersInfo( SPI_GETSCREENSAVEACTIVE , 0 , ( PVOID ) &bScreenSaverState , 0 );

		if ( bScreenSaverState )
			// Disable screensaver
			SystemParametersInfo( SPI_SETSCREENSAVEACTIVE , FALSE , NULL , 0 );

		dwStickyKeysFlags = 0;
		dwFilterKeysFlags = 0;
		dwToggleKeysFlags = 0;


		ZeroMemory( &StickyKeysStruct , dwStickyKeysStructSize );
		ZeroMemory( &FilterKeysStruct , dwFilterKeysStructSize );
		ZeroMemory( &ToggleKeysStruct , dwToggleKeysStructSize );

		StickyKeysStruct.cbSize = dwStickyKeysStructSize;
		FilterKeysStruct.cbSize = dwFilterKeysStructSize;
		ToggleKeysStruct.cbSize = dwToggleKeysStructSize;

		// Saving current state
		SystemParametersInfo( SPI_GETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		SystemParametersInfo( SPI_GETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		SystemParametersInfo( SPI_GETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );

		if ( StickyKeysStruct.dwFlags & SKF_AVAILABLE ) {
			// Disable StickyKeys feature
			dwStickyKeysFlags = StickyKeysStruct.dwFlags;
			StickyKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( FilterKeysStruct.dwFlags & FKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwFilterKeysFlags = FilterKeysStruct.dwFlags;
			FilterKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( ToggleKeysStruct.dwFlags & TKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwToggleKeysFlags = ToggleKeysStruct.dwFlags;
			ToggleKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}
	}

	~damn_keys_filter	()
	{
		if ( bScreenSaverState )
			// Restoring screen saver
			SystemParametersInfo( SPI_SETSCREENSAVEACTIVE , TRUE , NULL , 0 );

		if ( dwStickyKeysFlags) {
			// Restore StickyKeys feature
			StickyKeysStruct.dwFlags = dwStickyKeysFlags;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( dwFilterKeysFlags ) {
			// Restore FilterKeys feature
			FilterKeysStruct.dwFlags = dwFilterKeysFlags;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( dwToggleKeysFlags ) {
			// Restore FilterKeys feature
			ToggleKeysStruct.dwFlags = dwToggleKeysFlags;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}

	}
};

#undef dwStickyKeysStructSize
#undef dwFilterKeysStructSize
#undef dwToggleKeysStructSize

// ���������� ��� SecuROM-�
#include "securom_api.h"

// ������ ��� ����� ���������� THQ � ����� ������������ �������������
BOOL IsOutOfVirtualMemory()
{
#define VIRT_ERROR_SIZE 256
#define VIRT_MESSAGE_SIZE 512

	SECUROM_MARKER_HIGH_SECURITY_ON(1)

	// IMAGE_DIRECTORY_ENTRY_BASERELOC

	MEMORYSTATUSEX statex;
	DWORD dwPageFileInMB = 0;
	DWORD dwPhysMemInMB = 0;
	HINSTANCE hApp = 0;
	char	pszError[ VIRT_ERROR_SIZE ];
	string256	szMessage;

	ZeroMemory( &statex , sizeof( MEMORYSTATUSEX ) );
	statex.dwLength = sizeof( MEMORYSTATUSEX );
	
	if ( ! GlobalMemoryStatusEx( &statex ) )
		return 0;

	dwPageFileInMB = ( DWORD ) ( statex.ullTotalPageFile / ( 1024 * 1024 ) ) ;
	dwPhysMemInMB = ( DWORD ) ( statex.ullTotalPhys / ( 1024 * 1024 ) ) ;

	// �������� ���������� �������
	if ( ( dwPhysMemInMB >= 2000 ) && ( ( dwPageFileInMB + dwPhysMemInMB ) > 3500  ) )
		return 0;

	hApp = GetModuleHandle( NULL );

	if ( ! LoadString( hApp , RC_VIRT_MEM_ERROR , pszError , VIRT_ERROR_SIZE ) )
		return 0;
#ifdef NLC7  
	strcpy_s(szMessage, 255, "��� ������ ����������� NLC7 ��������� �� ����� 2�� ���������� � 3.5�� ����������� ������");
#else
	if ( ! LoadString( hApp , RC_VIRT_MEM_TEXT , szMessage , VIRT_MESSAGE_SIZE ) ) return 0;
#endif
	MessageBox( NULL , szMessage , pszError , MB_OK | MB_ICONHAND );

	SECUROM_MARKER_HIGH_SECURITY_OFF(1)

	return 1;
}

#include "xr_ioc_cmd.h"

typedef void DUMMY_STUFF (const void*,const u32&,void*);
XRCORE_API DUMMY_STUFF	*g_temporary_stuff;

#define TRIVIAL_ENCRYPTOR_DECODER
#include "trivial_encryptor.h"

//#define RUSSIAN_BUILD

#if 0
void foo	()
{
	typedef std::map<int,int>	TEST_MAP;
	TEST_MAP					temp;
	temp.insert					(std::make_pair(0,0));
	TEST_MAP::const_iterator	I = temp.upper_bound(2);
	if (I == temp.end())
		OutputDebugString		("end() returned\r\n");
	else
		OutputDebugString		("last element returned\r\n");

	typedef void*	pvoid;

	LPCSTR			path = "d:\\network\\stalker_net2";
	FILE			*f = fopen(path,"rb");
	int				file_handle = _fileno(f);
	u32				buffer_size = _filelength(file_handle);
	pvoid			buffer = xr_malloc(buffer_size);
	size_t			result = fread(buffer,buffer_size,1,f);
	R_ASSERT3		(!buffer_size || (result && (buffer_size >= result)),"Cannot read from file",path);
	fclose			(f);

	u32				compressed_buffer_size = rtc_csize(buffer_size);
	pvoid			compressed_buffer = xr_malloc(compressed_buffer_size);
	u32				compressed_size = rtc_compress(compressed_buffer,compressed_buffer_size,buffer,buffer_size);

	LPCSTR			compressed_path = "d:\\network\\stalker_net2.rtc";
	FILE			*f1 = fopen(compressed_path,"wb");
	fwrite			(compressed_buffer,compressed_size,1,f1);
	fclose			(f1);
}
#endif // 0

ENGINE_API	bool g_dedicated_server	= false;

#pragma optimize("gyt", off)  // for watches in debugger

void stat_memory_short ()
{

	// size_t  w_free, w_reserved, w_committed;
	// vminfo	(&w_free, &w_reserved, &w_committed);
	log_vminfo	();
	HANDLE _crt_h		= (HANDLE)_get_heap_handle();
	u32		_crt_heap		= mem_usage_impl(_crt_h, 0,0);
	u32		_process_heap	= mem_usage_impl(0, 0, 0);
	u32     _lib_heap		= mem_usage_impl( (HANDLE)1, 0, 0);
	u32		_fs_heap		= mem_usage_impl( (HANDLE)2, 0, 0);	 
	MsgCB("*#MEM_USAGE: crt heap[%7d K], process heap[%7d K], lib heap [%7d K] fs heap [%d K] ",
		_crt_heap / 1024, _process_heap / 1024, _lib_heap / 1024, _fs_heap / 1024);
}



int APIENTRY WinMain_impl(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     char *    lpCmdLine,
                     int       nCmdShow)
{
//	foo();
#ifndef DEDICATED_SERVER

	// Check for virtual memory

	if ( ( strstr( lpCmdLine , "--skipmemcheck" ) == NULL ) && IsOutOfVirtualMemory() )
		return 0;

	force_flush_log = true;	
	#if defined(LUAICP_COMPAT) && defined (LUACAP_ALWAYS_LOAD)
	LoadInterceptor();
#endif

	{
		wchar_t langs[32] = { L"ru-RU\0\0" };
		ULONG num;
		SetProcessPreferredUILanguages(MUI_LANGUAGE_NAME, langs, &num);
	}

	// Check for another instance
#ifdef NO_MULTI_INSTANCES
	#define STALKER_PRESENCE_MUTEX "STALKER-SoC"
	
	HANDLE hCheckPresenceMutex = INVALID_HANDLE_VALUE;
	hCheckPresenceMutex = OpenMutex( READ_CONTROL , FALSE ,  STALKER_PRESENCE_MUTEX );
	if ( hCheckPresenceMutex == NULL ) {
		// New mutex
		hCheckPresenceMutex = CreateMutex( NULL , FALSE , STALKER_PRESENCE_MUTEX );
		if ( hCheckPresenceMutex == NULL )
			// Shit happens
			return 2;
	} else {
		// Already running
		CloseHandle( hCheckPresenceMutex );
		return 1;
	}
#endif
#else // DEDICATED_SERVER
	g_dedicated_server			= true;
#endif // DEDICATED_SERVER

#ifdef LUAICP_COMPAT
	// alpet: �� ������ ���� ���������� ����� ������ ������� �������� �����, ��� ��� ����� ������� ��������� ���������� 
//	SetThreadAffinityMask		(GetCurrentThread(), 0xFFE);	
	// SetThreadPriority			(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);  // ����� ��������� ��������� �� ������������ ����� �� ������� ����
//	SetProcessPriorityBoost		(GetCurrentProcess(), FALSE);
	// SetPriorityClass			(GetCurrentProcess(), HIGH_PRIORITY_CLASS);          // ��� ���������� ��� ������� �������, �� ���� ������� ������� ������� �������� ����. 
#else
//	SetThreadAffinityMask		(GetCurrentThread(),1);
#endif

	// Title window
	logoWindow					= CreateDialog(GetModuleHandle(NULL),	MAKEINTRESOURCE(IDD_STARTUP), 0, logDlgProc );
	SetWindowPos				(
		logoWindow,
#ifdef  TRUE_RELEASE
		HWND_TOPMOST,
#else
		HWND_NOTOPMOST,
#endif // NDEBUG
		0,
		0,
		0,
		0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW
	);

	// AVI
	g_bIntroFinished			= TRUE;

	g_sLaunchOnExit_app[0]		= NULL;
	g_sLaunchOnExit_params[0]	= NULL;

	LPCSTR						fsgame_ltx_name = "-fsltx ";
	string_path					fsgame = "";
	if (strstr(lpCmdLine, fsgame_ltx_name)) {
		int						sz = xr_strlen(fsgame_ltx_name);
		sscanf					(strstr(lpCmdLine,fsgame_ltx_name)+sz,"%[^ ] ",fsgame);
	}

	// alpet: ��������� �������� ��� ��������� ������������
	LPCSTR						verb_filters_name = "-verb_filters ";
	string4096					verb_filters;
	if (LPCSTR arg = strstr(lpCmdLine, verb_filters_name)) {
		ZeroMemory				(&verb_filters, 4096);
		arg						+= xr_strlen(verb_filters_name);
		sscanf_s				(arg,"%[^ ] ", verb_filters, 4096);
		InitVerbosity			(verb_filters);
	}
	LPCSTR						verb_level_name = "-verbosity ";
	if (LPCSTR arg = strstr(lpCmdLine, verb_level_name)) {
		arg						+= xr_strlen(verb_level_name);
		sscanf_s				(arg,"%d", &verbosity_level, 4);		
		clamp<u32>				(verbosity_level, 0, 9);
	}


#ifdef LUAICP_COMPAT
#pragma todo("alpet: �������� ����������� � ���������� �����������")
	g_temporary_stuff = &trivial_encryptor::decode; 
#else
	g_temporary_stuff = &trivial_encryptor::decode;
#endif 
		
	try
	{
		stat_memory_short();	
		compute_build_id();
		Core._initialize("xray", NULL, TRUE, fsgame[0] ? fsgame : NULL);
		InitSettings();
		stat_memory_short();
	}
	catch (...)
	{
		Log("!#EXCEPTION: catched while core initialization ");
	}

#ifndef DEDICATED_SERVER
	{
		damn_keys_filter		filter;
		(void)filter;
#endif // DEDICATED_SERVER

		FPU::m24r				();		
		InitEngine				();

		InitInput				();

		InitConsole				();

		Engine.External.CreateRendererList();

		LPCSTR benchName = "-batch_benchmark ";
		if(strstr(lpCmdLine, benchName))
		{
			int sz = xr_strlen(benchName);
			string64				b_name;
			sscanf					(strstr(Core.Params,benchName)+sz,"%[^ ] ",b_name);
			doBenchmark				(b_name);
			return 0;
		}

		if (strstr(lpCmdLine,"-launcher")) 
		{
			int l_res = doLauncher();
			if (l_res != 0)
				return 0;
		};
		
#ifndef DEDICATED_SERVER
		if(strstr(Core.Params,"-r2a"))	
			Console->Execute			("renderer renderer_r2a");
		else
		if(strstr(Core.Params,"-r2"))	
			Console->Execute			("renderer renderer_r2");
		else
		{
			Msg("Loading  config for renderer ");
			CCC_LoadCFG_custom*	pTmp = xr_new<CCC_LoadCFG_custom>("renderer ");
			pTmp->Execute				(Console->ConfigFile);
			xr_delete					(pTmp);
		}	
#else
			Console->Execute			("renderer renderer_r1");
#endif
		try
		{
//.			InitInput					( );
			Engine.External.Initialize();
	//		Console->Execute			("stat_memory");

			Startup	 					( );
		}
		catch (...)
		{
			MsgCB("!EXCEPTION: catched in xr_3da.Startup()");
		}

		Core._destroy();

		char* _args[3];
		// check for need to execute something external
		if (/*xr_strlen(g_sLaunchOnExit_params) && */xr_strlen(g_sLaunchOnExit_app) ) 
		{
			string4096 ModuleFileName = "";		
			GetModuleFileName(NULL, ModuleFileName, 4096);

			string4096 ModuleFilePath		= "";
			char* ModuleName				= NULL;
			GetFullPathName					(ModuleFileName, 4096, ModuleFilePath, &ModuleName);
			ModuleName[0]					= 0;
			strcat							(ModuleFilePath, g_sLaunchOnExit_app);
			_args[0] 						= g_sLaunchOnExit_app;
			_args[1] 						= g_sLaunchOnExit_params;
			_args[2] 						= NULL;		
			
			_spawnv							(_P_NOWAIT, _args[0], _args);//, _envvar);
		}
#ifndef DEDICATED_SERVER
#ifdef NO_MULTI_INSTANCES		
		// Delete application presence mutex
		CloseHandle( hCheckPresenceMutex );
#endif
	}
	// here damn_keys_filter class instanse will be destroyed
#endif // DEDICATED_SERVER

	return						0;
}

int stack_overflow_exception_filter	(int exception_code)
{
   if (exception_code == EXCEPTION_STACK_OVERFLOW)
   {
       // Do not call _resetstkoflw here, because
       // at this point, the stack is not yet unwound.
       // Instead, signal that the handler (the __except block)
       // is to be executed.
       return EXCEPTION_EXECUTE_HANDLER;
   }
   else
       return EXCEPTION_CONTINUE_SEARCH;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     char *    lpCmdLine,
                     int       nCmdShow)
{
	__try 
	{
#ifdef DEDICATED_SERVER
		Debug._initialize	(true);
#else // DEDICATED_SERVER
		Debug._initialize	(false);
#endif // DEDICATED_SERVER

		WinMain_impl		(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
	}
	__except(SIMPLE_FILTER)
	{
		Log("#EXCEPTION: catched in WinMain_impl");
	}

	return					(0);
}

LPCSTR _GetFontTexName (LPCSTR section)
{
	static char* tex_names[]={"texture800","texture","texture1600"};
	int def_idx		= 1;//default 1024x768
	int idx			= def_idx;

#if 0
	u32 w = Device.dwWidth;

	if(w<=800)		idx = 0;
	else if(w<=1280)idx = 1;
	else 			idx = 2;
#else
	u32 h = Device.dwHeight;

	if(h<=600)		idx = 0;
	else if(h<=900)	idx = 1;
	else 			idx = 2;
#endif

	while(idx>=0){
		if( pSettings->line_exist(section,tex_names[idx]) )
			return pSettings->r_string(section,tex_names[idx]);
		--idx;
	}
	return pSettings->r_string(section,tex_names[def_idx]);
}

void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags)
{
	LPCSTR font_tex_name = _GetFontTexName(section);
	R_ASSERT(font_tex_name);

	if(!F){
		F = xr_new<CGameFont> ("font", font_tex_name, flags);
	}else
		F->Initialize("font",font_tex_name);

	if (pSettings->line_exist(section,"size")){
		float sz = pSettings->r_float(section,"size");
		if (flags&CGameFont::fsDeviceIndependent)	F->SetHeightI(sz);
		else										F->SetHeight(sz);
	}
	if (pSettings->line_exist(section,"interval"))
		F->SetInterval(pSettings->r_fvector2(section,"interval"));

}

CApplication::CApplication()
{	
	ll_dwReference	= 0;
	load_stage		= 0;
	load_texture = "ui\\ui_load";

	// events
	eQuit						= Engine.Event.Handler_Attach("KERNEL:quit",this);
	eStart						= Engine.Event.Handler_Attach("KERNEL:start",this);
	eStartLoad					= Engine.Event.Handler_Attach("KERNEL:load",this);
	eDisconnect					= Engine.Event.Handler_Attach("KERNEL:disconnect",this);
	eConsole					= Engine.Event.Handler_Attach("KERNEL:console",this);

	// levels
	Level_Current				= 0;
	Level_Scan					( );

	// Font
	pFontSystem					= NULL;

	// Register us
	Device.seqFrame.Add			(this, REG_PRIORITY_HIGH+1000);
	
	if (psDeviceFlags.test(mtSound))	Device.seqFrameMT.Add		(&SoundProcessor);
	else								Device.seqFrame.Add			(&SoundProcessor);

	Console->Show				( );
	Console->Execute		("stat_memory");

	GetThreadTimesEx(0, load_times[0], load_times[1]);
	load_times[2] = load_times[3] = 1;
	g_GameLoaded.Start();
	g_GameLoaded.Pause(TRUE);

	// App Title
	app_title[ 0 ] = '\0';
}

CApplication::~CApplication()
{
	Console->Hide				( );

	// font
	xr_delete					( pFontSystem		);

	Device.seqFrameMT.Remove	(&SoundProcessor);
	Device.seqFrame.Remove		(&SoundProcessor);
	Device.seqFrame.Remove		(this);


	// events
	Engine.Event.Handler_Detach	(eConsole,this);
	Engine.Event.Handler_Detach	(eDisconnect,this);
	Engine.Event.Handler_Detach	(eStartLoad,this);
	Engine.Event.Handler_Detach	(eStart,this);
	Engine.Event.Handler_Detach	(eQuit,this);
}

extern CRenderDevice Device;

void CApplication::OnEvent(EVENT E, u64 P1, u64 P2)
{
	if (E==eQuit)
	{
		PostQuitMessage	(0);
		
		for (u32 i=0; i<Levels.size(); i++)
		{
			xr_free(Levels[i].folder);
			xr_free(Levels[i].name);
		}
	}
	else if(E==eStart) 
	{
		LPSTR		op_server		= LPSTR	(P1);
		LPSTR		op_client		= LPSTR	(P2);
		R_ASSERT	(0==g_pGameLevel);
		R_ASSERT	(0!=g_pGamePersistent);

#ifdef NO_SINGLE
		Console->Execute("main_menu on");
		if (	(op_server == NULL)			||
				(!xr_strlen(op_server))		||
				(
					(	strstr(op_server, "/dm")	|| strstr(op_server, "/deathmatch") ||
						strstr(op_server, "/tdm")	|| strstr(op_server, "/teamdeathmatch") ||
						strstr(op_server, "/ah")	|| strstr(op_server, "/artefacthunt")
					) && 
					!strstr(op_server, "/alife")
				)
			)
#endif // #ifdef NO_SINGLE
		{		
			Console->Execute("main_menu off");
			Console->Hide();
			HRESULT cl = HW.pDevice->TestCooperativeLevel();
			if (cl != D3D_OK)
			{
				Msg("! #WARN: Device cooperative level = %s", Debug.error2string(cl));
				Device.Reset(false);
			}
			//-----------------------------------------------------------
			g_pGamePersistent->PreStart		(op_server);
			//-----------------------------------------------------------
			g_pGameLevel					= (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);			
			pApp->LoadPrepare				();
			// pApp->LoadTitleInt("st_app_start");		
			pApp->LoadBegin					(); 
			g_pGamePersistent->Start		(op_server);
			g_pGameLevel->net_Start			(op_server,op_client);
			pApp->LoadEnd					(); 
		}
		xr_free							(op_server);
		xr_free							(op_client);
	} 
	else if (E==eDisconnect) 
	{
		if (g_pGameLevel) 
		{
			Console->Hide			();
			g_pGameLevel->net_Stop	();
			DEL_INSTANCE			(g_pGameLevel);
			Console->Show			();
			
			if( (FALSE == Engine.Event.Peek("KERNEL:quit")) &&(FALSE == Engine.Event.Peek("KERNEL:start")) )
			{
				Console->Execute("main_menu off");
				Console->Execute("main_menu on");
			}
		}
		R_ASSERT			(0!=g_pGamePersistent);
		g_pGamePersistent->Disconnect();
	}
	else if (E == eConsole)
	{
		LPSTR command				= (LPSTR)P1;
		Console->ExecuteCommand		( command, false );
		xr_free						(command);
	}
}

static	CTimer	phase_timer		;
extern	ENGINE_API BOOL			g_appLoaded = FALSE;
extern	ENGINE_API BOOL			g_bootComplete = FALSE;

void CApplication::SetLoadTexture(LPCSTR _name)
{
	load_texture = _name;
}

void CApplication::LoadBegin	()
{
	ll_dwReference++;
	if (1==ll_dwReference)	{



		if (0 == load_stage)
		{
			load_timer.Start();
			switch_game_interactive (false, "::LoadBegin");
		}

		g_appLoaded			= FALSE;
		g_bootComplete		= FALSE;

#ifndef DEDICATED_SERVER
		_InitializeFont		(pFontSystem,"ui_font_graffiti19_russian",0);

		ll_hGeom.create		(FVF::F_TL, RCache.Vertex.Buffer(), RCache.QuadIB);
		curr_texture = "$null";
		UpdateTexture		(FALSE);
		ll_hGeom2.create		(FVF::F_TL, RCache.Vertex.Buffer(),NULL);
#endif
		phase_timer.Start	();
		

		CheckCopyProtection	();
	}
}

void CApplication::LoadPrepare()
{
	if (load_stage > 10)
	{
		phase_timer.Start	();
		ll_dwReference = 0;
		load_stage = 0;
		GetThreadTimesEx(0, load_times[0], load_times[1]);
		load_times[2] = load_times[3] = 1;	
	}
	// ���� ��� �� ���������
	g_GameLoaded.Start();
	g_GameLoaded.Pause(TRUE);

}

void CApplication::LoadEnd		()
{
	ll_dwReference--;
	if (0==ll_dwReference)		{		
		Log("@---------- CApplication::LoadEnd -------------- ");
		Msg						("* phase time: %d ms",phase_timer.GetElapsed_ms());
		size_t  w_free, w_reserved, w_committed;
		vminfo	(&w_free, &w_reserved, &w_committed);
		float mem_usage = ((float) w_committed) / 1048576.f;

		Msg						("* phase cmem: %.3f M", mem_usage);
		if (IsDebuggerPresent())
			Console->Execute			("stat_memory");


		g_appLoaded				= TRUE;
//		DUMP_PHASE;
	}
}

void CApplication::destroy_loading_shaders()
{
	hLevelLogo.destroy		();
	sh_progress.destroy		();
	curr_texture = "$null";
	g_bootComplete = TRUE;
//.	::Sound->mute			(false);
}

u32 calc_progress_color(u32, u32, int, int);

PROTECT_API void CApplication::LoadDraw		()
{
	// switch_game_interactive (false, "::LoadDraw");
	if(g_bGameInteractive)		return;
	Device.dwFrame				+= 1;

	if(!Device.Begin () )		return;

	if	(g_dedicated_server)
		Console->OnRender			();
	else
		load_draw_internal			();

	Device.End					();
	CheckCopyProtection			();
}

int CApplication::LoadStage()
{
	return load_stage;
}

void CApplication::LoadTitleInt(LPCSTR str)
{	
	
	size_t  w_free, w_reserved, w_committed;
	vminfo	(&w_free, &w_reserved, &w_committed);
	Device.Resources->DeferredLoad(load_stage < 25);
	if (load_stage < 20)
		switch_game_interactive (false, "::LoadTitleInt");

	// � ������������
	float kernel = 0.f, user = 0.f;
	GetThreadTimesEx(0, kernel, user);

	float delta_k = kernel - load_times[0];
	float delta_u = user   - load_times[1];
	if (0 == load_stage)
	{
		delta_k = delta_u = 0.f;
		load_times[2] = load_times[3] = 0.f; // clear current
		load_timer.Start();
	}

	load_times[0] = kernel;
	load_times[1] = user;	
	load_times[2] += delta_k;
	load_times[3] += delta_u;

	float mem_usage = ((float) w_committed) / 1048576.f;
	u32 ph_time = phase_timer.GetElapsed_ms();
	// logging
	string_path fname;
	FS.update_path(fname, "$logs$", "loadlevel_perf.csv");
	FILE *fh = fopen(fname, "a");
	if (fh)
	{
		LPCSTR title = app_title;
		if (0 == load_stage)
		{
			fputs ("stage_title; time_ms; user_ms; kernel_ms; mem_usage \n", fh);
			title = "inititalize";
		}

		fprintf(fh, "%d. %s;%d;%.1f;%.1f;%.1f\n", load_stage, title, ph_time, delta_u, delta_k, mem_usage); // ���������� � ���������� ���� ��������
		
		if (strstr(str, "st_load_end"))
		{
			ph_time = load_timer.GetElapsed_ms();
			fprintf(fh, "total_load;%d;%.0f;%.0f\r\n\r\n", ph_time, load_times[3], load_times[2]);			
			Device.Resources->DeferredLoad(FALSE);
			Device.Resources->DeferredUpload	();
			switch_game_interactive (true, "::LoadTitelInt - on st_load_end");
			Console->Execute("stat_memory");
			g_GameLoaded.Start();
			g_GameLoaded.Pause(FALSE);
		}


		fclose(fh);
	}

	load_stage++;
	VERIFY						(ll_dwReference);
	VERIFY						(str && xr_strlen(str)<256);
	strcpy_s						(app_title, str);

	Msg							("* phase time: %d ms, user time: %.1f ms, kernel time: %.1f ms", ph_time, delta_u, delta_k);	
	phase_timer.Start();

	Msg							("* phase cmem: %.3f M", mem_usage);
	
	if (IsDebuggerPresent())
		Console->Execute				("stat_memory");
	Msg							("Next phase begins: %s", app_title);
	
	if (g_pGamePersistent->GameType()==1 && strstr(Core.Params,"alife"))
		max_load_stage			= 27;
	else
		max_load_stage			= 27;
#ifndef DEDICATED_SERVER
	UpdateTexture				(TRUE);
#endif

	LoadDraw					();
}

void CApplication::LoadSwitch	()
{
}

void CApplication::SetLoadLogo			(ref_shader NewLoadLogo)
{
//	hLevelLogo = NewLoadLogo;
//	R_ASSERT(0);
};

// Sequential
void CApplication::OnFrame	( )
{
	Engine.Event.OnFrame			();
	g_SpatialSpace->update			();
	g_SpatialSpacePhysic->update	();
	if (g_pGameLevel)				g_pGameLevel->SoundEvent_Dispatch	( );
/*
	static float f_free = 1000.0;
	u32 i_frame = Device.dwFrame % 1000;

	if (0 == i_frame)
	{
		size_t  w_free, w_committed;
		vminfo	(&w_free, NULL, &w_committed);
		f_free = (float)w_free / 1000000.f;			
	}

	if (f_free < 250.0 && (i_frame > 500))
	{			
		pFontSystem->SetColor	(color_rgba(255,0,0,255));
		pFontSystem->SetAligment(CGameFont::alCenter);
		pFontSystem->OutSetI	(0,-.05f);
		pFontSystem->OutNext("������������� ������, ��������: %.1f ��", f_free);
	}
*/
}

void CApplication::Level_Append		(LPCSTR folder)
{
	string_path	N1,N2,N3,N4;
	strconcat	(sizeof(N1),N1,folder,"level");
	strconcat	(sizeof(N2),N2,folder,"level.ltx");
	strconcat	(sizeof(N3),N3,folder,"level.geom");
	strconcat	(sizeof(N4),N4,folder,"level.cform");
	if	(
		FS.exist("$game_levels$",N1)		&&
		FS.exist("$game_levels$",N2)		&&
		FS.exist("$game_levels$",N3)		&&
		FS.exist("$game_levels$",N4)	
		)
	{
		sLevelInfo			LI;
		LI.folder			= xr_strdup(folder);
		LI.name				= 0;
		Levels.push_back	(LI);
	}
}

void CApplication::Level_Scan()
{
/*	for (u32 i=0; i<Levels.size(); i++)
	{
		xr_free(Levels[i].folder);
		xr_free(Levels[i].name);
	}
	Levels.clear	();*/


	xr_vector<char*>* folder			= FS.file_list_open		("$game_levels$",FS_ListFolders|FS_RootOnly);
	R_ASSERT							(folder&&folder->size());
	
	for (u32 i=0; i<folder->size(); ++i)	
		Level_Append((*folder)[i]);
	
	FS.file_list_close		(folder);
#ifdef DEBUG
	folder									= FS.file_list_open		("$game_levels$","$debug$\\",FS_ListFolders|FS_RootOnly);
	if (folder){
		string_path	tmp_path;
		for (u32 i=0; i<folder->size(); i++)
		{
			strconcat			(sizeof(tmp_path),tmp_path,"$debug$\\",(*folder)[i]);
			Level_Append		(tmp_path);
		}

		FS.file_list_close	(folder);
	}
#endif
}

void CApplication::Level_Set(u32 L)
{
	if (L>=Levels.size())	return;
	Level_Current = L;
	FS.get_path	("$level$")->_set	(Levels[L].folder);


	string_path					temp;
	string_path					temp2;
	strconcat					(sizeof(temp),temp,"intro\\intro_",Levels[L].folder);
	temp[xr_strlen(temp)-1] = 0;
	if (FS.exist(temp2, "$game_textures$", temp, ".dds"))
		hLevelLogo.create	("font", temp);
	else
		hLevelLogo.create	("font", "intro\\intro_no_start_picture");
		

	CheckCopyProtection		();
}

int CApplication::Level_ID(LPCSTR name)
{
	char buffer	[256];
	strconcat	(sizeof(buffer),buffer,name,"\\");
	for (u32 I=0; I<Levels.size(); I++)
	{
		if (0==stricmp(buffer,Levels[I].folder))	return int(I);
	}
	return -1;
}

void CApplication::UpdateTexture(BOOL bDestroyBefore)
{
	if (load_texture != curr_texture)
	__try{
		if (bDestroyBefore) 
			sh_progress.destroy();
		sh_progress.create("hud\\default", load_texture.c_str());
		curr_texture = load_texture;
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!Exception catched in CApplication::UpdateTexture ");
	}
}

//launcher stuff----------------------------
extern "C"{
	typedef int	 __cdecl LauncherFunc	(int);
}
HMODULE			hLauncher		= NULL;
LauncherFunc*	pLauncher		= NULL;

void InitLauncher(){
	if(hLauncher)
		return;
	hLauncher	= LoadLibrary	("xrLauncher.dll");
	if (0==hLauncher)	R_CHK	(GetLastError());
	R_ASSERT2		(hLauncher,"xrLauncher DLL raised exception during loading or there is no xrLauncher.dll at all");

	pLauncher = (LauncherFunc*)GetProcAddress(hLauncher,"RunXRLauncher");
	R_ASSERT2		(pLauncher,"Cannot obtain RunXRLauncher function from xrLauncher.dll");
};

void FreeLauncher(){
	if (hLauncher)	{ 
		FreeLibrary(hLauncher); 
		hLauncher = NULL; pLauncher = NULL; };
}

int doLauncher()
{
/*
	execUserScript();
	InitLauncher();
	int res = pLauncher(0);
	FreeLauncher();
	if(res == 1) // do benchmark
		g_bBenchmark = true;

	if(g_bBenchmark){ //perform benchmark cycle
		doBenchmark();
	
		// InitLauncher	();
		// pLauncher	(2);	//show results
		// FreeLauncher	();

		Core._destroy			();
		return					(1);

	};
	if(res==8){//Quit
		Core._destroy			();
		return					(1);
	}
*/
	return 0;
}

void doBenchmark(LPCSTR name)
{
	g_bBenchmark = true;
	string_path in_file;
	FS.update_path(in_file,"$app_data_root$", name);
	CInifile ini(in_file);
	int test_count = ini.line_count("benchmark");
	LPCSTR test_name,t;
	shared_str test_command;
	for(int i=0;i<test_count;++i){
		ini.r_line			( "benchmark", i, &test_name, &t);
		strcpy_s				(g_sBenchmarkName, test_name);
		
		test_command		= ini.r_string_wb("benchmark",test_name);
		strcpy_s			(Core.Params,*test_command);
		_strlwr_s				(Core.Params);
		
		InitInput					();
		if(i){
			ZeroMemory(&HW,sizeof(CHW));
			InitEngine();
		}


		Engine.External.Initialize	( );

		strcpy_s						(Console->ConfigFile,  sizeof(Console->ConfigFile), "user.ltx");
		if (strstr(Core.Params,"-ltx ")) {
			string64				c_name;
			sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
			strcpy_s				(Console->ConfigFile,  sizeof(Console->ConfigFile), c_name);
		}

		Startup	 				();
	}
}

void CApplication::load_draw_internal()
{
	if(!sh_progress){
		CHK_DX			(HW.pDevice->Clear(0,0,D3DCLEAR_TARGET,D3DCOLOR_ARGB(0,0,0,0),1,0));
		return;
	}
		// Draw logo
		u32	Offset;
		u32	C						= 0xffffffff;
		u32	_w						= Device.dwWidth;
		u32	_h						= Device.dwHeight;
		FVF::TL* pv					= NULL;

//progress
		float bw					= 1024.0f;
		float bh					= 768.0f;
		Fvector2					k; k.set(float(_w)/bw, float(_h)/bh);

		RCache.set_Shader			(sh_progress);
		CTexture*	T				= RCache.get_ActiveTexture(0);
		Fvector2					tsz;
		tsz.set						((float)T->get_Width(),(float)T->get_Height());
		Frect						back_text_coords;
		Frect						back_coords;
		Fvector2					back_size;

//progress background
		static float offs			= -0.5f;

		back_size.set				(1024,768);
		back_text_coords.lt.set		(0,0);back_text_coords.rb.add(back_text_coords.lt,back_size);
		back_coords.lt.set			(offs, offs); back_coords.rb.add(back_coords.lt,back_size);

		back_coords.lt.mul			(k);back_coords.rb.mul(k);

		back_text_coords.lt.x/=tsz.x; back_text_coords.lt.y/=tsz.y; back_text_coords.rb.x/=tsz.x; back_text_coords.rb.y/=tsz.y;
		pv							= (FVF::TL*) RCache.Vertex.Lock(4,ll_hGeom.stride(),Offset);
		pv->set						(back_coords.lt.x,	back_coords.rb.y,	C,back_text_coords.lt.x,	back_text_coords.rb.y);	pv++;
		pv->set						(back_coords.lt.x,	back_coords.lt.y,	C,back_text_coords.lt.x,	back_text_coords.lt.y);	pv++;
		pv->set						(back_coords.rb.x,	back_coords.rb.y,	C,back_text_coords.rb.x,	back_text_coords.rb.y);	pv++;
		pv->set						(back_coords.rb.x,	back_coords.lt.y,	C,back_text_coords.rb.x,	back_text_coords.lt.y);	pv++;
		RCache.Vertex.Unlock		(4,ll_hGeom.stride());

		RCache.set_Geometry			(ll_hGeom);
		RCache.Render				(D3DPT_TRIANGLELIST,Offset,0,4,0,2);

//progress bar
		back_size.set				(268,37);
		back_text_coords.lt.set		(0,768);back_text_coords.rb.add(back_text_coords.lt,back_size);
		back_coords.lt.set			(379 ,726);back_coords.rb.add(back_coords.lt,back_size);

		back_coords.lt.mul			(k);back_coords.rb.mul(k);

		back_text_coords.lt.x/=tsz.x; back_text_coords.lt.y/=tsz.y; back_text_coords.rb.x/=tsz.x; back_text_coords.rb.y/=tsz.y;



		u32 v_cnt					= 40;
		pv							= (FVF::TL*)RCache.Vertex.Lock	(2*(v_cnt+1),ll_hGeom2.stride(),Offset);
		FVF::TL* _pv				= pv;
		float pos_delta				= back_coords.width()/v_cnt;
		float tc_delta				= back_text_coords.width()/v_cnt;
		u32 clr = C;

		if (load_stage > max_load_stage)
			Msg("!#WARN: load_stage = %d / %d", load_stage, max_load_stage);

		for(u32 idx=0; idx<v_cnt+1; ++idx){
			clr =					calc_progress_color(idx,v_cnt,load_stage,max_load_stage);
			pv->set					(back_coords.lt.x+pos_delta*idx+offs,	back_coords.rb.y+offs,	0+EPS_S, 1, clr, back_text_coords.lt.x+tc_delta*idx,	back_text_coords.rb.y);	pv++;
			pv->set					(back_coords.lt.x+pos_delta*idx+offs,	back_coords.lt.y+offs,	0+EPS_S, 1, clr, back_text_coords.lt.x+tc_delta*idx,	back_text_coords.lt.y);	pv++;
		}
		VERIFY						(u32(pv-_pv)==2*(v_cnt+1));
		RCache.Vertex.Unlock		(2*(v_cnt+1),ll_hGeom2.stride());

		RCache.set_Geometry			(ll_hGeom2);
		RCache.Render				(D3DPT_TRIANGLESTRIP, Offset, 2*v_cnt);


		// Draw title
		VERIFY						(pFontSystem);
		pFontSystem->Clear			();
		pFontSystem->SetColor		(color_rgba(157,140,120,255));
		pFontSystem->SetAligment	(CGameFont::alCenter);
		static string256 real_title = { 0 };
		if (NULL == strstr(app_title, "#"))
			strcpy_s(real_title, 256, app_title);

		pFontSystem->OutI			(0.f,0.815f, real_title);
		pFontSystem->OnRender		();


//draw level-specific screenshot
		if(hLevelLogo){
			Frect						r;
			r.lt.set					(257,369);
			r.lt.x						+= offs;
			r.lt.y						+= offs;
			r.rb.add					(r.lt,Fvector2().set(512,256));
			r.lt.mul					(k);						
			r.rb.mul					(k);						
			pv							= (FVF::TL*) RCache.Vertex.Lock(4,ll_hGeom.stride(),Offset);
			pv->set						(r.lt.x,				r.rb.y,		C, 0, 1);	pv++;
			pv->set						(r.lt.x,				r.lt.y,		C, 0, 0);	pv++;
			pv->set						(r.rb.x,				r.rb.y,		C, 1, 1);	pv++;
			pv->set						(r.rb.x,				r.lt.y,		C, 1, 0);	pv++;
			RCache.Vertex.Unlock		(4,ll_hGeom.stride());

			RCache.set_Shader			(hLevelLogo);
			RCache.set_Geometry			(ll_hGeom);
			RCache.Render				(D3DPT_TRIANGLELIST,Offset,0,4,0,2);
		}

}

u32 calc_progress_color(u32 idx, u32 total, int stage, int max_stage)
{
	if(idx>(total/2)) 
		idx	= total-idx;


	float kk			= (float(stage+1)/float(max_stage))*(total/2.0f);
	float f				= 1/(exp((float(idx)-kk)*0.5f)+1.0f);

	return color_argb_f		(f,1.0f,1.0f,1.0f);
}
