#include "stdafx.h"
#pragma hdrstop

#include "xrdebug.h"

#include "dxerr.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include <malloc.h>
#include <direct.h>
#pragma warning(pop)


BOOL					crash_flag = FALSE;

#pragma optimize("gyts", off)

typedef struct _STACK_SLOT {
	int		desc;
	void	*ptr;
} STACK_SLOT;

typedef xr_vector   <STACK_SLOT> DEBUG_STACK;
xr_map <DWORD, DEBUG_STACK*>   debug_stacks;




// alpet: user-friendly crashes )
#define ERROR_MESSAGES_RU

#ifdef ERROR_MESSAGES_RU

#define  MSG_SEE_LOG			"�������� ��� ���� � �������� � ��������� �����������\r\n"
#define  MSG_PRESS_CANCEL		"%s������� \"������\" ��� ���������� ����������%s"
#define  MSG_PRESS_RETRY		"������� \"���������\" ��� ����������� ����������%s"
#define  MSG_PRESS_CONTINUE		"������� \"����������\" ��� ����������� ���������� �\r\n ������������� ������� ������ %s%s"
#define  MSG_FATAL_ERROR_OK		"������������ ������\n\n������� OK ��� ���������� ���������"
#define  MSG_OK_TO_ABORT		"������� OK ��� ���������� ���������\r\n"
#define  ERR_HANDLER_BASE		"������ ������� ���������� ������"
#define  ERR_HANDLER_INVP		"������ ���������� ������ '�������� ��������' "
#define  ERR_OUT_OF_MEMORY		"����������� ��������� ����������� ������"
#define  ERR_PURE_VFUNC			"����� ����������� ������� �� ������������ ��� ������ �������"
#define	 ERR_UNEXP_TERM			"����������� ���������� ���������"
// #define  ERR_APP_ABORTING	"��������� ������������� ����������"
#define  ERR_APP_ABORTING		"���������� ��������"
#define  ERR_FLOAT_POINT		"������ ���������� � ��������� ������"
#define  ERR_ILLEGAL_INSTR		"�������� ���������� ��"
#define  ERR_TERMINATION_3		"���������� � ����� 3"

#else

#define  MSG_SEE_LOG			"See log file and minidump for detailed information\r\n"
#define  MSG_PRESS_CANCEL		"%sPress CANCEL to abort execution%s"
#define  MSG_PRESS_RETRY		"Press TRY AGAIN to continue execution%s"
#define  MSG_PRESS_CONTINUE		"Press CONTINUE to continue execution and ignore all the errors of this type%s%s"
#define  MSG_FATAL_ERROR_OK		"Fatal error occured\n\nPress OK to abort program execution"
#define  MSG_OK_TO_ABORT		"Press OK to abort execution\r\n"
#define  ERR_HANDLER_BASE		"base error handler is invoked!"
#define  ERR_HANDLER_INPV		"invalid parameter error handler is invoked!"
#define  ERR_OUT_OF_MEMORY		"std: out of memory"
#define  ERR_PURE_VFUNC			"pure virtual function call"
#define	 ERR_UNEXP_TERM			"unexpected program termination"
#define  ERR_APP_ABORTING		"application is aborting"
#define  ERR_FLOAT_POINT		"floating point error"
#define  ERR_ILLEGAL_INSTR		"illegal instruction"
#define  ERR_TERMINATION_3		"termination with exit code 3"


#endif


extern bool shared_str_initialized;
extern bool force_flush_log;

// KD: we don't need BugTrap since it provides _only_ nice ui window and e-mail sending
#ifdef __BORLANDC__
    #	include "d3d9.h"
    #	include "d3dx9.h"
    #	include "D3DX_Wrapper.h"
    #	pragma comment(lib,"EToolsB.lib")
    #	define DEBUG_INVOKE	DebugBreak()
        static BOOL			bException	= TRUE;
//    #   define USE_BUG_TRAP
#else
//    #   define USE_BUG_TRAP
#ifdef _WIN64
    #	define DEBUG_INVOKE	DebugBreak()
#else
    #	define DEBUG_INVOKE	__asm int 3
#endif
        static BOOL			bException	= FALSE;
#endif

#ifndef _M_AMD64
#	ifndef __BORLANDC__
#		pragma comment(lib,"dxerr.lib")
#	endif
#endif

#include <dbghelp.h>						// MiniDump flags

#ifdef USE_BUG_TRAP
#ifdef _WIN64
#	include "bugtrap.h"						// for BugTrap functionality
#	pragma comment(lib,"BugTrap-x64.lib")		// Link to x64 dll
#else
#	include "bugtrap.h"						// for BugTrap functionality
    #ifndef __BORLANDC__
        #	pragma comment(lib,"BugTrap.lib")		// Link to ANSI DLL
    #else
        #	pragma comment(lib,"BugTrapB.lib")		// Link to ANSI DLL
    #endif
#endif
#endif // USE_BUG_TRAP

#include <new.h>							// for _set_new_mode
#include <signal.h>							// for signals

#if 1//def DEBUG
#	define USE_OWN_MINI_DUMP
#	define USE_OWN_ERROR_MESSAGE_WINDOW
#else // DEBUG
#	define USE_OWN_MINI_DUMP
#endif // DEBUG

XRCORE_API	xrDebug		Debug;

static bool	error_after_dialog = false;

extern void copy_to_clipboard	(const char *string);

void copy_to_clipboard	(const char *string)
{
	if (IsDebuggerPresent())
		return;

	if (!OpenClipboard(0))
		return;

	HGLOBAL				handle = GlobalAlloc(GHND | GMEM_DDESHARE,(strlen(string) + 1)*sizeof(char));
	if (!handle) {
		CloseClipboard	();
		return;
	}

	char				*memory = (char*)GlobalLock(handle);
	strcpy				(memory,string);
	GlobalUnlock		(handle);
	EmptyClipboard		();
	SetClipboardData	(CF_TEXT,handle);
	CloseClipboard		();
}

