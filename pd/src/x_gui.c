/* Copyright (c) 1997-2000 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* dialogs.  LATER, deal with the situation where the object goes 
away before the panel does... */

#include "config.h"

#include "m_pd.h"

// The built in NWJS filedialog is currently broken on some linux builds.
// it has been replaced with tinyfiledialogs, a native C library which
// works on all platforms. In the case that NWJS fixes their issues,
// this compiler directive can be enabled to revert to the default NWJS
// file dialogs without having to change any other code. This directive
// can also be used if there is a specific platform that has issues with
// tinyfiledialogs. By setting -DUSE_NWJS_FILEDIALOGS, the built in
// file dialogs will be used.
#ifndef USE_NWJS_FILEDIALOGS
#include "tinyfd/tinyfiledialogs.h"
#include <pthread.h>
#endif
#include "g_canvas.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define snprintf _snprintf  /* for pdcontrol object */
#endif

// jsarlo
EXTERN void magicGlass_setup(void);
// end jsarlo

/* --------------------- graphics responder  ---------------- */

/* make one of these if you want to put up a dialog window but want to be
protected from getting deleted and then having the dialog call you back.  In
this design the calling object doesn't have to keep the address of the dialog
window around; instead we keep a list of all open dialogs.  Any object that
might have dialogs, when it is deleted, simply checks down the dialog window
list and breaks off any dialogs that might later have sent messages to it. 
Only when the dialog window itself closes do we delete the gfxstub object. */

static t_class *gfxstub_class;

typedef struct _gfxstub
{
    t_pd x_pd;
    t_pd *x_owner;
    void *x_key;
    t_symbol *x_sym;
    struct _gfxstub *x_next;
} t_gfxstub;

static t_gfxstub *gfxstub_list;

    /* create a new one.  the "key" is an address by which the owner
    will identify it later; if the owner only wants one dialog, this
    could just be a pointer to the owner itself.  The string "cmd"
    is a TK command to create the dialog, with "%s" embedded in
    it so we can provide a name by which the GUI can send us back
    messages; e.g., "pdtk_canvas_dofont %s 10". */

void gfxstub_new(t_pd *owner, void *key, const char *cmd)
{
    char buf[4*MAXPDSTRING];
    char namebuf[80];
    char sprintfbuf[MAXPDSTRING];
    char *afterpercent;
    t_int afterpercentlen;
    t_gfxstub *x;
    t_symbol *s;
        /* if any exists with matching key, burn it. */
    for (x = gfxstub_list; x; x = x->x_next)
        if (x->x_key == key)
            gfxstub_deleteforkey(key);
    if (strlen(cmd) + 50 > 4*MAXPDSTRING)
    {
        bug("audio dialog too long");
        bug("%s", cmd);
        return;
    }
    x = (t_gfxstub *)pd_new(gfxstub_class);
    sprintf(namebuf, ".gfxstub%zx", (t_uint)x);

    s = gensym(namebuf);
    pd_bind(&x->x_pd, s);
    x->x_owner = owner;
    x->x_sym = s;
    x->x_key = key;
    x->x_next = gfxstub_list;
    gfxstub_list = x;
    /* only replace first %s so sprintf() doesn't crash */
    afterpercent = strchr(cmd, '%') + 2;
    afterpercentlen = afterpercent - cmd;
    strncpy(sprintfbuf, cmd, afterpercentlen);
    sprintfbuf[afterpercentlen] = '\0';
    sprintf(buf, sprintfbuf, s->s_name);
    strncat(buf, afterpercent, (4*MAXPDSTRING) - afterpercentlen);
    sys_gui(buf);
}

/* This is the new API for gfxstub.  We forgo all the
   string formatting junk and just return the string id */
char *gfxstub_new2(t_pd *owner, void *key)
{
    static char namebuf[80];
    t_gfxstub *x;
    t_symbol *s;
        /* if any exists with matching key, burn it. */
    for (x = gfxstub_list; x; x = x->x_next)
        if (x->x_key == key)
            gfxstub_deleteforkey(key);
    x = (t_gfxstub *)pd_new(gfxstub_class);
    sprintf(namebuf, ".gfxstub%zx", (t_uint)x);
    s = gensym(namebuf);
    pd_bind(&x->x_pd, s);
    x->x_owner = owner;
    x->x_sym = s;
    x->x_key = key;
    x->x_next = gfxstub_list;
    gfxstub_list = x;
    return (namebuf);
}




