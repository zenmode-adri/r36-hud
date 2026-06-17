/*
 * overlay_ui.c — R36 Overlay Configurator
 * SDL2 + SDL_ttf standalone UI for libr36overlay.so
 *
 * Build (cross):
 *   aarch64-linux-gnu-gcc -O2 -o overlay_ui overlay_ui.c -lSDL2 -lSDL2_ttf
 */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Paths ─────────────────────────────────────────────────────────────── */
#define FONT_BOLD   "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#define FONT_NORM   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define CONFIG_PATH "/home/ark/.config/r36overlay.conf"
#define SO_PATH     "/usr/local/lib/libr36overlay.so"
#define PRELOAD     "/etc/ld.so.preload"

/* ── Config ─────────────────────────────────────────────────────────────── */
typedef struct {
    int show_fps;
    int show_cpu_temp;
    int show_cpu_mhz;
    int show_gpu_mhz;
    int show_ram_mhz;
    int show_ram_pct;
    int show_vdd_arm;
    int show_vdd_logic;
    int show_bat;
    int corner;  /* 0=TL 1=TR 2=BL 3=BR */
    int scale;   /* 1 or 2               */
    int font;    /* 0=8x8 (default), 1=8x16 */
} OvlConfig;

static OvlConfig cfg;

static void cfg_defaults(OvlConfig *c) {
    c->show_fps      = 1;
    c->show_cpu_temp = 1;
    c->show_cpu_mhz  = 1;
    c->show_gpu_mhz  = 1;
    c->show_ram_mhz  = 1;
    c->show_ram_pct  = 0;
    c->show_vdd_arm  = 0;
    c->show_vdd_logic= 0;
    c->show_bat      = 1;
    c->corner        = 0;
    c->scale         = 1;
    c->font          = 0;
}

static void cfg_load(OvlConfig *c) {
    cfg_defaults(c);
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[64];
        if (sscanf(line, "%63[^=]=%63s", key, val) != 2) continue;
        int v = atoi(val);
        if (!strcmp(key,"show_fps"))       c->show_fps       = v;
        if (!strcmp(key,"show_cpu_temp"))  c->show_cpu_temp  = v;
        if (!strcmp(key,"show_cpu_mhz"))   c->show_cpu_mhz   = v;
        if (!strcmp(key,"show_gpu_mhz"))   c->show_gpu_mhz   = v;
        if (!strcmp(key,"show_ram_mhz"))   c->show_ram_mhz   = v;
        if (!strcmp(key,"show_ram_pct"))   c->show_ram_pct   = v;
        if (!strcmp(key,"show_vdd_arm"))   c->show_vdd_arm   = v;
        if (!strcmp(key,"show_vdd_logic")) c->show_vdd_logic = v;
        if (!strcmp(key,"show_bat"))       c->show_bat       = v;
        if (!strcmp(key,"corner"))         c->corner         = v;
        if (!strcmp(key,"scale"))          c->scale          = (v==2)?2:1;
        if (!strcmp(key,"font"))           c->font           = (v==1)?1:0;
    }
    fclose(f);
}

static void cfg_save(const OvlConfig *c) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f,
        "show_fps=%d\n"
        "show_cpu_temp=%d\n"
        "show_cpu_mhz=%d\n"
        "show_gpu_mhz=%d\n"
        "show_ram_mhz=%d\n"
        "show_ram_pct=%d\n"
        "show_vdd_arm=%d\n"
        "show_vdd_logic=%d\n"
        "show_bat=%d\n"
        "corner=%d\n"
        "scale=%d\n"
        "font=%d\n",
        c->show_fps, c->show_cpu_temp, c->show_cpu_mhz,
        c->show_gpu_mhz, c->show_ram_mhz, c->show_ram_pct,
        c->show_vdd_arm, c->show_vdd_logic, c->show_bat,
        c->corner, c->scale, c->font);
    fclose(f);
}

/* ── Install helpers ─────────────────────────────────────────────────────── */
static int so_exists(void) {
    return access(SO_PATH, F_OK) == 0;
}

