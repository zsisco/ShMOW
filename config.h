#ifndef CONFIG_H
#define CONFIG_H

// Mod (Mod1 == alt) and master size
#define MOD Mod1Mask

// Colors
#define FOCUS "rgb:88/88/ff"
#define UNFOCUS "rgb:fd/f6/e3"
#define STATUS "rgb:3c/b3/71"

#define PANEL -18
#define FONTFACTOR 6

const char* dmenucmd[] = {"dmenu_run",NULL};
const char* urxvtcmd[] = {"urxvt",NULL};
const char* filecmd[] = {"urxvt -e ranger",NULL};

#define DESKTOPCHANGE(K,N) \
    { MOD,             K,         change_desktop,   {.i=N}}, \
    { MOD|ShiftMask,   K,         client_to_desktop,{.i=N}}, 

// Shortcuts
static struct key keys[] = {
    //MOD              KEY        FUNCTION           ARGS
    { MOD|ShiftMask,   XK_q,      kill_client,       {NULL}},
    { MOD,             XK_Tab,    next_win,          {NULL}},
    { MOD|ShiftMask,   XK_Tab,    prev_win,          {NULL}},
    { MOD,             XK_b,      togglepanel,       {NULL}},

    { MOD,             XK_comma,  prev_desktop,      {NULL}},
    { MOD,             XK_period, next_desktop,      {NULL}},

      DESKTOPCHANGE(   XK_1,                              1)
      DESKTOPCHANGE(   XK_2,                              2)
      DESKTOPCHANGE(   XK_3,                              3)

    { MOD,             XK_r,      spawn,             {.com = filecmd}},
    { MOD,             XK_p,      spawn,             {.com = dmenucmd}},
    { MOD,             XK_Return, spawn,             {.com = urxvtcmd}},

    { MOD|ControlMask, XK_t,      quit,              {NULL}}
};

#endif