t_int gfxstub_haveproperties(void *key)
{
    t_gfxstub *x;
    for (x = gfxstub_list; x; x = x->x_next)
    {
        if (x->x_key == key)
        {
            return (t_int)x;
        }
    }
    return 0;
}

static void gfxstub_offlist(t_gfxstub *x)
{
    t_gfxstub *y1, *y2;
    if (gfxstub_list == x)
        gfxstub_list = x->x_next;
    else for (y1 = gfxstub_list; y2 = y1->x_next; y1 = y2)
        if (y2 == x) 
    {
        y1->x_next = y2->x_next;
        break;
    }
}

    /* if the owner disappears, we still may have to stay around until our
    dialog window signs off.  Anyway we can now tell the GUI to destroy the
    window.  */
void gfxstub_deleteforkey(void *key)
{
    t_gfxstub *y;
    int didit = 1;
    while (didit)
    {
        didit = 0;
        for (y = gfxstub_list; y; y = y->x_next)
        {
            if (y->x_key == key)
            {
                char tagbuf[MAXPDSTRING];
                sprintf(tagbuf, ".gfxstub%zx", (t_uint)y);
                gui_vmess("gui_remove_gfxstub", "s",
                    tagbuf);
                 
                y->x_owner = 0;
                gfxstub_offlist(y);
                didit = 1;
                break;
            }
        }
    }
}

/* --------- pd messages for gfxstub (these come from the GUI) ---------- */

    /* "cancel" to request that we close the dialog window. */
static void gfxstub_cancel(t_gfxstub *x)
{
    gfxstub_deleteforkey(x->x_key);
}

    /* "signoff" comes from the GUI to say the dialog window closed. */
static void gfxstub_signoff(t_gfxstub *x)
{
    gfxstub_offlist(x);
    pd_free(&x->x_pd);
}

static t_binbuf *gfxstub_binbuf;

    /* a series of "data" messages rebuilds a scalar */
static void gfxstub_data(t_gfxstub *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!gfxstub_binbuf)
        gfxstub_binbuf = binbuf_new();
    binbuf_add(gfxstub_binbuf, argc, argv);
    binbuf_addsemi(gfxstub_binbuf);
}
    /* the "end" message terminates rebuilding the scalar */
static void gfxstub_end(t_gfxstub *x)
{
    canvas_dataproperties((t_canvas *)x->x_owner,
        (t_scalar *)x->x_key, gfxstub_binbuf);
    binbuf_free(gfxstub_binbuf);
    gfxstub_binbuf = 0;
}

    /* anything else is a message from the dialog window to the owner;
    just forward it. */
static void gfxstub_anything(t_gfxstub *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_owner)
        pd_typedmess(x->x_owner, s, argc, argv);
}

static void gfxstub_free(t_gfxstub *x)
{
    pd_unbind(&x->x_pd, x->x_sym);
}

static void gfxstub_setup(void)
{
    gfxstub_class = class_new(gensym("gfxstub"), 0, (t_method)gfxstub_free,
        sizeof(t_gfxstub), CLASS_PD, 0);
    class_addanything(gfxstub_class, gfxstub_anything);
    class_addmethod(gfxstub_class, (t_method)gfxstub_signoff,
        gensym("signoff"), 0);
    class_addmethod(gfxstub_class, (t_method)gfxstub_data,
        gensym("data"), A_GIMME, 0);
    class_addmethod(gfxstub_class, (t_method)gfxstub_end,
        gensym("end"), 0);
    class_addmethod(gfxstub_class, (t_method)gfxstub_cancel,
        gensym("cancel"), 0);
}

/* -------------------------- file dialog ------------------------------ */
#define FILEDIALOG_OPEN 0
#define FILEDIALOG_OPENFOLDER 1
#define FILEDIALOG_OPENMULTI 2
#define FILEDIALOG_SAVE 3

static t_class *filedialog_class;

typedef struct _filedialog
{
    t_object x_obj;
    t_symbol *x_s;
} t_filedialog;

static t_filedialog *filedialog_obj;

static t_filedialog *filedialog_new( void)
{
    char buf[50];
    t_filedialog *x = (t_filedialog *)pd_new(filedialog_class);
    sprintf(buf, "d%zx", (t_uint)x);
    x->x_s = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_s);
    return (x);
}