static int preload_has_overlay(void) {
    FILE *f = fopen(PRELOAD, "r");
    if (!f) return 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, "libr36overlay")) { found = 1; break; }
    fclose(f);
    return found;
}

static void preload_add(void) {
    system("echo ark | sudo -S bash -c \""
           "grep -qxF '" SO_PATH "' " PRELOAD " 2>/dev/null"
           " || echo '" SO_PATH "' >> " PRELOAD "\"");
}

static void preload_remove(void) {
    system("echo ark | sudo -S sed -i '/libr36overlay/d' " PRELOAD);
}

/* ── SDL2 globals ────────────────────────────────────────────────────────── */
static SDL_Window   *win;
static SDL_Renderer *ren;
static TTF_Font     *fnt_big, *fnt_med, *fnt_sm;
static SDL_GameController *gc;
static int W, H;
static volatile int running = 1;

/* ── Draw helpers ────────────────────────────────────────────────────────── */
static void setcol(Uint8 r,Uint8 g,Uint8 b){SDL_SetRenderDrawColor(ren,r,g,b,255);}
static void fillrect(int x,int y,int w,int h){SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(ren,&rc);}

static void rounded(int x,int y,int w,int h,int rad,Uint8 r,Uint8 g,Uint8 b){
    setcol(r,g,b);
    SDL_Rect mid={x,y+rad,w,h-2*rad}; SDL_RenderFillRect(ren,&mid);
    SDL_Rect top={x+rad,y,w-2*rad,rad}; SDL_RenderFillRect(ren,&top);
    SDL_Rect bot={x+rad,y+h-rad,w-2*rad,rad}; SDL_RenderFillRect(ren,&bot);
    for(int dy=0;dy<rad;dy++){
        int dx=rad-(int)SDL_sqrtf((float)(rad*rad-dy*dy));
        SDL_RenderDrawLine(ren,x+dx,y+dy,x+w-dx,y+dy);
        SDL_RenderDrawLine(ren,x+dx,y+h-1-dy,x+w-dx,y+h-1-dy);
    }
}

static void txt(TTF_Font *f,const char *s,int x,int y,Uint8 r,Uint8 g,Uint8 b){
    if(!s||!s[0])return;
    SDL_Color c={r,g,b,255};
    SDL_Surface *sur=TTF_RenderUTF8_Blended(f,s,c); if(!sur)return;
    SDL_Texture *tex=SDL_CreateTextureFromSurface(ren,sur);
    SDL_Rect dst={x,y,sur->w,sur->h}; SDL_RenderCopy(ren,tex,NULL,&dst);
    SDL_DestroyTexture(tex); SDL_FreeSurface(sur);
}
static int txtw(TTF_Font *f,const char *s){int w=0;TTF_SizeUTF8(f,s,&w,NULL);return w;}
static void txtr(TTF_Font *f,const char *s,int rx,int y,Uint8 r,Uint8 g,Uint8 b){
    txt(f,s,rx-txtw(f,s),y,r,g,b);
}

static void draw_bg(void)  { setcol(8,10,22); SDL_RenderClear(ren); }
static void draw_header(const char *title, const char *sub){
    setcol(16,18,38); fillrect(0,0,W,48);
    setcol(60,100,255); SDL_RenderDrawLine(ren,0,48,W,48);
    txt(fnt_big,title,28,10,100,160,255);
    if(sub) txtr(fnt_sm,sub,W-16,17,70,80,130);
}
static void draw_footer(const char *hint){
    setcol(16,18,38); fillrect(0,H-26,W,26);
    setcol(40,44,80); SDL_RenderDrawLine(ren,0,H-26,W,H-26);
    txt(fnt_sm,hint,28,H-20,70,75,110);
}

