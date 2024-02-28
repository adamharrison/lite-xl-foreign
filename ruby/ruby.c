#include <ruby.h>
#ifdef RUBY_STANDALONE
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
#else
  #define LITE_XL_PLUGIN_ENTRYPOINT
  #include <lite_xl_plugin_api.h>
#endif

static VALUE cLuaFunction;
static VALUE cLuaTable;
static VALUE cLua;
void f_ruby_ruby_to_lua(VALUE value, lua_State* L);
VALUE f_ruby_lua_to_ruby(lua_State* L, int index);

static lua_State* VALUETOSTATE(VALUE value) {
  return (lua_State*)NUM2LL(value);
}
static VALUE STATETOVALUE(lua_State* L) {
  return LL2NUM((long long)L);
}


typedef struct s_ruby_function_lua {
  lua_State* L;
  int reference;
} s_ruby_function_lua;

size_t f_ruby_lua_function_size(const void* data) {
  return sizeof(s_ruby_function_lua);
}
void f_ruby_lua_function_free(void* data) {
  free(data);
}


static const rb_data_type_t f_ruby_lua_function_type = {
  .wrap_struct_name = "lua_function",
  .function = {
    .dmark = NULL,
    .dfree = f_ruby_lua_function_free,
    .dsize = f_ruby_lua_function_size
  },
  .data = NULL,
  .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE f_ruby_lua_function_alloc(VALUE self) {
  s_ruby_function_lua* data = malloc(sizeof(s_ruby_function_lua));
  return TypedData_Wrap_Struct(self, &f_ruby_lua_function_type, data);
}

VALUE f_ruby_lua_function_initialize(VALUE self, VALUE state, VALUE func) {
  s_ruby_function_lua* data;
  TypedData_Get_Struct(self, s_ruby_function_lua, &f_ruby_lua_function_type, data);
  data->L = VALUETOSTATE(state);
  data->reference = NUM2INT(func);
  return self;
}

VALUE f_ruby_lua_function_call(int argc, VALUE* argv, VALUE self) {
  s_ruby_function_lua* data;
  TypedData_Get_Struct(self, s_ruby_function_lua, &f_ruby_lua_function_type, data);
  lua_State* L = data->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, data->reference);
  for (int i = 0; i < argc; ++i) {
    f_ruby_ruby_to_lua(argv[i], L);
  }
  lua_call(L, 1, 1);
  VALUE ret = f_ruby_lua_to_ruby(L, -1);
  lua_pop(L, 1);
  return ret;
}


typedef struct s_ruby_table_lua {
  lua_State* L;
  int reference;
} s_ruby_table_lua;

size_t f_ruby_lua_table_size(const void* data) {
  return sizeof(s_ruby_table_lua);
}
void f_ruby_lua_table_free(void* data) {
  free(data);
}

static const rb_data_type_t f_ruby_lua_table_type = {
  .wrap_struct_name = "lua_table",
  .function = {
    .dmark = NULL,
    .dfree = f_ruby_lua_table_free,
    .dsize = f_ruby_lua_table_size
  },
  .data = NULL,
  .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};


VALUE f_ruby_lua_table_alloc(VALUE self) {
  s_ruby_table_lua* data = malloc(sizeof(s_ruby_table_lua));
  return TypedData_Wrap_Struct(self, &f_ruby_lua_table_type, data);
}

VALUE f_ruby_lua_table_initialize(VALUE self, VALUE state, VALUE func) {
  s_ruby_table_lua* data;
  TypedData_Get_Struct(self, s_ruby_table_lua, &f_ruby_lua_table_type, data);
  data->L = VALUETOSTATE(state);
  data->reference = NUM2INT(func);
  return self;
}


VALUE f_ruby_lua_table_method_missing(int argc, VALUE* argv, VALUE self) {
  s_ruby_table_lua* data;
  TypedData_Get_Struct(self, s_ruby_table_lua, &f_ruby_lua_table_type, data);
  lua_State* L = data->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, data->reference);
  const char* field = StringValueCStr(argv[0]);
  if (argc == 1) {
    // GET
    lua_getfield(L, -1, field);
    VALUE ret = f_ruby_lua_to_ruby(L, -1);
    lua_pop(L, 1);
    return ret;
  } else if (argc == 2) {
    // SET
    int len = strlen(field);
    if (len == 0 || field[len-1] != '=')
      rb_exc_raise(rb_eRuntimeError);
    f_ruby_ruby_to_lua(argv[1], L);
    lua_setfield(L, -2, field);
  } else
    rb_exc_raise(rb_eRuntimeError);
  return Qnil;
}