static void filedialog_free(t_filedialog *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_s);
}

#ifndef USE_NWJS_FILEDIALOGS
// Currently, the built-in NW.js file dialog works find on Windows and Mac, just not on some linux distributions.
// The tool tinyfd is completely cross platform, so it will also work on Windows and Mac, but since we are using
// threading to prevent blocking the UI and Audio, this code is not currently cross playform. In the future,
// if nw.js fixes their issues, this code can be removed and the #else can be used for all platforms. On the
// other hand, if nw.js's dialog breaks on MacOS or Windows, this code could be used on those platforms, with
// some minor adjustments to the threading.
struct filedialog_state
{
    char* callback_name;
    char* working_dir;
    int mode;
};

static void filedialog_thread(struct filedialog_state *args)
{

#ifdef _WIN32
    // Fix paths for windows because windows is dumb
    char* str = args->working_dir;
    while (*str != '\0') {
        if (*str == '/')
            *str = '\\';
        str++;
    }
#endif

    // Get a file path from the system file picker
    char* result;
    if(args->mode == FILEDIALOG_OPEN)
        result = tinyfd_openFileDialog("Choose a File", args->working_dir, 0, NULL, NULL, 0);
    else if(args->mode == FILEDIALOG_OPENFOLDER)
        result = tinyfd_selectFolderDialog("Choose a Folder", args->working_dir);
    else if(args->mode == FILEDIALOG_OPENMULTI)
        result = tinyfd_openFileDialog("Choose Files", args->working_dir, 0, NULL, NULL, 1);
    else
        result = tinyfd_saveFileDialog("Choose a Location", args->working_dir, 0, NULL, NULL);

    // Trigger the callback for whatever object requested a file dialog
    if(result != NULL)
        gui_vmess("file_dialog_callback", "ss", args->callback_name, result);

    // Free the memory we used;
    if(args->working_dir != NULL)
        free(args->working_dir);
    free(args);

    pthread_exit(NULL);
    
}

static void filedialog_perform(int mode, t_symbol *callback_name, t_symbol *initial_dir)
{
    // Builds the arguments to be passed to the worker thread
    struct filedialog_state *args = malloc(sizeof(struct filedialog_state));
    args->callback_name = callback_name->s_name;
    args->mode = mode;
    if(initial_dir != NULL && initial_dir->s_name != NULL && strlen(initial_dir->s_name) != 0)
        args->working_dir = strcpy(malloc((strlen(initial_dir->s_name) + 1) * sizeof(char)), initial_dir->s_name);
    else
        args->working_dir = NULL;

    // Spawns and detaches the worker thread
    pthread_t thread;
    pthread_create(&thread, NULL, (void*)filedialog_thread, args);
    pthread_detach(thread);
}
#else
static void filedialog_perform(int mode, t_symbol *callback_name, t_symbol *initial_dir)
{
    gui_vmess("file_dialog", "xsss", NULL, mode == FILEDIALOG_SAVE ? "save" : "open", callback_name->s_name, initial_dir->s_name);
}
#endif

static void filedialog_trigger(t_filedialog *x, t_symbol *s, int argc, t_atom *argv)
    //t_float mode, t_symbol *callback_name, t_symbol *initial_dir)
{
    //post("argc=%d", argc);
    if (argc != 3) return;
    /*
    post ("filedialog_trigger %d <%s> <%s>",
        (int)atom_getfloat(argv),
        atom_getsymbol(argv+1)->s_name,
        atom_getsymbol(argv+2)->s_name);
    */
    filedialog_perform((int)atom_getfloat(argv),
        atom_getsymbol(argv+1),
        atom_getsymbol(argv+2));
    //filedialog_perform((int)mode, callback_name, initial_dir);
}

static void filedialog_setup(void)
{
    filedialog_class = class_new(gensym("filedialog"),
        (t_newmethod)filedialog_new, (t_method)filedialog_free,
        sizeof(t_filedialog), 0, 0);
    class_addmethod(filedialog_class, (t_method)filedialog_trigger,
        gensym("trigger"), A_GIMME, 0);

    // We will create exactly one filedialog object, for the purpose
    // of receiving messages from pdgui.js that will select the proper
    // platform-specific file dialog code. On Windows and Mac, file dialogs
    // are passed back to pdgui.js to handle, on Linux, they are handled
    // in C.
    filedialog_obj = filedialog_new();
}