void update_clipboard	(const char *string)
{
#ifdef DEBUG
	if (IsDebuggerPresent())
		return;

	if (!OpenClipboard(0))
		return;

	HGLOBAL				handle = GetClipboardData(CF_TEXT);
	if (!handle) {
		CloseClipboard		();
		copy_to_clipboard	(string);
		return;
	}

	LPSTR				memory = (char*)GlobalLock(handle);
	u32					memory_length = xr_strlen(memory);
	u32					string_length = xr_strlen(string);
	LPSTR				buffer = (LPSTR)_alloca((memory_length + string_length + 1)*sizeof(char));
	strcpy				(buffer,memory);
	GlobalUnlock		(handle);

	strcat				(buffer,string);
	CloseClipboard		();
	copy_to_clipboard	(buffer);
#endif // DEBUG
}

extern LPCSTR BuildStackTrace();
extern char g_stackTrace[100][4096];
extern int	g_stackTraceCount;


void LogStackTrace(LPCSTR header)
{
	bool ss_init = shared_str_initialized; // alpet: ��� ��������� ����� ��� ���-����� ���� ���������� � shared_str::doc
	shared_str_initialized = false;
	__try
	{
		Msg("%s", header);
		BuildStackTrace();

		for (int i = 1; i < g_stackTraceCount; ++i)
			Msg(" %s", g_stackTrace[i]);

		FlushLog();
	}
	__finally
	{
		shared_str_initialized = ss_init;
	}
	
}

extern void BuildStackTrace(struct _EXCEPTION_POINTERS *g_BlackBoxUIExPtrs);

XRCORE_API void LogStackTraceEx(struct _EXCEPTION_POINTERS *pExPtrs)
{	
	// ������� ��� �������� ������ (!)
	bool ss_init = shared_str_initialized; // alpet: ��� ��������� ����� ��� ���-����� ���� ���������� � shared_str::doc
	

	if (pExPtrs->ContextRecord)
	__try	
	{		
		force_flush_log = true;
		shared_str_initialized = false;
		CONTEXT rec = *pExPtrs->ContextRecord; // ��������� ����� ����������
 		//  Msg("##DEBUG: sizeof(CONTEXT) = %d bytes ", sizeof(rec));
		// pExPtrs->ExceptionRecord = NULL;
		BuildStackTrace(pExPtrs); // ������ ������� ������������  ContextRecord.EIP
		if (g_stackTraceCount > 0)
			Msg("!Exception stack trace %d lines, EIP = 0x%08x, ESP = 0x%08x: \n* 0 %s",
					g_stackTraceCount, rec.Eip, rec.Esp, g_stackTrace[0]);
		else
			Msg("!BuildStackTrace produced %d lines. Exception EIP = 0x%08x, ESP = 0x%08x:\n* %s", 
					g_stackTraceCount, rec.Eip, rec.Esp, g_stackTrace[0]);

		for (int line = 1; line < g_stackTraceCount; line ++)
			 Msg("# %d %s", line, g_stackTrace[line]);

		Sleep(500);
	}
	__finally
	{
		shared_str_initialized = ss_init;
	}
}

typedef struct _FUNC_INFO {
	PVOID   pAddr;
	LPSTR   name; 
	LPDWORD pDisp;
	BOOL	result;
	HANDLE  hComplete;
} FUNC_INFO;


void safe_func_name(void *param)
{
	FUNC_INFO &info = *(FUNC_INFO*)param;
	__try
	{
		R_ASSERT(info.name);

		SymSetOptions(SymGetOptions() |
			SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_ANYTHING | SYMOPT_UNDNAME |	SYMOPT_LOAD_LINES);
		static HANDLE hPID = 0;
		if (hPID == 0)
		{
			hPID = (HANDLE)GetCurrentProcessId();
			string_path app_bin;
			GetModuleFileName(0, app_bin, sizeof(app_bin));
			_strlwr_s(app_bin, sizeof(app_bin));
			LPSTR exe = strstr(app_bin, "xr_3da.exe");
			if (exe)
				exe[0] = 0;

			SymInitialize(hPID, app_bin, TRUE);
		}

		PIMAGEHLP_SYMBOL sym = (PIMAGEHLP_SYMBOL) VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
		R_ASSERT2(sym, "VirtualAlloc failed to allocate 4K block");
		ZeroMemory(sym, 4096);
		sym->Size = 4096;
		sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
		sym->MaxNameLength = 1020;
		info.result = SymGetSymFromAddr(hPID, (DWORD)info.pAddr, info.pDisp, sym);
		LPCSTR name = sym->Name;
		if (info.result && xr_strlen(name) <= 1020)
		__try 
		{
		   strncpy_s(info.name, 1023, name, 1020);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			sprintf_s(info.name, 1023, "strncpy_s failed, sym->Name length = %d", xr_strlen(name));
		}
		VirtualFree(sym, 0, MEM_RELEASE);

	}
	__finally
	{
		SetEvent(info.hComplete);
	}
}
XRCORE_API BOOL GetFunctionInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp)
{
	FUNC_INFO info;
	info.hComplete = CreateEvent(NULL, TRUE, FALSE, NULL);
	info.name = name;
	info.pAddr = pAddr;
	info.pDisp = pDisp;
	info.result = FALSE;
	safe_func_name(&info);
	// thread_spawn(safe_func_name, "safe_func_name_getter", 128 * 1024, &info);
	// WaitForSingleObject(info.hComplete, 13333);
	CloseHandle(info.hComplete);
	return info.result;
}

