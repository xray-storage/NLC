////////////////////////////////////////////////////////////////////////////
//	Module 		: script_vars_storage.cpp
//	Created 	: 19.10.2014
//  Modified    : 10.12.2014
//	Author		: Alexander Petrov
//	Description : global script vars class, with saving content to savegame
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "pch_script.h"
#include "alife_space.h"
#include "script_vars_storage.h"
#include "script_net_packet.h"
#include "script_engine.h"
#include "../lua_tools.h"
#include "../../xrNetServer/NET_utils.h"

// #pragma todo("alpet: ����� ������� �������� �����������")
// #pragma optimize("gyts", off)  
 
DLL_API CScriptVarsStorage g_ScriptVars;

#define  SMALL_VAR_SIZE	  sizeof(SCRIPT_VAR::s_value)

IC bool is_number(LPCSTR s)
{
	string16 tmp;
	int i = atoi(s);
	return (0 == _strcmpi(s, itoa(i, tmp, 10)));
}

void SCRIPT_VAR::release()
{
	SCRIPT_VAR sv = *this;
	ZeroMemory(this, sizeof(SCRIPT_VAR));

	if (LUA_TTABLE == sv.eff_type())
	{		
		sv.T->release(); // ����� �������� ������ � ��������
		return;
	}

	if (LUA_TNETPACKET == sv.eff_type())
	{		
		xr_delete(sv.P);
		return;
	}
		
	if (sv.type & SVT_ALLOCATED)
		xr_free(sv.data);	
}

void* SCRIPT_VAR::smart_alloc(int new_type, u32 cb) // ����� ��������������� ��������� ������
{
	// autodetect embedded var-size
	switch (new_type & 0xffff)
	{
	case LUA_TBOOLEAN: 
		cb = sizeof(b_value); break;
	case LUA_TNUMBER:
		cb = sizeof(n_value); break;	
	}

	size = cb;

	if (type & SVT_ALLOCATED) // ���� ������ ���������� ���� ������������ � ������
	{
		if (size > SMALL_VAR_SIZE)
			data = xr_realloc(data, size);
		else
			xr_free(data);
	}
	else
	{
		if (size > SMALL_VAR_SIZE)
			data = xr_malloc(size);	
	}

	if (size > SMALL_VAR_SIZE)
	{
		type = new_type | SVT_ALLOCATED;
		return data;
	}
	else
	{
		type = new_type;
		return &s_value;
	}
}

CScriptVarsTable::~CScriptVarsTable()
{
	clear();
}

void CScriptVarsTable::clear()
{
	auto it = m_map.begin();
	auto end = m_map.end();
	for (; it != end; it++)
		it->second.release();					
	m_map.clear();
}

int  CScriptVarsTable::load(IReader  &memory_stream)
{
	clear();	
	u32 var_count = memory_stream.r_u32();	
	is_array = true;
	int loaded = 0;
	// while (var_count-- > 0 && )  
	for (u32 n_var = 0; n_var < var_count && memory_stream.elapsed() >= 4; n_var++)
	{		
		loaded++;
		u32 pos = memory_stream.tell();
		if (-1 == memory_stream.r_s32())
		{
			Msg("!#ERROR: script vars table %s loaded vars %d from %d, and found EOT flag ", name(), n_var, var_count);			
			// return;
		}
		else
			memory_stream.seek(pos);

		shared_str var_name;
		SCRIPT_VAR sv;
		memory_stream.r_stringZ(var_name);		
		sv.type = (int)memory_stream.r_u32();
		sv.size = memory_stream.r_u32();
		MsgV ("3SCRIPT_VARS", "#DBG: CScriptVarsTable::load n_var = %3d type = 0x%08x   size = %5d  name =  %s.%s   ", n_var, sv.type, sv.size, name(), *var_name);
		if (LUA_TTABLE == sv.eff_type())
		{			
			memory_stream.seek (memory_stream.tell() - 4);
			sv.T = xr_new<CScriptVarsTable>();
			sv.T->add_ref();
			sv.T->is_array = (sv.type & SVT_ARRAY_TABLE) > 0; 
			sv.T->zero_key = (sv.type & SVT_ARRAY_ZEROK) > 0; 
			sv.T->m_name.sprintf("%s.%s", name(), *var_name);
			loaded += sv.T->load(memory_stream);
			m_map[var_name] = sv;
			continue;
		}

		

		if (LUA_TNETPACKET == sv.eff_type()	)
		{
			sv.P = xr_new<NET_Packet>();
			sv.P->B.count = sv.size;
			sv.P->r_seek(memory_stream.r_u32());			
			memory_stream.r (sv.P->B.data, sv.size);		
			m_map[var_name] = sv;
			continue;
		}
		
		{  // default var type
			if (sv.size > sizeof(sv.s_value))
			{
				sv.data = xr_malloc(sv.size);
				memory_stream.r(sv.data, sv.size);

				if (LUA_TSTRING == sv.eff_type() && -1 == *(int*)sv.data) // ����� ����� �� ������� ������
					Msg("!#WARN: probably string load failed = %s ", (LPCSTR) sv.data);

			}
			else
				memory_stream.r(&sv.s_value, sv.size);

			m_map[var_name] = sv;
		}		
		if (!is_number(*var_name))
			 is_array = false;
	}
		
	if (-1 != memory_stream.r_s32())
		Debug.fatal(DEBUG_INFO, "EOT flag not loaded for table %s", name());

	return loaded;
}