int f_ruby_ruby_object_get(lua_State* L) {
  lua_rawgeti(L, 1, 1);
  VALUE self = (VALUE)lua_touserdata(L, -1);
  VALUE ret;
  switch (TYPE(self)) {
    case T_OBJECT: {
      const char* field = luaL_checkstring(L, 2);
      ret = rb_iv_get(self, field);
    } break;
    case T_HASH: {
      VALUE field = f_ruby_lua_to_ruby(L, 2);
      ret = rb_hash_lookup(self, field);
    } break;
    default:
      assert(false);
    break;
  }
  f_ruby_ruby_to_lua(ret, L);
  return 1;
}

int f_ruby_ruby_object_set(lua_State* L) {
  lua_rawgeti(L, 1, 1);
  VALUE self = (VALUE)lua_touserdata(L, -1);
  VALUE val = f_ruby_lua_to_ruby(L, 3);
  switch (self) {
    case T_OBJECT: {
      const char* field = luaL_checkstring(L, 2);
      rb_iv_set(self, field, val);
    } break;
    case T_HASH: {
      VALUE field = f_ruby_lua_to_ruby(L, 2);
      rb_hash_aset(self, field, val);
    } break;
  }

  return 0;
}

int f_ruby_ruby_object_call(lua_State* L) {
  lua_rawgeti(L, 1, 1);
  VALUE self = (VALUE)lua_touserdata(L, -1);
  int argc = lua_gettop(L) - 1;
  VALUE argv[argc];
  for (int i = 0; i < argc; ++i)
    argv[i] = f_ruby_lua_to_ruby(L, i+1);
  VALUE ret = rb_funcallv(self,  rb_intern("call"), argc, argv);
  f_ruby_ruby_to_lua(ret, L);
  return 1;
}


VALUE f_ruby_require(VALUE self, VALUE path) {
  lua_State* L = VALUETOSTATE(rb_iv_get(self, "@state"));
  lua_getglobal(L, "require");
  f_ruby_ruby_to_lua(path, L);
  lua_call(L, 1, 1);
  VALUE ret = f_ruby_lua_to_ruby(L, -1);
  lua_pop(L, 1);
  return ret;
}


VALUE f_ruby_lua_initialize(VALUE self, VALUE state) {
  rb_iv_set(self, "@state", state);
}

void f_ruby_register_types(lua_State* L) {
  cLuaFunction = rb_define_class("LuaFunction", rb_cObject);
  rb_define_alloc_func(cLuaFunction, f_ruby_lua_function_alloc);
  rb_define_method(cLuaFunction, "initialize", f_ruby_lua_function_initialize, 2);
  rb_define_method(cLuaFunction, "call", f_ruby_lua_function_call, -1);

  cLuaTable = rb_define_class("LuaTable", rb_cObject);
  rb_define_alloc_func(cLuaTable, f_ruby_lua_table_alloc);
  rb_define_method(cLuaTable, "initialize", f_ruby_lua_table_initialize, 2);
  rb_define_method(cLuaTable, "method_missing", f_ruby_lua_table_method_missing,-1);

  cLua = rb_define_class("Lua", rb_cObject);
  rb_define_method(cLua, "initialize", f_ruby_lua_initialize, 1);
  rb_define_method(cLua, "require", f_ruby_require, 1);

  VALUE lua = rb_obj_alloc(cLua);
  VALUE argv = STATETOVALUE(L);
  rb_obj_call_init(lua, 1, &argv);
  rb_gv_set("$lua", lua);
}