/* -------------------------- openpanel ------------------------------ */

static t_class *openpanel_class;

typedef struct _openpanel
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_symbol *x_s;
    int x_mode;
} t_openpanel;

static void *openpanel_new(t_floatarg mode)
{
    char buf[50];
    t_openpanel *x = (t_openpanel *)pd_new(openpanel_class);
    x->x_canvas = canvas_getcurrent();
    x->x_mode = (mode < 0 || mode > 2) ? 0 : mode;
    sprintf(buf, "d%zx", (t_uint)x);
    x->x_s = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_s);
    outlet_new(&x->x_obj, &s_symbol);
    gui_vmess("file_dialog_obj", "s", filedialog_obj->x_s->s_name);
    return (x);
}

static void openpanel_symbol(t_openpanel *x, t_symbol *s)
{
    char *path = (s && s->s_name) ? s->s_name : "$pd_opendir";
    gui_vmess("gui_openpanel", "xssi",
        x->x_canvas,
        x->x_s->s_name,
        path,
        x->x_mode);
}

static void openpanel_bang(t_openpanel *x)
{
    openpanel_symbol(x, &s_);
}

static void openpanel_callback(t_openpanel *x, t_symbol *s, int argc, t_atom *argv)
{
    if(x->x_mode != 2)
        outlet_symbol(x->x_obj.ob_outlet, argv->a_w.w_symbol);
    else
        outlet_list(x->x_obj.ob_outlet, s, argc, argv);
}


static void openpanel_free(t_openpanel *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_s);
}

static void openpanel_setup(void)
{
    openpanel_class = class_new(gensym("openpanel"),
        (t_newmethod)openpanel_new, (t_method)openpanel_free,
        sizeof(t_openpanel), 0, A_DEFFLOAT, 0);
    class_addbang(openpanel_class, openpanel_bang);
    class_addsymbol(openpanel_class, openpanel_symbol);
    class_addmethod(openpanel_class, (t_method)openpanel_callback,
        gensym("callback"), A_GIMME, 0);
}

/* -------------------------- savepanel ------------------------------ */

static t_class *savepanel_class;

typedef struct _savepanel
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_symbol *x_s;
} t_savepanel;

static void *savepanel_new( void)
{
    char buf[50];
    t_savepanel *x = (t_savepanel *)pd_new(savepanel_class);
    sprintf(buf, "d%zx", (t_uint)x);
    x->x_s = gensym(buf);
    x->x_canvas = canvas_getcurrent();
    pd_bind(&x->x_obj.ob_pd, x->x_s);
    outlet_new(&x->x_obj, &s_symbol);
    gui_vmess("file_dialog_obj", "s", x->x_s->s_name);
    return (x);
}

static void savepanel_symbol(t_savepanel *x, t_symbol *s)
{
    char *path = (s && s->s_name) ? s->s_name : "$pd_opendir";
    gui_vmess("gui_savepanel", "xss",
        x->x_canvas,
        x->x_s->s_name,
        path);
}

static void savepanel_bang(t_savepanel *x)
{
    savepanel_symbol(x, &s_);
}

static void savepanel_callback(t_savepanel *x, t_symbol *s)
{
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void savepanel_free(t_savepanel *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_s);
}

static void savepanel_setup(void)
{
    savepanel_class = class_new(gensym("savepanel"),
        (t_newmethod)savepanel_new, (t_method)savepanel_free,
        sizeof(t_savepanel), 0, 0);
    class_addbang(savepanel_class, savepanel_bang);
    class_addsymbol(savepanel_class, savepanel_symbol);
    class_addmethod(savepanel_class, (t_method)savepanel_callback,
        gensym("callback"), A_SYMBOL, 0);
}

/* ---------------------- key and its relatives ------------------ */

static t_symbol *key_sym, *keyup_sym, *keyname_sym;
static t_symbol *key_sym_a, *keyup_sym_a, *keyname_sym_a;
static t_class *key_class, *keyup_class, *keyname_class;

typedef struct _key
{
    t_object x_obj;
    t_symbol *x_keysym;
} t_key;