int CScriptVarsTable::save(IWriter  &memory_stream)
{
	memory_stream.w_u32(m_map.size());
	int saved = 0;
	auto it = m_map.begin();
	auto end = m_map.end();
	for (; it != end; it++)
	{
		saved++;
		SCRIPT_VAR &sv = it->second;
		memory_stream.w_stringZ(it->first);
		
		if (LUA_TTABLE == sv.eff_type())
		{
			sv.type = LUA_TTABLE;
			R_ASSERT3(sv.T, "SCRIPT_VAR.T unassigned for ", *it->first);

			if (sv.T->is_array)	sv.type |= SVT_ARRAY_TABLE;
			if (sv.T->zero_key)	sv.type |= SVT_ARRAY_ZEROK;			

			memory_stream.w_s32(sv.type);
			saved += sv.T->save(memory_stream);
			continue;
		}

		memory_stream.w_s32(sv.type);

		if (LUA_TNETPACKET == sv.eff_type())
		{			
			memory_stream.w_u32(sv.P->B.count); 
			memory_stream.w_u32(sv.P->r_tell());			
			memory_stream.w (&sv.P->B.data, sv.P->B.count);
			continue;
		}
		memory_stream.w_u32(sv.size); // sizeof *data
		if (sv.size > sizeof(sv.s_value))
			memory_stream.w (sv.data, sv.size);
		else
			memory_stream.w (&sv.s_value, sv.size);
	}

	memory_stream.w_s32(-1);
	return saved;
}

void CScriptVarsTable::release()
{ 
	CScriptVarsTable *self = this;
	if (self &&  --self->ref_count <= 0 && this != &g_ScriptVars) 
		xr_delete (self); 
}


int CScriptVarsStorage::load(IReader  &memory_stream)
{
	if (!memory_stream.find_chunk(SCRIPT_VARS_CHUNK_DATA))
#ifdef NLC_EXTENSIONS
	{
		Debug.fatal(DEBUG_INFO, "cannot find SCRIPT_VARS_CHUNK_DATA in memory_stream");
		return 0;
	}
#else
	   return 0; 
#endif
	int loaded = inherited::load(memory_stream);
	Msg	("* %d script vars are successfully loaded", loaded);
	return loaded;
}

int CScriptVarsStorage::save(IWriter  &memory_stream)
{
	if (0 == map().size()) return 0;

	memory_stream.open_chunk	(SCRIPT_VARS_CHUNK_DATA);
	int saved = inherited::save(memory_stream);
	memory_stream.close_chunk();
	
	Msg	("* %d script vars are successfully saved", saved);
	return saved;
}



using namespace luabind;
using namespace luabind::detail;



void register_method(lua_State *L, char *key, int mt_index, lua_CFunction f)
{	
	lua_pushcfunction(L, f);	
	lua_setfield(L, mt_index, key);
}