void f_ruby_ruby_to_lua(VALUE value, lua_State* L) {
  switch (TYPE(value)) {
    case T_UNDEF:
    case T_NIL:
    case T_NONE:
      lua_pushnil(L);
    break;
    case T_TRUE: lua_pushboolean(L, 1); break;
    case T_FALSE: lua_pushboolean(L, 0); break;
    case T_FLOAT:
    case T_FIXNUM:
      lua_pushnumber(L, NUM2DBL(value));
    break;
    case T_STRING:
      lua_pushstring(L, StringValueCStr(value));
    break;
    case T_BIGNUM: lua_pushinteger(L, NUM2LL(value)); break;
    case T_OBJECT:
    case T_HASH: {
      if (value == Qtrue || value == Qfalse) {
        lua_pushboolean(L, value == Qtrue ? 1 : 0);
      } else {
        lua_newtable(L);
        lua_pushlightuserdata(L, (void*)value);
        lua_rawseti(L, -2, 1);
        luaL_setmetatable(L, "ruby_object");
      }
    } break;
    default:
      fprintf(stderr, "SIGH: %d\n", TYPE(value));
      lua_pushlightuserdata(L, (void*)value);
    break;
  }
}

VALUE f_ruby_lua_to_ruby(lua_State* L, int index) {
  switch (lua_type(L, index)) {
    case LUA_TSTRING: return rb_str_new_cstr(lua_tostring(L, index)); break;
    case LUA_TNUMBER: return lua_isinteger(L, index) ? LL2NUM(lua_tointeger(L, index)) : DBL2NUM(lua_tonumber(L, index)); break;
    case LUA_TBOOLEAN: return lua_toboolean(L, index) ? Qtrue : Qfalse; break;
    case LUA_TFUNCTION: {
      VALUE result = rb_obj_alloc(cLuaFunction);
      lua_pushvalue(L, index);
      int reference = luaL_ref(L, LUA_REGISTRYINDEX);
      VALUE argv[2] = { STATETOVALUE(L), INT2NUM(reference) };
      rb_obj_call_init(result, 2, argv);
      return result;
    } break;
    case LUA_TTABLE: {
      VALUE result = rb_obj_alloc(cLuaTable);
      lua_pushvalue(L, index);
      int reference = luaL_ref(L, LUA_REGISTRYINDEX);
      VALUE argv[2] = { STATETOVALUE(L), INT2NUM(reference) };
      rb_obj_call_init(result, 2, argv);
      return result;
    } break;
    case LUA_TNIL: return Qnil; break;
    case LUA_TUSERDATA: return (VALUE)lua_touserdata(L, index); break;
  }
  return Qnil;
}


int f_ruby_eval(lua_State* L) {
  int state;
  VALUE result = rb_eval_string_protect(luaL_checkstring(L, -1), &state);
  if (state)
    return luaL_error(L, "error executing ruby code");
  f_ruby_ruby_to_lua(result, L);
  return 1;
}


int f_ruby_exec(lua_State* L) {
  int state;
  const char* filename = luaL_checkstring(L, -1);
  FILE* file = fopen(filename, "rb");
  if (!file)
    return luaL_error(L, "error loading ruby file %s: can't find file", filename);
  char* buffer;
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  buffer = malloc(length);
  fread(buffer, sizeof(char), length, file);
  fclose(file);
  VALUE result = rb_eval_string_protect(buffer, &state);
  free(buffer);
  if (state)
    return luaL_error(L, "error executing ruby code");
  f_ruby_ruby_to_lua(result, L);
  return 1;
}


int f_ruby_gc(lua_State* L) {
  ruby_finalize();
  return 0;
}

static const luaL_Reg ruby_object[] = {
  { "__index",       f_ruby_ruby_object_get    },
  { "__newindex",    f_ruby_ruby_object_set    },
  { "__call",        f_ruby_ruby_object_call   },
  { NULL,            NULL                      }
};

// Core functions, `request` is the primary function, and is stateless (minus the ssl config), and makes raw requests.
static const luaL_Reg ruby_api[] = {
  { "__gc",          f_ruby_gc           },
  { "eval",          f_ruby_eval         },
  { "exec",          f_ruby_exec         },
  { NULL,            NULL                }
};


#ifndef RUBY_STANDALONE
int luaopen_lite_xl_ruby(lua_State* L, void* XL) {
  lite_xl_plugin_init(XL);
#else
int luaopen_ruby(lua_State* L) {
#endif
  ruby_init();
  f_ruby_register_types(L);
  luaL_newmetatable(L, "ruby_object");
  luaL_setfuncs(L, ruby_object, 0);
  lua_pop(L, 1);
  luaL_newlib(L, ruby_api);
  lua_pushvalue(L, -1);
  lua_setmetatable(L, -2);
  return 1;
}