XRCORE_API BOOL GetObjectInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp)
{
	sprintf_s(name, 64, "unknown @ 0x%p", pAddr);

	PUINT_PTR pContent = (PUINT_PTR)pAddr;
	if (pContent && !IsBadReadPtr(pContent, 4))
	{
		PUINT_PTR vftable = (PUINT_PTR)pContent[0];
		if (vftable && !IsBadReadPtr(vftable, 4))
		{
			BOOL result = GetFunctionInfo((LPVOID)vftable, name, pDisp);
			if (result)
			{
				LPSTR trunc = strstr(name,     "::`vftable'");
				if (trunc) 
				   strcpy_s(trunc, 1020 - xr_strlen(name), "*");
			}
			return result;

		}
	}
	return FALSE;
}


volatile ULONG g_in_filter = 0;

void DumpDebugStack()
{
	string2048 buff = { 0 };
	
	
	if (debug_stacks.size() > 0)
	{
		MsgCB("##DEBUG_STACK/CONTEXT: ");
		for (auto _it = debug_stacks.begin(); _it != debug_stacks.end(); _it++)
		{
			MsgCB("* for thread %d:", _it->first);
			DEBUG_STACK &debug_stack = *_it->second;
			for (int it = debug_stack.size() - 1; it >= 0; it--)
			{
				STACK_SLOT &s = debug_stack[it];
				buff[0] = 0;
				DWORD disp = 0;
				sprintf_s(buff, 2048, "%d. ", it);
				LPSTR line = buff + xr_strlen(buff);

				if (s.desc)
					GetFunctionInfo(s.ptr, line, &disp);
				else
					GetObjectInfo(s.ptr, line, &disp);

				line = buff + xr_strlen(buff);

				if (disp)
					sprintf_s(line, 2048 - xr_strlen(buff), " + 0x%x", disp);

				MsgCB("*\t%s", buff);
			} // for stack
		} // for thread iter
	}
}

DEBUG_STACK& ThreadDebugStack()
{
	DWORD tid = GetCurrentThreadId();
	auto it = debug_stacks.find(tid);
	if (it == debug_stacks.end())
	{
		debug_stacks[tid] = xr_new<DEBUG_STACK>();
		it = debug_stacks.find(tid);
	}
	return *it->second;
}


XRCORE_API size_t DebugStackPush(int desc, void *ptr)
{	
	DEBUG_STACK &debug_stack = ThreadDebugStack();
	size_t sz = debug_stack.size();
	STACK_SLOT s;
	s.desc = desc;
	s.ptr  = ptr;
	debug_stack.push_back(s);
	return sz;
}

XRCORE_API void DebugStackPop(size_t dest_sz)
{
	DEBUG_STACK &debug_stack = ThreadDebugStack();
	int loops = 0;
	while (debug_stack.size() > dest_sz) {
	   debug_stack.pop_back();
	   loops++;
	};
	if (debug_stack.size() > 100)
		Log("!#ERROR: to large debug stack");
}

u32 InnerExceptionFilter(PEXCEPTION_POINTERS pExPtrs)
{
	Log("!#FATAL_ERROR: InnerExceptionFilter executed");
	LogStackTraceEx ( pExPtrs ); 
	return EXCEPTION_EXECUTE_HANDLER;
}