CScriptVarsTable *lua_tosvt(lua_State *L, int index, bool print_errors = true)
{
	if (!lua_isuserdata(L, index))
		return NULL;

	if (lua_objlen(L, index) != sizeof(SCRIPT_VAR))
	{
		if (print_errors)
		{
			Msg("# %s", GetLuaTraceback(L, 2));
			log_script_error("argument is not SCRIPT_VAR");
		}
		return NULL;
	}

	SCRIPT_VAR *sv = (SCRIPT_VAR*)lua_touserdata(L, index);	

	if (sv->type == LUA_TTABLE && sv->size >= 4)
		return sv->T;
	else
		if (print_errors)
		{
			Msg("# %s",  GetLuaTraceback(L, 2));
			log_script_error("Possible SCRIPT_VAR contents damaged");		
		}
	return NULL;
}


int script_vars_dump (lua_State *L, CScriptVarsTable *svt, bool unpack) // ���� ���� � �������� ���������� � �������
{	
	if (svt)
	{
		lua_createtable(L, 0, svt->size());
		int tidx = lua_gettop(L);
		if (tidx > 100)
		{
			log_script_error("script_vars_dump:  to many nested tables in dump");
			return 1;
		}

		// if (unpack && svt->is_array)	Msg("##DBG: unpacking array %s ", svt->name());

		int ignore_min =  MAXINT;
		int ignore_max = -MAXINT;

		CScriptVarsTable::SCRIPT_VARS_MAP &map = svt->map();
		auto end = map.end();
		if (svt->is_array)
		{
			int hashed = 0;
			string32 tmp;			
			for (int i = 1; i <= svt->size(); i++)  // serialized traverse
			{
				 LPCSTR key = itoa(i, tmp, 10);
				 auto it = map.find( key ); 
				 if (it == map.end())
				 {
					hashed++;
					continue;
				 }
				 ignore_min = min(ignore_min, i);
				 ignore_max = max(ignore_max, i);
				 SCRIPT_VAR &sv = it->second;
				 sv.type |= SVT_KEY_NUMERIC;
				 lua_pushinteger (L, i);
				 svt->get(L, key, unpack);
				 lua_settable(L, tidx);			
			}

			if (!hashed)
				return 1;
		}


		auto it = svt->map().begin();		
		int i = 0;
		for (; it != end; it++)
		{			
			i++;
			LPCSTR key = *it->first;			
			SCRIPT_VAR &sv = it->second;
			if (!key)
			{
				log_script_error("script_vars_dump: key unassigned in table 0x%p at [%d] ", (void*)svt, i);
				key = "0000";
			}

			if (strstr(key, "__") == key) continue;

			int i_key = atoi(key);

			if (sv.is_key_numeric() && ignore_min <= i_key && i_key <= ignore_max)			
				continue; // ������� ������ �������� � �������

			if ( sv.is_key_boolean() )
				lua_pushboolean(L, 0 == strcmp(key, "true"));
			else
				if ( sv.is_key_numeric() )
					lua_pushinteger(L, i_key);
				else
					lua_pushstring(L, key);
				
			svt->get(L, key, unpack);
			lua_settable(L, tidx);			

			// if (0 == _stricmp(key, "1"))Msg("!#WARN: found key == '1' in non-array table %s ", svt->name());
		}
	}
	else
		lua_pushnil(L);	

	return 1;
}
int script_vars_dump(lua_State *L) // ���� ���� � �������� ���������� � �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	return script_vars_dump (L, svt, !!lua_toboolean(L, 2));
}


int script_vars_size(lua_State *L)
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	if (svt && svt->is_array)
		lua_pushinteger(L, svt->size());
	else
		lua_pushinteger(L, 0);
	return 1;
}

// typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);
int dump_byte_code(lua_State *L, const void *src, size_t size, void *ud)
{
	if (!src || !size || !ud) return -1;
	CMemoryWriter *dst = (CMemoryWriter*) ud;
	dst->w(src, size);
	return 0;
}

int CScriptVarsTable::assign(lua_State *L,  int index)
{
	clear();
	int save_top = lua_gettop(L);
	is_array = false;
		
	LPCSTR tk = NULL;
	int count = lua_objlen(L, index); // ������� � array ����� ������� ��������� (����������� ����� ������ �� 1 �� X)
	string16 tmp;
	// ��������������� ������������ ��� ������� ����������� ������� �� 1 �� count
	for (int i = 1; i <= count; i++)		
	{
		is_array = true;
		lua_pushinteger (L, i);
		lua_gettable(L, index);
		set(L, itoa(i, tmp, 10), lua_gettop(L), LUA_TNUMBER);
		lua_pop(L, 1); // ������� �������� �� �����	
	}
	lua_settop(L, save_top);						
	lua_pushnil(L);  // ������ �������� �������
	while (lua_next(L, index) != 0)
	{
		set(L, -2, -1);
		lua_pop(L, 1); // ������� �������� �� �����	
	}				
	lua_settop(L, save_top);
	return size();
}

