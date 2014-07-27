#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
/*#include <X11/extensions/Xinerama.h>*/
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define TABLENGTH(X) (sizeof(X)/sizeof(*X))

typedef union {
    const char** com;
    const int i;
} Arg;

/* Structs */
struct key {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
};

typedef struct Client Client;
struct Client{
    /* Prev and next Client */
    Client *next;
    Client *prev;

    /* The window */
    Window win;
	char name[256];
};

typedef struct Desktop Desktop;
struct Desktop {
    int mode;
    Client *head;
    Client *current;
};

/* Functions */
static void add_window(Window w);
/* static void change_desktop(const Arg arg); */
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void propertynotify(XEvent *e);
static void die(const char* e);
static unsigned long getcolor(const char* color);
static void grabkeys();
static void keypress(XEvent *e);
static void kill_client();
static void maprequest(XEvent *e);
static void next_win();
static void prev_win();
static void quit();
static void remove_window(Window w);
static void send_kill_signal(Window w);
static void setup();
static void sigchld(int unused);
static void spawn(const Arg arg);
static void start();
static void tile();
static void update_current();
static void update_title(Client *c);
static void update_status(void);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static GC setcolor(const char* col);
static void drawbar();
static void togglepanel();

/* Include configuration file (need struct key) */
#include "config.h"

/* Static Variables */
static Display *dis;
static int bool_quit;
static int current_desktop;
static int mode; /* mode=1 for monocle */
static int sh;
static int sw;
static int screen;
static unsigned int win_focus;
static unsigned int win_unfocus;
static unsigned int win_status;
static Window root;
static Client *head;
static Client *current;
static Atom NetWMName;
static const char broken[] = "broken";
static char status_text[256];
/* draw bar stuff */
static GC gc;
static Colormap cmap;
static XColor color;

static int panel_pad;

/* Events array */
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [DestroyNotify] = destroynotify,
    [PropertyNotify] = propertynotify,
    [ConfigureRequest] = configurerequest
};

/* Desktop array */
static Desktop desktops[10];

void add_window(Window w) {
    Client *c,*t;

    if(!(c = (Client *)calloc(1,sizeof(Client))))
        die("Error calloc!");

    if(head == NULL) {
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        head = c;
    }
    else {
        for(t=head;t->next;t=t->next);

        c->next = NULL;
        c->prev = t;
        c->win = w;

        t->next = c;
    }

    update_title(c);
    fprintf(stdout, "add win: %s\n", c->name);
    current = c;
}

void togglepanel() {
    if (panel_pad == 0) {
        panel_pad = PANEL;
    }
    else {
        panel_pad = 0;
    }
    tile();
    update_current();
    drawbar();
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;

    /* For fullscreen mplayer (and maybe some other program) */
    Client *c;
    for(c=head;c;c=c->next)
        if(ev->window == c->win) {
            XMapWindow(dis,ev->window);
            return;
        }

    add_window(ev->window);
    XMapWindow(dis,ev->window);
    tile();
    update_current();
    update_status();
    drawbar();
}

void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dis, ev->window, ev->value_mask, &wc);
}

void propertynotify(XEvent *e) {
    XPropertyEvent *ev = &e->xproperty;
    Client *c, *tmp;

    fprintf(stdout, "propertynotify: we're in.\n");
    for(tmp=head; tmp; tmp=tmp->next) 
        if(tmp->win == ev->window) 
            c = tmp;

    if((ev->window == root) && (ev->atom == XA_WM_NAME)) {
        fprintf(stdout, "\troot XA_WM_NAME\n");
        update_status();
        drawbar();
    }

    else if(ev->state == PropertyDelete)
        return; /*ignore*/

    else if(c) {
        if(ev->atom == XA_WM_NAME || ev->atom == NetWMName) {
            fprintf(stdout, "\tclient name\n");
            update_title(c);
            drawbar();
        }
    }
    fprintf(stdout, "propertynotify: we're out.\n");
}