/* ── Input ───────────────────────────────────────────────────────────────── */
typedef struct{int up,dn,left,right,a,b,start,sel;}Keys;
static Keys poll_keys(void){
    Keys k={0}; SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_QUIT) running=0;
        if(ev.type==SDL_KEYDOWN) switch(ev.key.keysym.sym){
            case SDLK_UP:        k.up=1;    break;
            case SDLK_DOWN:      k.dn=1;    break;
            case SDLK_LEFT:      k.left=1;  break;
            case SDLK_RIGHT:     k.right=1; break;
            case SDLK_RETURN:    k.a=1;     break;
            case SDLK_BACKSPACE: k.b=1;     break;
            case SDLK_ESCAPE:    k.sel=1;   break;
        }
        if(ev.type==SDL_CONTROLLERBUTTONDOWN) switch(ev.cbutton.button){
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    k.up=1;    break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  k.dn=1;    break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  k.left=1;  break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: k.right=1; break;
            case SDL_CONTROLLER_BUTTON_A:          k.a=1;     break;
            case SDL_CONTROLLER_BUTTON_B:          k.b=1;     break;
            case SDL_CONTROLLER_BUTTON_START:      k.start=1; break;
            case SDL_CONTROLLER_BUTTON_BACK:       k.sel=1;   break;
        }
    }
    return k;
}

/* ── Generic single-select list ─────────────────────────────────────────── */
#define MAX_ITEMS 12
typedef struct { char label[64]; char desc[80]; char tag[32]; } LItem;

static int list_select(const char *title, const char *sub,
                       LItem items[], int n, int initial, const char *hint) {
    int sel = initial, scroll = 0;
    const int IH = 52, PAD = 24;
    const int HDR = 48, FTR = 26;
    int area = H - HDR - FTR;
    int vis = area / IH; if (vis < 1) vis = 1;
    int LY = HDR + (area - (n < vis ? n : vis) * IH) / 2;
    if (LY < HDR + 4) LY = HDR + 4;

    while (running) {
        Keys k = poll_keys();
        if (k.up) { sel=(sel-1+n)%n; if(sel<scroll)scroll=sel; if(sel>scroll+vis-1)scroll=sel-vis+1; }
        if (k.dn) { sel=(sel+1)%n;   if(sel>=scroll+vis)scroll=sel-vis+1; if(sel<scroll)scroll=sel; }
        if (k.a) return sel;
        if (k.b || k.sel) return -1;

        draw_bg(); draw_header(title, sub);
        for (int i = scroll; i < n && i < scroll+vis; i++) {
            int iy = LY + (i-scroll)*IH;
            if (i == sel) {
                rounded(PAD,iy,W-2*PAD,IH-5,8,30,60,180);
                setcol(80,140,255); fillrect(PAD,iy,4,IH-5);
                txt(fnt_med,items[i].label,PAD+18,iy+6,255,255,255);
                txt(fnt_sm, items[i].desc, PAD+18,iy+28,160,190,255);
                if(items[i].tag[0]) txtr(fnt_sm,items[i].tag,W-PAD-8,iy+18,255,220,80);
            } else {
                rounded(PAD,iy,W-2*PAD,IH-5,8,16,18,34);
                txt(fnt_med,items[i].label,PAD+18,iy+6,180,185,210);
                txt(fnt_sm, items[i].desc, PAD+18,iy+28,80,85,110);
                if(items[i].tag[0]) txtr(fnt_sm,items[i].tag,W-PAD-8,iy+18,140,160,200);
            }
        }
        char def[128];
        if (hint) snprintf(def,sizeof(def),"%s",hint);
        else      snprintf(def,sizeof(def),"[DPAD] Navegar  [A] Seleccionar  [B] Atras");
        draw_footer(def);
        SDL_RenderPresent(ren); SDL_Delay(16);
    }
    return -1;
}

/* ── Multi-toggle screen ─────────────────────────────────────────────────── */
typedef struct { char label[64]; char desc[64]; int *value; } Toggle;