PSCRIPT_VAR CScriptVarsTable::find_var(const shared_str &k)
{
	SCRIPT_VARS_MAP::iterator it = map().find(k);
	if (it != map().end())
		return &it->second;

	return NULL;
}


void CScriptVarsTable::get(lua_State *L, LPCSTR k, bool unpack)
{
	// shared_str key (k);
	shared_str from;

	PSCRIPT_VAR pv = NULL; 
	try
	{
		R_ASSERT2(this,		"CScriptVarsTable::get this unassigned!");
		R_ASSERT2(k,		"CScriptVarsTable::get key unassigned!");
		if (IsDebuggerPresent() && 0)
		{			
			LPCSTR name = this->name();			
			from = GetLuaTraceback(L, 2);
			string4096 msg;
			sprintf_s (msg, 4096, "$#CONTEXT: CScriptVarsTable::get this = 0x%p   name = %s (ref_count=%d), key = %s, from\n %s ",				    
					 				(void*)this, name ? name : "NULL", this->ref_count, k ? k : "NULL", *from);
			MsgCB(msg); // WARN: ������������� ���� ������� ������ ���� �� ��������� ������� � 4-������������ ��������
			
		}

		pv = find_var(k);	 	
	}
	catch (...)
	{
		Msg("!EXCEPTION: probably using CScriptVarsTable object after destroying from. ");
		if (from.size()) Log(*from);
		MsgCB("#DUMP_CONTEXT");
		log_script_error("Exception catched in CScriptVarsTable::get ");
	}



	if (pv)
	{
		SCRIPT_VAR &sv = *pv;
		void *p;

		switch (sv.eff_type())
		{
		case LUA_TBOOLEAN:
			lua_pushboolean(L, sv.b_value);
			break;
		case LUA_TNUMBER:
			lua_pushnumber(L, sv.n_value);
			break;	
		case LUA_TSTRING:
			if (sv.size <= sizeof(sv.s_value))
				lua_pushstring(L, sv.s_value);
			else
				lua_pushstring(L, (char*)sv.data);
			break;
		case LUA_TTABLE:
			if (unpack)
				script_vars_dump (L, sv.T, unpack);
			else			
				lua_pushsvt(L, sv.T);
			break;
		case LUA_TFUNCTION:
			if (luaL_loadbuffer(L, (LPCSTR)sv.data, sv.size, k) != 0)
				lua_pushnil (L);
			break;
		case LUA_TUSERDATA:
			p = lua_newuserdata(L, sv.size);
			memcpy(p, sv.data, sv.size);
			break;			

		case LUA_TNETPACKET:
			convert_to_lua<NET_Packet*>(L, sv.P); 
			break;
		};
	}
	else
		lua_pushnil(L);
}

void CScriptVarsTable::set(lua_State *L, int key_index, int value_index)
{
	LPCSTR tk = NULL;
	string16 tmp;

	int kt = lua_type(L, key_index);
	switch (kt)
	{
	case LUA_TBOOLEAN:
		tk = lua_toboolean(L, key_index) ? "true" : "false";	break;
	case LUA_TNUMBER: 
		tk = itoa(lua_tointeger(L, key_index), tmp, 10); 	    break;
	case LUA_TSTRING: // ��������������� �� ��������� �������� � ������, �������� � ����� lua_next
		tk = lua_tostring(L, key_index); 						break;										
	}

	bool push_val = (value_index != -1) && (value_index != lua_gettop(L));

	if (tk)
	{
		is_array &= (LUA_TNUMBER == kt);  // ���� ��� ������ � ������� � ������������� ������				
		if (push_val)
			lua_pushvalue(L, value_index);
		set(L, tk, lua_gettop(L), kt);
		if (push_val)
			lua_pop(L, 1);
	}


}