void destroynotify(XEvent *e) {
    int i=0;
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    // Uber (and ugly) hack ;)
    for(c=head;c;c=c->next)
        if(ev->window == c->win)
            i++;
    
    // End of the hack
    if(i == 0)
        return;

    remove_window(ev->window);
    tile();
    update_current();
}

void die(const char* e) {
    fprintf(stdout,"catwm: %s\n",e);
    exit(1);
}

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(dis,screen);

    if(!XAllocNamedColor(dis,map,color,&c,&c))
        die("Error parsing color!");

    return c.pixel;
}

void grabkeys() {
    int i;
    KeyCode code;

    for(i=0;i<TABLENGTH(keys);++i) {
        if((code = XKeysymToKeycode(dis,keys[i].keysym))) {
            XGrabKey(dis,code,keys[i].mod,root,True,GrabModeAsync,GrabModeAsync);
        }
    }
}

void keypress(XEvent *e) {
    int i;
    XKeyEvent ke = e->xkey;
    KeySym keysym = XKeycodeToKeysym(dis,ke.keycode,0);

    for(i=0;i<TABLENGTH(keys);++i) {
        if(keys[i].keysym == keysym && keys[i].mod == ke.state) {
            keys[i].function(keys[i].arg);
        }
    }
}

void kill_client() {
	if(current != NULL) {
		/* send delete signal to window */
		XEvent ke;
		ke.type = ClientMessage;
		ke.xclient.window = current->win;
		ke.xclient.message_type = XInternAtom(dis, "WM_PROTOCOLS", True);
		ke.xclient.format = 32;
		ke.xclient.data.l[0] = XInternAtom(dis, "WM_DELETE_WINDOW", True);
		ke.xclient.data.l[1] = CurrentTime;
		XSendEvent(dis, current->win, False, NoEventMask, &ke);
		send_kill_signal(current->win);
	}
}

void next_win() {
    Client *c;

    if(current != NULL && head != NULL) {
		if(current->next == NULL)
            c = head;
        else
            c = current->next;

        current = c;
        update_current();
    }
}

void prev_win() {
    Client *c;

    if(current != NULL && head != NULL) {
        if(current->prev == NULL)
            for(c=head;c->next;c=c->next);
        else
            c = current->prev;

        current = c;
        update_current();
    }
}

void quit() {
    Window root_return, parent;
    Window *children;
    int i;
    unsigned int nchildren;
    XEvent ev;

    /*
    * if a Client refuses to terminate itself,
    * we kill every window remaining the brutal way.
    * Since we're stuck in the while(nchildren > 0) { ... } loop
    * we can't exit through the main method.
    * This all happens if MOD+q is pushed a second time.
    */
    if(bool_quit == 1) {
        XUngrabKey(dis, AnyKey, AnyModifier, root);
        XDestroySubwindows(dis, root);
        fprintf(stdout, "catwm: Thanks for using!\n");
        XCloseDisplay(dis);
        die("forced shutdown");
    }

    bool_quit = 1;
    XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
    for(i = 0; i < nchildren; i++) {
        send_kill_signal(children[i]);
    }
    /* keep alive until all windows are killed */
    while(nchildren > 0) {
        XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
        XNextEvent(dis,&ev);
        if(events[ev.type])
            events[ev.type](&ev);
    }

    XUngrabKey(dis,AnyKey,AnyModifier,root);
    fprintf(stdout,"catwm: Thanks for using!\n");
}

void remove_window(Window w) {
    Client *c;

    // CHANGE THIS UGLY CODE
    for(c=head;c;c=c->next) {

        if(c->win == w) {
            if(c->prev == NULL && c->next == NULL) {
                free(head);
                head = NULL;
                current = NULL;
                return;
            }

            if(c->prev == NULL) {
                head = c->next;
                c->next->prev = NULL;
                current = c->next;
            }
            else if(c->next == NULL) {
                c->prev->next = NULL;
                current = c->prev;
            }
            else {
                c->prev->next = c->next;
                c->next->prev = c->prev;
                current = c->prev;
            }

            free(c);
            return;
        }
    }
}