static void screen_toggles(const char *title, Toggle items[], int n) {
    int sel = 0, scroll = 0;
    const int IH = 52, PAD = 24;
    const int HDR = 48, FTR = 26;
    int area = H - HDR - FTR;
    int vis = area / IH; if (vis < 1) vis = 1;
    int LY = HDR + (area - (n < vis ? n : vis) * IH) / 2;
    if (LY < HDR + 4) LY = HDR + 4;

    while (running) {
        Keys k = poll_keys();
        if (k.up) { sel=(sel-1+n)%n; if(sel<scroll)scroll=sel; if(sel>scroll+vis-1)scroll=sel-vis+1; }
        if (k.dn) { sel=(sel+1)%n;   if(sel>=scroll+vis)scroll=sel-vis+1; if(sel<scroll)scroll=sel; }
        if (k.a) *items[sel].value ^= 1;
        if (k.b || k.sel) { cfg_save(&cfg); return; }

        draw_bg(); draw_header(title, NULL);
        for (int i = scroll; i < n && i < scroll+vis; i++) {
            int iy = LY + (i-scroll)*IH;
            int on = *items[i].value;
            if (i == sel) {
                rounded(PAD,iy,W-2*PAD,IH-5,8,30,60,180);
                setcol(80,140,255); fillrect(PAD,iy,4,IH-5);
                txt(fnt_med,items[i].label,PAD+18,iy+6,255,255,255);
                txt(fnt_sm, items[i].desc, PAD+18,iy+28,160,190,255);
                txtr(fnt_sm,on?"[X]":"[ ]",W-PAD-8,iy+18,on?100:255,on?255:80,on?120:80);
            } else {
                rounded(PAD,iy,W-2*PAD,IH-5,8,16,18,34);
                txt(fnt_med,items[i].label,PAD+18,iy+6,180,185,210);
                txt(fnt_sm, items[i].desc, PAD+18,iy+28,80,85,110);
                txtr(fnt_sm,on?"[X]":"[ ]",W-PAD-8,iy+18,on?80:140,on?200:160,on?100:200);
            }
        }
        draw_footer("[DPAD] Navegar  [A] Toggle  [B] Guardar y volver");
        SDL_RenderPresent(ren); SDL_Delay(16);
    }
}

/* ── Confirm screen (yes/no) ────────────────────────────────────────────── */
static int confirm(const char *title, const char *msg,
                   const char *yes, const char *no) {
    int sel = 0;
    const int PAD = 24, BTN_H = 46, BTN_W = (W - 2*PAD - 24) / 2;
    while (running) {
        Keys k = poll_keys();
        if (k.up||k.dn||k.left||k.right) sel ^= 1;
        if (k.a) return sel == 0;
        if (k.b||k.sel) return 0;
        draw_bg(); draw_header(title, NULL);
        txt(fnt_med, msg, PAD, 80, 200, 210, 240);
        int btn_y = H/2 + 20;
        int ty = btn_y + BTN_H/2 - 10;
        int yw = txtw(fnt_med,yes), nw = txtw(fnt_med,no);
        if(sel==0){rounded(PAD,btn_y,BTN_W,BTN_H,8,30,60,180);txt(fnt_med,yes,PAD+BTN_W/2-yw/2,ty,255,255,255);}
        else      {rounded(PAD,btn_y,BTN_W,BTN_H,8,16,18,34); txt(fnt_med,yes,PAD+BTN_W/2-yw/2,ty,130,135,160);}
        int x2 = PAD+BTN_W+24;
        if(sel==1){rounded(x2,btn_y,BTN_W,BTN_H,8,30,60,180);txt(fnt_med,no,x2+BTN_W/2-nw/2,ty,255,255,255);}
        else      {rounded(x2,btn_y,BTN_W,BTN_H,8,16,18,34); txt(fnt_med,no,x2+BTN_W/2-nw/2,ty,130,135,160);}
        draw_footer("[DPAD] Seleccionar  [A] Confirmar  [B] Cancelar");
        SDL_RenderPresent(ren); SDL_Delay(16);
    }
    return 0;
}