void CScriptVarsTable::set(lua_State *L, LPCSTR k, int index, int key_type)
{
	shared_str key(k);	
	SCRIPT_VAR sv;
	
	bool exists = false;
	auto it = map().find(key);
	if (it != map().end())
	{
		sv = it->second;
		exists = true;
	}
	else
		ZeroMemory(&sv, sizeof(sv));
	
	int new_type = lua_type(L, index);
	if (LUA_TBOOLEAN == key_type) 
		new_type |= SVT_KEY_BOOLEAN;
	if (LUA_TNUMBER  == key_type) 
		new_type |= SVT_KEY_NUMERIC;
	
	switch (new_type & 0xffff)
	{
	case LUA_TNONE:
	case LUA_TNIL:		
		if (exists)
		{
			Msg("# deleting script_var %s ", k);
			map().erase(it);
		}
		sv.release();
		return;		
	case LUA_TBOOLEAN:
		sv.smart_alloc(new_type);
		sv.b_value = !!lua_toboolean(L, index);		
		break;
	case LUA_TNUMBER:
		sv.smart_alloc(new_type);
		sv.n_value = lua_tonumber(L, index);		
		break;
	case LUA_TSTRING:
	{
		const char *s = lua_tostring(L, index);
		char *dst = (char*)sv.smart_alloc(new_type, xr_strlen(s) + 1);
		strcpy_s(dst, sv.size, s); // ���������� �������� ����� �� 7 ��������		
		break;
	}
	case LUA_TTABLE:
	{		
		// ���������� ������� � �������� ���������� (�������� �������������)
		if (new_type != sv.type)
		{
			sv.release();
			sv.type = new_type;
			sv.size = 4;
			sv.T = xr_new<CScriptVarsTable>();
			sv.T->add_ref();
		}
		sv.T->m_name.sprintf("%s.%s", *m_name, *key);
		sv.T->assign(L, index);		
		break;
	}
	case LUA_TFUNCTION:
	{
		CMemoryWriter stream;				
		lua_dump(L, dump_byte_code, &stream);
		void *dst = sv.smart_alloc(new_type, stream.size());
		memcpy_s(dst, sv.size, stream.pointer(), sv.size);	
		break;
	}
	case LUA_TUSERDATA:
	{
		object_rep* rep = is_class_object(L, index);
		if (rep && strstr(rep->crep()->name(), "net_packet"))
		{
			sv.smart_alloc(LUA_TNETPACKET); // ��������� ������ ������
			NET_Packet *src = (NET_Packet *)rep->ptr();
			sv.size = src->B.count;			
			sv.P = xr_new<NET_Packet>();
			sv.P->B.count = sv.size;
			sv.P->r_seek(src->r_tell());			
			memcpy_s(sv.P->B.data, sizeof(sv.P->B.data), src->B.data, sv.size);
			map()[key] = sv;
			return;
		}
		sv.size = 1;
		if (lua_gettop(L) > index)
			sv.size = lua_tointeger(L, index + 1);
		else
			sv.size = lua_objlen(L, index);
		if (sizeof(sv) == sv.size) 
		{			
			sv.T = lua_tosvt(L, index, false);  // ������� ����������, ��� ���������� ����������� �����
			if (sv.T)  
			{
				sv.T->add_ref();
				sv.type = LUA_TTABLE;
				sv.size = 4;
				break;
			}
		}



		void *dst = sv.smart_alloc(new_type, sv.size); // ��������� ������ ������		
		memcpy_s(dst, sv.size, lua_touserdata(L, index), sv.size);
		break;
	}
	default:
		log_script_error("script_vars not supported lua type %d for var %s", sv.type, *key);
		if (exists) 
			map().erase(it);
		sv.release();
		break;
	};
	
	map()[key] = sv;
}

int script_vars_assign(lua_State *L)
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);
	if (svt && lua_istable(L, 2))
		svt->assign(L, 2);
	return 0;
}

int script_vars_index(lua_State *L) // ������ ���������� �� �������� �� ���������� �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);

	LPCSTR k = lua_tostring(L, 2);
	
	if (!k || !xr_strlen(k) || !svt)
	{
		lua_pushnil(L);
		return 1;
	}
	
	svt->get(L, k, false);
	return 1;
}


int script_vars_release(lua_State *L)
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);		
	svt->release();
	return 0;
}

