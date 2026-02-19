#include "core.h"

// log

static FILE* g_log = NULL;
static CRITICAL_SECTION g_logcs;
static bool g_log_ok = false;

void log_init(const char* path) {
    if (g_log_ok) return;
    InitializeCriticalSection(&g_logcs);
    g_log = fopen(path, "a");
    if (!g_log) return;
    g_log_ok = true;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "\n--- %04d-%02d-%02d %02d:%02d:%02d ---\n",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    fflush(g_log);
}

void log_msg(const char* fmt, ...) {
    if (!g_log_ok) return;
    EnterCriticalSection(&g_logcs);
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list a; va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fprintf(g_log, "\n");
    fflush(g_log);
    LeaveCriticalSection(&g_logcs);
}

void log_close() {
    if (g_log) { fclose(g_log); g_log = NULL; }
    if (g_log_ok) { DeleteCriticalSection(&g_logcs); g_log_ok = false; }
}

// ini

static char g_ini[MAX_PATH] = {0};

void ini_set_path(const char* path) { GetFullPathNameA(path, MAX_PATH, g_ini, NULL); }

int ini_int(const char* sec, const char* key, int def) {
    return GetPrivateProfileIntA(sec, key, def, g_ini);
}

float ini_float(const char* sec, const char* key, float def) {
    char b[64];
    GetPrivateProfileStringA(sec, key, "", b, 64, g_ini);
    return b[0] ? (float)atof(b) : def;
}

bool ini_bool(const char* sec, const char* key, bool def) {
    char b[16];
    GetPrivateProfileStringA(sec, key, "", b, 16, g_ini);
    if (!b[0]) return def;
    return (_stricmp(b,"true")==0 || b[0]=='1');
}

int ini_vkey(const char* sec, const char* key, int def) {
    char b[32];
    GetPrivateProfileStringA(sec, key, "", b, 32, g_ini);
    if (!b[0]) return def;
    char* s = b; while(*s==' ')s++;
    int v = parse_vk(s);
    return v ? v : def;
}

// aob scan

static int hv(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+c-'a';
    if(c>='A'&&c<='F') return 10+c-'A';
    return -1;
}

struct PB { BYTE v; bool w; };

static int parse_pat(const char* sig, PB* out, int mx) {
    int n=0; const char* p=sig;
    while(*p && n<mx) {
        while(*p==' ')p++;
        if(!*p) break;
        if(p[0]=='?'&&p[1]=='?') { out[n].v=0; out[n].w=true; n++; p+=2; }
        else {
            int hi=hv(p[0]), lo=hv(p[1]);
            if(hi<0||lo<0) return 0;
            out[n].v=(BYTE)((hi<<4)|lo); out[n].w=false; n++; p+=2;
        }
    }
    return n;
}

BYTE* aob_scan(const char* mod_name, const char* sig) {
    HMODULE hm = mod_name ? GetModuleHandleA(mod_name) : GetModuleHandleA(NULL);
    if(!hm) { log_msg("scan: '%s' not loaded", mod_name?mod_name:"main"); return NULL; }
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), hm, &mi, sizeof(mi));
    BYTE* base = (BYTE*)mi.lpBaseOfDll;
    SIZE_T sz = mi.SizeOfImage;
    PB pat[128]; int pl = parse_pat(sig, pat, 128);
    if(!pl) { log_msg("scan: bad sig"); return NULL; }

    for(SIZE_T i=0; i<=sz-pl; i++) {
        bool ok=true;
        for(int j=0;j<pl;j++) if(!pat[j].w && base[i+j]!=pat[j].v){ok=false;break;}
        if(ok) { log_msg("scan: %p (+0x%X)", base+i, (DWORD)i); return base+i; }
    }
    log_msg("scan: miss");
    return NULL;
}

// hooks

struct Hook { BYTE* tgt; BYTE* tramp; BYTE sv[16]; int len; bool on; };
#define MAXHK 32
static Hook g_hooks[MAXHK]; static int g_nh=0;