u32 SimpleExceptionFilter (PEXCEPTION_POINTERS pExPtrs)
{
	string1024 buff = { 0 };
	ULONG in_count = InterlockedIncrement(&g_in_filter);
		
	buff[0] = 0;

#define  NT_ERROR            0xC0000000
#define  CPP_ABORT		     0x40000015		

	if (pExPtrs)
	__try
	{
		auto rec = pExPtrs->ExceptionRecord;
		string128 code_info;
		sprintf_s(code_info, 128, "0x%08x", rec->ExceptionCode);
		u32 e0 = rec->ExceptionInformation[0];
		u32 e1 = rec->ExceptionInformation[1];

		switch (rec->ExceptionCode)
		{
		case	STATUS_TIMEOUT:		strcpy_s(code_info, 128,  "Timeout"); break;
		case STATUS_BREAKPOINT:		strcpy_s(code_info, 128,  "Breakpoint"); break;
	    case STATUS_SINGLE_STEP:	strcpy_s(code_info, 128,  "Single step or data break"); break;
		case NT_ERROR + 0x005:		sprintf_s(code_info, 128, "������ ������� %s ������ �� ������ 0x%08x ", (e0 > 0 ? "������" : "������"), e1); break;
		case NT_ERROR + 0x006:		strcpy_s(code_info, 128,  "In page Error"); break;
		case NT_ERROR + 0x008:		strcpy_s(code_info, 128,  "�������� ����������"); break;		
		case NT_ERROR + 0x017:		strcpy_s(code_info, 128,  "������������ ������"); break;
		case NT_ERROR + 0x01D:		strcpy_s(code_info, 128,  "Illegal instruction"); break;
		case NT_ERROR + 0x025:		strcpy_s(code_info, 128,  "Nonconttinuable Exception"); break;
		case NT_ERROR + 0x026:		strcpy_s(code_info, 128,  "Invalid disposition"); break;
		case NT_ERROR + 0x08C:		strcpy_s(code_info, 128,  "Array Bounds Exceeded"); break;
		case NT_ERROR + 0x094:		strcpy_s(code_info, 128,  "������������� ������� �� ����"); break;
		case NT_ERROR + 0x095:		strcpy_s(code_info, 128,  "������������� ������������"); break;
		case NT_ERROR + 0x096:		strcpy_s(code_info, 128,  "����������������� ����������"); break;
		case NT_ERROR + 0x0FD:		strcpy_s(code_info, 128,  "������������ �����"); break;
		case NT_ERROR + 0x13A:		strcpy_s(code_info, 128,  "����� Ctrl + C"); break;
		case CPP_ABORT:				strcpy_s(code_info, 128, "C/C++ Abort Exception"); break;
		}

		string1024 func_info = { 0 };
		DWORD disp = 0;

		sprintf_s(func_info, "0x%08p", rec->ExceptionAddress);

		if (GetFunctionInfo(rec->ExceptionAddress, func_info, &disp))
			sprintf_s(func_info + xr_strlen(func_info), 1024 - xr_strlen(func_info), " + 0x%04x", disp);
			

		sprintf_s(buff, 512, "'%s' at '%s', flags = 0x%04x ", code_info, func_info, rec->ExceptionFlags);
		MsgCB("##EXCEPTION_INFO:  %s", buff);
		for (int i = 0; i < (int)rec->NumberParameters; i++)
			MsgCB("# \t\t ExceptionInformation[%x] = 0x%p", i, (void*) rec->ExceptionInformation[i]);

		DumpDebugStack();

	}
	__except (InnerExceptionFilter(GetExceptionInformation()))
	{
	}

	LogStackTraceEx ( pExPtrs ); 
		
	if (IsDebuggerPresent())
	{
		SleepEx(500, FALSE);
		__asm int 3;
		InterlockedDecrement(&g_in_filter);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	if (in_count <= 1)
		Debug.fatal(DEBUG_INFO, "������� ������ ����������, ����������:\n %s", buff);
	InterlockedDecrement(&g_in_filter);

	return EXCEPTION_EXECUTE_HANDLER;
}


void gather_info		(const char *expression, const char *description, const char *argument0, const char *argument1, const char *file, int line, const char *function, LPSTR assertion_info)
{
	force_flush_log = true;
	LPSTR				buffer = assertion_info;
	
	LPCSTR				endline = "\n";
	LPCSTR				prefix = "![error]";
	bool				extended_description = (description && !argument0 && strchr(description,'\n'));
	for (int i=0; i<2; ++i) {
		if (!i)
			buffer		+= sprintf(buffer,"%s!#FATAL ERROR%s%s",endline,endline,endline);
		buffer			+= sprintf(buffer,"%sExpression    : %s%s",prefix,expression,endline);
		buffer			+= sprintf(buffer,"%sFunction      : %s%s",prefix,function,endline);
		buffer			+= sprintf(buffer,"%sFile          : %s%s",prefix,file,endline);
		buffer			+= sprintf(buffer,"%sLine          : %d%s",prefix,line,endline);
		
		if (extended_description) {
			buffer		+= sprintf(buffer,"%s%s%s",endline,description,endline);
			if (argument0) {
				if (argument1) {
					buffer	+= sprintf(buffer,"%s%s",argument0,endline);
					buffer	+= sprintf(buffer,"%s%s",argument1,endline);
				}
				else
					buffer	+= sprintf(buffer,"%s%s",argument0,endline);
			}
		}
		else {
			buffer		+= sprintf(buffer,"%sDescription   : %s%s",prefix,description,endline);
			if (argument0) {
				if (argument1) {
					buffer	+= sprintf(buffer,"%sArgument 0    : %s%s",prefix,argument0,endline);
					buffer	+= sprintf(buffer,"%sArgument 1    : %s%s",prefix,argument1,endline);
				}
				else
					buffer	+= sprintf(buffer,"%sArguments     : %s%s",prefix,argument0,endline);
			}
		}

		buffer			+= sprintf(buffer,"%s",endline);
		if (!i) {
			if (shared_str_initialized) {
				Msg		("%s",assertion_info);
				FlushLog();
			}
			buffer		= assertion_info;
			endline		= "\r\n";
			prefix		= "";
		}				
	}

#ifdef USE_MEMORY_MONITOR
	memory_monitor::flush_each_time	(true);
	memory_monitor::flush_each_time	(false);
#endif // USE_MEMORY_MONITOR	

	MsgCB("$#DUMP_CONTEXT"); // alpet: ����� ���������, ����� ����������� ����� �������
	HandleCrash("gather_info");

	if (!strstr(GetCommandLine(),"-no_call_stack_assert")) {

#ifdef USE_OWN_ERROR_MESSAGE_WINDOW


		buffer			+= sprintf(buffer, MSG_SEE_LOG);

//		buffer			+= sprintf(buffer,"stack trace:%s%s",endline,endline);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW
		bool ss_init = shared_str_initialized;
		shared_str_initialized = false;
		__try
		{
			BuildStackTrace();
			Msg("!stack trace:\n");
			for (int i = 2; i < g_stackTraceCount; ++i)
				Msg("!\t %s", g_stackTrace[i]);
		}
		__finally
		{
			shared_str_initialized = ss_init;
		}


#ifdef USE_OWN_ERROR_MESSAGE_WINDOW
//			buffer		+= sprintf(buffer,"%s%s",g_stackTrace[i],endline);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW
				

		if (!IsDebuggerPresent())
			 copy_to_clipboard	(assertion_info);
	}
}

void xrDebug::do_exit	(const std::string &message)
{
	FlushLog			();
	MessageBox			(NULL,message.c_str(),"Error",MB_OK|MB_ICONERROR|MB_SYSTEMMODAL);
	TerminateProcess	(GetCurrentProcess(),1);
}

void xrDebug::backend	(const char *expression, const char *description, const char *argument0, const char *argument1, const char *file, int line, const char *function, bool &ignore_always)
{	
	force_flush_log = true;

	static xrCriticalSection CS
#ifdef PROFILE_CRITICAL_SECTIONS
	(MUTEX_PROFILE_ID(xrDebug::backend))
#endif // PROFILE_CRITICAL_SECTIONS
	;

	CS.Enter			();

	error_after_dialog	= true;

	string4096			assertion_info;
	gather_info			(expression, description, argument0, argument1, file, line, function, assertion_info);

#ifdef USE_OWN_ERROR_MESSAGE_WINDOW
	LPCSTR				endline = "\r\n";
	LPSTR				buffer = assertion_info + xr_strlen(assertion_info);
	buffer				+= sprintf(buffer, MSG_PRESS_CANCEL,   endline,endline);
	buffer				+= sprintf(buffer, MSG_PRESS_RETRY,    endline);
	buffer				+= sprintf(buffer, MSG_PRESS_CONTINUE, endline,endline);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW

	if (handler)
		handler			();

	if (get_on_dialog())
		get_on_dialog()	(true);

#ifdef XRCORE_STATIC
	MessageBox			(NULL,assertion_info,"X-Ray error",MB_OK|MB_ICONERROR|MB_SYSTEMMODAL);
#else
#	ifdef USE_OWN_ERROR_MESSAGE_WINDOW
		ShowWindow(GetTopWindow(NULL), SW_MINIMIZE);
		ShowCursor(TRUE);
		MSG msg;
		for (int i = 0; i < 128; i++)
		  if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) 
		    {
  			   TranslateMessage(&msg);
			   DispatchMessage(&msg);
			}

		int					result = 
			MessageBox(
				NULL,
				assertion_info,
				"��������� ������",
				MB_CANCELTRYCONTINUE|MB_ICONERROR|MB_TASKMODAL
			);

		ShowWindow(GetTopWindow(NULL), SW_SHOW);
		switch (result) {
			case IDCANCEL : {
				DEBUG_INVOKE;
				break;
			}
			case IDTRYAGAIN : {
				error_after_dialog	= false;
				break;
			}
			case IDCONTINUE : {
				error_after_dialog	= false;
				ignore_always	= true;
				break;
			}
			default : NODEFAULT;
		}
#	else // USE_OWN_ERROR_MESSAGE_WINDOW
#		ifdef USE_BUG_TRAP
			BT_SetUserMessage	(assertion_info);
#		endif // USE_BUG_TRAP
		DEBUG_INVOKE;
#	endif // USE_OWN_ERROR_MESSAGE_WINDOW
#endif

	if (get_on_dialog())
		get_on_dialog()	(false);

	CS.Leave			();
}