/* ── Screens ─────────────────────────────────────────────────────────────── */
static void screen_metrics(void) {
    Toggle items[] = {
        {"FPS",           "Frames por segundo",                        &cfg.show_fps      },
        {"CPU Temp",      "Temperatura en grados C",                   &cfg.show_cpu_temp },
        {"CPU MHz",       "Frecuencia actual del CPU",                 &cfg.show_cpu_mhz  },
        {"GPU MHz",       "Frecuencia actual Mali-G31",                &cfg.show_gpu_mhz  },
        {"RAM MHz",       "Frecuencia actual DMC",                     &cfg.show_ram_mhz  },
        {"RAM %",         "Uso de memoria (porcentaje)",               &cfg.show_ram_pct  },
        {"CPU voltaje",   "vdd_arm — voltaje del CPU",                  &cfg.show_vdd_arm  },
        {"GPU/RAM voltaje","vdd_logic — rail compartido GPU y RAM",    &cfg.show_vdd_logic},
        {"Bateria",       "% + mA (cargando) o mW (descargando)",      &cfg.show_bat      },
    };
    screen_toggles("Metricas", items, 9);
}

static void screen_position(void) {
    LItem items[] = {
        {"Arriba izquierda", "Esquina superior izquierda", ""},
        {"Arriba derecha",   "Esquina superior derecha",   ""},
        {"Abajo izquierda",  "Esquina inferior izquierda", ""},
        {"Abajo derecha",    "Esquina inferior derecha",   ""},
    };
    for (int i = 0; i < 4; i++)
        if (i == cfg.corner) snprintf(items[i].tag,sizeof(items[i].tag),"ACTIVO");
    int r = list_select("Posicion", "Esquina del overlay", items, 4, cfg.corner, NULL);
    if (r >= 0) { cfg.corner = r; cfg_save(&cfg); }
}

static void screen_scale(void) {
    LItem items[] = {
        {"1x", "Normal (8x8 px por caracter)",  ""},
        {"2x", "Grande (16x16 px por caracter)", ""},
    };
    int ini = cfg.scale == 2 ? 1 : 0;
    for (int i = 0; i < 2; i++)
        if ((i==0&&cfg.scale==1)||(i==1&&cfg.scale==2))
            snprintf(items[i].tag,sizeof(items[i].tag),"ACTIVO");
    int r = list_select("Escala", "Tamano del texto", items, 2, ini, NULL);
    if (r >= 0) { cfg.scale = (r==1)?2:1; cfg_save(&cfg); }
}

static void screen_font(void) {
    LItem items[] = {
        {"8x8 compacta",  "Fuente pequena, mas texto por linea (default)", ""},
        {"8x16 VGA",      "Fuente mas alta, mejor legibilidad",             ""},
    };
    int ini = cfg.font == 1 ? 1 : 0;
    if (cfg.font == 0) snprintf(items[0].tag,sizeof(items[0].tag),"ACTIVO");
    else               snprintf(items[1].tag,sizeof(items[1].tag),"ACTIVO");
    int r = list_select("Fuente", "Estilo de letra del overlay", items, 2, ini, NULL);
    if (r >= 0) { cfg.font = r; cfg_save(&cfg); }
}

static void screen_install(void) {
    int installed = so_exists() && preload_has_overlay();

    if (!so_exists()) {
        draw_bg(); draw_header("Instalar Overlay", NULL);
        txt(fnt_med, "libr36overlay.so no encontrado en:", 28, 90, 255,80,80);
        txt(fnt_sm,  SO_PATH, 28, 115, 200,200,200);
        txt(fnt_sm,  "Ejecuta deploy_overlay.py primero.", 28, 140, 160,160,180);
        draw_footer("[B] Volver");
        SDL_RenderPresent(ren);
        SDL_Delay(200);
        while (running) {
            Keys k = poll_keys();
            if (k.b || k.sel || k.a) return;
            SDL_Delay(16);
        }
        return;
    }

    if (installed) {
        if (confirm("Desinstalar", "Quitar overlay de /etc/ld.so.preload?",
                    "Desinstalar", "Cancelar")) {
            preload_remove();
        }
    } else {
        if (confirm("Instalar", "Anadir overlay a /etc/ld.so.preload?\n(activo en todos los emuladores)",
                    "Instalar", "Cancelar")) {
            preload_add();
        }
    }
}

