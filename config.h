#ifndef CONFIG_H
#define CONFIG_H

// Mod (Mod1 == alt) and master size
#define MOD Mod1Mask

// Colors
#define FOCUS "rgb:88/88/ff"
#define UNFOCUS "rgb:fd/f6/e3"
#define STATUS "rgb:cd/85/3f"
#define STATUSTXT "rgb:f9/f9/e9"

// How much of the bar to hide (all of it). Do not change!
#define PANEL -16
#define FONTFACTOR 6

const char* dmenucmd[] = {"dmenu_run",NULL};
const char* urxvtcmd[] = {"urxvt",NULL};

// Shortcuts
static struct key keys[] = {
    //MOD              KEY        FUNCTION     ARGS
    { MOD|ShiftMask,   XK_q,      kill_client, {NULL}},
    { MOD,             XK_Tab,    next_win,    {NULL}},
    { MOD|ShiftMask,   XK_Tab,    prev_win,    {NULL}},
    { MOD,             XK_b,      togglepanel, {NULL}},
    { MOD,             XK_p,      spawn,       {.com = dmenucmd}},
    { MOD,             XK_Return, spawn,       {.com = urxvtcmd}},
    { MOD|ControlMask, XK_t,      quit,        {NULL}}
};

#endif
