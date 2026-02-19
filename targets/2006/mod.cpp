// dmc3se trainer + dx9 overlay
// for the 2006 ubisoft pc port (32-bit dmc3se.exe)
// all addresses from cheat engine + x64dbg

#include "../../core/core.h"
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

static ModState g;
static bool g_run = false;

// addresses
// base is always 0x400000 for this exe, no ASLR
// player struct is a static alloc not heap, pointer scan on hp ended up here

#define OFF_PLAYER  0x188A600

// player struct offsets (from CE struct dissect, some of these took a while)
#define PL_WORKRATE 0x0010      // float, 1.0 = normal speed
#define PL_POS      0x004C      // x,y,z,w floats
#define PL_DT       0x2834      // float
#define PL_HP       0x2A4C      // float, 1000 per bar segment

// lockon target ptr, deref for enemy base. NULL when not locked on
#define OFF_LOCKON  0x188E1E4

// damage calc mid-function hook
// hp delta is at [esp+0x24] as float, knockback at +0x20, stun at +0x1C
// took a while to find the right spot - bp on enemy hp write then trace back
#define OFF_DMGCALC 0x046C4C

// tried to find the air hike counter byte but too many candidates
// scanning for the inc instruction instead
#define SIG_JUMP    "FF 86 ?? ?? 00 00 83 BE ?? ?? 00 00 02"

// stuff i found but dont need right now
//static DWORD off_style = 0x76B220;   // current style id
//static DWORD off_d3ddev = 0x212F374; // game's d3d device
//static DWORD off_cam = 0x76BAAC;     // camera controller

// enemy hp offsets. enemies are different classes but these offsets
// are the same for all of them (checked on a few enemy types)
// position is at +0x10 (same as the define in state_defaults)
#define EN_HP    0x21B0
#define EN_MAXHP 0x2184

// state 

static BYTE* g_base = NULL;
static BYTE sv_jmp[6];
static bool p_jmp = 0;

// dmg hook 
// intercepts all combat damage for combo tracking + multiplier

static BYTE* g_orig_dmg = NULL;

static void __cdecl process_dmg(float* hp_delta) {
    float d = *hp_delta;
    if(d <= 0) return;

    g.combo_hits++;
    g.combo_dmg += d;
    g.combo_last_t = GetTickCount();

    if(g.dmg_mult != 1.0f) *hp_delta = d * g.dmg_mult;
    if(g.one_hit) *hp_delta = 99999.0f;
}

// naked because we need precise stack control
static void __declspec(naked) hooked_dmg() {
    __asm {
        pushad                  // +32
        pushfd                  // +4  = +36 total
        lea eax, [esp+0x3C]    // original [esp+0x24] shifted by 36
        push eax
        call process_dmg
        add esp, 4
        popfd
        popad
        jmp [g_orig_dmg]
    }
}

// dx9 overlay 
// endscene hook based on the dummy device vtable trick
// w2s math from a gamedeception post, rest is mine

typedef HRESULT(WINAPI* tEndScene)(IDirect3DDevice9*);
typedef HRESULT(WINAPI* tReset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static tEndScene orig_es = NULL;
static tReset orig_rst = NULL;

static void* g_font = NULL;
static HMODULE g_d3dx = NULL;

// d3dxfont vtable slots
#define FONT_RELEASE      2
#define FONT_DRAWTEXTA    14
#define FONT_ONLOST       16
#define FONT_ONRESET      17

struct TLVert { float x,y,z,w; DWORD col; };

static void draw_rect(IDirect3DDevice9* dev, float x, float y, float w, float h, DWORD col) {
    TLVert v[4] = {
        {x,   y,   0,1,col}, {x+w, y,   0,1,col},
        {x,   y+h, 0,1,col}, {x+w, y+h, 0,1,col},
    };
    dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TLVert));
}

