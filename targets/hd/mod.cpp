// dmc3 hd collection trainer
// loaded by dinput8.dll proxy, 64-bit
//
// features: inf hp/dt, turbo hotkey, inf jumps, mid-mission style switch
// no overlay (dx11 hook is a different project, maybe later)
//
// addresses from x64dbg on the steam hd collection (dmc3.exe)

#include "../../core/core.h"

static ModState g;
static bool g_run = false;
static BYTE* g_base = NULL;

// ---- addresses (dmc3.exe, steam hd collection, 64-bit) --------------------
// all offsets from module base

// HP subtract instruction: subss xmm0,xmm8  (5 bytes: F3 41 0F 5C C0)
// found by bping on hp write after taking damage and tracing up
#define OFF_HP_SUB   0x88517

// DT drain instruction: subss xmm1,xmm0  (4 bytes: F3 0F 5C C8)
#define OFF_DT_DRAIN 0x1E1842

// game speed float - 1.0=normal, game's turbo option writes 1.2 here
// we just do the same thing but with a hotkey
#define OFF_SPEED    0xCF2D90

// entity pool pointer - deref to get the entity pool base
// player (dante) is always the first entity
// player actor has:
//   +0x411C  hp float
//   +0x40EC  max hp float
//   +0x3EB8  dt gauge float
//   +0x3F11  air hike counter (byte, 0=can jump again)
//   +0x6338  style id (int, 0=trickster 1=swordmaster 2=gunslinger 3=royalguard)
#define OFF_POOL     0xC90E10

// ---- player pointer ---------------------------------------------------

static BYTE* get_player() {
    BYTE* pool = *(BYTE**)(g_base + OFF_POOL);
    if(!pool || IsBadReadPtr(pool, 8)) return NULL;
    BYTE* player = *(BYTE**)pool;
    if(!player || IsBadReadPtr(player, 0x7000)) return NULL;
    return player;
}

// ---- nop state -------------------------------------------------------

static BYTE sv_hp[5], sv_dt[4];
static bool p_hp=0, p_dt=0;

// ---- apply -----------------------------------------------------------

static void apply() {
    if(!g.ready) return;

    // inf HP - nop the subtract instruction
    if(g.a_hp && g.inf_hp && !p_hp) { nop_patch(g.a_hp, 5, sv_hp); p_hp=1; }
    if(g.a_hp && !g.inf_hp && p_hp) { nop_restore(g.a_hp, 5, sv_hp); p_hp=0; }

    // inf DT - nop the drain instruction
    if(g.a_dt && g.inf_dt && !p_dt) { nop_patch(g.a_dt, 4, sv_dt); p_dt=1; }
    if(g.a_dt && !g.inf_dt && p_dt) { nop_restore(g.a_dt, 4, sv_dt); p_dt=0; }

    // turbo - write to speed float directly, same as what the game menu does
    if(g.a_speed)
        *(float*)g.a_speed = g.turbo ? g.game_speed : 1.0f;

    // inf jumps + style switch need the player actor pointer
    BYTE* player = get_player();
    if(player) {
        // zero the air hike counter every frame
        if(g.inf_jumps)
            *(BYTE*)(player + 0x3F11) = 0;

        // write style id if style switch is on
        // style_id starts at -1 (no override), gets set when you cycle
        if(g.style_switch && g.style_id >= 0)
            *(int*)(player + 0x6338) = g.style_id;
    }
}

// ---- setup -----------------------------------------------------------

// check expected bytes at an address before we patch it
// if they dont match we're probably on a different exe version
static bool verify_bytes(BYTE* addr, const BYTE* expected, int len, const char* name) {
    for(int i=0; i<len; i++) {
        if(addr[i] != expected[i]) {
            log_msg("!! %s: byte mismatch at +%d (got %02X, want %02X) - wrong exe version?",
                name, i, addr[i], expected[i]);
            return false;
        }
    }
    return true;
}

static void setup_addrs() {
    g_base = (BYTE*)GetModuleHandleA(NULL);
    log_msg("base: %p", g_base);

    g.a_hp    = g_base + OFF_HP_SUB;
    g.a_dt    = g_base + OFF_DT_DRAIN;
    g.a_speed = g_base + OFF_SPEED;

    // verify we're patching the right instructions
    // hp sub: subss xmm0,xmm8  = F3 41 0F 5C C0
    BYTE exp_hp[] = {0xF3,0x41,0x0F,0x5C,0xC0};
    if(!verify_bytes(g.a_hp, exp_hp, 5, "hp_sub"))
        g.a_hp = NULL;

    // dt drain: subss xmm1,xmm0  = F3 0F 5C C8
    BYTE exp_dt[] = {0xF3,0x0F,0x5C,0xC8};
    if(!verify_bytes(g.a_dt, exp_dt, 4, "dt_drain"))
        g.a_dt = NULL;

    // speed float sanity check - make sure address is readable first
    if(!IsBadReadPtr(g.a_speed, 4)) {
        float spd = *(float*)g.a_speed;
        if(spd < 0.5f || spd > 3.0f) {
            log_msg("warning: speed float looks wrong (%.2f) - wrong exe?", spd);
            g.a_speed = NULL;
        } else {
            log_msg("speed: %.2f ok", spd);
        }
    } else {
        log_msg("!! speed address unreadable - wrong exe?");
        g.a_speed = NULL;
    }

    log_msg("hp=%p dt=%p spd=%p", g.a_hp, g.a_dt, g.a_speed);
}

static void setup_keys() {
    hk_add("hp",     ini_vkey("Keys","ToggleHP",    VK_F1), cb_inf_hp,      &g);
    hk_add("dt",     ini_vkey("Keys","ToggleDT",    VK_F2), cb_inf_dt,      &g);
    hk_add("turbo",  ini_vkey("Keys","Turbo",       VK_F3), cb_turbo,       &g);
    hk_add("jumps",  ini_vkey("Keys","InfJumps",    VK_F4), cb_inf_jumps,   &g);
    hk_add("stysw",  ini_vkey("Keys","StyleSwitch", VK_F5), cb_style_switch,&g);
    hk_add("style",  ini_vkey("Keys","CycleStyle",  VK_F6), cb_cycle_style, &g);
}

static DWORD WINAPI init_thread(LPVOID) {
    log_init("dmc3_mod.log");
    log_msg("dmc3 hd trainer | %s", __DATE__);

    Sleep(6000); // hd collection takes longer to init than the 2006 port

    state_defaults(&g);
    ini_set_path("dmc3_mod.ini");
    g.inf_hp      = ini_bool("Toggles","InfiniteHP",   false);
    g.inf_dt      = ini_bool("Toggles","InfiniteDT",   false);
    g.turbo       = ini_bool("Toggles","Turbo",        false);
    g.style_switch= ini_bool("Toggles","StyleSwitch",  false);
    g.game_speed  = ini_float("Multipliers","TurboSpeed", 1.2f);

    setup_keys();
    setup_addrs();

    g.ready = true;
    state_log(&g);
    log_msg("ready");

    hk_start();
    g_run = true;
    while(g_run) { apply(); Sleep(16); }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID) {
    if(reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hmod);
        CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
    }
    if(reason == DLL_PROCESS_DETACH) {
        g_run = false;
        hk_stop();
        // restore nop patches so game code is clean
        if(p_hp && g.a_hp) nop_restore(g.a_hp, 5, sv_hp);
        if(p_dt && g.a_dt) nop_restore(g.a_dt, 4, sv_dt);
        hook_remove_all();
        log_close();
    }
    return TRUE;
}
