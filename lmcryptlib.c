﻿/**
 * MCrypt lib for Lua
 * @version 0.1
 * @author microwish@gmail.com
 */
#include <mcrypt.h>

#include <lua.h>//Lua 5.1.5
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int td_ref = 1;

static int module_open(lua_State *L)
{
	const char *algorithm, *algorithm_dir, *mode, *mode_dir;
	size_t l;

	if (lua_gettop(L) != 4) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "4 arguments required");
		return 2;
	}

	//arg#1
	if (!(algorithm = lua_tostring(L, 1))) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 invalid");
		return 2;
	}

	//arg#2
	if (lua_isnoneornil(L, 2)) {
		algorithm_dir = NULL;
	} else {
		if (!(algorithm_dir = lua_tolstring(L, 2, &l))) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "arg#2 invalid");
			return 2;
		}
		if (l == 0)
			algorithm_dir = NULL;
	}

	//arg#3
	if (!(mode = lua_tostring(L, 3))) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#3 invalid");
		return 2;
	}

	//arg#4
	if (lua_isnoneornil(L, 4)) {
			mode_dir = NULL;
	} else {
		if (!(mode_dir = lua_tolstring(L, 4, &l))) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "arg#4 invalid");
			return 2;
		}
		if (l == 0)
			mode_dir = NULL;
	}

	MCRYPT *tdp = (MCRYPT *)lua_newuserdata(L, sizeof(MCRYPT));

	*tdp = mcrypt_module_open((char *)algorithm, (char *)algorithm_dir, (char *)mode, (char *)mode_dir);
	if (*tdp == MCRYPT_FAILED) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Could not open encryption module");
		return 2;
	}

	td_ref = luaL_ref(L, LUA_ENVIRONINDEX);

	lua_pushboolean(L, 1);

	return 1;
}

//FIXME tdp
#define RETRIEVE_TD(L, tdp) \
	do { \
		lua_rawgeti((L), LUA_ENVIRONINDEX, td_ref); \
		tdp = (MCRYPT *)lua_touserdata((L), -1); \
		if (!tdp) { \
			lua_pushboolean((L), 0); \
			lua_pushliteral((L), "Must open MCrypt module first"); \
			return 2; \
		} \
		lua_pop((L), 1); \
	} while (0)

static int enc_get_key_size(lua_State *L)
{
	MCRYPT *tdp;
	int size;

	RETRIEVE_TD(L, tdp);

	size = mcrypt_enc_get_key_size(*tdp);
	lua_pushinteger(L, (lua_Integer)size);

	return 1;
}

static int enc_get_iv_size(lua_State *L)
{
	MCRYPT *tdp;
	int size;

	RETRIEVE_TD(L, tdp);

	size = mcrypt_enc_get_iv_size(*tdp);
	lua_pushinteger(L, (lua_Integer)size);

	return 1;
}

static int generic_init(lua_State *L)
{
	if (lua_gettop(L) != 2) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "2 argumetns required");
		return 2;
	}

	MCRYPT *tdp;
	char *key, *iv, *iv_s;
	size_t key_len;
	int key_size, iv_size;

	//arg#1
	if (!(key = (char *)lua_tolstring(L, 1, &key_len))) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 invalid");
		return 2;
	}

	//arg#2
	if (!(iv = (char *)lua_tostring(L, 2))) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#2 invalid");
		return 2;
	}

	RETRIEVE_TD(L, tdp);

	key_size = mcrypt_enc_get_key_size(*tdp);
	if (key_len > (size_t)key_size) {
		key_len = (size_t)key_size;
	}

	iv_size = mcrypt_enc_get_iv_size(*tdp);
	if (iv_size <= 0) {
		iv_s = NULL;
	} else {
		if (!(iv_s = calloc(iv_size + 1, sizeof(char)))) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "Could not alloc IV");
			return 2;
		}
		memcpy(iv_s, iv, iv_size);
	}

	mcrypt_generic_deinit(*tdp);

	//TODO
	//tell more specific errors according to returned codes
	if (mcrypt_generic_init(*tdp, (void *)key, (int)key_len, (void *)iv_s) < 0) {
		free(iv_s);
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "MCrypt generic init failed");
		return 2;
	}
	free(iv_s);

	return 0;
}

