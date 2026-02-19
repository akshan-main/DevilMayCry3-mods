#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <psapi.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#pragma comment(lib, "psapi.lib")

// log
void log_init(const char* path);
void log_msg(const char* fmt, ...);
void log_close();

// ini (wraps GetPrivateProfileString, call ini_set_path first)
void  ini_set_path(const char* path);
int   ini_int(const char* sec, const char* key, int def);
float ini_float(const char* sec, const char* key, float def);
bool  ini_bool(const char* sec, const char* key, bool def);
int   ini_vkey(const char* sec, const char* key, int def);

// aob scanner
BYTE* aob_scan(const char* mod_name, const char* sig);

// x86 detour
bool hook_install(const char* tag, BYTE* target, BYTE* detour, int stolen, BYTE** trampoline);
void hook_remove_all();
void nop_patch(BYTE* addr, int len, BYTE* save);
void nop_restore(BYTE* addr, int len, const BYTE* save);
void mem_patch(BYTE* addr, const BYTE* data, int len);

// hotkeys
typedef void (*hk_fn)(void*);
void hk_add(const char* name, int vk, hk_fn fn, void* ctx);
void hk_start();
void hk_stop();
int  parse_vk(const char* name);

// features
struct ModState {
    bool inf_hp;
    bool inf_dt;
    bool one_hit;
    bool inf_jumps;
    bool turbo;
    bool show_hp;       // enemy hp bars overlay
    bool show_combo;    // combo damage tracker

    float dmg_mult;
    float game_speed;   // 1.0=normal, 1.2=ps2 turbo

    // combo tracker
    int   combo_hits;
    float combo_dmg;
    DWORD combo_last_t; // GetTickCount of last hit
    int   combo_best;

    // addresses (NULL = not found, that feature won't work)
    BYTE* a_player;     // player struct base (static alloc in bss)
    BYTE* a_hp;         // player HP float
    BYTE* a_dt;         // player DT float
    BYTE* a_dmg;        // damage calc hook point
    BYTE* a_speed;      // player workrate float (for turbo)
    BYTE* a_jumps;      // jump counter byte (if found)
    BYTE* a_lockon;     // locked-on enemy ptr (deref to get enemy base)

    // enemy struct layout
    int ent_hp_off;
    int ent_maxhp_off;
    int ent_pos_off;

    bool ready;
};

void state_defaults(ModState* s);
void state_log(ModState* s);

// callbacks for hotkeys
void cb_inf_hp(void* s);
void cb_inf_dt(void* s);
void cb_one_hit(void* s);
void cb_inf_jumps(void* s);
void cb_turbo(void* s);
void cb_show_hp(void* s);
void cb_show_combo(void* s);
void cb_cycle_dmg(void* s);