LPCSTR xrDebug::error2string	(long code)
{
	LPCSTR				result	= 0;
	static	string1024	desc_storage;

#ifdef _M_AMD64
#else
	result				= DXGetErrorDescription	(code);
#endif
	if (0==result) 
	{
		FormatMessage	(FORMAT_MESSAGE_FROM_SYSTEM,0,code,0,desc_storage,sizeof(desc_storage)-1,0);
		result			= desc_storage;
	}
	return		result	;
}

void xrDebug::error		(long hr, const char* expr, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		(error2string(hr),expr,0,0,file,line,function,ignore_always);
}

void xrDebug::error		(long hr, const char* expr, const char* e2, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		(error2string(hr),expr,e2,0,file,line,function,ignore_always);
}

void xrDebug::fail		(const char *e1, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		("assertion failed",e1,0,0,file,line,function,ignore_always);
}

void xrDebug::fail		(const char *e1, const std::string &e2, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		(e1,e2.c_str(),0,0,file,line,function,ignore_always);
}

void xrDebug::fail		(const char *e1, const char *e2, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		(e1,e2,0,0,file,line,function,ignore_always);
}

void xrDebug::fail		(const char *e1, const char *e2, const char *e3, const char *file, int line, const char *function, bool &ignore_always)
{
	backend		(e1,e2,e3,0,file,line,function,ignore_always);
}

void xrDebug::fail		(const char *e1, const char *e2, const char *e3, const char *e4, const char *file, int line, const char *function, bool &ignore_always)
{	
	backend		(e1,e2,e3,e4,file,line,function,ignore_always);
}

void __cdecl xrDebug::fatal(const char *file, int line, const char *function, const char* F, ...)
{
	string4096	buffer;
	crash_flag = TRUE;
	__try
	{

		OutputDebugString(" xrDebug::fatal executing ");
		if (file)
		{
			sprintf_s(buffer, MAX_PATH, "from %s:%d", file, line);
			OutputDebugString(buffer);
		}
		if (function)
		{
			sprintf_s(buffer, 4096, " in function %s ", function);
			OutputDebugString(buffer);
		}

		va_list		p;
		va_start(p, F);
		vsprintf(buffer, F, p);
		OutputDebugString(buffer);
		va_end(p);

		bool		ignore_always = true;
		backend("fatal error", "<no expression>", buffer, 0, file, line, function, ignore_always);
	}
	__finally
	{
		crash_flag = FALSE;
	}
}