void tile() {
    Client *c;

    /* If only one window */
    if(head != NULL && head->next == NULL) {
        XMoveResizeWindow(dis,head->win,0,15+panel_pad,sw-2,sh-15-panel_pad);
    }
    else if(head != NULL) {
        switch(mode) {
            case 0:
                /*
				 * not monocle
				*/
                break;
            case 1:
                for(c=head;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,0,15+panel_pad,sw-2,sh-15-panel_pad);
                }
                break;
            default:
                break;
        }
    }
}

void update_current() {
    Client *c;

    for(c=head;c;c=c->next)
        if(current == c) {
            /* "Enable" current window */
            XSetWindowBorderWidth(dis,c->win,0);
            XSetWindowBorder(dis,c->win,win_focus);
            XSetInputFocus(dis,c->win,RevertToParent,CurrentTime);
            XRaiseWindow(dis,c->win);
            drawbar();
            fprintf(stdout, "\tcurrent name: %s\n", c->name);
        }
        else
            XSetWindowBorder(dis,c->win,win_unfocus);
}

void send_kill_signal(Window w) {
    XEvent ke;
    ke.type = ClientMessage;
    ke.xclient.window = w;
    ke.xclient.message_type = XInternAtom(dis, "WM_PROTOCOLS", True);
    ke.xclient.format = 32;
    ke.xclient.data.l[0] = XInternAtom(dis, "WM_DELETE_WINDOW", True);
    ke.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ke);
}