static int generic(lua_State *L)
{
	if (lua_gettop(L) != 1) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "1 argument required");
		return 2;
	}

	char *plain;
	size_t l;
	MCRYPT *tdp;

	if (!(plain = (char *)lua_tolstring(L, 1, &l)) || l == 0) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 invalid");
		return 2;
	}

	RETRIEVE_TD(L, tdp);

	if (mcrypt_enc_is_block_mode(*tdp) == 1) {
		int block_size = mcrypt_enc_get_block_size(*tdp);
		int data_size = ((l - 1) / block_size + 1) * block_size;
		char *plain_s = calloc(data_size + 1, sizeof(char));
		if (!plain_s) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "Cloud not align block");
			return 2;
		}
		memcpy(plain_s, plain, l);

		if(mcrypt_generic(*tdp, (void *)plain_s, data_size)) {
			free(plain_s);
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "MCrypt generic failed");
			return 2;
		}
		lua_pushlstring(L, plain_s, (size_t)data_size);
		free(plain_s);
	} else {
		if(mcrypt_generic(*tdp, (void *)plain, (int)l)) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "MCrypt generic failed");
			return 2;
		}
		lua_pushlstring(L, plain, l);
	}

	return 1;
}

static int de_generic(lua_State *L)
{
	if (lua_gettop(L) < 1) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "1 argument required");
		return 2;
	}

	char *ciphered;
	size_t l;
	MCRYPT *tdp;

	if (!(ciphered = (char *)lua_tolstring(L, 1, &l)) || l < 1) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 invalied");
		return 2;
	}

	RETRIEVE_TD(L, tdp);

	if (mcrypt_enc_is_block_mode(*tdp) == 1) {
		int block_size = mcrypt_enc_get_block_size(*tdp);
		int data_size = ((l - 1) / block_size + 1) * block_size;
		char *ciphered_s = calloc(data_size + 1, sizeof(char));
		if (!ciphered_s) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "Cloud not align block");
			return 2;
		}
		memcpy(ciphered_s, ciphered, l);

		if(mdecrypt_generic(*tdp, (void *)ciphered_s, data_size)) {
			free(ciphered_s);
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "MCrypt de-generic failed");
			return 2;
		}
		lua_pushlstring(L, ciphered_s, (size_t)data_size);
		free(ciphered_s);
	} else {
		if (mdecrypt_generic(*tdp, (void *)ciphered, (int)l)) {
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "MCrypt de-generic failed");
			return 2;
		}
		lua_pushlstring(L, ciphered, l);
	}

	return 1;
}

static int generic_deinit(lua_State *L)
{
	MCRYPT *tdp;

	RETRIEVE_TD(L, tdp);

	if (mcrypt_generic_deinit(*tdp) < 0) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Could not terminate encryption specifier");
		return 2;
	}

	lua_pushboolean(L, 1);

	return 1;
}

static int module_close(lua_State *L)
{
	MCRYPT *tdp;

	RETRIEVE_TD(L, tdp);

	mcrypt_module_close(*tdp);

	luaL_unref(L, LUA_ENVIRONINDEX, td_ref);

	lua_pushboolean(L, 1);

	return 1;
}

static const struct luaL_Reg mcrypt_lib[] = {
	{ "module_open", module_open },
	{ "enc_get_key_size", enc_get_key_size },
	{ "enc_get_iv_size", enc_get_iv_size },
	{ "generic_init", generic_init },
	{ "generic", generic },
	{ "de_generic", de_generic },
	{ "generic_deinit", generic_deinit },
	{ "module_close", module_close },
	{ NULL, NULL }
};

static int settablereadonly(lua_State *L)
{
	return luaL_error(L, "Must not update a read-only table");
}

#define LUA_MCRYPTLIBNAME "mcrypt"

LUALIB_API int luaopen_mcrypt(lua_State *L)
{
	lua_createtable(L, 1, 0);
	lua_replace(L, LUA_ENVIRONINDEX);

	//main table for this module
	lua_newtable(L);

	//metatable for the main table
	lua_createtable(L, 0, 2);

	luaL_register(L, LUA_MCRYPTLIBNAME, mcrypt_lib);

	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, settablereadonly);
	lua_setfield(L, -2, "__newindex");

	lua_setmetatable(L, -2);

	return 1;
}