static void *key_new(t_symbol *s, int argc, t_atom *argv)
{
    t_key *x = (t_key *)pd_new(key_class);
    outlet_new(&x->x_obj, &s_float);
    if (argc > 0 && argv->a_type == A_FLOAT)
        if (atom_getfloatarg(0, argc, argv) == 0)
            x->x_keysym = key_sym;
        else if (atom_getfloatarg(0, argc, argv) == 1)
            x->x_keysym = key_sym_a;

    if (!x->x_keysym)
        x->x_keysym = key_sym;
    pd_bind(&x->x_obj.ob_pd, x->x_keysym);
    return (x);
}

static void key_float(t_key *x, t_floatarg f)
{
    outlet_float(x->x_obj.ob_outlet, f);
}

static void key_free(t_key *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_keysym);
}

typedef struct _keyup
{
    t_object x_obj;
    t_symbol *x_keysym;
} t_keyup;

static void *keyup_new(t_symbol *s, int argc, t_atom *argv)
{
    t_keyup *x = (t_keyup *)pd_new(keyup_class);
    outlet_new(&x->x_obj, &s_float);
    if (argc > 0 && argv->a_type == A_FLOAT)
        if (atom_getfloatarg(0, argc, argv) == 0)
            x->x_keysym = keyup_sym;
        else if (atom_getfloatarg(0, argc, argv) == 1)
            x->x_keysym = keyup_sym_a;

    if (!x->x_keysym)
        x->x_keysym = keyup_sym;
    pd_bind(&x->x_obj.ob_pd, x->x_keysym);
    return (x);
}

static void keyup_float(t_keyup *x, t_floatarg f)
{
    outlet_float(x->x_obj.ob_outlet, f);
}

static void keyup_free(t_keyup *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_keysym);
}

typedef struct _keyname
{
    t_object x_obj;
    t_symbol *x_keysym;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
} t_keyname;

static void *keyname_new(t_symbol *s, int argc, t_atom *argv)
{
    t_keyname *x = (t_keyname *)pd_new(keyname_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_symbol);
    if (argc > 0 && argv->a_type == A_FLOAT)
        if (atom_getfloatarg(0, argc, argv) == 0)
            x->x_keysym = keyname_sym;
        else if (atom_getfloatarg(0, argc, argv) == 1)
            x->x_keysym = keyname_sym_a;

    if (!x->x_keysym)
        x->x_keysym = keyname_sym;
    pd_bind(&x->x_obj.ob_pd, x->x_keysym);
    return (x);
}

static void keyname_list(t_keyname *x, t_symbol *s, int ac, t_atom *av)
{
    outlet_symbol(x->x_outlet2, atom_getsymbolarg(1, ac, av));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, ac, av));
}

static void keyname_free(t_keyname *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_keysym);
}

static void key_setup(void)
{
    key_class = class_new(gensym("key"),
        (t_newmethod)key_new, (t_method)key_free,
        sizeof(t_key), 0, A_GIMME, 0);
    class_addfloat(key_class, key_float);
    key_sym = gensym("#key");
    key_sym_a = gensym("#key_a");

    keyup_class = class_new(gensym("keyup"),
        (t_newmethod)keyup_new, (t_method)keyup_free,
        sizeof(t_keyup), 0, A_GIMME, 0);
    class_addfloat(keyup_class, keyup_float);
    keyup_sym = gensym("#keyup");
    keyup_sym_a = gensym("#keyup_a");
    //class_sethelpsymbol(keyup_class, gensym("key"));
    
    keyname_class = class_new(gensym("keyname"),
        (t_newmethod)keyname_new, (t_method)keyname_free,
        sizeof(t_keyname), 0, A_GIMME, 0);
    class_addlist(keyname_class, keyname_list);
    keyname_sym = gensym("#keyname");
    keyname_sym_a = gensym("#keyname_a");
    //class_sethelpsymbol(keyname_class, gensym("key"));
}

/* ------------------ mouse classes for legacy externals ------------------ */

/* Every other legacy external library has some ad hoc code for getting
   mouse state within a Pd patch. All of them have different weird interfaces
   and some are outright buggy.

   Most of these return screen coordinates. This is unfortunately more of
   a pain than it should be in nw.js. Instead, we return window coordinates
   and hope that this is good enough for the uses to which these external
   classes have been put. At worst the user can make the relevant canvas
   full screen and get the desired behavior (minus the offset for the menu).

   Most of the uses for mouse coordinates seem to do with tutorials that map
   x/y positions to amplitude, frequency, etc. So these classes should be
   good enough to build abstractions to do an end run around the relevant
   externals.
*/