int script_vars_new_index(lua_State *L) // ������ ���������� �� �������� �� ���������� �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	svt->set(L, 2, 3);
	return 0;
}

int lua_pushsvt(lua_State *L, CScriptVarsTable *T)
{
 	SCRIPT_VAR *sv = (SCRIPT_VAR*) lua_newuserdata(L, sizeof(SCRIPT_VAR));	
	sv->type = LUA_TTABLE;
	sv->size = 4;
	sv->T = T;	
	T->add_ref();  // ����������� ������ ���������� � lua
	int value_index = lua_gettop(L);
	luaL_getmetatable(L, "GMT_SCRIPT_VARS");
	int mt = lua_gettop(L);
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		luaL_newmetatable(L, "GMT_SCRIPT_VARS");
		register_method(L, "__gc", mt, script_vars_release);
		register_method(L, "__len", mt, script_vars_size);
		register_method(L, "__call", mt, script_vars_dump);
		register_method(L, "__index", mt, script_vars_index);
		register_method(L, "__newindex", mt, script_vars_new_index);
		lua_pushvalue(L, mt);
		lua_setfield(L, mt, "__metatable");
	}
	lua_setmetatable(L, value_index);	
	return 1;
}

int script_vars_create(lua_State *L) // �������� ������� ���������� � ������������ ���������� �� ������� ���������
{
	CScriptVarsTable *svt = xr_new<CScriptVarsTable>(); // ����� ��������� ������ �� ����, �.�. ������ ����� �� �����������!
	if (lua_isstring(L, 1))
		svt->set_name(lua_tostring(L, 1));

	if (lua_istable(L, 2))
		svt->assign(L, 2);

	return lua_pushsvt(L, svt);	
}

int script_vars_export(lua_State *L)  // ���������� ������� ���������� � ���-�����
{
	CMemoryWriter stream;
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	svt->save(stream);
	object_rep* rep = is_class_object(L, 2);
	if (rep && strstr(rep->crep()->name(), "net_packet"))
	{		
		NET_Packet *dst = (NET_Packet *)rep->ptr();
		dst->w(stream.pointer(), stream.size());
		lua_pushinteger(L, stream.size());
	}
	else
		lua_pushinteger(L, 0);
	return 1;
}

int script_vars_import(lua_State *L) // �������� ������� ���������� �� ���-������ 
{
	CScriptVarsTable *svt = NULL;
	if (lua_isuserdata(L, 1))
		svt = lua_tosvt(L, 1);
	if (!svt)
		svt = xr_new<CScriptVarsTable>(); // ����� ��������� ������ �� ����, �.�. ������ ����� �� �����������!

	if (lua_isstring(L, 1))
		svt->set_name(lua_tostring(L, 1));


	object_rep* rep = is_class_object(L, 2);
	if (rep && strstr(rep->crep()->name(), "net_packet"))
	{
		NET_Packet *src = (NET_Packet *)rep->ptr();		
		void *from = &src->B.data[src->r_pos];
		IReader reader(from, src->r_elapsed());
		svt->load(reader);
	}
	
	return lua_pushsvt(L, svt);	
}

int get_stored_vars(lua_State *L)
{
	return lua_pushsvt(L, &g_ScriptVars);	
}

DLL_API void* find_stored_var(void *storage, LPCSTR key) // ��� ������������ �������������
{	
	CScriptVarsTable *t = NULL;
	PSCRIPT_VAR pv = (PSCRIPT_VAR)storage;
	if (pv && pv->T) t = pv->T;
	if (NULL == t)
		t = &g_ScriptVars;

	return t->find_var(key);
}


void CScriptVarsStorage::script_register(lua_State *L)
{
	g_ScriptVars.set_name("g_ScriptVars");
	module(L)
		[
			def("get_stored_vars"			,				&get_stored_vars	, raw(_1)),
			def("vars_table_assign"			,				&script_vars_assign , raw(_1)),
			def("vars_table_create"			,				&script_vars_create	, raw(_1)),
			def("vars_table_dump"			,(lua_CFunction)&script_vars_dump, raw(_1)), // (void (CScriptGameObject::*)(u8,u8,u8))
			def("vars_table_export"			,				&script_vars_export	, raw(_1)),
			def("vars_table_import"			,				&script_vars_import	, raw(_1))
		];		

}