/* ── Main menu ───────────────────────────────────────────────────────────── */
static void screen_main(void) {
    while (running) {
        int installed = so_exists() && preload_has_overlay();

        LItem items[6];
        /* 0: Metricas */
        snprintf(items[0].label,64,"Metricas");
        snprintf(items[0].desc, 80,"Elegir que mostrar en el overlay");
        items[0].tag[0] = 0;

        /* 1: Posicion */
        const char *corners[] = {"Arriba-Izq","Arriba-Der","Abajo-Izq","Abajo-Der"};
        snprintf(items[1].label,64,"Posicion");
        snprintf(items[1].desc, 80,"Esquina donde aparece el overlay");
        snprintf(items[1].tag, 32,"%s", corners[cfg.corner]);

        /* 2: Escala */
        snprintf(items[2].label,64,"Escala");
        snprintf(items[2].desc, 80,"Tamano del texto");
        snprintf(items[2].tag, 32,"%dx", cfg.scale);

        /* 3: Fuente */
        snprintf(items[3].label,64,"Fuente");
        snprintf(items[3].desc, 80,"Estilo de letra del overlay");
        snprintf(items[3].tag, 32, cfg.font == 1 ? "8x16" : "8x8");

        /* 4: Instalar / Desinstalar */
        if (installed) {
            snprintf(items[4].label,64,"Desinstalar");
            snprintf(items[4].desc, 80,"Quitar de /etc/ld.so.preload");
            snprintf(items[4].tag, 32,"ACTIVO");
        } else {
            snprintf(items[4].label,64,"Instalar");
            snprintf(items[4].desc, 80,"Activar en todos los emuladores");
            items[4].tag[0] = 0;
        }

        /* 5: Salir */
        snprintf(items[5].label,64,"Salir");
        snprintf(items[5].desc, 80,"Cerrar R36 Overlay Config");
        items[5].tag[0] = 0;

        char sub[64];
        snprintf(sub, sizeof(sub), "LD_PRELOAD: %s", installed ? "ACTIVO" : "inactivo");

        int sel = 0;
        int r = list_select("R36 Overlay", sub, items, 6, sel, NULL);
        if (r < 0 || r == 5) { running = 0; return; }

        if (r == 0) screen_metrics();
        if (r == 1) screen_position();
        if (r == 2) screen_scale();
        if (r == 3) screen_font();
        if (r == 4) screen_install();
    }
}

/* ── App init / destroy ─────────────────────────────────────────────────── */
static void app_init(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    TTF_Init();
    win = SDL_CreateWindow("R36 Overlay Config",
                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                           640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_GetWindowSize(win, &W, &H);
    fnt_big = TTF_OpenFont(FONT_BOLD, 18);
    fnt_med = TTF_OpenFont(FONT_BOLD, 15);
    fnt_sm  = TTF_OpenFont(FONT_NORM, 13);
    if (!fnt_big) fnt_big = TTF_OpenFont(FONT_NORM, 18);
    if (!fnt_med) fnt_med = TTF_OpenFont(FONT_NORM, 15);
    if (!fnt_sm)  fnt_sm  = TTF_OpenFont(FONT_NORM, 13);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { gc = SDL_GameControllerOpen(i); break; }
}

static void app_destroy(void) {
    if (gc) SDL_GameControllerClose(gc);
    if (fnt_big) TTF_CloseFont(fnt_big);
    if (fnt_med) TTF_CloseFont(fnt_med);
    if (fnt_sm)  TTF_CloseFont(fnt_sm);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
}

int main(void) {
    cfg_load(&cfg);
    app_init();
    screen_main();
    app_destroy();
    return 0;
}