// world->screen, returns false if behind camera
// grabbed this from a forum post and cleaned it up
static bool w2s(IDirect3DDevice9* dev, float wx, float wy, float wz, float* sx, float* sy) {
    D3DMATRIX view, proj;
    D3DVIEWPORT9 vp;
    dev->GetTransform(D3DTS_VIEW, &view);
    dev->GetTransform(D3DTS_PROJECTION, &proj);
    dev->GetViewport(&vp);

    float vx = wx*view._11 + wy*view._21 + wz*view._31 + view._41;
    float vy = wx*view._12 + wy*view._22 + wz*view._32 + view._42;
    //float vz = wx*view._13 + wy*view._23 + wz*view._33 + view._43;
    float vw = wx*view._14 + wy*view._24 + wz*view._34 + view._44;

    float px = vx*proj._11 + vy*proj._21 + vw*proj._41;
    float py = vx*proj._12 + vy*proj._22 + vw*proj._42;
    float pw = vx*proj._14 + vy*proj._24 + vw*proj._44;

    if(pw < 0.001f) return false;
    *sx = vp.X + (1.0f + px/pw) * vp.Width * 0.5f;
    *sy = vp.Y + (1.0f - py/pw) * vp.Height * 0.5f;
    return true;
}

static void font_draw(void* font, const char* txt, RECT* rc, DWORD fmt, DWORD color) {
    if(!font) return;
    typedef int(__stdcall* fn)(void*,void*,const char*,int,RECT*,DWORD,DWORD);
    fn f = (fn)(*(void***)font)[FONT_DRAWTEXTA];
    f(font, NULL, txt, -1, rc, fmt, color);
}

static void render_overlay(IDirect3DDevice9* dev) {
    DWORD old_fvf, old_z, old_ab, old_lit;
    dev->GetFVF(&old_fvf);
    dev->GetRenderState(D3DRS_ZENABLE, &old_z);
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &old_ab);
    dev->GetRenderState(D3DRS_LIGHTING, &old_lit);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetTexture(0, NULL);

    // enemy hp bar - only shows for locked-on target
    // tried iterating all enemies but they arent in a flat array,
    // each type is a different size struct allocated separately.
    // locked-on target is good enough and way simpler
    if(g.show_hp && g.a_lockon) {
        BYTE* enemy = *(BYTE**)g.a_lockon;
        if(enemy && !IsBadReadPtr(enemy, 0x2200)) {
            float hp    = *(float*)(enemy + EN_HP);
            float maxhp = *(float*)(enemy + EN_MAXHP);
            if(maxhp > 0 && hp > 0) {
                float* epos = (float*)(enemy + 0x10);
                float sx, sy;
                // +2.2 on Y to put the bar above the enemy's head-ish
                if(w2s(dev, epos[0], epos[1] + 2.2f, epos[2], &sx, &sy)) {
                    float pct = hp / maxhp;
                    if(pct > 1.0f) pct = 1.0f;

                    float bx = sx - 30, by = sy - 6;
                    draw_rect(dev, bx-1, by-1, 62, 8, 0xC0000000);
                    BYTE r = (BYTE)(255*(1.0f-pct));
                    BYTE gr = (BYTE)(255*pct);
                    draw_rect(dev, bx, by, 60*pct, 6, 0xD0000000|(r<<16)|(gr<<8));

                    if(g_font) {
                        char buf[32];
                        _snprintf(buf, 32, "%.0f/%.0f", hp, maxhp);
                        RECT rc = {(LONG)bx, (LONG)(by+8), (LONG)(bx+60), (LONG)(by+22)};
                        font_draw(g_font, buf, &rc, 1, 0xC0FFFFFF);
                    }
                }
            }
        }
    }

    // combo tracker
    if(g.show_combo && g_font) {
        DWORD now = GetTickCount();
        if(g.combo_hits > 0 && (now - g.combo_last_t) > 2000) {
            if(g.combo_hits > g.combo_best) g.combo_best = g.combo_hits;
            g.combo_hits = 0;
            g.combo_dmg = 0;
        }

        if(g.combo_hits > 0) {
            char buf[128];
            _snprintf(buf, 128, "%d HITS  %.0f DMG", g.combo_hits, g.combo_dmg);
            RECT rc = {0, 40, 400, 80};
            RECT rs = {2, 42, 402, 82}; // shadow
            font_draw(g_font, buf, &rs, 0, 0xFF000000);
            DWORD c = g.combo_hits >= 50 ? 0xFFFF4444 :
                      g.combo_hits >= 20 ? 0xFFFFAA00 : 0xFFFFFFFF;
            font_draw(g_font, buf, &rc, 0, c);
        }
        if(g.combo_best > 0) {
            char buf[64]; _snprintf(buf, 64, "BEST: %d", g.combo_best);
            RECT rc = {0, 70, 200, 90};
            font_draw(g_font, buf, &rc, 0, 0x80FFFFFF);
        }
    }

    // status bar
    if(g_font) {
        char s[256];
        _snprintf(s, 256,
            "[F1] HP:%s  [F2] DT:%s  [F3] DMG:%.0fx  [F4] OHK:%s\n"
            "[F5] Turbo:%s  [F6] Jumps:%s  [F7] HP Bar  [F8] Combo",
            g.inf_hp?"INF":"---", g.inf_dt?"INF":"---",
            g.dmg_mult, g.one_hit?"ON":"---",
            g.turbo?"ON":"---", g.inf_jumps?"ON":"---");
        D3DVIEWPORT9 vp; dev->GetViewport(&vp);
        RECT rc = {4, (LONG)(vp.Height-44), (LONG)vp.Width, (LONG)vp.Height};
        RECT rs = {5, (LONG)(vp.Height-43), (LONG)vp.Width, (LONG)vp.Height};
        font_draw(g_font, s, &rs, 0, 0xC0000000);
        font_draw(g_font, s, &rc, 0, 0xC0FFFFFF);
    }

    dev->SetFVF(old_fvf);
    dev->SetRenderState(D3DRS_ZENABLE, old_z);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, old_ab);
    dev->SetRenderState(D3DRS_LIGHTING, old_lit);
}