static t_symbol *mousemotion_sym, *mouseclick_sym, *mousewheel_sym;
static t_class *mousemotion_class, *mouseclick_class, *mousewheel_class;

typedef struct _mousemotion
{
    t_object x_obj;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
} t_mousemotion;

static void *mousemotion_new( void)
{
    t_mousemotion *x = (t_mousemotion *)pd_new(mousemotion_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_float);
    pd_bind(&x->x_obj.ob_pd, mousemotion_sym);
    return (x);
}

static void mousemotion_list(t_mousemotion *x, t_symbol *s, int argc,
    t_atom *argv)
{
    outlet_float(x->x_outlet2, atom_getfloatarg(1, argc, argv));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, argc, argv));
}

static void mousemotion_free(t_mousemotion *x)
{
    pd_unbind(&x->x_obj.ob_pd, mousemotion_sym);
}

typedef struct _mouseclick
{
    t_object x_obj;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
    t_outlet *x_outlet3;
    t_outlet *x_outlet4;
} t_mouseclick;

static void *mouseclick_new( void)
{
    t_mouseclick *x = (t_mouseclick *)pd_new(mouseclick_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet3 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet4 = outlet_new(&x->x_obj, &s_float);
    pd_bind(&x->x_obj.ob_pd, mouseclick_sym);
    return (x);
}

static void mouseclick_list(t_mouseclick *x, t_symbol *s, int argc,
    t_atom *argv)
{
    outlet_float(x->x_outlet4, atom_getfloatarg(3, argc, argv));
    outlet_float(x->x_outlet3, atom_getfloatarg(2, argc, argv));
    outlet_float(x->x_outlet2, atom_getfloatarg(1, argc, argv));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, argc, argv));
}

static void mouseclick_free(t_mouseclick *x)
{
    pd_unbind(&x->x_obj.ob_pd, mouseclick_sym);
}

typedef struct _mousewheel
{
    t_object x_obj;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
    t_outlet *x_outlet3;
} t_mousewheel;

static void *mousewheel_new( void)
{
    t_mousewheel *x = (t_mousewheel *)pd_new(mousewheel_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet3 = outlet_new(&x->x_obj, &s_float);
    pd_bind(&x->x_obj.ob_pd, mousewheel_sym);
    return (x);
}

static void mousewheel_list(t_mousewheel *x, t_symbol *s, int argc,
    t_atom *argv)
{
    outlet_float(x->x_outlet3, atom_getfloatarg(2, argc, argv));
    outlet_float(x->x_outlet2, atom_getfloatarg(1, argc, argv));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, argc, argv));
}

static void mousewheel_free(t_mousewheel *x)
{
    pd_unbind(&x->x_obj.ob_pd, mousewheel_sym);
}

static void mouse_setup(void)
{
    mousemotion_class = class_new(gensym("legacy_mousemotion"),
        (t_newmethod)mousemotion_new, (t_method)mousemotion_free,
        sizeof(t_mousemotion), CLASS_NOINLET, 0);
    class_addlist(mousemotion_class, mousemotion_list);
    mousemotion_sym = gensym("#legacy_mousemotion");

    mouseclick_class = class_new(gensym("legacy_mouseclick"),
        (t_newmethod)mouseclick_new, (t_method)mouseclick_free,
        sizeof(t_mouseclick), CLASS_NOINLET, 0);
    class_addlist(mouseclick_class, mouseclick_list);
    mouseclick_sym = gensym("#legacy_mouseclick");

    mousewheel_class = class_new(gensym("legacy_mousewheel"),
        (t_newmethod)mousewheel_new, (t_method)mousewheel_free,
        sizeof(t_mousewheel), CLASS_NOINLET, 0);
    class_addfloat(mousewheel_class, mousewheel_list);
    mousewheel_sym = gensym("#legacy_mousewheel");
}

/* ------------------------ pdcontrol --------------------------------- */

static t_class *pdcontrol_class;

typedef struct _pdcontrol
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_outlet *x_outlet;
} t_pdcontrol;

static void *pdcontrol_new( void)
{
    t_pdcontrol *x = (t_pdcontrol *)pd_new(pdcontrol_class);
    x->x_canvas = canvas_getcurrent();
    x->x_outlet = outlet_new(&x->x_obj, 0);
    return (x);
}

    /* output containing directory of patch.  optional args:
    1. a number, zero for this patch, one for the parent, etc.;
    2. a symbol to concatenate onto the directory; */