void xrDebug::verify_error		(LPCSTR e1, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s", file, line, function, e1); }
void xrDebug::verify_error		(LPCSTR e1, const std::string &e2, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s %s", file, line, function, e1, e2.c_str()); }
void xrDebug::verify_error		(LPCSTR e1, LPCSTR e2, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s %s", file, line, function, e1, e2); }
void xrDebug::verify_error		(LPCSTR e1, LPCSTR e2, LPCSTR e3, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s %s %s", file, line, function, e1, e2, e3); }
void xrDebug::verify_error		(LPCSTR e1, LPCSTR e2, LPCSTR e3, LPCSTR e4, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s %s %s %s", file, line, function, e1, e2, e3, e4); }
void xrDebug::verify_error		(LPCSTR e1, LPCSTR e2, LPCSTR e3, LPCSTR e4, LPCSTR e5, LPCSTR file, int line, LPCSTR function)
{	Msg("!VERIFY_FAILED: %s[%d] {%s}  %s %s %s %s %s", file, line, function, e1, e2, e3, e4, e5); }


int out_of_memory_handler	(size_t size)
{
	Memory.mem_compact		();
#ifndef _EDITOR
	u32						crt_heap		= mem_usage_impl((HANDLE)_get_heap_handle(),0,0);
#else // _EDITOR
	u32						crt_heap		= 0;
#endif // _EDITOR
	u32						process_heap	= mem_usage_impl(GetProcessHeap(),0,0);
	int						eco_strings		= (int)g_pStringContainer->stat_economy			();
	int						eco_smem		= (int)g_pSharedMemoryContainer->stat_economy	();
	Msg						("* [x-ray]: crt heap[%d K], process heap[%d K]",crt_heap/1024,process_heap/1024);
	Msg						("* [x-ray]: economy: strings[%d K], smem[%d K]",eco_strings/1024,eco_smem);
	Debug.fatal				(DEBUG_INFO,"Out of memory. Memory request: %d K",size/1024);
	return					1;
}

extern LPCSTR log_name();

void CALLBACK PreErrorHandler	(INT_PTR)
{
#ifdef USE_BUG_TRAP
	if (!xr_FS || !FS.m_Flags.test(CLocatorAPI::flReady))
		return;

	string_path				log_folder;

	__try {
		FS.update_path		(log_folder,"$logs$","");
		if ((log_folder[0] != '\\') && (log_folder[1] != ':')) {
			string256		current_folder;
			_getcwd			(current_folder,sizeof(current_folder));
			
			string256		relative_path;
			strcpy			(relative_path,log_folder);
			strconcat		(sizeof(log_folder),log_folder,current_folder,"\\",relative_path);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		strcpy				(log_folder,"logs");
	}

	BT_AddLogFile			(log_name());
#endif // USE_BUG_TRAP
}

#ifdef USE_BUG_TRAP
void SetupExceptionHandler	(const bool &dedicated)
{
	BT_InstallSehFilter		();
#ifndef USE_OWN_ERROR_MESSAGE_WINDOW
	if (!dedicated && !strstr(GetCommandLine(),"-silent_error_mode"))
		BT_SetActivityType	(BTA_SHOWUI);
	else
		BT_SetActivityType	(BTA_SAVEREPORT);
#else // USE_OWN_ERROR_MESSAGE_WINDOW
	BT_SetActivityType		(BTA_SAVEREPORT);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW

	BT_SetDialogMessage				(
		BTDM_INTRO2,
		"\
This is XRay Engine crash reporting client. \
To help the development process, \
please Submit Bug or save report and email it manually (button More...).\
\r\nMany thanks in advance and sorry for the inconvenience."
	);

	BT_SetPreErrHandler		(PreErrorHandler,0);
	BT_SetAppName			("XRay Engine");
	BT_SetReportFormat		(BTRF_TEXT);
	BT_SetFlags				(BTF_DETAILEDMODE | /**BTF_EDIETMAIL | /**/BTF_ATTACHREPORT /**| BTF_LISTPROCESSES /**| BTF_SHOWADVANCEDUI /**| BTF_SCREENCAPTURE/**/);
	BT_SetDumpType			(
		MiniDumpWithDataSegs |
//		MiniDumpWithFullMemory |
//		MiniDumpWithHandleData |
//		MiniDumpFilterMemory |
//		MiniDumpScanMemory |
//		MiniDumpWithUnloadedModules |
#ifndef _EDITOR
		MiniDumpWithIndirectlyReferencedMemory |
#endif // _EDITOR
//		MiniDumpFilterModulePaths |
//		MiniDumpWithProcessThreadData |
//		MiniDumpWithPrivateReadWriteMemory |
//		MiniDumpWithoutOptionalData |
//		MiniDumpWithFullMemoryInfo |
//		MiniDumpWithThreadInfo |
//		MiniDumpWithCodeSegs |
		0
	);
	BT_SetSupportEMail		("crash-report@stalker-game.com");
//	BT_SetSupportServer		("localhost", 9999);
//	BT_SetSupportURL		("www.gsc-game.com");
}
#endif // USE_BUG_TRAP

#if 1
extern void BuildStackTrace(struct _EXCEPTION_POINTERS *pExceptionInfo);
typedef LONG WINAPI UnhandledExceptionFilterType(struct _EXCEPTION_POINTERS *pExceptionInfo);
typedef LONG ( __stdcall *PFNCHFILTFN ) ( EXCEPTION_POINTERS * pExPtrs ) ;
extern "C" BOOL __stdcall SetCrashHandlerFilter ( PFNCHFILTFN pFn );

static UnhandledExceptionFilterType	*previous_filter = 0;

#ifdef USE_OWN_MINI_DUMP
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
										 CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
										 CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
										 CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
										 );

void FlushLog				(LPCSTR file_name);


HMODULE LoadDebugHlp()
{
	HMODULE hDll = GetModuleHandle ("dbghelp.dll");
	if (hDll)
		return hDll;

	string_path		szDbgHelpPath;

	if (GetModuleFileName(NULL, szDbgHelpPath, _MAX_PATH))
	{
		char *pSlash = strchr(szDbgHelpPath, '\\');
		if (pSlash)
		{
			strcpy(pSlash + 1, "DBGHELP.DLL");
			// alpet: �������� ������ ����������, �.�. ������ ���� ����� ����� ������ � ������� ����� �������
			DWORD nope;
			DWORD size = GetFileVersionInfoSize(szDbgHelpPath, &nope);
			if (size > 0)
			{
				LPVOID ver_data = xr_malloc(size);
				if (GetFileVersionInfo(szDbgHelpPath, NULL, size, ver_data))
				{
					VS_FIXEDFILEINFO *info = NULL;
					UINT len;
					VerQueryValue(ver_data, "\\", (LPVOID*)&info, &len);
					if (info && info->dwFileVersionMS >= 6)
						hDll = ::LoadLibrary(szDbgHelpPath);
					else
						Msg("!#ERROR: dbghelp.dll version is old for this build.");
				}

				xr_free(ver_data);
			}



		}
	}

	if (hDll == NULL)
	{
		// load any version we can
		hDll = ::LoadLibrary("DBGHELP.DLL");
	}

	return hDll;
}

void save_mini_dump			(_EXCEPTION_POINTERS *pExceptionInfo)
{
	// firstly see if dbghelp.dll is around and has the function we need
	// look next to the EXE first, as the one in System32 might be old 
	// (e.g. Windows 2000)
	LPCTSTR szResult = NULL;
	HMODULE hDll = LoadDebugHlp();

	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress( hDll, "MiniDumpWriteDump" );
		if (pDump)
		{
			string_path	szDumpPath;
			string_path	szScratch;
			string64	t_stemp;

			timestamp	(t_stemp);
			strcpy		( szDumpPath, Core.ApplicationName);
			strcat		( szDumpPath, "_"					);
			strcat		( szDumpPath, Core.UserName			);
			strcat		( szDumpPath, "_"					);
			strcat		( szDumpPath, t_stemp				);
			strcat		( szDumpPath, ".mdmp"				);

			__try {
				if (FS.path_exist("$logs$"))
					FS.update_path	(szDumpPath,"$logs$",szDumpPath);
			}
            __except( EXCEPTION_EXECUTE_HANDLER ) {
				string_path	temp;
				strcpy		(temp,szDumpPath);
				strcpy		(szDumpPath,"logs/");
				strcat		(szDumpPath,temp);
            }

			string_path	log_file_name;
			strconcat	(sizeof(log_file_name), log_file_name, szDumpPath, ".log");
			FlushLog	(log_file_name);

			// create the file
			HANDLE hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if (INVALID_HANDLE_VALUE==hFile)	
			{
				// try to place into current directory
				MoveMemory	(szDumpPath,szDumpPath+5,strlen(szDumpPath));
				hFile		= ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			}
			if (hFile!=INVALID_HANDLE_VALUE)
			{
				_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

				ExInfo.ThreadId				= ::GetCurrentThreadId();
				ExInfo.ExceptionPointers	= pExceptionInfo;
				ExInfo.ClientPointers		= NULL;

				// write the dump
				MINIDUMP_TYPE	dump_flags	= MINIDUMP_TYPE(MiniDumpNormal | MiniDumpFilterMemory | MiniDumpScanMemory );

				BOOL bOK = pDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, dump_flags, &ExInfo, NULL, NULL );
				if (bOK)
				{
					sprintf( szScratch, "Saved dump file to '%s'", szDumpPath );
					szResult = szScratch;
//					retval = EXCEPTION_EXECUTE_HANDLER;
				}
				else
				{
					sprintf( szScratch, "Failed to save dump file to '%s' (error %d)", szDumpPath, GetLastError() );
					szResult = szScratch;
				}
				::CloseHandle(hFile);
			}
			else
			{
				sprintf( szScratch, "Failed to create dump file '%s' (error %d)", szDumpPath, GetLastError() );
				szResult = szScratch;
			}
		}
		else
		{
			szResult = "DBGHELP.DLL too old";
		}
	}
	else
	{
		szResult = "DBGHELP.DLL not found";
	}
}
#endif // USE_OWN_MINI_DUMP

void format_message	(LPSTR buffer, const u32 &buffer_size)
{
    LPVOID		message;
    DWORD		error_code = GetLastError(); 

	if (!error_code) {
		*buffer	= 0;
		return;
	}

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message,
        0,
		NULL
	);

	sprintf		(buffer,"[error][%8d]    : %s",error_code,message);
    LocalFree	(message);
}

LONG WINAPI UnhandledFilter	(_EXCEPTION_POINTERS *pExceptionInfo)
{
	string256				error_message;
	format_message			(error_message,sizeof(error_message));

	force_flush_log = true;

	if (!error_after_dialog && !strstr(GetCommandLine(),"-no_call_stack_assert"))
	__try
	{
		CONTEXT				save = *pExceptionInfo->ContextRecord;
		BuildStackTrace		(pExceptionInfo);
		*pExceptionInfo->ContextRecord = save;
				
		MsgCB			("$#DUMP_CONTEXT");
		Msg				("Unhandled exception stack trace:\n");
		copy_to_clipboard	("stack trace:\r\n\r\n");

		string4096			buffer;
		for (int i=0; i<g_stackTraceCount; ++i) 
		{			
			Msg			("%s",g_stackTrace[i]);
			sprintf			(buffer,"%s\r\n",g_stackTrace[i]);
			update_clipboard(buffer);
		}

		if (*error_message) 
		{			
			Msg			("\n%s",error_message);
			strcat			(error_message,"\r\n");
			update_clipboard(error_message);
		}
	}
	__finally
	{
		FlushLog();
	}

#ifdef USE_OWN_MINI_DUMP
		save_mini_dump		(pExceptionInfo);
#endif // USE_OWN_MINI_DUMP
#ifdef USE_OWN_ERROR_MESSAGE_WINDOW
	if (!error_after_dialog) {
		if (Debug.get_on_dialog())
			Debug.get_on_dialog()	(true);

		MessageBox(NULL, MSG_FATAL_ERROR_OK, "Fatal error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
	}
#endif // USE_OWN_ERROR_MESSAGE_WINDOW

	if (!previous_filter) {
#ifdef USE_OWN_ERROR_MESSAGE_WINDOW
		if (Debug.get_on_dialog())
			Debug.get_on_dialog()	(false);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW

		return				(EXCEPTION_CONTINUE_SEARCH) ;
	}

	previous_filter			(pExceptionInfo);

#ifdef USE_OWN_ERROR_MESSAGE_WINDOW
	if (Debug.get_on_dialog())
		Debug.get_on_dialog()		(false);
#endif // USE_OWN_ERROR_MESSAGE_WINDOW

	return					(EXCEPTION_CONTINUE_SEARCH) ;
}
#endif

//////////////////////////////////////////////////////////////////////
#ifdef M_BORLAND
	namespace std{
		extern new_handler _RTLENTRY _EXPFUNC set_new_handler( new_handler new_p );
	};

	static void __cdecl def_new_handler() 
    {
		FATAL		("Out of memory.");
    }

    void	xrDebug::_initialize		(const bool &dedicated)
    {
		handler							= 0;
		m_on_dialog						= 0;
        std::set_new_handler			(def_new_handler);	// exception-handler for 'out of memory' condition
//		::SetUnhandledExceptionFilter	(UnhandledFilter);	// exception handler to all "unhandled" exceptions
    }
#else
    typedef int		(__cdecl * _PNH)( size_t );
    _CRTIMP int		__cdecl _set_new_mode( int );
    _CRTIMP _PNH	__cdecl _set_new_handler( _PNH );

#ifndef USE_BUG_TRAP
	void _terminate		()
	{
		if (strstr(GetCommandLine(),"-silent_error_mode"))
			exit				(-1);

		string4096				assertion_info;
		
		gather_info				(
			"<no expression>",
			"Unexpected application termination",
			0,
			0,
	#ifdef _ANONYMOUS_BUILD
			"",
			0,
	#else
			__FILE__,
			__LINE__,
	#endif
	#ifndef _EDITOR
			__FUNCTION__,
	#else // _EDITOR
			"",
	#endif // _EDITOR
			assertion_info
		);
		
		// LPCSTR					endline = "\r\n";
		LPSTR					buffer = assertion_info + xr_strlen(assertion_info);
		buffer					+= sprintf(buffer, MSG_OK_TO_ABORT);

		MessageBox				(
			GetTopWindow(NULL),
			assertion_info,
			"Fatal Error",
			MB_OK|MB_ICONERROR|MB_SYSTEMMODAL
		);
		
		exit					(-1);
	//	FATAL					("Unexpected application termination");
	}
#endif // USE_BUG_TRAP

	void debug_on_thread_spawn			()
	{
#ifdef USE_BUG_TRAP
		BT_SetTerminate					();
#else // USE_BUG_TRAP
		std::set_terminate				(_terminate);
#endif // USE_BUG_TRAP
	}

	static void handler_base				(LPCSTR reason_string)
	{
		bool							ignore_always = false;
		Debug.backend					(
			ERR_HANDLER_BASE,
			reason_string,
			0,
			0,
			DEBUG_INFO,
			ignore_always
		);
	}

	static void invalid_parameter_handler	(
			const wchar_t *expression,
			const wchar_t *function,
			const wchar_t *file,
			unsigned int line,
			uintptr_t reserved
		)
	{
		bool							ignore_always = false;

		string4096						expression_;
		string4096						function_;
		string4096						file_;
		size_t							converted_chars = 0;
//		errno_t							err = 
		if (expression)
			wcstombs_s	(
				&converted_chars, 
				expression_,
				sizeof(expression_),
				expression,
				(wcslen(expression) + 1)*2*sizeof(char)
			);
		else
			strcpy_s					(expression_,"");

		if (function)
			wcstombs_s	(
				&converted_chars, 
				function_,
				sizeof(function_),
				function,
				(wcslen(function) + 1)*2*sizeof(char)
			);
		else
			strcpy_s					(function_,__FUNCTION__);

		if (file)
			wcstombs_s	(
				&converted_chars, 
				file_,
				sizeof(file_),
				file,
				(wcslen(file) + 1)*2*sizeof(char)
			);
		else {
			line						= __LINE__;
			strcpy_s					(file_,__FILE__);
		}

		Debug.backend					(
			ERR_HANDLER_INVP,
			expression_,
			0,
			0,
			file_,
			line,
			function_,
			ignore_always
		);
	}

	static void std_out_of_memory_handler	()
	{
		MsgCB("!#FATAL: Oops! std_out_of_memory_handler executing...");
		handler_base					(ERR_OUT_OF_MEMORY);
	}

	static void pure_call_handler			()
	{
		handler_base					(ERR_PURE_VFUNC);
	}

#ifdef CS_USE_EXCEPTIONS
	static void unexpected_handler			()
	{
		handler_base					(ERR_UNEXP_TERM);
	}
#endif // CS_USE_EXCEPTIONS

	static void abort_handler				(int signal)
	{
		handler_base					(ERR_APP_ABORTING);
	}

	static void floating_point_handler		(int signal)
	{
		handler_base					(ERR_FLOAT_POINT);
	}

	static void illegal_instruction_handler	(int signal)
	{
		handler_base					(ERR_ILLEGAL_INSTR);
	}

//	static void storage_access_handler		(int signal)
//	{
//		handler_base					("illegal storage access");
//	}

	static void termination_handler			(int signal)
	{
		handler_base					(ERR_TERMINATION_3);
	}

    void	xrDebug::_initialize		(const bool &dedicated)
    {
		debug_on_thread_spawn			();

		_set_abort_behavior				(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		signal							(SIGABRT,			abort_handler);
		signal							(SIGABRT_COMPAT,	abort_handler);
		signal							(SIGFPE,			floating_point_handler);
		signal							(SIGILL,			illegal_instruction_handler);
		signal							(SIGINT,			0);
//		signal							(SIGSEGV,			storage_access_handler);
		signal							(SIGTERM,			termination_handler);

		_set_invalid_parameter_handler	(&invalid_parameter_handler);

		_set_new_mode					(1);
		_set_new_handler				(&out_of_memory_handler);
		std::set_new_handler			(&std_out_of_memory_handler);

		_set_purecall_handler			(&pure_call_handler);

#if 0// should be if we use exceptions
		std::set_unexpected				(_terminate);
#endif

#ifdef USE_BUG_TRAP
		SetupExceptionHandler			(dedicated);
#endif // USE_BUG_TRAP
		previous_filter					= ::SetUnhandledExceptionFilter(UnhandledFilter);	// exception handler to all "unhandled" exceptions

#if 0
		struct foo {static void	recurs	(const u32 &count)
		{
			if (!count)
				return;

			_alloca			(4096);
			recurs			(count - 1);
		}};
		foo::recurs			(u32(-1));
		std::terminate		();
#endif // 0
	}
#endif