void update_title(Client *c) {
	if(!gettextprop(c->win, NetWMName, c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if(c->name[0] == '\0') /* hack to mark broken Clients */
		strcpy(c->name, broken);
}

void update_status(void) {
    if(!gettextprop(root, XA_WM_NAME, status_text, sizeof(status_text)))
        strcpy(status_text, "ShMOW");
}

Bool gettextprop(Window w, Atom atom, char *text, unsigned int size) {
	char **list = NULL;
	int n;
	XTextProperty name;

	if(!text || size == 0)
		return False;
	text[0] = '\0';
	XGetTextProperty(dis, w, &name, atom);
	if(!name.nitems)
		return False;
	if(name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if(XmbTextPropertyToTextList(dis, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return True;
}

GC setcolor(const char* col) {
	XAllocNamedColor(dis,cmap,col,&color,&color);
	XSetForeground(dis,gc,color.pixel);
	return gc;
}

void drawbar() {

    update_title(current);

    int count; Client *c;
    for (c=head; c; c = c->next) update_title(c);
    for (count=0, c=head; c; c = c->next) if (c) count++;

    int statusw = strlen(status_text) * FONTFACTOR;
    int barw = sw - statusw;
    int pix_cur_win = (int)(round(barw / pow(count,0.75)));
    int pix_unfocus_total = barw - pix_cur_win;
    int pix_per_win = (count==1) ? pix_unfocus_total : pix_unfocus_total / (count - 1);
    int x_coord = 0;

    c = head;

	Window win = XCreateSimpleWindow(dis, root, 0, panel_pad, sw, 15, 0, win_unfocus, win_unfocus);
	XMapWindow(dis, win);
    /*if (c == current) {
	    XDrawRectangle(dis, win, setcolor(FOCUS), 0, panel_pad, barw, 14);
	    XFillRectangle(dis, win, setcolor(FOCUS), 0, panel_pad, barw, 15);
    }
    else {*/
	    XDrawRectangle(dis, win, setcolor(UNFOCUS), 0, panel_pad, barw, 14);
	    XFillRectangle(dis, win, setcolor(UNFOCUS), 0, panel_pad, barw, 15);
    //}

	XDrawRectangle(dis, win, setcolor(STATUS), barw, panel_pad, statusw, 14);
	XFillRectangle(dis, win, setcolor(STATUS), barw, panel_pad, statusw, 15);
    fprintf(stdout, "status_text %s\n", status_text);
    XDrawString(dis, win, setcolor(UNFOCUS), barw+1, 12, status_text, strlen(status_text));

    while (c) {
        
        char buf[(pix_per_win/10)+11];
        char curbuf[(pix_cur_win/7)+11];
        char name[(pix_per_win/10)+1];
        char curname[(pix_per_win/7)+1];
        strcpy(buf, " ");
        strcpy(curbuf, " ");
        strcpy(name, " ");
        strcpy(curname, " ");

        if (c == current) {
            memcpy(curname, &c->name[0], pix_cur_win/7);
            curname[(pix_cur_win/7)] = '\0';
            strcat(curbuf, curname);
        }
        else {
            memcpy(name, &c->name[0], pix_per_win/10);
            name[(pix_per_win/10)] = '\0';
            strcat(buf, name);
        }

	    if (c == current) {
	        XFillRectangle(dis,win,setcolor(FOCUS),x_coord,0,pix_cur_win,16);
            XDrawString(dis,win,setcolor(UNFOCUS),x_coord,12,curbuf,strlen(curbuf));
        }
        else {
	        XFillRectangle(dis,win,setcolor(UNFOCUS),x_coord,0,pix_per_win,16);
            XDrawString(dis,win,setcolor(FOCUS),x_coord,12,buf,strlen(buf));
        }
    
        x_coord += (c == current) ? pix_cur_win : pix_per_win;

        c=c->next;
    }
    
	XSync(dis, False);
}

void setup() {
    /* Install a signal */
    sigchld(0);

    /* Screen and root window */
    screen = DefaultScreen(dis);
    root = RootWindow(dis,screen);

    /* initialize monitors and desktops */
    /*XineramaScreenInfo *info = XineramaQueryScreens(dis, &nmonitors);

    if (!nmonitors || !info)
        errx(EXIT_FAILURE, "Xinerama is not active");
    if (!(monitors = calloc(nmonitors, sizeof(Monitor))))
        err(EXIT_FAILURE, "cannot allocate monitors");

    for (int m = 0; m < nmonitors; m++) {
        monitors[m] = (Monitor){ .x = info[m].x_org, .y = info[m].y_org,
                                 .w = info[m].width, .h = info[m].height };
        for (unsigned int d = 0; d < 10; d++)
            monitors[m].desktops[d] = (Desktop){ .mode = DEFAULT_MODE, .sbar = SHOW_PANEL };
    }
    XFree(info);*/

    /* Screen width and height */
    sw = XDisplayWidth(dis,screen);
    sh = XDisplayHeight(dis,screen);

    strcpy(status_text, "ShMOW!!!\0");
    panel_pad = 0;

    /* Colors */
    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);
    win_status = getcolor(STATUS);

    /* Shortcuts */
    grabkeys();

    /* Monocle */
    mode = 1;

    /* For exiting */
    bool_quit = 0;

    /* List of Clients */
    head = NULL;
    current = NULL;

    /* Set up all desktops */
    int i;
    for(i=0;i<TABLENGTH(desktops);++i) {
        desktops[i].mode = mode;
        desktops[i].head = head;
        desktops[i].current = current;
    }

    /* EWMH */
    NetWMName = XInternAtom(dis, "_NET_WM_NAME", False);

    /* Select first dekstop by default */
    const Arg arg = {.i = 1};
    current_desktop = arg.i;
    /* change_desktop(arg);  questionable?? */


    /* gc init */
    cmap = DefaultColormap(dis,screen);
    XGCValues val;
    val.font = XLoadFont(dis,"fixed");
    gc = XCreateGC(dis,root,GCFont,&val);

    /* To catch maprequest and destroynotify (if other wm running) */
    XSelectInput(dis,root,SubstructureNotifyMask|SubstructureRedirectMask|PropertyChangeMask);
}

void sigchld(int unused) {
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(dis)
                close(ConnectionNumber(dis));

            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}

void start() {
    XEvent ev;

    /* Main loop, just dispatch events */
    while(!bool_quit && !XNextEvent(dis,&ev)) {
        if(events[ev.type])
            events[ev.type](&ev);
    }
}

int main(int argc, char **argv) {
    /* Open display */
    if(!(dis = XOpenDisplay(NULL)))
        die("Cannot open display!");

    /* Setup env */
    setup();

    /* Start wm */
    start();

    /* Close display */
    XCloseDisplay(dis);

    return 0;
}