bool hook_install(const char* tag, BYTE* target, BYTE* detour, int stolen, BYTE** trampoline) {
    if(!target||!detour||stolen<5||g_nh>=MAXHK) return false;
    BYTE* tr = (BYTE*)VirtualAlloc(NULL, stolen+5, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if(!tr) { log_msg("hook '%s': alloc fail", tag); return false; }
    memcpy(tr, target, stolen);
    tr[stolen]=0xE9;
    *(DWORD*)(tr+stolen+1) = (DWORD)((target+stolen)-(tr+stolen+5));

    DWORD old;
    VirtualProtect(target, stolen, PAGE_EXECUTE_READWRITE, &old);
    Hook* h = &g_hooks[g_nh];
    memcpy(h->sv, target, stolen);
    h->tgt=target; h->tramp=tr; h->len=stolen; h->on=true;
    target[0]=0xE9;
    *(DWORD*)(target+1) = (DWORD)(detour-target-5);
    for(int i=5;i<stolen;i++) target[i]=0x90;
    VirtualProtect(target, stolen, old, &old);
    *trampoline=tr; g_nh++;
    log_msg("hook '%s' %p->%p", tag, target, detour);
    return true;
}

void hook_remove_all() {
    for(int i=0;i<g_nh;i++) {
        Hook* h=&g_hooks[i]; if(!h->on)continue;
        DWORD old;
        VirtualProtect(h->tgt,h->len,PAGE_EXECUTE_READWRITE,&old);
        memcpy(h->tgt,h->sv,h->len);
        VirtualProtect(h->tgt,h->len,old,&old);
        VirtualFree(h->tramp,0,MEM_RELEASE); h->on=false;
    }
}

void nop_patch(BYTE* a, int l, BYTE* sv) {
    DWORD old; VirtualProtect(a,l,PAGE_EXECUTE_READWRITE,&old);
    if(sv) memcpy(sv,a,l); memset(a,0x90,l);
    VirtualProtect(a,l,old,&old);
}
void nop_restore(BYTE* a, int l, const BYTE* sv) {
    DWORD old; VirtualProtect(a,l,PAGE_EXECUTE_READWRITE,&old);
    memcpy(a,sv,l); VirtualProtect(a,l,old,&old);
}
void mem_patch(BYTE* a, const BYTE* d, int l) {
    DWORD old; VirtualProtect(a,l,PAGE_EXECUTE_READWRITE,&old);
    memcpy(a,d,l); VirtualProtect(a,l,old,&old);
}

// hotkeys

struct HK { int vk; hk_fn fn; void* ctx; bool was; };
static HK g_hk[32]; static int g_nhk=0;
static HANDLE g_hkt=NULL; static volatile bool g_hkr=false;

void hk_add(const char* name, int vk, hk_fn fn, void* ctx) {
    if(g_nhk>=32)return;
    g_hk[g_nhk].vk=vk; g_hk[g_nhk].fn=fn; g_hk[g_nhk].ctx=ctx;
    g_hk[g_nhk].was=false; g_nhk++;
    log_msg("key '%s' = 0x%02X", name, vk);
}

static DWORD WINAPI hk_thread(LPVOID) {
    while(g_hkr) {
        for(int i=0;i<g_nhk;i++) {
            bool d=(GetAsyncKeyState(g_hk[i].vk)&0x8000)!=0;
            if(d&&!g_hk[i].was) g_hk[i].fn(g_hk[i].ctx);
            g_hk[i].was=d;
        }
        Sleep(16);
    }
    return 0;
}
void hk_start() { g_hkr=true; g_hkt=CreateThread(NULL,0,hk_thread,NULL,0,NULL); }
void hk_stop()  { g_hkr=false; if(g_hkt){WaitForSingleObject(g_hkt,2000);CloseHandle(g_hkt);} }

int parse_vk(const char* s) {
    if(!s||!*s)return 0;
    if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')) return (int)strtol(s,NULL,16);
    if((s[0]=='F'||s[0]=='f')&&s[1]>='1'&&s[1]<='9') {
        int n=atoi(s+1); if(n>=1&&n<=12) return VK_F1+n-1;
    }
    struct{const char*n;int v;} m[]={
        {"INSERT",VK_INSERT},{"DELETE",VK_DELETE},{"HOME",VK_HOME},
        {"END",VK_END},{"PAGEUP",VK_PRIOR},{"PAGEDOWN",VK_NEXT},
        {"TAB",VK_TAB},{"SPACE",VK_SPACE},{"ENTER",VK_RETURN},
        {"ESC",VK_ESCAPE},{NULL,0}
    };
    for(int i=0;m[i].n;i++) if(_stricmp(s,m[i].n)==0) return m[i].v;
    if(strlen(s)==1) { char c=(char)toupper(s[0]); if((c>='A'&&c<='Z')||(c>='0'&&c<='9')) return c; }
    return 0;
}

// modstate

void state_defaults(ModState* s) {
    memset(s,0,sizeof(ModState));
    s->dmg_mult = 1.0f;
    s->game_speed = 1.2f;  // default turbo speed (ps2 turbo was ~1.2x)
    s->show_hp = true;     // hp bars on by default
    s->show_combo = true;
    // enemy struct offsets (found via CE struct dissect)
    s->ent_hp_off = 0x21B0;
    s->ent_maxhp_off = 0x2184;
    s->ent_pos_off = 0x10;
}

void state_log(ModState* s) {
    log_msg("  inf_hp=%d inf_dt=%d ohk=%d jumps=%d turbo=%d", s->inf_hp, s->inf_dt, s->one_hit, s->inf_jumps, s->turbo);
    log_msg("  dmg=%.0fx speed=%.1f hp_bars=%d combo=%d", s->dmg_mult, s->game_speed, s->show_hp, s->show_combo);
    log_msg("  player=%p hp=%p dt=%p dmg=%p spd=%p jmp=%p lockon=%p",
        s->a_player, s->a_hp, s->a_dt, s->a_dmg, s->a_speed, s->a_jumps, s->a_lockon);
}

void cb_inf_hp(void* p)    { ModState* s=(ModState*)p; s->inf_hp=!s->inf_hp; log_msg("inf_hp: %d",s->inf_hp); }
void cb_inf_dt(void* p)    { ModState* s=(ModState*)p; s->inf_dt=!s->inf_dt; log_msg("inf_dt: %d",s->inf_dt); }
void cb_one_hit(void* p)   { ModState* s=(ModState*)p; s->one_hit=!s->one_hit; log_msg("one_hit: %d",s->one_hit); }
void cb_inf_jumps(void* p) { ModState* s=(ModState*)p; s->inf_jumps=!s->inf_jumps; log_msg("inf_jumps: %d",s->inf_jumps); }
void cb_show_hp(void* p)   { ModState* s=(ModState*)p; s->show_hp=!s->show_hp; log_msg("hp_bars: %d",s->show_hp); }
void cb_show_combo(void* p){ ModState* s=(ModState*)p; s->show_combo=!s->show_combo; log_msg("combo: %d",s->show_combo); }

void cb_turbo(void* p) {
    ModState* s=(ModState*)p; s->turbo=!s->turbo;
    log_msg("turbo: %d (%.1fx)", s->turbo, s->game_speed);
}

void cb_cycle_dmg(void* p) {
    ModState* s=(ModState*)p;
    if(s->dmg_mult<1.5f) s->dmg_mult=2.0f;
    else if(s->dmg_mult<3.0f) s->dmg_mult=5.0f;
    else if(s->dmg_mult<7.0f) s->dmg_mult=10.0f;
    else s->dmg_mult=1.0f;
    log_msg("dmg: %.0fx", s->dmg_mult);
}