static void create_font(IDirect3DDevice9* dev) {
    if(!g_d3dx) {
        const char* dlls[] = {"d3dx9_43.dll","d3dx9_42.dll","d3dx9_41.dll","d3dx9_40.dll","d3dx9_39.dll",NULL};
        for(int i=0;dlls[i];i++) { g_d3dx=LoadLibraryA(dlls[i]); if(g_d3dx) break; }
    }
    if(!g_d3dx) { log_msg("d3dx9 not found, no text overlay"); return; }
    typedef HRESULT(WINAPI* pCreate)(IDirect3DDevice9*,INT,UINT,UINT,UINT,BOOL,DWORD,DWORD,DWORD,DWORD,LPCSTR,void**);
    pCreate fn = (pCreate)GetProcAddress(g_d3dx, "D3DXCreateFontA");
    if(!fn) return;
    fn(dev, 15, 0, 600, 1, FALSE, DEFAULT_CHARSET, 0, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Consolas", &g_font);
}

static HRESULT WINAPI hk_endscene(IDirect3DDevice9* dev) {
    static bool once = true;
    if(once) { create_font(dev); once=false; }
    if(g.ready) render_overlay(dev);
    return orig_es(dev);
}

static HRESULT WINAPI hk_reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if(g_font) {
        typedef ULONG(__stdcall*fn)(void*);
        ((fn)(*(void***)g_font)[FONT_ONLOST])(g_font);
    }
    HRESULT hr = orig_rst(dev, pp);
    if(SUCCEEDED(hr) && g_font) {
        typedef HRESULT(__stdcall*fn)(void*);
        ((fn)(*(void***)g_font)[FONT_ONRESET])(g_font);
    }
    return hr;
}

static void setup_overlay() {
    // dummy device trick to get vtable ptrs for endscene/reset
    WNDCLASSEXA wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=DefWindowProcA;
    wc.hInstance=GetModuleHandleA(NULL); wc.lpszClassName="__d3d9";
    RegisterClassExA(&wc);
    HWND hw = CreateWindowExA(0,"__d3d9","",WS_OVERLAPPEDWINDOW,0,0,4,4,NULL,NULL,wc.hInstance,NULL);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if(!d3d) { log_msg("overlay: d3d9 fail"); DestroyWindow(hw); return; }

    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow=hw;
    IDirect3DDevice9* dev=NULL;
    if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hw,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev))) {
        log_msg("overlay: device fail"); d3d->Release(); DestroyWindow(hw); return;
    }

    void** vt = *(void***)dev;
    tEndScene real_es = (tEndScene)vt[42];  // EndScene
    tReset real_rst = (tReset)vt[16];       // Reset
    dev->Release(); d3d->Release(); DestroyWindow(hw);

    BYTE* tr=NULL;
    if(hook_install("EndScene",(BYTE*)real_es,(BYTE*)hk_endscene,5,&tr))
        orig_es=(tEndScene)tr;
    if(hook_install("Reset",(BYTE*)real_rst,(BYTE*)hk_reset,5,&tr))
        orig_rst=(tReset)tr;
    log_msg("overlay ok");
}

// setup