static void pdcontrol_dir(t_pdcontrol *x, t_symbol *s, t_floatarg f)
{
    t_canvas *c = x->x_canvas;
    int i;
    for (i = 0; i < (int)f; i++)
    {
        while (!c->gl_env)  /* back up to containing canvas or abstraction */
            c = c->gl_owner;
        if (c->gl_owner)    /* back up one more into an owner if any */
            c = c->gl_owner;
    }
    if (*s->s_name)
    {
        char buf[MAXPDSTRING];
        snprintf(buf, MAXPDSTRING, "%s/%s",
            canvas_getdir(c)->s_name, s->s_name);
        buf[MAXPDSTRING-1] = 0;
        outlet_symbol(x->x_outlet, gensym(buf));
    }
    else outlet_symbol(x->x_outlet, canvas_getdir(c));
}

static void pdcontrol_args(t_pdcontrol *x, t_floatarg f)
{
    t_canvas *c = x->x_canvas;
    int i;
    int argc;
    t_atom *argv, *escape;
    for (i = 0; i < (int)f; i++)
    {
        while (!c->gl_env)  /* back up to containing canvas or abstraction */
            c = c->gl_owner;
        if (c->gl_owner)    /* back up one more into an owner if any */
            c = c->gl_owner;
    }
    canvas_setcurrent(c);
    canvas_getargs(&argc, &argv);
    canvas_unsetcurrent(c);
    escape = argv;
    for (i = 0; i < argc; i++)
    {
        // here we inversely escape \\\ to \, e.g. \\\$0 becomes \$0 to
        // match how the arguments look inside the object. this can be
        // tested using the pdcontrol-help.pd subpatch where printing
        // arguments needs to match the original version
        /*
        if(escape->a_type == A_SYMBOL)
        {
            post("pdcontrol args: len=%d 0<%c> 1<%c> 2<%c>",
                 strlen(escape->a_w.w_symbol->s_name),
                 escape->a_w.w_symbol->s_name[0],
                 escape->a_w.w_symbol->s_name[1],
                 escape->a_w.w_symbol->s_name[2]);
        }
        */
        if(escape->a_type == A_SYMBOL &&
            strlen(escape->a_w.w_symbol->s_name) > 1 &&
            escape->a_w.w_symbol->s_name[0] == '\\')
        {
            memmove(escape->a_w.w_symbol->s_name,
                    escape->a_w.w_symbol->s_name+1,
                    strlen(escape->a_w.w_symbol->s_name));
        }
        escape++;
    }
    outlet_list(x->x_outlet, &s_list, argc, argv);
}

static void pdcontrol_browse(t_pdcontrol *x, t_symbol *s)
{
    gui_vmess("gui_pddplink_open", "ss",
              s->s_name,
              "null");
    //char buf[MAXPDSTRING];
    //snprintf(buf, MAXPDSTRING, "::pd_menucommands::menu_openfile {%s}\n",
    //    s->s_name);
    //buf[MAXPDSTRING-1] = 0;
    //sys_gui(buf);
}

static void pdcontrol_isvisible(t_pdcontrol *x)
{
    outlet_float(x->x_outlet, glist_isvisible(x->x_canvas));
}

static void pdcontrol_setup(void)
{
    pdcontrol_class = class_new(gensym("pdcontrol"),
        (t_newmethod)pdcontrol_new, 0, sizeof(t_pdcontrol), 0, 0);
    class_addmethod(pdcontrol_class, (t_method)pdcontrol_dir,
        gensym("dir"), A_DEFFLOAT, A_DEFSYMBOL, 0);
    class_addmethod(pdcontrol_class, (t_method)pdcontrol_args,
        gensym("args"), A_DEFFLOAT, 0);
    class_addmethod(pdcontrol_class, (t_method)pdcontrol_browse,
        gensym("browse"), A_SYMBOL, 0);
    class_addmethod(pdcontrol_class, (t_method)pdcontrol_isvisible,
        gensym("isvisible"), 0);
}

/* -------------------------- setup routine ------------------------------ */

void x_gui_setup(void)
{
    gfxstub_setup();
    filedialog_setup();
    openpanel_setup();
    savepanel_setup();
    key_setup();
    pdcontrol_setup();
    mouse_setup();
    // jsarlo
    magicGlass_setup();
    // end jsarlo
}