static void setup_addrs() {
    g_base = (BYTE*)GetModuleHandleA(NULL);
    log_msg("base: %p", g_base);

    g.a_player = g_base + OFF_PLAYER;
    g.a_hp = g.a_player + PL_HP;
    g.a_dt = g.a_player + PL_DT;
    g.a_speed = g.a_player + PL_WORKRATE;
    g.a_lockon = g_base + OFF_LOCKON;
    g.a_dmg = g_base + OFF_DMGCALC;

    // sanity check - if the player pos is garbage we're probably in the wrong exe
    if(IsBadReadPtr(g.a_player, 0x100)) {
        log_msg("!! player base bad (%p) - wrong exe?", g.a_player);
    } else {
        float* pos = (float*)(g.a_player + PL_POS);
        log_msg("player @ %p, pos=(%.0f,%.0f,%.0f)", g.a_player, pos[0], pos[1], pos[2]);
    }

    // jump counter - aob scan because i couldnt find the offset manually
    g.a_jumps = aob_scan(NULL, SIG_JUMP);

    // hook the damage function
    if(hook_install("dmgcalc", g.a_dmg, (BYTE*)hooked_dmg, 5, &g_orig_dmg))
        log_msg("dmg hook ok");
    else
        log_msg("dmg hook FAILED");
}

static void apply() {
    if(!g.ready) return;

    // hp/dt freeze - just write max value every frame
    if(g.a_hp && g.inf_hp) *(float*)g.a_hp = 20000.0f;
    if(g.a_dt && g.inf_dt) *(float*)g.a_dt = 10000.0f;

    // turbo
    if(g.a_speed)
        *(float*)g.a_speed = g.turbo ? g.game_speed : 1.0f;

    // nop jump counter inc (only if aob found it)
    if(g.a_jumps && g.inf_jumps && !p_jmp) { nop_patch(g.a_jumps,6,sv_jmp); p_jmp=1; }
    if(g.a_jumps && !g.inf_jumps && p_jmp) { nop_restore(g.a_jumps,6,sv_jmp); p_jmp=0; }
}

static void setup_keys() {
    hk_add("hp",    ini_vkey("Keys","ToggleHP",   VK_F1), cb_inf_hp,    &g);
    hk_add("dt",    ini_vkey("Keys","ToggleDT",   VK_F2), cb_inf_dt,    &g);
    hk_add("dmg",   ini_vkey("Keys","CycleDmg",   VK_F3), cb_cycle_dmg, &g);
    hk_add("ohk",   ini_vkey("Keys","OneHitKill", VK_F4), cb_one_hit,   &g);
    hk_add("turbo", ini_vkey("Keys","Turbo",      VK_F5), cb_turbo,     &g);
    hk_add("jumps", ini_vkey("Keys","InfJumps",   VK_F6), cb_inf_jumps, &g);
    hk_add("hpbar", ini_vkey("Keys","HPBars",     VK_F7), cb_show_hp,   &g);
    hk_add("combo", ini_vkey("Keys","Combo",      VK_F8), cb_show_combo,&g);
}

static DWORD WINAPI init_thread(LPVOID) {
    log_init("dmc3_mod.log");
    log_msg("dmc3 trainer v1 | %s", __DATE__);

    Sleep(5000); // wait for game to load

    state_defaults(&g);
    ini_set_path("dmc3_mod.ini");
    g.inf_hp     = ini_bool("Toggles","InfiniteHP", false);
    g.inf_dt     = ini_bool("Toggles","InfiniteDT", false);
    g.turbo      = ini_bool("Toggles","Turbo",      false);
    g.show_hp    = ini_bool("Overlay","HPBars",      true);
    g.show_combo = ini_bool("Overlay","ComboTracker",true);
    g.dmg_mult   = ini_float("Multipliers","Damage",     1.0f);
    g.game_speed = ini_float("Multipliers","TurboSpeed",  1.2f);

    setup_keys();
    setup_addrs();
    setup_overlay();

    g.ready = true;
    state_log(&g);
    log_msg("ready");

    hk_start();
    g_run = true;
    while(g_run) { apply(); Sleep(16); }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hmod);
        CreateThread(NULL,0,init_thread,NULL,0,NULL);
    }
    if(reason==DLL_PROCESS_DETACH) {
        g_run=false; hk_stop(); hook_remove_all();
        if(g_font) { typedef ULONG(__stdcall*fn)(void*); ((fn)(*(void***)g_font)[FONT_RELEASE])(g_font); }
        log_close();
    }
    return TRUE;
}
