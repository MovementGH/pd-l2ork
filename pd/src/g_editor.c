/* Copyright (c) 2009-2013 Ivica Ico Bukvic and others. Code based on Pure-Data source

Original Pure-Data source copyright (c) 1997-2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

*/

#include <stdlib.h>
#include <stdio.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "g_magicglass.h"
#include "g_canvas.h"
#include "s_utf8.h" /*-- moo --*/
#include "g_undo.h"
#include "x_preset.h"
#include <string.h>
#include <math.h>
#include "g_all_guis.h"

void glist_readfrombinbuf(t_glist *x, t_binbuf *b, char *filename,
    int selectem);

void open_via_helppath(const char *name, const char *dir);
const char *class_gethelpdir(const t_class *c);

//static int toggle_moving = 0; //global variable

/* ------------------ forward declarations --------------- */
static void canvas_doclear(t_canvas *x);
void glist_setlastxymod(t_glist *gl, int xval, int yval, int mod);
void glist_setlastxy(t_glist *gl, int xval, int yval);
static void glist_donewloadbangs(t_glist *x);
static t_binbuf *canvas_docopy(t_canvas *x);
static void canvas_dopaste(t_canvas *x, t_binbuf *b);
static void canvas_paste(t_canvas *x);
static void canvas_clearline(t_canvas *x);
static t_binbuf *copy_binbuf;
static int clipboard_istext = 0;
//static char *canvas_textcopybuf;
//static int canvas_textcopybufsize;
static t_glist *glist_finddirty(t_glist *x);
static void canvas_reselect(t_canvas *x);
static void canvas_cut(t_canvas *x);
static int paste_xyoffset = 0; /* a counter of pastes to make x,y offsets */
//static void canvas_mouseup_gop(t_canvas *x, t_gobj *g);
void canvas_done_popup(t_canvas *x, t_float which, t_float xpos,
    t_float ypos);
static void canvas_doarrange(t_canvas *x, t_float which, t_gobj *oldy,
    t_gobj *oldy_prev, t_gobj *oldy_next);
static void canvas_paste_xyoffset(t_canvas *x);
void canvas_setgraph(t_glist *x, int flag, int nogoprect);
void canvas_mouseup(t_canvas *x, t_floatarg fxpos, t_floatarg fypos,
    t_floatarg fwhich);
static int outlet_issignal = 0;
static int inlet_issignal = 0;
static int last_inlet_filter = 0;
static int last_outlet_filter = 0;
static int copyfromexternalbuffer = 0;
static int tooltips = 0;
static int objtooltip = 0;
static int screenx1; /* screen coordinates when doing copyfromexternalbuffer */
static int screeny1;
static int screenx2;
static int screeny2;
static int copiedfont;
static void canvas_dofont(t_canvas *x, t_floatarg font, t_floatarg xresize,
    t_floatarg yresize);
int canvas_apply_restore_original_position(t_canvas *x, int orig_pos);
extern void canvas_draw_gop_resize_hooks(t_canvas *x);
static void canvas_font(t_canvas *x, t_floatarg font, t_floatarg oldfont,
    t_floatarg resize, t_floatarg preview);
void canvas_displaceselection(t_canvas *x, int dx, int dy);
void canvas_motion(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg fmod);
void canvas_passthrough_click(t_canvas *x, t_int xpos, t_int ypos, t_int mousedown);
/* for updating preset_node locations in case of operations that alter
   glist object locations (tofront/back, cut, delete, undo/redo cut/delete) */
extern void glob_preset_node_list_check_loc_and_update(void);
// for preset_node
extern t_class *text_class;
// for iemgui objects' wonky click area
//extern void iemgui_getrect_mouse(t_gobj *x, int *xp1, int *yp1,
//    int *xp2, int *yp2);
// ico 2021-11-16: from s_file.c
extern int sys_curved_cords;
//extern int garray_joc(t_garray *x);

static int canvas_find_index1, canvas_find_index2, canvas_find_wholeword,
           canvas_num_found = 0;
static t_binbuf *canvas_findbuf;

int do_not_redraw = 0;     // used to optimize redrawing
int old_displace = 0;      // for legacy displaces within gop that are not
                           // visible to displaceselection

/* used when autopatching to bypass check whether one is trying to
   connect signal with non-signal nlet since this is impossible to
   figure out when the newly created object is an empty one */
int connect_exception = 0;

/* used to test whether the shift is pressed and if so,
   handle various connection exceptions (e.g. multiconnect) */
int glob_lmclick = 0;
int glob_shift = 0;
int glob_ctrl = 0;
int glob_alt = 0;

static t_glist *canvas_last_glist;
static int canvas_last_glist_x=20, canvas_last_glist_y=20, canvas_last_glist_mod,
    canvas_last_glist_click;
static t_gobj *resized_gobj;

struct _outlet
{
    t_object *o_owner;
    struct _outlet *o_next;
    t_outconnect *o_connections;
    t_symbol *o_sym;
};

/* used for new duplicate behavior where we can "duplicate" into new window */
static t_canvas *c_selection;

/* iemgui uses black inlets and outlets while default objects use gray ones
   add here more as necessary */
int gobj_filter_highlight_behavior(t_text *y)
{
    return (y->te_iemgui);
}

/* ---------------- mathieu 2014.08 --------------------------------- */

void canvas_raise_all_cords (t_canvas *x) {
    /* I think this is deprecated... instead of raising cords, we
       just insert all gobjs (and scalars) before the first patch
       cord.  This way we don't have to constantly raise all the
       patch cords above everything else. */
    //sys_vgui(".x%zx.c raise all_cords\n", x);
}

static void canvas_enteritem (t_canvas *x, int xpos, int ypos, const char *tag) {
    // This will be replaced by GUI-side functionality once we combine
    // object and xlet drawing in a single command from Pd.
    //sys_vgui("pdtk_canvas_enteritem .x%x.c %d %d %s -1\n",
    //    x, xpos, ypos, tag);
}

static void canvas_leaveitem (t_canvas *x) {
    // See comment above
    //sys_vgui("pdtk_canvas_leaveitem .x%x.c\n", x);
}

static void tooltip_erase (t_canvas *x) {
    if (objtooltip) {
        objtooltip = 0;
        canvas_leaveitem(x);
    }
}

// removes highlight from an nlet
static void canvas_nlet_conf (t_canvas *x, int type) {
    int isiemgui = type=='o' ? last_outlet_filter : last_inlet_filter;
    int issignal = type=='o' ? outlet_issignal : inlet_issignal;
    //sys_vgui(".x%x.c itemconfigure %s -stroke %s -fill %s -strokewidth 1\n", x,
    //  type=='o' ? x->gl_editor->canvas_cnct_outlet_tag : x->gl_editor->canvas_cnct_inlet_tag,
    //  (isiemgui ? "$pd_colors(iemgui_nlet)" : 
    //    (issignal ? "$pd_colors(signal_cord)" : "$pd_colors(control_cord)")),
    //    (issignal ? "$pd_colors(signal_nlet)" : "$pd_colors(control_nlet)"));

    /* this is rather confusing, but the canvas_cnct_[xlet]_tag already
       includes the type and index concatenated to the end. */
    gui_vmess("gui_gobj_configure_io", "xsiii",
        x,
        type == 'o' ? x->gl_editor->canvas_cnct_outlet_tag :
            x->gl_editor->canvas_cnct_inlet_tag,
        isiemgui,
        issignal,
        1);
}

void canvas_getscroll (t_canvas *x) {
    //sys_vgui("pdtk_canvas_getscroll .x%zx.c\n",(long)x);
    gui_vmess("gui_canvas_get_overriding_scroll", "x", x);
}

/* ---------------- generic widget behavior ------------------------- */

void gobj_getrect(t_gobj *x, t_glist *glist, int *x1, int *y1,
    int *x2, int *y2)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_getrectfn)
        (*x->g_pd->c_wb->w_getrectfn)(x, glist, x1, y1, x2, y2);
}

void gobj_displace(t_gobj *x, t_glist *glist, int dx, int dy)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_displacefn)
        (*x->g_pd->c_wb->w_displacefn)(x, glist, dx, dy);
}

void gobj_displace_withtag(t_gobj *x, t_glist *glist, int dx, int dy)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_displacefnwtag)
        (*x->g_pd->c_wb->w_displacefnwtag)(x, glist, dx, dy);
}

void gobj_select(t_gobj *x, t_glist *glist, int state)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_selectfn)
        (*x->g_pd->c_wb->w_selectfn)(x, glist, state);
}

void gobj_activate(t_gobj *x, t_glist *glist, int state)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_activatefn)
        (*x->g_pd->c_wb->w_activatefn)(x, glist, state);
}

void gobj_delete(t_gobj *x, t_glist *glist)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_deletefn)
        (*x->g_pd->c_wb->w_deletefn)(x, glist);
}

int gobj_shouldvis(t_gobj *x, struct _glist *glist)
{
    t_object *ob;
    //fprintf(stderr,"shouldvis %d %d %d %d\n",
    //    glist->gl_havewindow, glist->gl_isgraph,
    //    glist->gl_goprect, glist->gl_owner != NULL);
        /* if our parent is a graph, and if that graph itself isn't
        visible, then we aren't either. */
    if (!glist->gl_havewindow && glist->gl_isgraph && glist->gl_owner
        && !gobj_shouldvis(&glist->gl_gobj, glist->gl_owner))
            return (0);
        /* if we're graphing-on-parent and the object falls outside the
        graph rectangle, don't draw it. */
    if (!glist->gl_havewindow && glist->gl_isgraph && glist->gl_goprect &&
        glist->gl_owner && (pd_class(&x->g_pd) != scalar_class) &&
        (pd_class(&x->g_pd) != garray_class))
    {
        /* if we're graphing-on-parent and the object falls outside the
        graph rectangle, don't draw it. */
        int x1, y1, x2, y2, gx1, gy1, gx2, gy2, m;
        gobj_getrect(&glist->gl_gobj, glist->gl_owner, &x1, &y1, &x2, &y2);
        if (x1 > x2)
            m = x1, x1 = x2, x2 = m;
        if (y1 > y2)
            m = y1, y1 = y2, y2 = m;
        gobj_getrect(x, glist, &gx1, &gy1, &gx2, &gy2);
        //fprintf(stderr,
        //    "gobj_shouldvis gop: %d %d %d %d || object %d %d %d %d\n",
        //    x1, x2, y1, y2, gx1, gx2, gy1, gy2);

        // ico@vt.edu 20200914: replacing the original crude on/off calculation
        // that is commented out below, with the new one that returns 1 on any
        // object that is even partially visible, e.g. inside a GOP graph on
        // the parent canvas
        //if (gx1 < x1 || gx1 > x2 || gx2 < x1 || gx2 > x2 ||
        //    gy1 < y1 || gy1 > y2 || gy2 < y1 || gy2 > y2)
        if (gx1 > x2 || gx2 < x1 || gy2 < y1 || gy1 > y2)
        {
                /*
                post("gobj_shouldvis: does not fit within boundaries\n"
                       "gx1:%d gx2:%d gy1:%d gy2:%d\n"
                       "x1:%d x2:%d y1:%d y2:%d",
                       gx1, gx2, gy1, gy2, x1, x2, y1, y2);
                */
                return (0);
        }
        if (glist==glist_getcanvas(glist))
            canvas_raise_all_cords(glist);
    }
    if (ob = pd_checkobject(&x->g_pd))
    {
        /* return true if the text box should be drawn.  We don't show text
        boxes inside graphs---except comments, if we're doing the new
        (goprect) style. */
        //fprintf(stderr,"pd_checkobject %zx\n", x);
        /*fprintf(stderr,"pd_checkobject %d %d %d %d %d %d %d\n",
            glist->gl_havewindow, 
            (ob->te_pd != canvas_class ? 1:0),
            (ob->te_pd->c_wb != &text_widgetbehavior ? 1:0),
            (ob->te_pd == canvas_class ? 1:0),
            ((t_glist *)ob)->gl_isgraph,
            glist->gl_goprect,
            (ob->te_type == T_TEXT ? 1:0));*/
        if (x->g_pd == preset_hub_class)
        {
            //fprintf(stderr, "glist_select do not select invised preset_hub "
            //                "in K12 mode\n");
            t_preset_hub *ph = (t_preset_hub *)x;
            if (ph->ph_invis > 0) return(0);
        }
        /*fprintf(stderr,"checking %d\n", (glist->gl_havewindow ||
            (ob->te_pd != canvas_class &&
                ob->te_pd->c_wb != &text_widgetbehavior) ||
            (ob->te_pd == canvas_class && (((t_glist *)ob)->gl_isgraph)) ||
            (glist->gl_goprect && (ob->te_type == T_TEXT))));*/
        return (glist->gl_havewindow ||
            (ob->te_pd != canvas_class &&
                ob->te_pd->c_wb != &text_widgetbehavior) ||
            (ob->te_pd == canvas_class && (((t_glist *)ob)->gl_isgraph)) ||
            (glist->gl_goprect && (ob->te_type == T_TEXT)));
    }
    else {
        //fprintf(stderr,"else return 1\n");
        return (1);
    }
}

void gobj_vis(t_gobj *x, struct _glist *glist, int flag)
{
    if (do_not_redraw) return;
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_visfn && gobj_shouldvis(x, glist))
        (*x->g_pd->c_wb->w_visfn)(x, glist, flag);
}

int gobj_click(t_gobj *x, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_clickfn)
        return ((*x->g_pd->c_wb->w_clickfn)(x,
            glist, xpix, ypix, shift, alt, dbl, doit));
    else return (0);
}

/* ------------------------ managing the selection ----------------- */

// direction -1 = lower, 1 = raise
int canvas_restore_original_position(t_glist *x, t_gobj *y, const char* objtag,
    int dir)
{
    /* we do this instead to save us costly redraw of the canvas */
    //fprintf(stderr,"canvas_restore_original_position %zx %zx %s %d\n",
    //    (t_int)x, (t_int)y, objtag, dir);
    int ret = 0;
    // we only do this if we are not embedded inside gop, otherwise
    // when gop is done redrawing we will get properly repositioned
    // by gop's call to restore original position
    if (pd_class(&y->g_pd) != canvas_class || ((t_glist *)y)->gl_owner == x)
    {
        t_object *ob = NULL;
        t_rtext *yrnxt = NULL;

        if (y->g_next)
        {
            ob = pd_checkobject(&y->g_next->g_pd);
        }
        if (ob)
        {
            yrnxt = glist_findrtext(x, (t_text *)&ob->ob_g);
        }
        if (y)
        {
            ob = pd_checkobject(&y->g_pd);
        }
        else
        {
            ret = 1;
        }
        if (ret != 1)
        {
            if (dir == -1)
            {
                if (x->gl_list == y)
                {
                    /* we get here if we are supposed to go all the way
                       to the bottom */
                    gui_vmess("gui_lower", "xs",
                        x, objtag ? objtag : "selected");
                }
                else if (yrnxt)
                {
                    /* lower into middle */
                    gui_vmess("gui_find_lowest_and_arrange", "xss",
                        x, rtext_gettag(yrnxt), objtag ? objtag : "selected");
                }
                /*else
                {
                    // fall back to legacy redraw for objects
                    //   that are not patchable
                    canvas_redraw(x);
                    ret = -1;
                }*/
            }
            else
            {
                if (yrnxt)
                {
                    /* raise into middle */
                    gui_vmess("gui_find_lowest_and_arrange", "xss",
                        x, rtext_gettag(yrnxt), objtag ? objtag : "selected");
                }
                else if (y->g_next == NULL)
                {
                    /* we get here if we are supposed to go all the way
                       to the top */
                    gui_vmess("gui_raise", "xs",
                        x, objtag ? objtag : "selected");
                }
                /*else
                {
                    // fall back to legacy redraw for objects
                    //   that are not patchable
                    canvas_redraw(x);
                    ret = -1;
                }*/
            }
        }
    }
    return(ret);
}

void canvas_check_nlet_highlights(t_glist *x)
{
    if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
    {
        canvas_nlet_conf(x,'i');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_inlet_tag[0] = 0;
        //if (x->gl_editor->e_onmotion == MA_CONNECT) {
            x->gl_editor->e_onmotion = MA_NONE;
            canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        //}
    }

    if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0 &&
        x->gl_editor->e_onmotion != MA_CONNECT)
    {
        canvas_nlet_conf(x,'o');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_outlet_tag[0] = 0;
        //if (x->gl_editor->e_onmotion == MA_CONNECT) {
            x->gl_editor->e_onmotion = MA_NONE;
            canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        //}            
    }
}

void glist_selectline(t_glist *x, t_outconnect *oc, int index1,
    int outno, int index2, int inno)
{
    char tagbuf[MAXPDSTRING];
    if (x->gl_editor)
    {
        glist_noselect(x);
        x->gl_editor->e_selectedline = 1;
        x->gl_editor->e_selectline_index1 = index1;
        x->gl_editor->e_selectline_outno = outno;
        x->gl_editor->e_selectline_index2 = index2;
        x->gl_editor->e_selectline_inno = inno;
        x->gl_editor->e_selectline_tag = oc;
        sprintf(tagbuf, "l%zx", (t_int)oc);
        gui_vmess("gui_canvas_select_line", "xs", x, tagbuf);
        c_selection = x;
        canvas_draw_gop_resize_hooks(x);
    }
}

void glist_deselectline(t_glist *x)
{
    char tagbuf[MAXPDSTRING];
    if (x->gl_editor)
    {
        t_linetraverser t;
        t_outconnect *oc;
        x->gl_editor->e_selectedline = 0;
        linetraverser_start(&t, glist_getcanvas(x));
        do {
            oc = linetraverser_next(&t);
        } while (oc && oc != x->gl_editor->e_selectline_tag);
        canvas_draw_gop_resize_hooks(x);
        sprintf(tagbuf, "l%zx",
            (t_int)x->gl_editor->e_selectline_tag);
        gui_vmess("gui_canvas_deselect_line", "xs", x, tagbuf);
    }    
}

int glist_isselected(t_glist *x, t_gobj *y)
{
#ifndef __EMSCRIPTEN__
    if (x->gl_editor)
    {
        t_selection *sel;
        for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            if (sel->sel_what == y) return (1);
    }
#endif
    return (0);
}

    /* call this for unselected objects only */
void glist_select(t_glist *x, t_gobj *y)
{
    //fprintf(stderr,"glist_select c_selection=%zx x=%zx\n",
    //    (t_int)c_selection, (t_int)x);
    if (x->gl_editor)
    {
        //fprintf(stderr,"have editor\n");
#ifdef PDL2ORK
        // exception: if preset_hub is hidden,
        // do not select it
        if (y->g_pd == preset_hub_class)
        {
            //fprintf(stderr,"glist_select do not select invised
            // preset_hub\n");
            t_preset_hub *ph = (t_preset_hub *)y;
            if (ph->ph_invis > 0) return;
        }
#endif
        if (c_selection && c_selection != x)
        {
            //fprintf(stderr,"deselecting\n");
            glist_noselect(c_selection);
        }
        t_selection *sel = (t_selection *)getbytes(sizeof(*sel));
        if (x->gl_editor->e_selectedline)
        {
            //fprintf(stderr,"deselecting line\n");
            glist_deselectline(x);
        }
            /* LATER #ifdef out the following check */
        if (glist_isselected(x, y)) bug("glist_select");
        sel->sel_next = x->gl_editor->e_selection;
        sel->sel_what = y;
        x->gl_editor->e_selection = sel;
        gobj_select(y, x, 1);
        c_selection = x;

        /* Not sure if this is still needed */
        //sys_vgui("pdtk_canvas_update_edit_menu .x%zx 1\n", x);
        canvas_draw_gop_resize_hooks(x);
    }
    //fprintf(stderr,"select done\n");
}

    /* recursively deselect everything in a gobj "g", if it happens to be
    a glist, in preparation for deselecting g itself in glist_dselect() */
static void glist_checkanddeselectall(t_glist *gl, t_gobj *g)
{
    t_glist *gl2;
    t_gobj *g2;
    if (pd_class(&g->g_pd) != canvas_class)
        return;
    gl2 = (t_glist *)g;
    for (g2 = gl2->gl_list; g2; g2 = g2->g_next)
        glist_checkanddeselectall(gl2, g2);
    glist_noselect(gl2);
}

int glist_getindex(t_glist *x, t_gobj *y);

    /* call this for selected objects only */
void glist_deselect(t_glist *x, t_gobj *y)
{
    //post("deselect");
    int fixdsp = 0;
    //static int reenter = 0;
    /* if (reenter) return; */
    //reenter = 1;
    if (x->gl_editor)
    {
        t_selection *sel, *sel2;
        t_rtext *fuddy = 0;
        t_rtext *z = 0;
        if (!glist_isselected(x, y)) bug("glist_deselect");
        // following information is for undo_apply
        // we need info on the old object's position
        // in the gl_list so that we can restore it
        int pos = glist_getindex(glist_getcanvas(x), y);
        if (x->gl_editor->e_textedfor)
        {
            //fprintf(stderr, "e_textedfor\n");
            fuddy = glist_findrtext(x, (t_text *)y);
            if (x->gl_editor->e_textedfor == fuddy)
            {
                //fprintf(stderr, "e_textedfor == fuddy\n");
                if (x->gl_editor->e_textdirty)
                {
                    //fprintf(stderr, "textdirty yes\n");
                    z = fuddy;
                    canvas_stowconnections(glist_getcanvas(x));
                    glist_checkanddeselectall(x, y);
                }
                gobj_activate(y, x, 0);
            }
            if (zgetfn(&y->g_pd, gensym("dsp")))
                fixdsp = canvas_suspend_dsp();
        }
        if ((sel = x->gl_editor->e_selection)->sel_what == y)
        {
            x->gl_editor->e_selection = x->gl_editor->e_selection->sel_next;
            gobj_select(sel->sel_what, x, 0);
            freebytes(sel, sizeof(*sel));
        }
        else
        {
            for (sel = x->gl_editor->e_selection; sel2 = sel->sel_next;
                sel = sel2)
            {
                if (sel2->sel_what == y)
                {
                    sel->sel_next = sel2->sel_next;
                    gobj_select(sel2->sel_what, x, 0);
                    freebytes(sel2, sizeof(*sel2));
                    break;
                }
            }
        }
        // if we have an invalid object even if the text hasn't changed,
        // we should still try to recreate it
        if (!z && fuddy && pd_class(&((t_text *)y)->te_pd) == text_class &&
            ((t_text *)y)->te_type != T_TEXT)
        {
            z = fuddy;
        }

        if (z)
        {
            //fprintf(stderr, "setto\n");
            char *buf;
            int bufsize;

            rtext_gettext(z, &buf, &bufsize);
            text_setto((t_text *)y, x, buf, bufsize, pos);
            canvas_fixlinesfor(glist_getcanvas(x), (t_text *)y);
            x->gl_editor->e_textedfor = 0;
        }
        if (fixdsp)
            canvas_resume_dsp(1);
        /* Not sure if this is still needed */
        //if (!x->gl_editor->e_selection)
        //    sys_vgui("pdtk_canvas_update_edit_menu .x%zx 0\n", x);
        canvas_draw_gop_resize_hooks(x);
    }
    //reenter = 0;
}

void glist_noselect(t_glist *x)
{
    //post("glist_noselect");
    if (x->gl_editor)
    {
        if (x->gl_editor->e_selection)
        {
            while (x->gl_editor->e_selection)
                glist_deselect(x, x->gl_editor->e_selection->sel_what);
        }
        if (x->gl_editor->e_selectedline)
            glist_deselectline(x);
        if (c_selection == x)
            c_selection = NULL;
        canvas_draw_gop_resize_hooks(x);
    }
}

void glist_selectall(t_glist *x)
{
    if (x->gl_editor)
    {
        glist_noselect(x);
        if (x->gl_list)
        {
            t_gobj *y = x->gl_list;
#ifdef PDL2ORK
            // exception: if we are in K12 mode and preset_hub is hidden,
            // do not select it
            if (y->g_pd == preset_hub_class)
            {
                //fprintf(stderr,"glist_select do not select "
                //               "invised preset_hub in K12 mode\n");
                t_preset_hub *ph = (t_preset_hub *)y;
                if (ph->ph_invis > 0) y = y->g_next;
            }
#endif
            if (y)
            {
                t_selection *sel = (t_selection *)getbytes(sizeof(*sel));
                x->gl_editor->e_selection = sel;
                sel->sel_what = y;
                gobj_select(y, x, 1);
                while (y = y->g_next)
                {
                    t_selection *sel2 = (t_selection *)getbytes(sizeof(*sel2));
                    sel->sel_next = sel2;
                    sel = sel2;
                    sel->sel_what = y;
                    gobj_select(y, x, 1);
                }
                sel->sel_next = 0;
                c_selection = x;
            }
        }
        canvas_draw_gop_resize_hooks(x);
    }
}

    /* get the index of a gobj in a glist.  If y is zero, return the
    total number of objects. */
int glist_getindex(t_glist *x, t_gobj *y)
{
    t_gobj *y2;
    int indx;

    for (y2 = x->gl_list, indx = 0; y2 && y2 != y; y2 = y2->g_next)
        indx++;
    return (indx);
}

    /* get the index of the object, among selected items, if "selected"
    is set; otherwise, among unselected ones.  If y is zero, just
    counts the selected or unselected objects. */
int glist_selectionindex(t_glist *x, t_gobj *y, int selected)
{
    t_gobj *y2;
    int indx;

    for (y2 = x->gl_list, indx = 0; y2 && y2 != y; y2 = y2->g_next)
        if (selected == glist_isselected(x, y2))
            indx++;
    return (indx);
}

t_gobj *glist_nth(t_glist *x, int n)
{
    t_gobj *y;
    int indx;
    for (y = x->gl_list, indx = 0; y; y = y->g_next, indx++)
        if (indx == n)
            return (y);
    return (0);
}

/* ------------------- support for undo/redo  -------------------------- */

static t_undofn canvas_undo_fn;         /* current undo function if any */
static int canvas_undo_whatnext;        /* whether we can now UNDO or REDO */
static void *canvas_undo_buf;           /* data private to the undo function */
static t_canvas *canvas_undo_canvas;    /* which canvas we can undo on */
const char *canvas_undo_name;

void canvas_setundo(t_canvas *x, t_undofn undofn, void *buf,
    const char *name)
{
    //fprintf(stderr,"canvas_setundo %s\n", name);

    int hadone = 0;
        /* blow away the old undo information.  In one special case the
        old undo info is re-used; if so we shouldn't free it here. */
    if (canvas_undo_fn && canvas_undo_buf && (buf != canvas_undo_buf))
    {
        //fprintf(stderr,"hadone canvas_setundo\n");
        (*canvas_undo_fn)(canvas_undo_canvas, canvas_undo_buf, UNDO_FREE);
        hadone = 1;
    }
    canvas_undo_canvas = x;
    canvas_undo_fn = undofn;
    canvas_undo_buf = buf;
    canvas_undo_whatnext = UNDO_UNDO;
    canvas_undo_name = name;
    //if (x && glist_isvisible(x) && glist_istoplevel(x))
    if (x)
    {
        // enable undo in menu
        gui_vmess("gui_undo_menu", "xss",
            x, name, "no");
    }
    else if (hadone)
    {
        /* can't figure out what this does... */
        gui_vmess("gui_undo_menu ", "xss",
            "nobody", "no", "no");
    }
}

    /* clear undo if it happens to be for the canvas x.
     (but if x is 0, clear it regardless of who owns it.) */
void canvas_noundo(t_canvas *x)
{
    if (!x || (x == canvas_undo_canvas))
        canvas_setundo(0, 0, 0, "foo");
}

/* ------- specific undo methods: 1. connect -------- */

typedef struct _undo_connect    
{
    int u_index1;
    int u_outletno;
    int u_index2;
    int u_inletno;
} t_undo_connect;

void *canvas_undo_set_disconnect(t_canvas *x,
    int index1, int outno, int index2, int inno);

/* connect just calls disconnect actions backward... (see below) */
void *canvas_undo_set_connect(t_canvas *x,
    int index1, int outno, int index2, int inno)
{
    return (canvas_undo_set_disconnect(x, index1, outno, index2, inno));
}

void canvas_undo_connect(t_canvas *x, void *z, int action)
{
    int myaction;
    if (action == UNDO_UNDO)
        myaction = UNDO_REDO;
    else if (action == UNDO_REDO)
        myaction = UNDO_UNDO;
    else myaction = action;
    canvas_undo_disconnect(x, z, myaction);
}

/* ------- specific undo methods: 2. disconnect -------- */

void *canvas_undo_set_disconnect(t_canvas *x,
    int index1, int outno, int index2, int inno)
{
    t_undo_connect *buf = (t_undo_connect *)getbytes(sizeof(*buf));
    buf->u_index1 = index1;
    buf->u_outletno = outno;
    buf->u_index2 = index2;
    buf->u_inletno = inno;
    return (buf);
}

void canvas_disconnect(t_canvas *x,
    t_float index1, t_float outno, t_float index2, t_float inno)
{
    //fprintf(stderr,"canvas_disconnect\n");
    char tagbuf[MAXPDSTRING];
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        int srcno = canvas_getindex(x, &t.tr_ob->ob_g);
        int sinkno = canvas_getindex(x, &t.tr_ob2->ob_g);
        if (srcno == index1 && t.tr_outno == outno &&
            sinkno == index2 && t.tr_inno == inno)
        {
            sprintf(tagbuf, "l%zx", (t_int)oc);
            gui_vmess("gui_canvas_delete_line", "xs",
                x, tagbuf);
            // jsarlo
            if(x->gl_editor && x->gl_editor->gl_magic_glass)
            {
                magicGlass_unbind(x->gl_editor->gl_magic_glass);
                magicGlass_hide(x->gl_editor->gl_magic_glass);
            }
            // end jsarlo
            obj_disconnect(t.tr_ob, t.tr_outno, t.tr_ob2, t.tr_inno);

            /* if we are dealing with a preset_node, make sure to also
               disconnect its invisible return node. We trust here that
               the object has been already connected to a valid object
               so we blindly disconnect first outlet with the first inlet */
            if (pd_class(&t.tr_ob->ob_g.g_pd) == preset_node_class && t.tr_outno == 0)
            {
                //fprintf(stderr,"gotta disconnect hidden one too...\n");
                obj_disconnect(t.tr_ob2, 0, t.tr_ob, 0);
            }
            break;
        }
    }
}

void canvas_undo_disconnect(t_canvas *x, void *z, int action)
{
    t_undo_connect *buf = z;
    if (action == UNDO_UNDO)
    {
        canvas_connect(x, buf->u_index1, buf->u_outletno,
            buf->u_index2, buf->u_inletno);
    }
    else if (action == UNDO_REDO)
    {
        canvas_disconnect(x, buf->u_index1, buf->u_outletno,
            buf->u_index2, buf->u_inletno);
    }
    else if (action == UNDO_FREE)
        t_freebytes(buf, sizeof(*buf));
}

/* ---------- ... 3. cut, clear, and typing into objects: -------- */

#define UCUT_CUT 1          /* operation was a cut */
#define UCUT_CLEAR 2        /* .. a clear */

// following action is not needed any more LATER remove any signs of UCUT_TEXT
// since recreate takes care of this in a more elegant way
#define UCUT_TEXT 3         /* text typed into a box */

typedef struct _undo_cut        
{
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
    t_binbuf *u_redotextbuf;    /* buffer to paste back for redo if TEXT */
    int u_mode;                 /* from flags above */
    int n_obj;                  /* number of selected objects to be cut */
    int p_a[1];    /* array of original glist positions of selected objects.
                      At least one object is selected, we dynamically resize
                      it later */
} t_undo_cut;

void *canvas_undo_set_cut(t_canvas *x, int mode)
{
    t_undo_cut *buf;
    t_gobj *y;
    t_linetraverser t;
    t_outconnect *oc;
    int nnotsel= glist_selectionindex(x, 0, 0);
    int nsel = glist_selectionindex(x, 0, 1);
    buf = (t_undo_cut *)getbytes(sizeof(*buf) +
        sizeof(buf->p_a[0]) * (nsel - 1));
    buf->n_obj = nsel;
    buf->u_mode = mode;
    buf->u_redotextbuf = 0;

        /* store connections into/out of the selection */
    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    //if (linetraverser_next(&t)) {
    while (oc = linetraverser_next(&t))
    {
        int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
        int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (issel1 != issel2)
        {
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0) +
                    glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
    //}
    if (mode == UCUT_TEXT)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }
    else if (mode == UCUT_CUT)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }
    else if (mode == UCUT_CLEAR)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }

    //instantiate num_obj and fill array of positions of selected objects
    if (mode == UCUT_CUT || mode == UCUT_CLEAR)
    {
        int i = 0, j = 0;
        if (x->gl_list)
        {
            for (y = x->gl_list; y; y = y->g_next)
            {
                if (glist_isselected(x, y))
                {
                    buf->p_a[i] = j;
                    i++;
                }
                j++;
            }
        }
        //for (i = 0; i < buf->n_obj; i++)
        //    fprintf(stderr,"%d position = %d\n", i, buf->p_a[i]);
    }

    return (buf);
}

void canvas_undo_cut(t_canvas *x, void *z, int action)
{
    //fprintf(stderr, "canvas_undo_cut canvas=%d buf=%d action=%d\n",
    //    (int)x, (int)z, action);
    t_undo_cut *buf = z;
    int mode = buf->u_mode;
    if (action == UNDO_UNDO)
    {
        do_not_redraw += 1;
        //fprintf(stderr,"UNDO_UNDO\n");
        if (mode == UCUT_CUT)
        {
            //fprintf(stderr, "UCUT_CUT\n");
            canvas_dopaste(x, buf->u_objectbuf);
        }
        else if (mode == UCUT_CLEAR)
        {
            //fprintf(stderr, "UCUT_CLEAR\n");
            canvas_dopaste(x, buf->u_objectbuf);
        }
        else if (mode == UCUT_TEXT)
        {
            //fprintf(stderr, "UCUT_TEXT\n");
            t_gobj *y1, *y2;
            glist_noselect(x);
            for (y1 = x->gl_list; y2 = y1->g_next; y1 = y2)
                ;
            if (y1)
            {
                if (!buf->u_redotextbuf)
                {
                    glist_noselect(x);
                    glist_select(x, y1);
                    buf->u_redotextbuf = canvas_docopy(x);
                    glist_noselect(x);
                }
                glist_delete(x, y1);
            }
            canvas_dopaste(x, buf->u_objectbuf);
        }
        pd_bind(&x->gl_pd, gensym("#X"));
        binbuf_eval(buf->u_reconnectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));

        //now reposition objects to their original locations
        if (mode == UCUT_CUT || mode == UCUT_CLEAR)
        {
            //fprintf(stderr,"reordering\n");
            int i = 0;

            /* location of the first newly pasted object */
            int paste_pos = glist_getindex(x,0) - buf->n_obj;
            //fprintf(stderr,"paste_pos %d\n", paste_pos);
            t_gobj *y_prev, *y, *y_next;
            for (i = 0; i < buf->n_obj; i++)
            {
                //first check if we are in the same position already
                if (paste_pos+i != buf->p_a[i])
                {
                    //fprintf(stderr,"not in the right place\n");
                    y_prev = glist_nth(x, paste_pos-1+i);
                    y = glist_nth(x, paste_pos+i);
                    y_next = glist_nth(x, paste_pos+1+i);
                    //if the object is supposed to be first in the gl_list
                    if (buf->p_a[i] == 0)
                    {
                        if (y_prev && y_next)
                        {
                            y_prev->g_next = y_next;
                        }
                        else if (y_prev && !y_next)
                            y_prev->g_next = NULL;
                        //now put the moved object at the beginning of the cue
                        y->g_next = glist_nth(x, 0);
                        x->gl_list = y;
                        //LATER when objects are properly tagged lower y here
                    }
                    //if the object is supposed to be at the current end
                    //of gl_list-- can this ever happen???
                    /*else if (!glist_nth(x,buf->p_a[i])) {

                    }*/
                    //if the object is supposed to be in the middle of gl_list
                    else {
                        if (y_prev && y_next)
                        {
                            y_prev->g_next = y_next;
                        }
                        else if (y_prev && !y_next)
                        {
                            y_prev->g_next = NULL;
                        }
                        //now put the moved object in its right place
                        y_prev = glist_nth(x, buf->p_a[i]-1);
                        y_next = glist_nth(x, buf->p_a[i]);

                        y_prev->g_next = y;
                        y->g_next = y_next;
                        //LATER when objects are properly tagged lower y here
                    }
                }
            }
            do_not_redraw -= 1;
            //LATER disable redrawing here
            // TODO!: move the object to be on the DOM stack, rather than
            // redrawing everything
            canvas_redraw(x);
            if (x->gl_owner && glist_isvisible(x->gl_owner))
            {
                gobj_vis((t_gobj *)x, x->gl_owner, 0);
                gobj_vis((t_gobj *)x, x->gl_owner, 1);
            }
            glob_preset_node_list_check_loc_and_update();
        }
    }
    else if (action == UNDO_REDO)
    {
        //fprintf(stderr,"UNDO_REDO\n");
        if (mode == UCUT_CUT || mode == UCUT_CLEAR)
        {
            //we can't just blindly do clear here when the user may have
            //unselected things between undo and redo, so first let's select
            //the right stuff
            glist_noselect(x);
            int i = 0;
            for (i = 0; i < buf->n_obj; i++)
                glist_select(x, glist_nth(x, buf->p_a[i]));
            canvas_doclear(x);
            glob_preset_node_list_check_loc_and_update();
        }
        else if (mode == UCUT_TEXT)
        {
            t_gobj *y1, *y2;
            for (y1 = x->gl_list; y2 = y1->g_next; y1 = y2)
                ;
            if (y1)
                glist_delete(x, y1);
            canvas_dopaste(x, buf->u_redotextbuf);
            pd_bind(&x->gl_pd, gensym("#X"));
            binbuf_eval(buf->u_reconnectbuf, 0, 0, 0);
            pd_unbind(&x->gl_pd, gensym("#X"));
        }
    }
    else if (action == UNDO_FREE)
    {
        //fprintf(stderr,"UNDO_FREE\n");
        if (buf->u_objectbuf)
            binbuf_free(buf->u_objectbuf);
        if (buf->u_reconnectbuf)
            binbuf_free(buf->u_reconnectbuf);
        if (buf->u_redotextbuf)
            binbuf_free(buf->u_redotextbuf);
        if (buf != NULL)
        {
            t_freebytes(buf, sizeof(*buf) +
                sizeof(buf->p_a[0]) * (buf->n_obj-1));
        }
    }
}

/* --------- 4. motion, including "tidy up" and stretching ----------- */

typedef struct _undo_move_elem  
{
    int e_index;
    int e_xpix;
    int e_ypix;
} t_undo_move_elem;

typedef struct _undo_move       
{
    t_undo_move_elem *u_vec;
    int u_n;
} t_undo_move;

static int canvas_undo_already_set_move;

void *canvas_undo_set_move(t_canvas *x, int selected)
{
    int x1, y1, x2, y2, i, indx;
    t_gobj *y;
    t_undo_move *buf =  (t_undo_move *)getbytes(sizeof(*buf));
    buf->u_n = selected ? glist_selectionindex(x, 0, 1) : glist_getindex(x, 0);
    buf->u_vec = (t_undo_move_elem *)getbytes(sizeof(*buf->u_vec) *
        (selected ? glist_selectionindex(x, 0, 1) : glist_getindex(x, 0)));
    if (selected)
    {
        for (y = x->gl_list, i = indx = 0; y; y = y->g_next, indx++)
            if (glist_isselected(x, y))
        {
            gobj_getrect(y, x, &x1, &y1, &x2, &y2);
            buf->u_vec[i].e_index = indx;
            buf->u_vec[i].e_xpix = x1;
            buf->u_vec[i].e_ypix = y1;
            i++;
        }
    }
    else
    {
        for (y = x->gl_list, indx = 0; y; y = y->g_next, indx++)
        {
            gobj_getrect(y, x, &x1, &y1, &x2, &y2);
            buf->u_vec[indx].e_index = indx;
            buf->u_vec[indx].e_xpix = x1;
            buf->u_vec[indx].e_ypix = y1;
        }
    }
    canvas_undo_already_set_move = 1;
    return (buf);
}

void canvas_undo_move(t_canvas *x, void *z, int action)
{
    t_undo_move *buf = z;
    t_class *cl;
    int resortin = 0, resortout = 0;
    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        int i;
        int x1=0, y1=0, x2=0, y2=0, newx=0, newy=0;
        t_gobj *y;
        //do_not_redraw = 1;
        for (i = 0; i < buf->u_n; i++)
        {
            y = glist_nth(x, buf->u_vec[i].e_index);
            newx = buf->u_vec[i].e_xpix;
            newy = buf->u_vec[i].e_ypix;
            if (y)
            {
                glist_noselect(x);
                glist_select(x, y);
                gobj_getrect(y, x, &x1, &y1, &x2, &y2);
                canvas_displaceselection(x, newx-x1, newy - y1);
                buf->u_vec[i].e_xpix = x1;
                buf->u_vec[i].e_ypix = y1;
                cl = pd_class(&y->g_pd);
                if (cl == vinlet_class) resortin = 1;
                else if (cl == voutlet_class) resortout = 1;
            }
        }
        glist_noselect(x);
        for (i = 0; i < buf->u_n; i++)
        {
            y = glist_nth(x, buf->u_vec[i].e_index);
            if (y) glist_select(x, y);
        }
        //do_not_redraw = 0;
        //canvas_redraw(x);
        if (resortin) canvas_resortinlets(x);
        if (resortout) canvas_resortoutlets(x);
    }
    else if (action == UNDO_FREE)
    {
        t_freebytes(buf->u_vec, buf->u_n * sizeof(*buf->u_vec));
        t_freebytes(buf, sizeof(*buf));
    }
}

/* --------- 5. paste (also duplicate) ----------- */

typedef struct _undo_paste      
{
    int u_index;            /* index of first object pasted */
    int u_sel_index;        /* index of object selected at the time the other
                               object was pasted (for autopatching) */
    int u_offset;           /* offset for duplicated items (since it differs
                               when duplicated into same or different canvas */
    t_binbuf *u_objectbuf;  /* here we store actual copied data */
} t_undo_paste;

void *canvas_undo_set_paste(t_canvas *x, int offset, int duplicate,
    int d_offset)
{
    t_undo_paste *buf =  (t_undo_paste *)getbytes(sizeof(*buf));
    buf->u_index = glist_getindex(x, 0) - offset; //do we need offset at all?
    if (!duplicate && x->gl_editor->e_selection &&
        !x->gl_editor->e_selection->sel_next)
    {
        //if only one object is selected which will warrant autopatching
        buf->u_sel_index = glist_getindex(x,
            x->gl_editor->e_selection->sel_what);
        //fprintf(stderr,"canvas_undo_set_paste selected object index %d\n",
        //    buf->u_sel_index);
    }
    else
    {
        buf->u_sel_index = -1;
    }
    buf->u_offset = d_offset;
    buf->u_objectbuf = binbuf_duplicate(copy_binbuf);
    return (buf);
}

void canvas_undo_paste(t_canvas *x, void *z, int action)
{
    t_undo_paste *buf = z;
    if (action == UNDO_UNDO)
    {
        t_gobj *y;
        glist_noselect(x);
        for (y = glist_nth(x, buf->u_index); y; y = y->g_next)
            glist_select(x, y);
        canvas_doclear(x);
    }
    else if (action == UNDO_REDO)
    {
        //if (buf->u_offset)
        //    do_not_redraw += 1;
        glist_noselect(x);
        //if the pasted object is supposed to be autopatched
        //then select the object it should be autopatched to
        if (buf->u_sel_index > -1)
        {
            //fprintf(stderr,"undo trying autopatch\n");
            glist_select(x, glist_nth(x, buf->u_sel_index));
        }
        canvas_dopaste(x, buf->u_objectbuf);
            /* if it was "duplicate" have to re-enact the displacement. */
        if (buf->u_offset)
        {
            //do_not_redraw -= 1;
            //for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            //    gobj_displace(sel->sel_what, x, 10, 10);
            canvas_paste_xyoffset(x);
        }
    }
    else if (action == UNDO_FREE)
    {
        if (buf->u_objectbuf)
            binbuf_free(buf->u_objectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
}

void clone_iterate(t_pd *z, t_canvas_iterator it, void* data);
int clone_match(t_pd *z, t_symbol *name, t_symbol *dir);
int clone_isab(t_pd *z);
int clone_matchab(t_pd *z, t_ab_definition *source);

/* packed data passing structure for glist_doreload */
typedef struct _reload_data
{
    t_symbol *n;
    t_symbol *d;
    t_gobj *e;
} t_reload_data;

static void glist_doreload_packed(t_canvas *x, void *z);

    /* recursively check for abstractions to reload as result of a save. 
    Don't reload the one we just saved ("except") though. */
    /*  LATER try to do the same trick for externs. */
void glist_doreload(t_glist *gl, t_symbol *name, t_symbol *dir,
    t_gobj *except)
{
    //fprintf(stderr,"doreload\n");
    t_gobj *g;
    int i, nobj = glist_getindex(gl, 0);  /* number of objects */
    int hadwindow = gl->gl_havewindow;
    int found = 0;
    int remakeit = 0;
    // to optimize redrawing we select all objects that need to be updated
    // and redraw them together. Then we look for sub-patches that may have
    // more of the same...
    for (g = gl->gl_list, i = 0; g && i < nobj; i++)
    {
            /* remake the object if it's an abstraction that appears to have
            been loaded from the file we just saved */
        remakeit = (g != except && pd_class(&g->g_pd) == canvas_class &&
            canvas_isabstraction((t_canvas *)g) && !((t_canvas *)g)->gl_isab &&
                ((t_canvas *)g)->gl_name == name &&
                    canvas_getdir((t_canvas *)g) == dir);

            /* also remake it if it's a "clone" with that name */
        if (pd_class(&g->g_pd) == clone_class &&
            clone_match(&g->g_pd, name, dir))
        {
                /* LATER try not to remake the one that equals "except" */
            remakeit = 1;
        }

        if (remakeit)
        {
                /* we're going to remake the object, so "g" will go stale.
                Get its index here, and afterward restore g.  Also, the
                replacement will be at the end of the list, so we don't
                do g = g->g_next in this case. */
            //int j = glist_getindex(gl, g);
            //fprintf(stderr, "rebuildlicious %d\n", j);

            // Bugfix for cases where canvas_vis doesn't actually create a
            // new editor. We need to fix canvas_vis so that the bug doesn't
            // get triggered. But since we know this fixes a regression we'll
            // keep this as a point in the history as we fix canvas_vis. Once
            // that's done we can remove this call.
            canvas_create_editor(gl);

            if (!gl->gl_havewindow)
            {
                canvas_vis(glist_getcanvas(gl), 1);
                //fprintf(stderr,"calling canvas_vis\n");
            }
            if (!found)
            {
                glist_noselect(gl);
                found = 1;
            }
            glist_select(gl, g);
            //canvas_setundo(gl, canvas_undo_cut,
            //    canvas_undo_set_cut(gl, UCUT_CLEAR), "clear");
            //canvas_undo_add(gl, 3, "clear",
            //    canvas_undo_set_cut(gl, UCUT_CLEAR));

            //canvas_cut(gl);
            //canvas_undo_undo(gl);
            //glist_noselect(gl);
            //g = glist_nth(gl, j+1);
        }
        g = g->g_next;
    }
    if (found)
    {
        canvas_cut(gl);
        canvas_undo_undo(gl);
        //canvas_undo_rebranch(gl);
        glist_noselect(gl);
    }

    // now look for sub-patches...
    for (g = gl->gl_list, i = 0; g && i < nobj; i++)
    {
        if (g != except && pd_class(&g->g_pd) == canvas_class &&

            (!canvas_isabstraction((t_canvas *)g) ||
                 ((t_canvas *)g)->gl_isab ||
                 ((t_canvas *)g)->gl_name != name ||
                 canvas_getdir((t_canvas *)g) != dir)
           )
                glist_doreload((t_canvas *)g, name, dir, except);

        /* also reload the instances within ab-based clone objects
            *COMMENT: missing recursive case for abstraction-based clone
                objects that don't match with the one we are reloading?  */
        if(pd_class(&g->g_pd) == clone_class && clone_isab(&g->g_pd))
        {
            t_reload_data d;
            d.n = name; d.d = dir; d.e = except;
            clone_iterate(&g->g_pd, glist_doreload_packed, &d);
        }
        g = g->g_next;
    }
    if (!hadwindow && gl->gl_havewindow)
        canvas_vis(glist_getcanvas(gl), 0);
}

static void glist_doreload_packed(t_canvas *x, void *z)
{
    t_reload_data *data = (t_reload_data *)z;
    glist_doreload(x, data->n, data->d, data->e);
}

    /* this flag stops canvases from being marked "dirty" if we have to touch
    them to reload an abstraction; also suppress window list update */
int glist_amreloadingabstractions = 0;

    /* call glist_doreload on everyone */
void canvas_reload(t_symbol *name, t_symbol *dir, t_gobj *except)
{
    t_canvas *x;
    int dspwas = canvas_suspend_dsp();
    glist_amreloadingabstractions = 1;
        /* find all root canvases */
    for (x = pd_this->pd_canvaslist; x; x = x->gl_next)
        glist_doreload(x, name, dir, except);
    glist_amreloadingabstractions = 0;
    canvas_resume_dsp(dspwas);
}

/* packed data passing structure for glist_doreload_ab */
typedef struct _reload_ab_data
{
    t_ab_definition *a;
    t_gobj *e;
} t_reload_ab_data;

static void glist_doreload_ab_packed(t_canvas *x, void *data);

/* recursive ab reload method */
static void glist_doreload_ab(t_canvas *x, t_ab_definition *a, t_gobj *e)
{
    t_gobj *g;
    int i, nobj = glist_getindex(x, 0);
    int found = 0, remakeit = 0;

    for (g = x->gl_list, i = 0; g && i < nobj; i++)
    {
        remakeit = (g != e && pd_class(&g->g_pd) == canvas_class && 
            canvas_isabstraction((t_canvas *)g)
            && ((t_canvas *)g)->gl_isab && ((t_canvas *)g)->gl_absource == a);

        remakeit = remakeit || (pd_class(&g->g_pd) == clone_class && clone_matchab(&g->g_pd, a));

        if(remakeit)
        {
            canvas_create_editor(x);
            if (!found)
            {
                glist_noselect(x);
                found = 1;
            }
            glist_select(x, g);
        }
        /* since this one won't be reloaded, we need to trigger initbang manually */
        else if(g == e)
            canvas_initbang((t_canvas *)g);
        g = g->g_next;
    }
    if (found)
    {
        canvas_cut(x);
        canvas_undo_undo(x);
        glist_noselect(x);
    }

    for (g = x->gl_list, i = 0; g && i < nobj; i++)
    {
        if(pd_class(&g->g_pd) == canvas_class && (!canvas_isabstraction((t_canvas *)g)
                    || (((t_canvas *)g)->gl_isab && (((t_canvas *)g)->gl_absource != a))))
            glist_doreload_ab((t_canvas *)g, a, e);

        /* also reload the instances within ab-based clone objects */
        if(pd_class(&g->g_pd) == clone_class
            && clone_isab(&g->g_pd) && !clone_matchab(&g->g_pd, a))
        {
            t_reload_ab_data d;
            d.a = a; d.e = e;
            clone_iterate(&g->g_pd, glist_doreload_ab_packed, &d);
        }
        g = g->g_next;
    }
}

static void glist_doreload_ab_packed(t_canvas *x, void *z)
{
    t_reload_ab_data *data = (t_reload_ab_data *)z;
    glist_doreload_ab(x, data->a, data->e);
}

/* reload ab instances */
void canvas_reload_ab(t_canvas *x)
{
    t_canvas *c = canvas_getrootfor_ab(x);
    int dspwas = canvas_suspend_dsp();
    glist_amreloadingabstractions = 1;
    glist_doreload_ab(c, x->gl_absource, &x->gl_gobj);
    glist_amreloadingabstractions = 0;
    canvas_resume_dsp(dspwas);
    /* we set the dirty flag of the root canvas (where the ab definitions
        are stored) because its file contents (in this case, the definition
        for an specific ab) has been edited */
    canvas_dirty(c, 1);
}

/* --------- 6. apply  ----------- */

typedef struct _undo_apply        
{
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
    int u_index;                /* index of the previous object */
} t_undo_apply;

void *canvas_undo_set_apply(t_canvas *x, int n)
{
    //fprintf(stderr,"canvas_undo_set_apply\n");
    t_undo_apply *buf;
    t_gobj *obj;
    t_linetraverser t;
    t_outconnect *oc;
    /* enable editor (in case it is disabled) and select the object
       we are working on */
    if (!x->gl_edit)
        canvas_editmode(x, 1);

    // deselect all objects (if we are editing one while multiple are
    // selected, upon undoing this will recreate other selected objects,
    // effectively resulting in unwanted duplicates)
    // LATER: consider allowing concurrent editing of multiple objects
    glist_noselect(x);

    obj = glist_nth(x, n);
    if (obj && !glist_isselected(x, obj))
        glist_select(x, obj);
    /* get number of all items for the offset below */
    int nnotsel= glist_selectionindex(x, 0, 0);
    buf = (t_undo_apply *)getbytes(sizeof(*buf));

    /* store connections into/out of the selection */
    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
        int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (issel1 != issel2)
        {
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0) +
                    glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
    /* copy object in its current state */
    buf->u_objectbuf = canvas_docopy(x);

    /* store index of the currently selected object */
    buf->u_index = n;

    return (buf);
}

void canvas_undo_apply(t_canvas *x, void *z, int action)
{
    //fprintf(stderr,"canvas_undo_apply\n");
    t_undo_apply *buf = z;
    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        /* find current instance */
        glist_noselect(x);
        glist_select(x, glist_nth(x, buf->u_index));

        /* copy it for the new undo/redo */
        t_binbuf *tmp = canvas_docopy(x);

        /* delete current instance */
        canvas_doclear(x);

        /* replace it with previous instance */
        canvas_dopaste(x, buf->u_objectbuf);

        /* change previous instance with current one */        
        buf->u_objectbuf = tmp;

        /* connections should stay the same */
        pd_bind(&x->gl_pd, gensym("#X"));
        binbuf_eval(buf->u_reconnectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));

        //now we need to reposition the object to its original place
        if (canvas_apply_restore_original_position(x, buf->u_index))
            canvas_redraw(x);
    }
    else if (action == UNDO_FREE)
    {
        if (buf->u_objectbuf)
            binbuf_free(buf->u_objectbuf);
        if (buf->u_reconnectbuf)
            binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
}

//legacy wrapper
void canvas_apply_setundo(t_canvas *x, t_gobj *y)
{
    canvas_undo_add(x, 6, "apply",
        canvas_undo_set_apply(x, glist_getindex(x,y)));
}

int canvas_apply_restore_original_position(t_canvas *x, int orig_pos)
{
    t_gobj *y, *y_prev, *y_next;
    //get the last object
    y = glist_nth(x, glist_getindex(x, 0) - 1);
    if (glist_getindex(x, y) != orig_pos)
    {
        //first make the object prior to the pasted one the end of the list
        y_prev = glist_nth(x, glist_getindex(x, 0) - 2);
        if (y_prev)
            y_prev->g_next = NULL;
        //if the object is supposed to be first in the gl_list
        if (orig_pos == 0)
        {
            y->g_next = glist_nth(x, 0);
            x->gl_list = y;
        }
        //if the object is supposed to be in the middle of the gl_list
        else {
            y_prev = glist_nth(x, orig_pos-1);
            y_next = y_prev->g_next;
            y_prev->g_next = y;
            y->g_next = y_next;
        }
        return(1);
    }
    return(0);
}

/* --------- 7. arrange (to front/back)  ----------- */

typedef struct _undo_arrange       
{
    int u_previndex;            /* old index */
    int u_newindex;                /* new index */
} t_undo_arrange;

void *canvas_undo_set_arrange(t_canvas *x, t_gobj *obj, int newindex)
{
    // newindex tells us is the new index at the beginning (0) or the end (1)

    t_undo_arrange *buf;
    /* enable editor (in case it is disabled) and select the object
       we are working on */
    if (!x->gl_edit)
        canvas_editmode(x, 1);

    // select the object
    if (!glist_isselected(x, obj))
        glist_select(x, obj);

    buf = (t_undo_arrange *)getbytes(sizeof(*buf));

    // set the u_newindex appropriately
    if (newindex == 0) buf->u_newindex = 0;
    else buf->u_newindex = glist_getindex(x, 0) - 1;

    /* store index of the currently selected object */
    buf->u_previndex = glist_getindex(x, obj);

    //fprintf(stderr,"undo_set_arrange %d %d\n",
    //    buf->u_previndex, buf->u_newindex);        

    return (buf);
}

void canvas_undo_arrange(t_canvas *x, void *z, int action)
{
    t_undo_arrange *buf = z;
    t_gobj *y=NULL, *prev=NULL, *next=NULL;

    if (!x->gl_edit)
        canvas_editmode(x, 1);

    if (action == UNDO_UNDO)
    {
        // this is our object
        y = glist_nth(x, buf->u_newindex);

        //fprintf(stderr,"canvas_undo_arrange UNDO_UNDO %d %d\n",
        //    buf->u_previndex, buf->u_newindex);        

        /* select object */
        glist_noselect(x);
        glist_select(x, y);

        if (buf->u_newindex)
        {
            // if it is the last object
            
            // first previous object should point to nothing
            prev = glist_nth(x, buf->u_newindex - 1);
            prev->g_next = NULL;    

            /* now we reuse vars for the following:
               old index should be right before the object previndex
               is pointing to as the object was moved to the end */

            /* old position is not first */
            if (buf->u_previndex)
            {
                prev = glist_nth(x, buf->u_previndex - 1);
                next = prev->g_next;

                // now readjust pointers
                prev->g_next = y;
                y->g_next = next;
            }
            /* old position is first */
            else {
                prev = NULL;
                next = x->gl_list;

                // now readjust pointers
                y->g_next = next;
                x->gl_list = y;
            }

            // and finally redraw canvas
            //canvas_redraw(x);
            // we do this instead to save us costly redraw of the canvas
            canvas_restore_original_position(x, y, 0, -1);

            glob_preset_node_list_check_loc_and_update();
        }
        else {
            // if it is the first object

            /* old index should be right after the object previndex
               is pointing to as the object was moved to the end */
            prev = glist_nth(x, buf->u_previndex);

            // next may be NULL and that is ok
            next = prev->g_next;

            //first glist pointer needs to point to the second object
            x->gl_list = y->g_next;

            //now readjust pointers
            prev->g_next = y;
            y->g_next = next;

            // and finally redraw canvas
            //canvas_redraw(x);
            // we do this instead to save us costly redraw of the canvas
            canvas_restore_original_position(x, y, 0, 1);
            glob_preset_node_list_check_loc_and_update();
        }
    }
    else if (action == UNDO_REDO)
    {
        // find our object
        y = glist_nth(x, buf->u_previndex);

        //fprintf(stderr,"canvas_undo_arrange UNDO_REDO %d %d\n",
        //    buf->u_previndex, buf->u_newindex);    

        /* select object */
        glist_noselect(x);
        glist_select(x, y);

        int action;
        if (!buf->u_newindex) action = 4;
        else action = 3;

        t_gobj *oldy_prev=NULL, *oldy_next=NULL;

        // if there is an object before ours (in other words our index is > 0)
        if (glist_getindex(x,y))
            oldy_prev = glist_nth(x, buf->u_previndex - 1);
            
        // if there is an object after ours
        if (y->g_next)
            oldy_next = y->g_next;

        canvas_doarrange(x, action, y, oldy_prev, oldy_next);
    }
    else if (action == UNDO_FREE)
    {
        t_freebytes(buf, sizeof(*buf));
    }
}

void canvas_arrange_setundo(t_canvas *x, t_gobj *obj, int newindex)
{
    canvas_setundo(x, canvas_undo_arrange,
        canvas_undo_set_arrange(x, obj, newindex), "arrange");
}

/* --------- 8. apply on canvas ----------- */

typedef struct _undo_canvas_properties      
{
    int gl_pixwidth;            /* width in pixels (on parent, if a graph) */
    int gl_pixheight;
    t_float gl_x1;              /* bounding rectangle in our own coordinates */
    t_float gl_y1;
    t_float gl_x2;
    t_float gl_y2;
    int gl_screenx1;            /* screen coordinates when toplevel */
    int gl_screeny1;
    int gl_screenx2;
    int gl_screeny2;
    int gl_xmargin;             /* origin for GOP rectangle */
    int gl_ymargin;

    unsigned int gl_goprect:1;  /* draw rectangle for graph-on-parent */
    unsigned int gl_isgraph:1;  /* show as graph on parent */
    unsigned int gl_hidetext:1; /* hide object-name + args when doing
                                   graph on parent */
} t_undo_canvas_properties;

//t_undo_canvas_properties global_buf;

void *canvas_undo_set_canvas(t_canvas *x)
{
    /* enable editor (in case it is disabled) */
    //if (x->gl_havewindow && !x->gl_edit)
    //    canvas_editmode(x, 1);

    t_undo_canvas_properties *buf =
        (t_undo_canvas_properties *)getbytes(sizeof(*buf));

    buf->gl_pixwidth = x->gl_pixwidth;
    buf->gl_pixheight = x->gl_pixheight;
    buf->gl_x1 = x->gl_x1;
    buf->gl_y1 = x->gl_y1;
    buf->gl_x2 = x->gl_x2;
    buf->gl_y2 = x->gl_y2;
    buf->gl_screenx1 = x->gl_screenx1;
    buf->gl_screeny1 = x->gl_screeny1;
    buf->gl_screenx2 = x->gl_screenx2;
    buf->gl_screeny2 = x->gl_screeny2;
    buf->gl_xmargin = x->gl_xmargin;
    buf->gl_ymargin = x->gl_ymargin;
    buf->gl_goprect = x->gl_goprect;
    buf->gl_isgraph = x->gl_isgraph;
    buf->gl_hidetext = x->gl_hidetext;
    
    return (buf);
}

extern t_int gfxstub_haveproperties(void *key);

void canvas_undo_canvas_apply(t_canvas *x, void *z, int action)
{
    t_undo_canvas_properties *buf = (t_undo_canvas_properties *)z;
    t_undo_canvas_properties tmp;

    if (!x->gl_edit)
        canvas_editmode(x, 1);

    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        //close properties window first
        t_int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            //sys_vgui("destroy .gfxstub%zx\n", properties);
            gfxstub_deleteforkey(x);
        }

        //store current canvas values into temporary data holder
        tmp.gl_pixwidth = x->gl_pixwidth;
        tmp.gl_pixheight = x->gl_pixheight;
        tmp.gl_x1 = x->gl_x1;
        tmp.gl_y1 = x->gl_y1;
        tmp.gl_x2 = x->gl_x2;
        tmp.gl_y2 = x->gl_y2;
        tmp.gl_screenx1 = x->gl_screenx1;
        tmp.gl_screeny1 = x->gl_screeny1;
        tmp.gl_screenx2 = x->gl_screenx2;
        tmp.gl_screeny2 = x->gl_screeny2;
        tmp.gl_xmargin = x->gl_xmargin;
        tmp.gl_ymargin = x->gl_ymargin;
        tmp.gl_goprect = x->gl_goprect;
        tmp.gl_isgraph = x->gl_isgraph;
        tmp.gl_hidetext = x->gl_hidetext;

        //change canvas values with the ones from the undo buffer
        x->gl_pixwidth = buf->gl_pixwidth;
        x->gl_pixheight = buf->gl_pixheight;
        x->gl_x1 = buf->gl_x1;
        x->gl_y1 = buf->gl_y1;
        x->gl_x2 = buf->gl_x2;
        x->gl_y2 = buf->gl_y2;
        x->gl_screenx1 = buf->gl_screenx1;
        x->gl_screeny1 = buf->gl_screeny1;
        x->gl_screenx2 = buf->gl_screenx2;
        x->gl_screeny2 = buf->gl_screeny2;
        x->gl_xmargin = buf->gl_xmargin;
        x->gl_ymargin = buf->gl_ymargin;
        x->gl_goprect = buf->gl_goprect;
        x->gl_isgraph = buf->gl_isgraph;
        x->gl_hidetext = buf->gl_hidetext;

        //copy data values from the temporary data to the undo buffer
        buf->gl_pixwidth = tmp.gl_pixwidth;
        buf->gl_pixheight = tmp.gl_pixheight;
        buf->gl_x1 = tmp.gl_x1;
        buf->gl_y1 = tmp.gl_y1;
        buf->gl_x2 = tmp.gl_x2;
        buf->gl_y2 = tmp.gl_y2;
        buf->gl_screenx1 = tmp.gl_screenx1;
        buf->gl_screeny1 = tmp.gl_screeny1;
        buf->gl_screenx2 = tmp.gl_screenx2;
        buf->gl_screeny2 = tmp.gl_screeny2;
        buf->gl_xmargin = tmp.gl_xmargin;
        buf->gl_ymargin = tmp.gl_ymargin;
        buf->gl_goprect = tmp.gl_goprect;
        buf->gl_isgraph = tmp.gl_isgraph;
        buf->gl_hidetext = tmp.gl_hidetext;

        //redraw
        canvas_setgraph(x, x->gl_isgraph + 2*x->gl_hidetext, 0);
        canvas_dirty(x, 1);

        if (x->gl_havewindow)
        {
            canvas_redraw(x);
        }
        if (x->gl_owner && glist_isvisible(x->gl_owner))
        {
            glist_noselect(x);
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            canvas_redraw(x->gl_owner);
        }
        //update scrollbars when GOP potentially exceeds window size
        t_canvas *canvas=(t_canvas *)glist_getcanvas(x);

        //if gop is being disabled go one level up
        /*if (!x->gl_isgraph && x->gl_owner) {
            canvas=canvas->gl_owner;
            canvas_redraw(canvas);
        }*/

        //if properties window is open,
        //update the properties with the previous window properties        
        /*t_int properties = gfxstub_haveproperties((void *)x);
        if (properties) {
            sys_vgui("pdtk_canvas_dialog_undo_update .gfxstub%zx %d %d\n",
                properties, x->gl_isgraph, x->gl_hidetext);
            sys_vgui(".gfxstub%zx.xscale.entry delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.xrange.entry1 insert 0 %d\n",
                properties, x->gl_x1);
            sys_vgui(".gfxstub%zx.yrange.entry1 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.yrange.entry1 insert 0 %d\n",
                properties, x->gl_y1);
            sys_vgui(".gfxstub%zx.xrange.entry2 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.xrange.entry2 insert 0 %d\n",
                properties, x->gl_x2);
            sys_vgui(".gfxstub%zx.yrange.entry2 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.yrange.entry2 insert 0 %d\n",
                properties, x->gl_y2);
            sys_vgui(".gfxstub%zx.xrange.entry3 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.xrange.entry3 insert 0 %d\n",
                properties, x->gl_pixwidth);
            sys_vgui(".gfxstub%zx.yrange.entry3 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.yrange.entry3 insert 0 %d\n",
                properties, x->gl_pixheight);
            sys_vgui(".gfxstub%zx.xrange.entry4 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.xrange.entry4 insert 0 %d\n",
                properties, x->gl_xmargin);
            sys_vgui(".gfxstub%zx.yrange.entry4 delete 0 end\n", properties);
            sys_vgui(".gfxstub%zx.yrange.entry4 insert 0 %d\n",
                properties, x->gl_ymargin);
        }*/

        scrollbar_update(x);
        if (canvas != x) scrollbar_update(canvas);
    }

    else if (action == UNDO_FREE)
    {
        if (buf)
            t_freebytes(buf, sizeof(*buf));
    }
}

void canvas_canvas_setundo(t_canvas *x)
{
    canvas_setundo(x, canvas_undo_canvas_apply,
        canvas_undo_set_canvas(x), "apply");
}

/* --------- 9. create ----------- */

typedef struct _undo_create      
{
    int u_index;                /* index of the created object object */
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
} t_undo_create;

void *canvas_undo_set_create(t_canvas *x)
{
    t_gobj *y;
    t_linetraverser t;
    t_outconnect *oc;
    int issel1, issel2;

    t_undo_create *buf = (t_undo_create *)getbytes(sizeof(*buf));
    buf->u_index = glist_getindex(x, 0) - 1;
    int nnotsel= glist_selectionindex(x, 0, 0);
    //fprintf(stderr,"buf->u_index=%d nnotsel=%d\n", buf->u_index, nnotsel);

    buf->u_objectbuf = binbuf_new();
    if (x->gl_list)
    {
        for (y = x->gl_list; y; y = y->g_next)
        {
            //if (glist_isselected(x, y)) {
            if (!y->g_next)
            {
                //fprintf(stderr,"undo_set_create: gobj_save\n");
                gobj_save(y, buf->u_objectbuf);
                break;
            }
        }
        buf->u_reconnectbuf = binbuf_new();
        linetraverser_start(&t, x);
        while (oc = linetraverser_next(&t))
        {
            //int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
            //int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
            issel1 = ( &t.tr_ob->ob_g == y ? 1 : 0);
            issel2 = ( &t.tr_ob2->ob_g == y ? 1 : 0);
            //fprintf(stderr,"undo_set_create linetraverser %d %d\n",
            //    issel1, issel2);
            if (issel1 != issel2)
            {
                //fprintf(stderr,"undo_set_create store connection\n");
                binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                    gensym("#X"), gensym("connect"),
                    (issel1 ? nnotsel : 0)
                        + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                    t.tr_outno,
                    (issel2 ? nnotsel : 0) +
                        glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                    t.tr_inno);
                /*char *text;
                int lengthp;
                binbuf_gettext(buf->u_reconnectbuf, &text, &lengthp);
                fprintf(stderr,"%s\n", text);*/
            }
        }
    }
    return (buf);
}

void canvas_undo_create(t_canvas *x, void *z, int action)
{
    t_undo_create *buf = z;
    t_gobj *y;

    //fprintf(stderr,"canvas = %zx buf->u_index = %d\n",
    //    (t_int)x, buf->u_index);

    if (action == UNDO_UNDO)
    {
        glist_noselect(x);
        y = glist_nth(x, buf->u_index);
        glist_select(x, y);
        canvas_doclear(x);
    }
    else if (action == UNDO_REDO)
    {
        pd_bind(&x->gl_pd, gensym("#X"));
           binbuf_eval(buf->u_objectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));
        pd_bind(&x->gl_pd, gensym("#X"));
           binbuf_eval(buf->u_reconnectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));
        if (pd_this->pd_newest && pd_class(pd_this->pd_newest) == canvas_class)
            canvas_loadbang((t_canvas *)pd_this->pd_newest);
        y = glist_nth(x, buf->u_index);
        glist_select(x, y);
    }
    else if (action == UNDO_FREE)
    {
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
}

/* ------ 10. recreate (called from text_setto after text has changed) ------ */

//recreate uses t_undo_create struct

void *canvas_undo_set_recreate(t_canvas *x, t_gobj *y, int pos)
{
    t_linetraverser t;
    t_outconnect *oc;
    int issel1, issel2;

    t_undo_create *buf = (t_undo_create *)getbytes(sizeof(*buf));
    buf->u_index = pos;
    int nnotsel= glist_selectionindex(x, 0, 0) - 1; // - 1 is a critical
                                                    // difference from
                                                    // the create
    //fprintf(stderr,"buf->u_index=%d nnotsel=%d\n", buf->u_index, nnotsel);
    buf->u_objectbuf = binbuf_new();
    //y = glist_nth(x, buf->u_index);
    //fprintf(stderr,"undo_set_create: gobj_save\n");
    gobj_save(y, buf->u_objectbuf);

    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        //int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
        //int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
        issel1 = ( &t.tr_ob->ob_g == y ? 1 : 0);
        issel2 = ( &t.tr_ob2->ob_g == y ? 1 : 0);
        //fprintf(stderr,"undo_set_create linetraverser %d %d\n",
        //    issel1, issel2);
        if (issel1 != issel2)
        {
            //fprintf(stderr,"undo_set_create store connection\n");
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0) +
                    glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
    return (buf);
}

void canvas_undo_recreate(t_canvas *x, void *z, int action)
{
    //fprintf(stderr,"canvas_undo_recreate\n");

    t_undo_create *buf = z;
    t_gobj *y = NULL;
    if (action == UNDO_UNDO)
        y = glist_nth(x, glist_getindex(x, 0) - 1);
    else if (action == UNDO_REDO)
        y = glist_nth(x, buf->u_index);

    //fprintf(stderr,"canvas = %zx buf->u_index = %d\n",
    //    (t_int)x, buf->u_index);

    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        // first copy new state of the current object
        t_undo_create *buf2 = (t_undo_create *)getbytes(sizeof(*buf));
        //buf2->u_index = glist_getindex(x, y);
        buf2->u_index = buf->u_index;

        //fprintf(stderr,"buf2->u_index=%d nnotsel=%d y_getindex=%d\n",
        //    buf2->u_index, nnotsel, glist_getindex(x, y));
        buf2->u_objectbuf = binbuf_new();
        //fprintf(stderr,"undo_set_recreate: gobj_save\n");
        gobj_save(y, buf2->u_objectbuf);

        buf2->u_reconnectbuf = binbuf_duplicate(buf->u_reconnectbuf);

        // now cut the existing object
        glist_noselect(x);
        glist_select(x, y);
        canvas_doclear(x);

        // then paste the old object
        pd_bind(&x->gl_pd, gensym("#X"));
           binbuf_eval(buf->u_objectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));
        pd_bind(&x->gl_pd, gensym("#X"));
           binbuf_eval(buf->u_reconnectbuf, 0, 0, 0);
        pd_unbind(&x->gl_pd, gensym("#X"));

        // free the old data
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));

        // readjust pointer
        // (this should probably belong into g_undo.c, but since it is
        // a unique case, we'll let it be for the time being)
        x->u_last->data = (void *)buf2;
        buf = buf2;

        // reposition object to its original place
        if (action == UNDO_UNDO)
            if (canvas_apply_restore_original_position(x, buf->u_index))
                canvas_redraw(x);

        // send a loadbang
        if (pd_this->pd_newest && pd_class(pd_this->pd_newest) == canvas_class)
            canvas_loadbang((t_canvas *)pd_this->pd_newest);

        // select
        if (action == UNDO_REDO)
            y = glist_nth(x, glist_getindex(x, 0) - 1);
        else
            y = glist_nth(x, buf->u_index);
        glist_select(x, y);
    }
    else if (action == UNDO_FREE)
    {
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
}

/* ----------- 11. font -------------- */

typedef struct _undo_font
{
    int font;
} t_undo_font;

void *canvas_undo_set_font(t_canvas *x, int font)
{
    t_undo_font *u_f = (t_undo_font *)getbytes(sizeof(*u_f));
    u_f->font = font;
    return (u_f);
}

void canvas_undo_font(t_canvas *x, void *z, int action)
{
    t_undo_font *u_f = z;

    if (action == UNDO_UNDO || action == UNDO_REDO) 
    {
        t_canvas *x2 = canvas_getrootfor(x);
        int tmp_font = x2->gl_font;
        t_int properties = gfxstub_haveproperties((void *)x2);
        if (properties)
        {
            char tagbuf[MAXPDSTRING];
            sprintf(tagbuf, ".gfxstub%zx", (t_uint)properties);
            gui_vmess("gui_font_dialog_change_size", "si",
                tagbuf,
                u_f->font);
        }
        else
        {
            //sys_vgui("dofont_apply .x%zx %d 1\n", x2, u_f->font);
            /* In pd.tk there is a global variable holding the last font
               size. So our dataflow is:
               1) Pd -> GUI dofont_apply as above
               2) GUI looks up the previous font size
               3) GUI -> Pd "canvasid font size old_size 100 no_undo"
               4) Pd -> GUI redraw the canvas with the new font sizes

               Can't figure out why we need to talk to the GUI in #1, so
               I'm just calling the canvas "font" method directly... */
            vmess(&x2->gl_pd, gensym("font"), "iiii",
                u_f->font, tmp_font, 100, 1);
        }
        u_f->font = tmp_font;
    }
    else if (action == UNDO_FREE)
    {
        if (u_f)
            freebytes(u_f, sizeof(*u_f));
    }
}

/* ------------------------------- abstract feature --------------------- */

static t_class *abstracthandler_class;

typedef struct abstracthandler
{
    t_object x_obj;
    t_symbol *sym;          /* symbol bound to the object */
    t_canvas *dialog;       /* canvas where the dialog will be displayed */
    t_canvas *tarjet;       /* tarjet subpatch */
    char *path;             /* abstraction path */
    t_binbuf *subpatch;     /* subpatch contents */
} t_abstracthandler;

static void *abstracthandler_new(void)
{
    t_abstracthandler *x = (t_abstracthandler *)pd_new(abstracthandler_class);
    char namebuf[80];
    sprintf(namebuf, "ah%lx", (t_int)x);
    x->sym = gensym(namebuf);
    pd_bind(&x->x_obj.ob_pd, x->sym);
    return (x);
}

static void abstracthandler_free(t_abstracthandler *x)
{
    freebytes(x->path, MAXPDSTRING);
    binbuf_free(x->subpatch);
    pd_unbind(&x->x_obj.ob_pd, x->sym);
}

/* gobj_activate, canvas_addtobuf and canvas_buftotex toghether and simplified*/
static void do_rename_light(t_gobj *z, t_glist *glist, const char *text)
{
    t_rtext *y = glist_findrtext(glist, (t_text *)z);
    glist->gl_editor->e_textedfor = y;
    char *buf = getbytes(strlen(text)+1);
    strcpy(buf, text);
    rtext_settext(y, buf, strlen(text)+1);
    glist->gl_editor->e_textdirty = 1;
    canvas_dirty(glist, 1);
    glist->gl_editor->e_onmotion = MA_NONE; //necessary?
}

int binbuf_match(t_binbuf *inbuf, t_binbuf *searchbuf, int wholeword);

/* traverses the whole subtree of the given canvas/patch, replacing all subpatches
    identical to the given one with an abstraction */
static int do_replace_subpatches(t_canvas *x, const char* label, t_binbuf *original)
{
    t_selection *list = 0;
    int edi = 0, num = 0;
    /* editor is needed in order to do the operations below */
    if(label && !x->gl_editor) { canvas_create_editor(x); edi = 1; }
    t_gobj *y, *yn;
    canvas_undo_add(x, UNDO_SEQUENCE_START, "replace", "subpatch replacements have been redone. "
    "All other possible replacements within other (sub)patches have not been affected");
    for(y = x->gl_list; y; y = yn)
    {
        yn = y->g_next; /* dirty hack,
                            'canvas_stowconnections' inside 'glist_deselect'
                            rearranges the glist */
        if(pd_class(&y->g_pd) == canvas_class &&
                !canvas_isabstraction((t_canvas *)y))
            {
                t_binbuf *tmp = binbuf_new(), *tmps = binbuf_new();
                gobj_save((t_gobj *)y, tmp);
                int i = 0, j = binbuf_getnatom(tmp)-2;
                t_atom *v = binbuf_getvec(tmp);
                while(v[i].a_type != A_SEMI) i++;
                i++;
                while(v[j].a_type != A_SEMI) j--;
                binbuf_restore(tmps, j-i+1, v+i);
                binbuf_free(tmp);
                if(binbuf_match(original, tmps, 0) &&
                        binbuf_getnatom(original) == binbuf_getnatom(tmps))
                {
                    if(label)
                    {
                        binbuf_free(tmps);
                        glist_noselect(x);
                        glist_select(x, y);
                        do_rename_light(y, x, label);
                        glist_deselect(x, y);
                    }
                    else num++;
                }
                else
                {
                    binbuf_free(tmps);
                    /* all the non-matching subpatches are stored in a list, */
                    t_selection *new = (t_selection *)getbytes(sizeof(t_selection));
                    new->sel_what = y;
                    new->sel_next = list;
                    list = new;
                }
            }
    }
    canvas_undo_add(x, UNDO_SEQUENCE_END, "replace", "subpatch replacements have been undone. "
    "All other possible replacements within other (sub)patches have not been affected");
    if(edi) canvas_destroy_editor(x);
    t_selection *z, *zn;
    /* the function is called recursively on each non-matching canvas.
        this has been left for last due to a visual bug that happens if
        they are explored as soon as they are traversed*/
    for(z = list; z; z = zn)
    {
        zn = z->sel_next;
        num += do_replace_subpatches((t_canvas *)z->sel_what, label, original);
        freebytes(z, sizeof(t_selection));
    }
    return num;
}

static void abstracthandler_callback(t_abstracthandler *x, t_symbol *s)
{
    char fullpath[MAXPDSTRING], label[MAXPDSTRING], *dir, *filename;
    char basename[MAXPDSTRING], buf[MAXPDSTRING];
    memset(fullpath, '\0', MAXPDSTRING);
    memset(basename, '\0', MAXPDSTRING);
    memset(label, '\0', MAXPDSTRING);
    sys_unbashfilename(s->s_name, fullpath);
    if (strlen(fullpath) < 3 || strcmp(fullpath+strlen(fullpath)-3, ".pd"))
        strcat(fullpath, ".pd");
    filename = strrchr(fullpath, '/') + 1;
    fullpath[filename - fullpath - 1] = '\0';
    dir = fullpath;
    strncpy(basename, filename, strlen(filename) - 3);
    int flag, prefix = 0;
    strncpy(buf,
            canvas_getdir(canvas_getrootfor(x->tarjet))->s_name,
            MAXPDSTRING);
    buf[MAXPDSTRING-1] = 0;
    if (flag =
        sys_relativizepath(buf, dir, label))
    {
        int len = strlen(label), creator, fd = -1;
        if (len && label[len-1] != '/')
            label[len] = '/';
        strncat(label, basename, MAXPDSTRING - strlen(label) - 1);
        /* check if there is a creator with the same name or if it's one of
           the built-in methods */
        t_symbol *sym = gensym(label);
        creator = (sym == &s_bang ||
                   sym == &s_float ||
                   sym == &s_symbol ||
                   sym == &s_blob ||
                   sym == &s_list ||
                   sym == &s_anything);
        if (!len && creator)
        {
            prefix = (!len && creator);
            creator = 0;
        }
        creator = (creator || zgetfn(&pd_objectmaker, sym));

        /* check if there in an abstraction with the same name in the
           search path */
        if (!creator)
        {
            char opendir[MAXPDSTRING], *filenameptr;
            fd = canvas_open(canvas_getrootfor(x->tarjet), label, ".pd",
                opendir, &filenameptr, MAXPDSTRING, 0); //high load
            if (fd > 0)
            {
                sys_close(fd);
                /* check if we are overwriting the file */
                if (!strncmp(dir, opendir, filenameptr - opendir - 1)) fd = -1;
            }
        }
        flag = !(creator || (fd > 0));

        if (flag && prefix)
        {
            strcpy(label, "./");
            strncat(label, basename, MAXPDSTRING - strlen(label) - 1);
        }
        else if (!flag)
            error("warning: couldn't use relative path, there is a coincidence "
                  "in the creator list or the search path");
    }
    if (!flag) /* absolute path is required */
    {
        memset(label, '\0', MAXPDSTRING);
        strcpy(label, dir);
        strcat(label, "/");
        strncat(label, basename, MAXPDSTRING - strlen(label) - 1);
        /* should check if 'filename' is one of the built-in special methods
            in order to inform the user about the nameclash problem */
    }
    x->path = (char *)getbytes(MAXPDSTRING);
    strcpy(x->path, label);

    /* save the subpatch into a separated pd file */
    t_atom at[3];
    SETSYMBOL(at, gensym(filename));
    SETSYMBOL(at+1, gensym(dir));
    SETFLOAT(at+2, 0.f);
    /* gl_env is set to non-zero in order to save the subcanvas as a
       root canvas */
    x->tarjet->gl_env = dummy_canvas_env(dir);
    typedmess(&x->tarjet->gl_pd, gensym("savetofile"), 3, at);
    x->tarjet->gl_env = 0;

    t_binbuf *tmp = binbuf_new(), *tmps = binbuf_new();
    gobj_save((t_gobj *)x->tarjet, tmp);
    /* only the internals are kept, the position on parent canvas may differ */
    int i = 0, j = binbuf_getnatom(tmp)-2, matches;
    t_atom *v = binbuf_getvec(tmp);
    while(v[i].a_type != A_SEMI)
    {
        i++;
    }
    i++;
    while(v[j].a_type != A_SEMI)
    {
        j--;
    }
    binbuf_restore(tmps, j-i+1, v+i);
    binbuf_free(tmp);
    matches = do_replace_subpatches(canvas_getrootfor(x->tarjet), 0, tmps);
    x->subpatch = tmps;

    /* trigger the frontend dialog for replacing options */
    gui_vmess("gui_canvas_abstract", "xsi",
                    x->dialog,
                    x->sym->s_name,
                    matches);
}

static void abstracthandler_dialog(t_abstracthandler *x, t_floatarg val)
{
    if (x->tarjet == x->dialog) canvas_vis(x->dialog, 0);
    t_canvas *owner = x->tarjet->gl_owner, *root = canvas_getrootfor(x->tarjet);
    int all = val;
    if (!all)
    {
        /* change the text of the subpatch object to create the abstraction,
            emulating the procedure done by the user. could be simplified */
        int edi = 0;
        if (!owner->gl_editor) { canvas_create_editor(owner); edi = 1; }
        glist_noselect(owner);
        glist_select(owner, &x->tarjet->gl_gobj);
        do_rename_light(&x->tarjet->gl_gobj, owner, x->path);
        glist_deselect(owner, &x->tarjet->gl_gobj);
        if (edi) canvas_destroy_editor(owner);

        /* select '[args]' slice
        canvas_editmode(owner, 1);
        t_gobj *abst = glist_nth(owner, glist_getindex(owner, 0)-1);
        int len = strlen(x->path);
        glist_select(owner, abst);
        gobj_activate(abst, owner, (0b1 << 31) |
            (((len+1) & 0x7FFF) << 16) |
            ((len+7) & 0xFFFF)); */
    }
    else
    {
        do_replace_subpatches(root, x->path, x->subpatch);
    }
    pd_free(&x->x_obj.ob_pd);
}

void abstracthandler_setup(void)
{
    abstracthandler_class = class_new(gensym("abstracthandler"), 0,
        (t_method)abstracthandler_free, sizeof(t_abstracthandler),
        CLASS_NOINLET, 0);
    class_addmethod(abstracthandler_class, (t_method)abstracthandler_callback,
        gensym("callback"), A_SYMBOL, 0);
    class_addmethod(abstracthandler_class, (t_method)abstracthandler_dialog,
        gensym("dialog"), A_FLOAT, 0);
}

/* ------------------------ event handling ------------------------ */

static char *cursorlist[] = {
    "cursor_runmode_nothing",
    "cursor_runmode_clickme",
    "cursor_runmode_thicken",
    "cursor_runmode_addpoint",
    "cursor_editmode_nothing",
    "cursor_editmode_connect",
    "cursor_editmode_disconnect",
    "cursor_editmode_resize",
    "cursor_editmode_resize_bottom_right",
    "cursor_scroll",
    "cursor_editmode_resize_vert",
    "cursor_editmode_move",
    "cursor_editmode_floating"
};

void canvas_setcursor(t_canvas *x, unsigned int cursornum)
{
    if (x->gl_havewindow)
    {
        //post("canvas_setcursor %d", cursornum);
        static t_canvas *xwas;
        static unsigned int cursorwas;
        if (cursornum >= sizeof(cursorlist)/sizeof *cursorlist)
        {
            bug("canvas_setcursor");
                return;
        }
        if (xwas != x || cursorwas != cursornum)
        {
            //post("...requesting cursor change", cursornum);
            gui_vmess("gui_canvas_cursor", "xs", x,
                cursorlist[cursornum]);
            xwas = x;
            cursorwas = cursornum;
        }
    }
}

    /* check if a point lies in a gobj.  */
int canvas_hitbox(t_canvas *x, t_gobj *y, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p)
{
    //post("canvas_hitbox %lx", y);
    int x1, y1, x2, y2;
    if (!gobj_shouldvis(y, x))
        return (0);
    gobj_getrect(y, x, &x1, &y1, &x2, &y2);
    //if (((t_text *)y)->te_iemgui)
    //    iemgui_getrect_mouse(y, &x1, &y1, &x2, &y2);
    //post("...canvas_hitbox xpos=%d ypos-%d | x1=%d y1=%d x2=%d y2=%d", xpos, ypos, x1, y1, x2, y2);
    // we also add a check that width is greater than 0 because we use this
    // to return value from objects that are designed to ignore clicks and
    // pass them below, e.g. pd-l2ork's version of ggee/image which uses this
    // feature in runtime mode only (we can use canvas->gl_edit check for this
    // within an external)
    if (x2-x1 > 0 && xpos >= x1 && xpos <= x2 && ypos >= y1 && ypos <= y2)
    {
        *x1p = x1;
        *y1p = y1;
        *x2p = x2;
        *y2p = y2;
        return (1);
    }
    else return (0);
}

    /* find the last gobj, if any, containing the point. */
t_gobj *canvas_findhitbox(t_canvas *x, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p)
{
    //post("canvas_findhitbox...");
    t_gobj *y, *rval = 0;
    int x1, y1, x2, y2;
    *x1p = -0x7fffffff;
    for (y = x->gl_list; y; y = y->g_next)
    {
        //post("   next");
        if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
        {
            //&& (x1 > *x1p))
            /* commented section looks for whichever is more to the right
               which is wrong since we are looking for topmost object */
                *x1p = x1, *y1p = y1, *x2p = x2, *y2p = y2, rval = y;
                //post("   got it");
        }
    }
    return (rval);
}

    /* find the last gobj, if any, containing the point that has nlets. 
       inout is 0 when looking for inlets, and 1 when looking for outlets */
t_gobj *canvas_findhitbox_w_nlets(t_canvas *x, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p, int inout)
{
    //post("canvas_findhitbox...");
    t_gobj *y, *rval = 0;
    t_object *ob;
    int x1, y1, x2, y2;
    *x1p = -0x7fffffff;
    for (y = x->gl_list; y; y = y->g_next)
    {
        //post("   next");
        if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
        {
            ob = pd_checkobject(&y->g_pd);
            if (ob && (inout == 0 ? obj_ninlets(ob) : obj_noutlets(ob)))
            //&& (x1 > *x1p))
            /* commented section looks for whichever is more to the right
               which is wrong since we are looking for topmost object */
                *x1p = x1, *y1p = y1, *x2p = x2, *y2p = y2, rval = y;
                //post("   got it");
        }
    }
    return (rval);
}

extern t_class *array_define_class;
extern int scalar_getcanvasfield(t_scalar *x);

// ico@vt.edu 2022-11-14: check if this or any of the parent canvases
// have the editable option disabled, and if so, ignore the right click
int canvas_is_editable(t_canvas *x)
{
    t_canvas *root = x;
    while (root->gl_owner)
    {
        if (!root->gl_editable)
            return(0);
        root = root->gl_owner;
    }
    return (root->gl_editable);
}

    /* right-clicking on a canvas object pops up a menu. */
static void canvas_rightclick(t_canvas *x, int xpos, int ypos, t_gobj *y_sel)
{
    //fprintf(stderr,"e_onmotion=%d\n",x->gl_editor->e_onmotion);
    if (x->gl_editor->e_onmotion != MA_NONE) return;

    if (!canvas_is_editable(x))
        return;

    if(x->gl_disableruntimepopup)
        return;

    int canprop, canopen, isobject, cansaveas;
    t_gobj *y = NULL;
    int x1, y1, x2, y2, scalar_has_canvas = 0;
    if (x->gl_editor->e_selection)
    {
        glist_noselect(x);
    }
    t_gobj *yclick = NULL;
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (y && canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
        {
            yclick = y;
        }
    }
    if (yclick)
    {
        y = yclick;
        if (x->gl_edit && !glist_isselected(x, y))
            glist_select(x, y);
    }
    // if we are in K12 mode and are requesting popup on comments,
    // bail as we don't want users to get into conventional help files
    if (sys_k12_mode && y && pd_class(&y->g_pd) == text_class)
        return;
    /* abstractions should only allow for properties inside them 
       otherwise they end-up being dirty without visible notification
       besides, why would one mess with their properties without
       seeing what is inside them? CURRENTLY DISABLED */
    //post("canvas_rightclick %d", (y && pd_class(&y->g_pd) == garray_class ? 1 : 0));
    canprop = (!y || (y && class_getpropertiesfn(pd_class(&y->g_pd)))
               // ico@vt.edu: the following ensures that even if we are clicking on
               // a garray, we should still be able to get the canvas properties
                || (y && pd_class(&y->g_pd) == garray_class)
               /*&& !canvas_isabstraction( ((t_glist*)y) )*/ );
    canopen = (y && zgetfn(&y->g_pd, gensym("menu-open")));
    /* we add an extra check for scalars to enable the "Open" button
       if they happen to have a canvas field inside them. */
    if (y && pd_class(&y->g_pd) == scalar_class)
    {
        if (scalar_getcanvasfield((t_scalar *)y))
            scalar_has_canvas = 1;
        canopen = scalar_has_canvas;
    }
    if (y || x->gl_editor->e_selection)
    {
        isobject = 1;
    }
    else isobject = 0;
    if (x->gl_owner && ((t_gobj *)x)->g_pd == array_define_class)
    {
        //fprintf(stderr,"owner=%s\n", ((t_gobj *)x)->g_pd->c_name->s_name);
        // special case: we are inside an array define and should not have
        // access to any options, so we disable them all
        // LATER: consider enabling help and perhaps even limited properties...
        return;
    }
    /* saveas option, only if it's a canvas and it isn't an abstraction */
    cansaveas = (canopen && pd_class(&y->g_pd) == canvas_class &&
                    !canvas_isabstraction((t_canvas *)y));
    /* or if it is the background of a subpatch */
    cansaveas = (cansaveas || (!y && canvas_getrootfor(x) != x &&
                    !canvas_isabstraction((t_canvas *)x)));
    // ico@vt.edu 2022-12-18: avoid creating help for the comment object
    // in the K12 mode, to avoid users to dig into non-K12 help files
    if (sys_k12_mode == 1 && pd_class(&y->g_pd) == text_class && 
        ((t_text *)y)->te_type == T_TEXT) {
        post("ignoring right-click in K12 mode for a comment object");
        return;
    }
    gui_vmess("gui_canvas_popup", "xiiiiii",
        x,
        xpos,
        ypos,
        canprop,
        canopen,
        cansaveas,
        isobject);
    // ico 2021-11-21: revert cursor to default in case we are above
    // an object (e.g. array) that has changed the cursor appearance
    canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
}

/* ----  editors -- perhaps this and "vis" should go to g_editor.c ------- */

static t_editor *editor_new(t_glist *owner)
{
    char buf[40];
    t_editor *x = (t_editor *)getbytes(sizeof(*x));
    x->e_connectbuf = binbuf_new();
    x->e_deleted = binbuf_new();
    x->e_glist = owner;
    sprintf(buf, "x%.6zx", (t_uint)owner);
    x->e_guiconnect = guiconnect_new(&owner->gl_pd, gensym(buf));
    x->gl_magic_glass = magicGlass_new(owner);
    x->canvas_cnct_inlet_tag[0] = 0;
    x->canvas_cnct_outlet_tag[0] = 0;
    x->exclusive = 0;
    return (x);
}

static void editor_free(t_editor *x, t_glist *y)
{
    glist_noselect(y);
    guiconnect_notarget(x->e_guiconnect, 1000);
    binbuf_free(x->e_connectbuf);
    binbuf_free(x->e_deleted);

    if (x->gl_magic_glass)
    {
          //magicGlass_free(x->gl_magic_glass);
        pd_free(&x->gl_magic_glass->x_obj.te_g.g_pd);
        //x->gl_magic_glass = NULL;
    }

    freebytes((void *)x, sizeof(*x));
}

    /* recursively create or destroy all editors of a glist and its 
    sub-glists, as long as they aren't toplevels. */
void canvas_create_editor(t_glist *x)
{
    //fprintf(stderr,"create_editor %zx\n", x);
    t_gobj *y;
    t_object *ob;
    if (!x->gl_editor)
    {
        x->gl_editor = editor_new(x);
        for (y = x->gl_list; y; y = y->g_next)
            if (ob = pd_checkobject(&y->g_pd))
                rtext_new(x, ob);
    }
}

void canvas_destroy_editor(t_glist *x)
{
    //fprintf(stderr,"destroy_editor %zx\n", x);
    t_gobj *y;
    t_object *ob;
    glist_noselect(x);
    if (x->gl_editor)
    {
        if (x->gl_list)
        {
            for (y = x->gl_list; y; y = y->g_next)
                if (ob = pd_checkobject(&y->g_pd))
                    rtext_free(glist_findrtext(x, ob));
        }
        //if (x->gl_editor) {
        editor_free(x->gl_editor, x);
        x->gl_editor = 0;
        //}
    }
}


// void canvas_reflecttitle(t_canvas *x);
// This should replace canvas_reflecttitle above
extern void canvas_args_to_string(char *namebuf, t_canvas *x);

void canvas_map(t_canvas *x, t_floatarg f);

    /* we call this when we want the window to become visible, mapped, and
    in front of all windows; or with "f" zero, when we want to get rid of
    the window. */
//extern t_array *garray_getarray(t_garray *x);
//extern void garray_fittograph(t_garray *x, int n, int flag);
//extern t_rtext *glist_findrtext(t_glist *gl, t_text *who);
//extern void rtext_gettext(t_rtext *x, char **buf, int *bufsize);

// ico@vt.edu 2020-08-24: update initial menu settings
void canvas_init_menu(t_canvas *x)
{   
    //post("g_editor.c canvas_init_menu %d", x->gl_font);
    // ico@vt.edu 2020-08-24: we now need this to init the menu font size
    gui_vmess("gui_menu_font_set_initial_size", "xi", x, x->gl_font);
    // ico@vt.edu 2022-11-30: check if this or a parent canvas
    // is not editable (parents' editability affects children
    // canvases) and store the value in the editable variable
    int editable = x->gl_editable;
    t_canvas *parent = x->gl_owner;
    while (parent) {
        if (!parent->gl_editable)
        {
            editable = 0;
            break;
        }
        parent = parent->gl_owner;
    }
    gui_vmess("canvas_menu_set_editable", "xi", x, editable);
}

// ico@vt.edu 2022-06-18: exposing class from the x_array.c
// for the canvas_click below, so that we can toggle off
// edit and inspector options on the gui side.
extern t_class *array_define_class;

extern int sys_snaptogrid; /* whether we are snapping to grid or not */
extern int sys_gridsize;

void canvas_vis(t_canvas *x, t_floatarg f)
{
    //fprintf(stderr,"canvas_vis .x%zx %f\n", (t_int)x, f);
    char geobuf[MAXPDSTRING];
    char argsbuf[MAXPDSTRING];
    sprintf(geobuf, "+%d+%d",
        (int)(x->gl_screenx1), (int)(x->gl_screeny1));

    t_gobj *g;
    t_int properties;

    int flag = (f != 0);
    if (flag)
    {
        /* If a subpatch/abstraction has GOP/gl_isgraph set, then it will have
         * a gl_editor already, if its not, it will not have a gl_editor.
         * canvas_create_editor(x) checks if a gl_editor is already created,
         * so its ok to run it on a canvas that already has a gl_editor. */
        if (x->gl_editor && x->gl_havewindow && glist_isvisible(x))
        {           /* just put us in front */
            //fprintf(stderr,"existing\n");
            //sys_vgui("raise .x%zx\n", x);
            //sys_vgui("focus .x%zx.c\n", x);
            //sys_vgui("wm deiconify .x%zx\n", x);  
            gui_vmess("gui_raise_window", "x", x);
        }
        else
        {
            // From glist_menu_open found in g_canvas.c to allow for
            // vis scripting of GOP-enabled abstractions
            if (glist_isvisible(x))
            {
                if (!glist_istoplevel(x))
                {
                    t_glist *gl2 = x->gl_owner;
                    if (gl2) //changed from !gl2
                        //bug("glist_menu_open"); /* shouldn't happen but not dangerous */
                    //else
                    {
                        /* erase ourself in parent window */
                        gobj_vis(&x->gl_gobj, gl2, 0);
                        /* get rid of our editor (and subeditors) */
                        if (x->gl_editor)
                            canvas_destroy_editor(x);
                        x->gl_havewindow = 1;
                        /* redraw ourself in parent window (blanked out this time) */
                        gobj_vis(&x->gl_gobj, gl2, 1);
                    }
                }
                else
                {
                    sys_vgui("focus .x%zx\n", (t_uint)x);
                }
            }
            else
            {
                if (x->gl_editor)
                canvas_destroy_editor(x);
            }
            //fprintf(stderr,"new\n");
            /* ico@vt.edu: here we use hasarray only for toplevel garrays
               because they require check_config every time resize is invoked.
               We may need to expand this to include scalars, as well. */
            canvas_create_editor(x);
            canvas_args_to_string(argsbuf, x);
            /* ico@vt.edu 2022-12-07: added check for whether the canvas is
               blank, which we use to resize window as needed for the k12 mode
            */
            int isblank = 0;
            //post("k12=%d glist?=%d glist_g_next=%d",
            //    sys_k12_mode, (x->gl_list ? 1 : 0),
            //(x->gl_list && x->gl_list->g_next ? 1 : 0));
            if (x->gl_list && !x->gl_list->g_next)
                isblank = 1;
            gui_vmess("gui_canvas_new", "xiisiiiissiiiiiis",
                x,
                (int)(x->gl_screenx2 - x->gl_screenx1),
                (int)(x->gl_screeny2 - x->gl_screeny1),
                geobuf,
                sys_snaptogrid,
		        sys_gridsize,
                x->gl_zoom,
                x->gl_edit,
                x->gl_name->s_name,
                canvas_getdir(x)->s_name,
                x->gl_dirty,
                (x->gl_dirties > 1 ?
                    (x->gl_dirty ? 2 : 1)
                    : (x->gl_dirties ? !x->gl_dirty : 0)),
                x->gl_noscroll,
                x->gl_nomenu,
                canvas_hasarray(x) + canvas_hastoplevelscalar(x) +
                    (x->gl_obj.ob_pd == array_define_class ? 1 : 0),
                isblank, // is this a blank canvas?
                argsbuf);

            /* It looks like this font size call is no longer needed,
               but I'm not sure why it was needed in the first place... */
            //sys_vgui("pdtk_canvas_set_font .x%zx %d\n", x, x->gl_font);
            //canvas_reflecttitle(x);
            x->gl_havewindow = 1;

            /* We can't update the scrollbars here, because we have to wait
               for the canvas window to load before anything else can happen.
               So we just call canvas_getscroll in the GUI after the window
               finishes loading. I'm not sure if there's an ulterior motive
               to this scrollbar_update here-- possibly related to graphs-- 
               so let's keep it here for reference in case we run into a
               but later. */
            //scrollbar_update(x);
            canvas_updatewindowlist();
        }
    }
    else    /* make invisible */
    {
        int i;
        t_canvas *x2;
        if (!x->gl_havewindow)
        {
                /* bug workaround -- a graph in a visible patch gets "invised"
                when the patch is closed, and must lose the editor here.  It's
                probably not the natural place to do this.  Other cases like
                subpatches fall here too but don'd need the editor freed, so
                we check if it exists. */
            if (x->gl_editor)
                canvas_destroy_editor(x);
            return;
        }
        /* can't imagine why we're updating the scrollbar on a canvas
           that's about to be unvis'd. Anyhow, if there happens to be some
           weird scrollbar bug try uncommenting the following line (and please
           documented why this has to be here if that's the case). */
        //scrollbar_update(x);
        glist_noselect(x);
        if (glist_isvisible(x))
            canvas_map(x, 0);
        canvas_destroy_editor(x);
        gui_vmess("gui_window_close", "x", x);
        // delete properties windows of objects in the patcher we're closing
        g = x->gl_list;
        while (g)
        {
            properties = gfxstub_haveproperties((void *)g);
            if (properties)
            {
                //sys_vgui("destroy .gfxstub%zx\n", properties);
                gfxstub_deleteforkey((void *)g);
            }
            g = g->g_next;
        }
        // now check if canvas has its properties open and
        // if the canvas is not gop-enabled or its parent is not visible
        // close its properties
        if (!x->gl_isgraph || x->gl_owner && !glist_isvisible(x->gl_owner))
        {
            properties = gfxstub_haveproperties((void *)x);
            if (properties)
            {
                //sys_vgui("destroy .gfxstub%zx\n", properties);
                gfxstub_deleteforkey((void *)x);
            }
        }
        
        for (i = 1, x2 = x; x2; x2 = x2->gl_next, i++)
            ;
        //sys_vgui(".mbar.find delete %d\n", i);
            /* if we're a graph on our parent, and if the parent exists
               and is visible, show ourselves on parent. */
        if (glist_isgraph(x) && x->gl_owner)
        {
            t_glist *gl2 = x->gl_owner;
            if (glist_isvisible(gl2))
                gobj_vis(&x->gl_gobj, gl2, 0);
            x->gl_havewindow = 0;
            if (glist_isvisible(gl2))
                gobj_vis(&x->gl_gobj, gl2, 1);
            // ico@vt.edu 2022-08-07: redraw parent window to honor correct
            // ordering of objects. Otherwise the redrawn GOP will be shown
            // in front of other objects.
            // TODO: his can be removed later when we implement correct
            // GOP drawing.
            glist_redraw(gl2);
        }
        else x->gl_havewindow = 0;
        canvas_updatewindowlist();
    }
}

    /* set a canvas up as a graph-on-parent.  Set reasonable defaults for
    any missing parameters and redraw things if necessary. */
void canvas_setgraph(t_glist *x, int flag, int nogoprect)
{
    //fprintf(stderr,"flag=%d\n",flag);
    if (!flag && glist_isgraph(x))
    {
        int hadeditor = (x->gl_editor != 0);
        if (x->gl_owner && !x->gl_loading && glist_isvisible(x->gl_owner))
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        if (hadeditor && !glist_isvisible(x))
            canvas_destroy_editor(x);
        x->gl_isgraph = 0;
        x->gl_hidetext = 0;
        if (x->gl_owner && !x->gl_loading && glist_isvisible(x->gl_owner))
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        }
        x->gl_goprect = 0;
    }
    else if (flag)
    {
        if (x->gl_pixwidth <= 0)
            x->gl_pixwidth = GLIST_DEFGRAPHWIDTH;

        if (x->gl_pixheight <= 0)
            x->gl_pixheight = GLIST_DEFGRAPHHEIGHT;

        if (x->gl_owner && !x->gl_loading && glist_isvisible(x->gl_owner))
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        }
        x->gl_isgraph = 1;
        x->gl_hidetext = !(!(flag&2));

        // check if we have array inside GOP, if so,
        // make sure hidetext is always hidden no matter what
        int hasarray = canvas_hasarray(x);
        if (hasarray) x->gl_hidetext = 1;

        if (!nogoprect && !x->gl_goprect && !hasarray)
        {
            // Ivica Ico Bukvic 5/16/10 <ico@bukvic.net>
            // this draws gop immediately when enabled
            x->gl_goprect = 1;
        }
        if (glist_isvisible(x) && x->gl_goprect)
        {
            glist_redraw(x);
        }
        if (x->gl_owner && !x->gl_loading && glist_isvisible(x->gl_owner))
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        }
    }
}

int garray_properties(t_garray *x, t_symbol **gfxstubp, t_symbol **namep,
    int *sizep, int *flagsp, t_symbol **fillp, t_symbol **outlinep);

    /* tell GUI to create a properties dialog on the canvas.  We tell
    the user the negative of the "pixel" y scale to make it appear to grow
    naturally upward, whereas pixels grow downward. */
void canvas_properties(t_gobj *z, t_glist *dummy)
{
    t_glist *x = (t_glist *)z;
    t_gobj *y;
    //char graphbuf[200];
    char *gfx_tag;

    gfx_tag = gfxstub_new2(&x->gl_pd, x);

    /* We need to go through and delete any
       gfxstubs for the arrays in this glist.
       Otherwise we could get a message to the
       GUI in the middle of our properties
       message below.  This is needed because
       the array properties just share the
       same window with the canvas properties.
    */
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == garray_class) 
            gfxstub_deleteforkey((t_garray *)y);

    gui_start_vmess("gui_canvas_dialog", "s", gfx_tag);
    gui_start_array(); /* Main array for nested arrays of attributes */

    gui_start_array(); /* Nested array for canvas attributes */
    if (glist_isgraph(x) != 0)
    {
        //sprintf(graphbuf,
        //    "pdtk_canvas_dialog %%s %g %g %d %g %g %g %g %d %d %d %d\n",
        //        0., 0.,
        //        glist_isgraph(x) ,//1,
        //        x->gl_x1, x->gl_y1, x->gl_x2, x->gl_y2, 
        //        (int)x->gl_pixwidth, (int)x->gl_pixheight,
        //        (int)x->gl_xmargin, (int)x->gl_ymargin);
        gui_s("x_scale");  gui_f(0.);
        gui_s("y_scale");  gui_f(0.);
        gui_s("display_flags"); gui_i(glist_isgraph(x));
        gui_s("x1");       gui_f(x->gl_x1);
        gui_s("y1");       gui_f(x->gl_y1);
        gui_s("x2");       gui_f(x->gl_x2);
        gui_s("y2");       gui_f(x->gl_y2);
        gui_s("x_pix");    gui_i((int)x->gl_pixwidth);
        gui_s("y_pix");    gui_i((int)x->gl_pixheight);
        gui_s("x_margin"); gui_i((int)x->gl_xmargin);
        gui_s("y_margin"); gui_i((int)x->gl_ymargin);
        gui_s("no_scroll");   gui_i(x->gl_noscroll);
        gui_s("no_menu");     gui_i(x->gl_nomenu);
        gui_s("gopspill");     gui_i(x->gl_gopspill);
    }
    else
    {
        //sprintf(graphbuf,
        //    "pdtk_canvas_dialog %%s %g %g %d %g %g %g %g %d %d %d %d\n",
        //        glist_dpixtodx(x, 1), -glist_dpixtody(x, 1),
        //        0,
        //        0., -1., 1., 1., 
        //        (int)x->gl_pixwidth, (int)x->gl_pixheight,
        //        (int)x->gl_xmargin, (int)x->gl_ymargin);
        gui_s("x_scale");  gui_f(glist_dpixtodx(x, 1));
        gui_s("y_scale");  gui_f(-glist_dpixtody(x, 1));
        gui_s("display_flags"); gui_i(0);
        gui_s("x1");       gui_f(0.);
        gui_s("y1");       gui_f(-1.);
        gui_s("x2");       gui_f(1.);
        gui_s("y2");       gui_f(1.);
        gui_s("x_pix");    gui_i((int)x->gl_pixwidth);
        gui_s("y_pix");    gui_i((int)x->gl_pixheight);
        gui_s("x_margin"); gui_i((int)x->gl_xmargin);
        gui_s("y_margin"); gui_i((int)x->gl_ymargin);
        gui_s("no_scroll");   gui_i(x->gl_noscroll);
        gui_s("no_menu");     gui_i(x->gl_nomenu);
        gui_s("gopspill");     gui_i(x->gl_gopspill);
    }
    //gfxstub_new(&x->gl_pd, x, graphbuf);

    gui_end_array(); /* end of nested array for canvas attributes */

        /* if any arrays are in the graph, pull out their attributes */
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (pd_class(&y->g_pd) == garray_class) 
        {
            t_symbol *gfxstub, *name, *fill, *outline;
            int size, flags;
            /* garray_properties can fail to find an array, so we won't
               send props if it has a return value of zero */
            if (garray_properties((t_garray *)y, &gfxstub, &name, &size, &flags,
                &fill, &outline))
            {
                gui_start_array(); /* inner array for this array's attributes */
                gui_s("array_gfxstub"); gui_s(gfxstub->s_name);
                gui_s("array_name"); gui_s(name->s_name);
                gui_s("array_size"); gui_i(size);
                gui_s("array_flags"); gui_i(flags);
                gui_s("array_fill"); gui_s(fill->s_name);
                gui_s("array_outline"); gui_s(outline->s_name);
                gui_end_array();
            }
        }
    }
    gui_end_array();
    gui_end_vmess();
}

    /* called from the gui when "OK" is selected on the canvas properties
        dialog.  Again we negate "y" scale. */
static void canvas_donecanvasdialog(t_glist *x,
    t_symbol *s, int argc, t_atom *argv)
{
    //fprintf(stderr,"canvas_donecanvasdialog\n");
    t_float xperpix, yperpix, x1, y1, x2, y2, xpix, ypix, xmargin, ymargin;
    int rx1=0, ry1=0, rx2=0, ry2=0; //for getrect
    int graphme;

    xperpix = atom_getfloatarg(0, argc, argv);
    yperpix = atom_getfloatarg(1, argc, argv);
    graphme = (int)(atom_getfloatarg(2, argc, argv));
    //fprintf(stderr,"graphme=%d\n", graphme);
    x1 = atom_getfloatarg(3, argc, argv);
    y1 = atom_getfloatarg(4, argc, argv);
    x2 = atom_getfloatarg(5, argc, argv);
    y2 = atom_getfloatarg(6, argc, argv);
    xpix = atom_getfloatarg(7, argc, argv);
    ypix = atom_getfloatarg(8, argc, argv);
    xmargin = atom_getfloatarg(9, argc, argv);
    ymargin = atom_getfloatarg(10, argc, argv);

    pd_vmess(&x->gl_pd, gensym("scroll"), "f",
        atom_getfloatarg(11, argc, argv));
    x->gl_nomenu = atom_getintarg(12, argc, argv);
    x->gl_gopspill = atom_getintarg(13, argc, argv);

    /* parent windows are treated differently than applies to
       individual objects */
    if (glist_getcanvas(x) != x && !canvas_isabstraction(x))
    {
        canvas_apply_setundo(glist_getcanvas(x), (t_gobj *)x);
    }
    else /*if (x1!=x->gl_x1 || x2!=x->gl_x2 || y1!=x->gl_y1 || y2!=x->gl_y2 ||
            graphme!=(x->gl_isgraph+2*x->gl_hidetext) ||
            x->gl_pixwidth!=xpix || x->gl_pixheight!=ypix ||
            x->gl_xmargin!=xmargin || x->gl_ymargin!=ymargin) {*/
    {    
        //canvas_canvas_setundo(x);
        canvas_undo_add(x, 8, "apply", canvas_undo_set_canvas(x));
        //fprintf(stderr,"canvas_apply_undo\n");
    }

    x->gl_pixwidth = xpix;
    x->gl_pixheight = ypix;
    x->gl_xmargin = xmargin;
    x->gl_ymargin = ymargin;

    yperpix = -yperpix;
    if (xperpix == 0)
        xperpix = 1;
    if (yperpix == 0)
        yperpix = 1;

    // check if we have array inside GOP, if so,
    // make sure GOP/hidetext is always enabled no matter what
    int hasarray = canvas_hasarray(x);
    if (hasarray && graphme != 3)
    {
        graphme = 3; //gop flag + bit-shifted hidetext
        post("Array graphs cannot have their 'graph on parent' "
             "or 'hide object name and arguments' options disabled");
    }
    if (graphme)
    {
        if (x1 != x2)
            x->gl_x1 = x1, x->gl_x2 = x2;
        else x->gl_x1 = 0, x->gl_x2 = 1;
        if (y1 != y2)
            x->gl_y1 = y1, x->gl_y2 = y2;
        else x->gl_y1 = 0, x->gl_y2 = 1;
    }
    else
    {
        if (xperpix > 0)
        {
            x->gl_x1 = 0;
            x->gl_x2 = xperpix;
        }
        else
        {
            x->gl_x1 = -xperpix * (x->gl_screenx2 - x->gl_screenx1);
            x->gl_x2 = x->gl_x1 + xperpix;
        }
        if (yperpix > 0)
        {
            x->gl_y1 = 0;
            x->gl_y2 = yperpix;
        }
        else
        {
            x->gl_y1 = -yperpix * (x->gl_screeny2 - x->gl_screeny1);
            x->gl_y2 = x->gl_y1 + yperpix;
        }
    }

        /* LATER avoid doing 2 redraws here (possibly one inside setgraph) */
    canvas_setgraph(x, graphme, 0);
    canvas_dirty(x, 1);

    // we're drawn at this point

    // make sure gop is never smaller than its text
    // if one wants smaller gop window, make sure to disable text
    if (x->gl_isgraph && !x->gl_hidetext && x->gl_owner)
    {
        //fprintf(stderr, "check size\n");
        gobj_getrect((t_gobj*)x, x->gl_owner, &rx1, &ry1, &rx2, &ry2);
        //fprintf(stderr,"%d %d %d %d\n", rx1, rx2, ry1, ry2);
        if (rx2-rx1 > x->gl_pixwidth)
        {
            x->gl_pixwidth = rx2-rx1;
            //fprintf(stderr,"change width\n");
        }
        if (ry2-ry1 > x->gl_pixheight)
        {
            x->gl_pixheight = ry2-ry1;
            //fprintf(stderr,"change height\n");
        }
    }

    if (x->gl_havewindow)
    {
        //fprintf(stderr,"donecanvasdialog canvas_redraw\n");
        canvas_redraw(x);
    }
    else if (x->gl_owner && glist_isvisible(x->gl_owner))
    {
        glist_noselect(x);
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        if (gobj_shouldvis(&x->gl_gobj, x->gl_owner))
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            //fprintf(stderr,"yes\n");
        }
        else
            canvas_redraw(glist_getcanvas(x->gl_owner));
        //canvas_redraw(x->gl_owner);
    }
    // ico@bukvic.net 100518 update scrollbars when
    // GOP potentially exceeds window size
    t_canvas *canvas=(t_canvas *)glist_getcanvas(x);
    //if gop is being disabled go one level up (if u can)
    if (!graphme && canvas->gl_owner) canvas=canvas->gl_owner;
    scrollbar_update(x);
}

/* called by undo/redo arrange and done_canvas_popup. only done_canvas_popup
   checks if it is a valid action and activates undo option */
static void canvas_doarrange(t_canvas *x, t_float which, t_gobj *oldy,
    t_gobj *oldy_prev, t_gobj *oldy_next)
{
    t_gobj *y_begin = x->gl_list;
    t_gobj *y_end = glist_nth(x, glist_getindex(x,0) - 1);

    if (which == 3) /* to front */
    {
        //put the object at the end of the cue
        y_end->g_next = oldy;
        oldy->g_next = NULL;

        // now fix links in the hole made in the list due to moving of the oldy
        // (we know there is oldy_next as y_end != oldy in canvas_done_popup)
        if (oldy_prev) //there is indeed more before the oldy position
            oldy_prev->g_next = oldy_next;
        else x->gl_list = oldy_next;

        // and finally redraw
        //fprintf(stderr,"raise\n");
        //canvas_raise_all_cords(x);
        gui_vmess("gui_raise", "xs", x, "selected");
    }
    if (which == 4) /* to back */
    {
        x->gl_list = oldy; //put it to the beginning of the cue
        oldy->g_next = y_begin; //make it point to the old beginning

        // now fix links in the hole made in the list due to moving of the oldy
        // (we know there is oldy_prev as y_begin != oldy in canvas_done_popup)
        if (oldy_next) //there is indeed more after oldy position
            oldy_prev->g_next = oldy_next;
        else oldy_prev->g_next = NULL; //oldy was the last in the cue

        // and finally redraw
        //fprintf(stderr,"lower\n");
        gui_vmess("gui_lower", "xs", x, "selected");
        //canvas_redraw(x);
    }
    canvas_dirty(x, 1);
    glob_preset_node_list_check_loc_and_update();
}

char *canvas_gethelpname(t_object *ob)
{
    if (ob->te_binbuf &&
        binbuf_getnatom(ob->te_binbuf) &&
        binbuf_getvec(ob->te_binbuf)->a_type == A_SYMBOL)
    {
        t_atom *a = binbuf_getvec(ob->te_binbuf);
        if (a->a_w.w_symbol == gensym("draw"))
            return "draw";
        else if (a->a_w.w_symbol == gensym("table"))
            return "table";
        else return (class_gethelpname(pd_class(&ob->te_pd)));
    }
    else
        return (class_gethelpname(pd_class(&ob->te_pd)));
}

    /* called from the gui when a popup menu comes back with "properties,"
        "open," or "help." */
    /* Ivica Ico Bukvic <ico@bukvic.net> 2010-11-17
       also added "To Front" and "To Back" */
void canvas_done_popup(t_canvas *x, t_float which, t_float xpos,
    t_float ypos)
{
    //fprintf(stderr,"x->gl_edit=%d\n", x->gl_edit);
    //fprintf(stderr,"canvas_done_pupup %zx\n", (t_int)x);
    char namebuf[FILENAME_MAX];
    t_gobj *y=NULL, *oldy=NULL, *oldy_prev=NULL, *oldy_next=NULL,
        *y_begin, *y_end=NULL;
    int x1, y1, x2, y2;

    // first deselect any objects that may be already selected
    // if doing action 3 or 4
    if (x->gl_edit)
    {
        if (which == 3 || which == 4)
        {
            if (x->gl_editor->e_selection && x->gl_editor->e_selection->sel_next)
                glist_noselect(x);
        }
        else glist_noselect(x);
    }

    // mark the beginning of the glist for front/back
    y_begin = x->gl_list;

    t_gobj *yclick = NULL;

    // ico 2023-08-17: move the enabling of editmode before
    // looking for a hitbox, so that the right hitbox is sought
    // for objects like mycanvas that have different hitboxes
    // depending on whether the patch is in editmode. this is
    // an issue due to an arguably broken way of using mycanvas'
    // selection handle as larger than the actual widget to serve
    // as a frame for all standardized help patches.
    // since we only got here because properties was enabled as
    // an option in the pop-up menu, which means, there was clearly
    // something under the cursor that has properties as an option,
    // we can go ahead and enable editmode here, rather than doing
    // so below in the properties if statement. that way, we always
    // get the optimal hitbox.

    if (which == 0 && !x->gl_edit)
        canvas_editmode(x, 1);

    ///if (which == 3 || which == 4) {
        // if no object has been selected for to-front/back action
    //if (!x->gl_editor->e_selection)
    //{
        //fprintf(stderr,"doing hitbox\n");
        for (y = x->gl_list; y; y = y->g_next)
        {
            if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
            {
                yclick = y;
            }
        }
        //if (yclick)
        //{ 
        //    y = yclick;
        //    if (!x->gl_edit)
        //        canvas_editmode(x, 1);
        //    if (!glist_isselected(x, y))
        //        glist_select(x, y);
        //}
    //}
    //}

    // this was a bogus/unsupported call for tofront/back--get me out of here!
    // we don't have to check for multiple objects being selected since we
    // did noselect above explicitly for cases 3 and 4 when detecting
    // more than one selected object
    // 20140902 Ico: I don't think we need this...
    if ((which == 3 || which == 4) && !yclick)
    {
        post("Popup action could not be performed because no object "
             "was found under the cursor...");
        return;
    }
    
    if (yclick)
    {
        for (y = x->gl_list; y; y = y->g_next)
        {
            if (which == 3 || which == 4) /* to-front or to-back */
            {
                // if next one is the one selected for moving
                if (y->g_next && yclick == y->g_next)
                {
                    oldy_prev = y;
                    oldy = y->g_next;
                    //if there is more after the selected object
                    if (oldy->g_next)
                        oldy_next = oldy->g_next;
                }
                else if (yclick == y && oldy == NULL)
                {
                    //selected obj is the first in the cue
                    oldy = y;
                    if (y->g_next)
                        oldy_next = y->g_next;
                }
            }
            else if (y == yclick)
            {
                if (which == 0)     /* properties */
                {
                    if (!class_getpropertiesfn(pd_class(&y->g_pd)))
                        continue;
                    else {
                        // ico 2023-08-17: disabled, see the beginning of
                        // this function
                        //if (!x->gl_edit)
                        //    canvas_editmode(x, 1);
                        if (!glist_isselected(x, y))
                            glist_select(x, y);
                        (*class_getpropertiesfn(pd_class(&y->g_pd)))(y, x);
                        return;
                    }
                }
                else if (which == 1)    /* open */
                {
                    //fprintf(stderr,"OPEN\n");
                    if (!zgetfn(&y->g_pd, gensym("menu-open")))
                        continue;
                    vmess(&y->g_pd, gensym("menu-open"), "");
                    return;
                }
                else if(which == 5)    /* saveas */
                {
                    t_abstracthandler *ah = abstracthandler_new();
                    ah->tarjet = (t_canvas *)y;
                    ah->dialog = x;

                    char buf[MAXPDSTRING];
                    sprintf(buf, "%s/%s.pd",
                        canvas_getdir(canvas_getrootfor((t_canvas *)y))->s_name,
                        ((t_canvas *)y)->gl_name->s_name);

                    gui_vmess("gui_savepanel", "xss",
                        x,
                        ah->sym->s_name,
                        buf);

                    return;
                }
                else if (which == 2)   /* help */
                {
                    char *dir;
                    if (pd_class(&y->g_pd) == canvas_class &&
                        canvas_isabstraction((t_canvas *)y))
                    {
                        t_object *ob = (t_object *)y;
                        int ac = binbuf_getnatom(ob->te_binbuf);
                        t_atom *av = binbuf_getvec(ob->te_binbuf);
                        if (ac < 1)
                            return;
                        atom_string(av, namebuf, FILENAME_MAX);
                        dir = canvas_getdir((t_canvas *)y)->s_name;
                    }
                    else
                    {
                        char *obname = (pd_class(&y->g_pd) == canvas_class) ?
                            canvas_gethelpname((t_object *)y) :
                            class_gethelpname(pd_class(&y->g_pd));
                        strncpy(namebuf, obname,
                            FILENAME_MAX-1);
                        namebuf[FILENAME_MAX-1] = 0;
                        dir = class_gethelpdir(pd_class(&y->g_pd));
                    }
                    //post("canvas_done_popup help namebuf=<%s>", namebuf);
                    if (strlen(namebuf) < 4 ||
                        strcmp(namebuf + strlen(namebuf) - 3, ".pd"))
                            strcat(namebuf, ".pd");
                    open_via_helppath(namebuf, dir);
                    return;
                }
            }
        }
    }

    y_end = glist_nth(x, glist_getindex(x,0) - 1);

    if (which == 3 || which == 4)
    {
        if (!x->gl_edit)
            canvas_editmode(x, 1);
        if (!glist_isselected(x, yclick))
            glist_select(x, yclick);
    }

    if (which == 3 && y_end != oldy) /* to front */
    {
        /* create appropriate undo action */
        //canvas_arrange_setundo(x, oldy, 1);
        canvas_undo_add(x, 7, "arrange", canvas_undo_set_arrange(x, oldy, 1));

        canvas_doarrange(x, which, oldy, oldy_prev, oldy_next);
    }
    if (which == 4 && y_begin != oldy) /* to back */
    {
        /* create appropriate undo action */
        //canvas_arrange_setundo(x, oldy, 0);
        canvas_undo_add(x, 7, "arrange", canvas_undo_set_arrange(x, oldy, 0));

        canvas_doarrange(x, which, oldy, oldy_prev, oldy_next);
    }
    if (which == 0)
    {
        if (!x->gl_edit)
            canvas_editmode(x, 1);
        canvas_properties((t_gobj *)x, x);
    }
    else if (which == 2)
        open_via_helppath("intro.pd", canvas_getdir((t_canvas *)x)->s_name);

    if (which == 5)
    {
        t_abstracthandler *ah = abstracthandler_new();
        ah->tarjet = x;
        ah->dialog = x;

        char buf[MAXPDSTRING];
        sprintf(buf, "%s/%s.pd", canvas_getdir(canvas_getrootfor(x))->s_name,
            ((t_canvas *)x)->gl_name->s_name);

        gui_vmess("gui_savepanel", "xss",
            x,
            ah->sym->s_name,
            buf);
    }
}

extern t_class *my_canvas_class; // for ignoring runtime clicks and resizing

/* For triggering text_widgetbehavior objects, graphs, atom/dropdown, iemgui and
   comment resizing */
static int text_resizing_hotspot(t_canvas *x, t_object *ob, int xpos, int ypos,
    int x1, int y1, int x2, int y2, int *typep)
{
    /* No resizing anchor in k12 mode, plus sanity checks */
    if (sys_k12_mode || !ob || x->gl_editor->e_textedfor || xpos > x2 ||
        ypos > y2)
        return 0;
    *typep = 0;
        /* objects that can only be resized along x-axis: regular text
           objects, atom boxes, and canvas without gop checked */
    if (ob->te_pd->c_wb == &text_widgetbehavior ||
        ob->te_type == T_ATOM ||
        (ob->ob_pd == canvas_class && !((t_canvas *)ob)->gl_isgraph))
    {
        if (xpos >= x2 - 4 && ypos < y2 - 4 && ypos > y1 + 4)
        {
            *typep = 1;
            return CURSOR_EDITMODE_RESIZE_X;
        }
    }

        /* gop canvases, gop red rectangle, scope, grid, iemguis except [cnv] and custom
           3rd party iemgui-based objects, such as ggee/image that have custom resizing
           behavior like mycanvas */
    if ((ob->te_iemgui == 1 && ob->ob_pd != my_canvas_class) ||
        (ob->ob_pd == canvas_class && ((t_canvas *)ob)->gl_isgraph) ||
        ob->ob_pd->c_name == gensym("Scope~") ||
        ob->ob_pd->c_name == gensym("grid"))
    {
            /* stay out of the way of outlet in the bottom right-hand corner */
        int offset = (obj_noutlets(ob) > 1) ? -IOHOTSPOT : 0;
            /* We want to disable horiz and vert anchors for [bng], [tgl],
               [hradio] and [vradio]. These widgets only have one
               dimension that can be resized-- the other is handled
               automatically. */
        int can_resize_x = (ob->ob_pd->c_name != gensym("bng") &&
            ob->ob_pd->c_name != gensym("tgl") &&
            ob->ob_pd->c_name != gensym("hradio") &&
            ob->ob_pd->c_name != gensym("vradio"));

        int can_resize_y = can_resize_x &&
            (ob->ob_pd->c_name != gensym("vsl") || x2 - x1 > 15) &&
            (ob->ob_pd->c_name != gensym("hsl") || x2 - x1 > 15);

        //post("compare right=%d leftmost-outlet-right-side=%d left=%d", \
        	x2-24, x1 + IOWIDTH + 6, x1);
        //post("ypos=%d y2-y1=%d", ypos, y2 - y1);
        //post("mouse=(%d,%d) box=(x:%d-%d,y:%d-%d) can_resize=(%d,%d)", \
        	xpos, ypos, x1, x2, y1, y2, can_resize_x, can_resize_y);

        if (can_resize_x &&
            xpos >= x2 - 5 && ypos <= y2 + offset - 10 && ypos > y1)
        {
        	//post("...x");
            return CURSOR_EDITMODE_RESIZE_X;
        }
        else if (xpos >= x2 - 5 && ypos <= y2 + offset && ypos > y2 + offset - 10)
        {
        	//post("...xy");
            return CURSOR_EDITMODE_RESIZE;
        }
        else if (can_resize_y &&
                 xpos > x1 + (ob->te_iemgui ||
                    obj_noutlets(ob) > 0 ? IOWIDTH + IOHOTSPOT : 0)  &&
                 ypos >= y2 + offset - 5 &&
                 ypos <= y2 + offset)
        {
        	//post("...y");
            return CURSOR_EDITMODE_RESIZE_Y;
        }
        else
        {
        	//post("...nothing");
            return 0;
        }
    }
    else
        return 0;
}

#define NOMOD 0
#define SHIFTMOD 1
#define CTRLMOD 2
#define ALTMOD 4
#define RIGHTCLICK 8

static double canvas_upclicktime;
static int canvas_upx, canvas_upy;
#define DCLICKINTERVAL 0.25

static int ctrl_runmode_warned;

// ico@vt.edu 2021-10-08: here we define these statically
// because they will be reused for the mouseup call
// where we redistribute calls to all objects that catch
// mouse motion, including mouse clicks while passing them
// also through
static int shiftmod = 0;
static int ctrlmod = 0;
static int altmod = 0;

static int snap_got_anchor;

    /* mouse click */
void canvas_doclick(t_canvas *x, int xpos, int ypos, int which,
    int mod, int doit)
{
    /* here we make global array_garray pointer defined in g_canvas.h
       point back to nothing--we use this pointer to pass information
       to array_motion so that we can update corresponding send when
       the array has been changed */
    array_garray = NULL;

    /* reset the snap_got_anchor variable so the the snap_to_grid feature
       can find its anchor object before it starts to displace a selection */
    snap_got_anchor = 0;
    //post("canvas_doclick %d %d | %d", xpos, ypos, doit);

    t_gobj *y;
    // instantiate total number of gobj in gl_list and a number of
    // passthrough objects, plus a counting variable
    int numgobj = 0, numptgobj = 0, i;

    int runmode, doublemod = 0, rightclick,
        in_text_resizing_hotspot, default_type;
    int x1=0, y1=0, x2=0, y2=0, clickreturned = 0, 
        passthroughclick = 0;
    t_gobj *yclick = NULL;
    t_object *ob;

    //fprintf(stderr,"MAIN canvas_doclick %d %d %d %d %d\n",
    //    xpos, ypos, which, mod, doit);
    
    if (!x->gl_editor)
    {
        bug("editor");
        return;
    }

    tooltip_erase(x);

    // used to debugging whether resized_gobj reverts to NULL
    // under all circumstances
    //post("resized_gobj=%lx", resized_gobj);
    
    // read key and mouse button states
    shiftmod = (mod & SHIFTMOD);
    ctrlmod = (mod & CTRLMOD);
    altmod = (mod & ALTMOD);
    runmode = (altmod || (!x->gl_edit));
    rightclick = (mod & RIGHTCLICK);

    // set global left mouse click variable
    if (!rightclick) glob_lmclick = doit;

    // return if user is connecting and holding shift (for multiconnect)
    if (x->gl_editor->e_onmotion == MA_CONNECT && glob_shift)
    {
        //fprintf(stderr,
        //    "MA_CONNECT + glob_shift--> mouse_doclick returning\n");
        return;
    }

    canvas_undo_already_set_move = 0;

    // if keyboard was grabbed, notify grabber and cancel the grab
    if (doit && x->gl_editor->e_grab && x->gl_editor->e_keyfn)
    {
        (* x->gl_editor->e_keyfn) (x->gl_editor->e_grab, 0);
        glist_grab(x, 0, 0, 0, 0, 0, 0, 0);
    }

    if (doit && !runmode && xpos == canvas_upx && ypos == canvas_upy &&
        sys_getrealtime() - canvas_upclicktime < DCLICKINTERVAL)
            doublemod = 1;
    x->gl_editor->e_lastmoved = 0;
    /* this was temporarily commented out on 5-23-2013 while fixing
       shift+click actions most likely I forgot to reenable it -- need
       to check for any regressions as this is needed to re-check
       scrollbar after something was created->startmotion->clicked
       to let go */
    if (doit)
    {
        // no matter what we do, we should reset the search when we click?
        // currently disabled
        //canvas_find_index1 = 0;
        //canvas_find_index2 = -1;
        //canvas_num_found = 0;
        //fprintf(stderr,"doit %d\n", x->gl_editor->e_onmotion);
        if (x->gl_editor->e_onmotion == MA_MOVE)
        {        
            //fprintf(stderr,"letting go of objects\n");
            t_selection *sel;
            for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            {
                if (sel && ((t_text *)sel->sel_what)->te_iemgui > 0)
                {
                    // iemgui exception to hide all handles that may interfere
                    // with the mouse cursor and its ability to move/deselect
                    // object(s) in question. Here we reinstate them once we've
                    // let go of the object(s)
                    gobj_select(sel->sel_what, x, 1);
                }
            }
            canvas_raise_all_cords(x);
            scrollbar_update(x);
        }
        if (x->gl_editor->e_onmotion != MA_SCROLL)
        {
            x->gl_editor->e_grab = 0;
            x->gl_editor->e_onmotion = MA_NONE;
        }
    }
    //post("click %d %d %d %d", xpos, ypos, which, mod);
    
    if (x->gl_editor->e_onmotion != MA_NONE)
    {
        //fprintf(stderr,"onmotion != MA_NONE\n");
        return;
    }

    x->gl_editor->e_xwas = xpos;
    x->gl_editor->e_ywas = ypos;
    //fprintf(stderr,"mouse %d %d\n", xpos, ypos);

    // if we are in runmode and it is not middle- or right-click
    if (runmode && !rightclick)
    {
        if (x->gl_list)
        {
            //post("...runmode && !rightclick");
            for (y = x->gl_list; y; y = y->g_next)
            {
                // ico@vt.edu 2021-11-12: here we need to check for
                // garray class since otherwise we are unable to click on
                // array points on a toplevel window (when opening the gop
                // it holds the array). this is because those points do
                // not return a valid ob. we also use this to update the
                // array_joc global variable. we can do this outside the
                // passthrough call since pd-l2ork does not allow mixing
                // of arrays and canvas objects.
                if(canvas_hasarray(x))
                {
                    // here we assign array_joc global variable, so that the
                    // array_doclick can be computed properly. we, however,
                    // use doit = 0, just to see if we are within the hitbox
                    // then we act on the topmost garray below after the
                    // passthrough call.
                    //array_joc = garray_joc((t_garray *)y);
                    if (gobj_click(y, x, xpos, ypos, shiftmod,
                        (altmod && (!x->gl_edit)) || ctrlmod, 0, 0))
                    {
                        yclick = y;
                    }
                }
                else if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
                {
                    // check if the object wants to be clicked
                    // (we pick the topmost clickable)
                    ob = pd_checkobject(&y->g_pd);
                    /*
                    post("...got hitbox ob?%d w_clickfn?%d iemgui=%d",
                        (ob ? 1 : 0), (y->g_pd->c_wb->w_clickfn ? 1 : 0),
                        ((t_text *)y)->te_iemgui
                    );
                    */
                    /* do not give clicks to comments or cnv during runtime */
                    /* ico@vt.edu 2021-10-08: also do not give clicks to the
                       ggee/image object if it is in click_mode 3 because that
                       one needs to be passed through below it, so we manually
                       acknowledge the click here without interrupting the flow */
                    //if (ob)
                    //    post("clicking on object with te_iemgui=%d",
                    //        ((t_text *)y)->te_iemgui);
                    if (!ob ||
                            (ob && y->g_pd->c_wb->w_clickfn && 
                            (ob->te_type != T_TEXT && ob->ob_pd != my_canvas_class &&
                            ((t_text *)y)->te_iemgui != 3))
                       )
                    {
                        yclick = y;
                        //post("...yclick is set to %lx %d <%s>", y,
                        //   ((t_text *)y)->te_iemgui, ob->ob_pd->c_name->s_name);
                    }
                }
            }
        }
        canvas_passthrough_click(x, xpos, ypos, doit);
        if (yclick)
        {
                clickreturned = gobj_click(yclick, x, xpos, ypos,
                    shiftmod, (altmod && (!x->gl_edit)) || ctrlmod,
                    0, doit);
                if (canvas_hasarray(x) && clickreturned)
                    clickreturned = CURSOR_RUNMODE_THICKEN; //CURSOR_EDITMODE_RESIZE_Y;
                //post("MAIN clicking %lx", yclick);
                //fprintf(stderr, "    MAIN clicking\n");
        }
        // if we are not clicking
        if (!doit)
        {
            //fprintf(stderr, "    MAIN not clicking\n");
            if (yclick || clickreturned)
            {
                //fprintf(stderr, "    MAIN cursor %d\n", clickreturned);
                canvas_setcursor(x, clickreturned);
            }
            else {
                //fprintf(stderr, "    MAIN cursor\n");
                canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
            }
        }
        return;
    }

        /* if in editmode click, fall here. */
        /* check you're in the rectangle */
    y = canvas_findhitbox(x, xpos, ypos, &x1, &y1, &x2, &y2);
        /* We've got a special case for handling the gop red rectangle. In that
           case we want all other click actions to take precedence, so we run
           the checks here first so we can keep the same conditional ordering
           we've had for ages. */
    if (y)
    {
        ob = pd_checkobject(&y->g_pd);
        in_text_resizing_hotspot = text_resizing_hotspot(x, ob, xpos, ypos,
            x1, y1, x2, y2, &default_type);
    }
    else
    {
        // ico@vt.edu 2021-11-13:
        // for testing for gop rect resize hotspots, we need to first
        // confirm that the redrect is drawn on the canvas we are currently
        // on. this is because if we are toplevel and the canvas only has
        // an array or scalars, there should be no redrect and therefore
        // no redrect visible to resize.
        if (x->gl_goprect)
            in_text_resizing_hotspot = text_resizing_hotspot(x, &x->gl_obj,
                xpos, ypos, x->gl_xmargin, x->gl_ymargin,
                x->gl_xmargin + x->gl_pixwidth,
                x->gl_ymargin + x->gl_pixheight, &default_type);
        else in_text_resizing_hotspot = 0;
    }

    // if we have located an object under the mouse
    if (y)
    {

        /* check for ctrlmod click and give a warning once in the console that
           the hotkey for temporary runmode has changed */
        if (!ctrl_runmode_warned && ctrlmod && !rightclick && doit) {
          post("\nwarning: The hotkey for temporary run mode has changed. "
               "Please press Alt instead of Ctrl to enable it.\n");
          ctrl_runmode_warned = 1;
        }

        // if we are right-clicking
        if (rightclick)
            canvas_rightclick(x, xpos, ypos, y);
        // if we are holding shift and we are not above an outlet
        else if (shiftmod &&
            x->gl_editor->canvas_cnct_outlet_tag[0] == 0)
        {
            // we are clicking and making a (de)selection
            // (only if we are not hovering above an outlet)
            if (doit)
            {
                if (glist_isselected(x, y))
                    glist_deselect(x, y);
                else glist_select(x, y);
            }
        }
        else
        {
            /* look for other stuff */
            int noutlet;
            int ninlet;
                /* resize?  only for "true" text boxes, canvases, iemguis,
                   and -- using an awful hack-- for the Scope~ and grid
                   objects by checking for the class name below.

                   One exception-- my_canvas. It has a weirdo interface
                   where the visual dimensions usually (i.e., by default)
                   extends well past the bounds of the bbox. For that reason
                   we have a virtual waterfall of conditionals flowing all
                   the way to the GUI just to handle resizing a rectangle.
                */

            // if we are inside a resizing hotspot of a text object...
            if (in_text_resizing_hotspot)
            {
                //post("in_text_resizing_hotspot...");
                // ...and we are clicking...
                if (doit)
                {
                    // ...select the object
                    //post("...clicking");
                    if (!glist_isselected(x, y) ||
                        x->gl_editor->e_selection->sel_next)
                    {
                        glist_noselect(x);
                        glist_select(x, y);
                    }
                    resized_gobj = y;

                    x->gl_editor->e_onmotion = MA_RESIZE;
                    x->gl_editor->e_xwas = x1;
                    x->gl_editor->e_ywas = y1;
                    x->gl_editor->e_xnew = xpos;
                    x->gl_editor->e_ynew = ypos;
                    /* For normal text objects/atom boxes, subpatches and
                       graphs we just go ahead and set an undo point here.
                       GUI objects have their own click callback where they
                       do this. */
                    int isgraph = (ob->ob_pd == canvas_class &&
                        ((t_canvas *)ob)->gl_isgraph);

                    if (default_type || isgraph)
                        canvas_undo_add(x, 6, "resize",
                            canvas_undo_set_apply(x, glist_getindex(x, y)));

                    /* Scalehandle callbacks */
                    if (isgraph)
                    {
                        t_scalehandle *sh = ((t_canvas *)ob)->x_handle;
                        /* Special case: we're abusing the value of h_scale
                           to differentiate between this case and the case
                           of clicking the red gop rectangle. */
                        sh->h_scale = 1;
                        pd_vmess(&sh->h_pd, gensym("_click"), "fff",
                            (t_float)in_text_resizing_hotspot,
                            (t_float)xpos, (t_float)ypos);
                    }
                    else if (ob->te_iemgui)
                    {
                        t_scalehandle *sh = ((t_iemgui *)ob)->x_handle;
                        pd_vmess(&sh->h_pd, gensym("_click"), "fff",
                            (t_float)in_text_resizing_hotspot,
                            (t_float)xpos, (t_float)ypos);
                    }
                    else if (!default_type)
                    {
                        /* Scope~ and grid */
                        pd_vmess(&ob->ob_pd, gensym("_click_for_resizing"),
                           "fff", (t_float)in_text_resizing_hotspot,
                           (t_float)xpos, (t_float)ypos);
                    }
                }
                // we are in the resize hotspot but are not clicking yet
                else
                {
                    canvas_setcursor(x, in_text_resizing_hotspot);
                    canvas_check_nlet_highlights(x);
                }
            }
            /* look for an outlet
                if object is valid, has outlets,
                and we are within the bottom area of an object
                ico@vt.edu: 2020-06-05 added expanded hotspot for
                nlets for easier pinpointing
            */
            else if (ob && (noutlet = obj_noutlets(ob)) &&
            	ypos >= y2-((y2-y1)/2 > IOHOTSPOT ? IOHOTSPOT : (y2-y1)/2-1))
            {
                int width = x2 - x1;
                int nout1 = (noutlet > 1 ? noutlet - 1 : 1);
                int closest = ((xpos-x1) * (nout1) + width/2)/width;
                int hotspot = x1 +
                    (width - IOWIDTH) * closest / (nout1);
                // if we are within the boundaries of an nlet
                /* ico@vt.edu: account for enlarged nlet when already
                   highlighted to make it easier to "hit" the nlet */
                //int enlarged = 0;
                //if (closest == last_outlet)
                //    enlarged = 5;
                //post("xpos=%d closest=%d noutlet=%d "
                //    "nout1=%d hotspot=%d IOWIDTH=%d enlarged=%d",
                //    xpos, closest, noutlet, nout1, hotspot, IOWIDTH, enlarged);
                // if we have found an outlet and are within its range...
                if (closest < noutlet &&
                    xpos >= (hotspot-IOHOTSPOT) && xpos <= (hotspot+IOWIDTH+IOHOTSPOT))
                {
                    //post("Outlet found...");
                    //...and we are clicking on it
                    if (doit)
                    {
                        //fprintf(stderr,"start connection\n");
                        int issignal = obj_issignaloutlet(ob, closest);
                        x->gl_editor->e_onmotion = MA_CONNECT;
                        x->gl_editor->e_xwas = hotspot + IOWIDTH / 2;
                        x->gl_editor->e_ywas = y2 - 2;
                        /* This repetition of args needs to be pruned below */
                        gui_vmess("gui_canvas_line", "xssiiiiiiiiiiii",
                            x,
                            "newcord",
                            (issignal ? "signal" : "control"),
                            sys_curved_cords,
                            hotspot + IOWIDTH / 2,
                            y2 - 2,
                            hotspot + IOWIDTH / 2,
                            y2 - 2,
                            hotspot + IOWIDTH / 2,
                            y2 - 2,
                            hotspot + IOWIDTH / 2,
                            y2 - 2,
                            hotspot + IOWIDTH / 2,
                            y2 - 2,
                            1);
                    }   
                    else
                    // jsarlo (...or, we are *not* clicking on it)
                    {
                        t_rtext *yr = glist_findrtext(x, (t_text *)&ob->ob_g);

                        // I guess this removes highlight from an outlet, but why?
                        // Perhaps just to make sure in case we were just hitting on
                        // another nlet in a previous update?
                        if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
                            canvas_nlet_conf(x,'o');
                        
                        // if we have found an object's rtext which we
                        // will use to tag the highlighted nlet accordingly
                        if (yr)
                        {
                            last_outlet_filter =
                                gobj_filter_highlight_behavior(
                                    (t_text *)&ob->ob_g);
                            // fill the outlet_tag name with hte detected outlet
                            sprintf(x->gl_editor->canvas_cnct_outlet_tag, 
                                "%so%d", rtext_gettag(yr), closest);
                            gui_vmess("gui_gobj_highlight_io", "xsi",
                                x,
                                x->gl_editor->canvas_cnct_outlet_tag,
                                1);

                            /* Might need a gui_vmess call here, but I haven't
                               seen where this code is called yet... */
                            //sys_vgui(".x%x.c raise %s\n",
                            //         x,
                            //         x->gl_editor->canvas_cnct_outlet_tag);
                            outlet_issignal = obj_issignaloutlet(ob,closest);
                            if (tooltips)
                            {
                                objtooltip = 1;
                                canvas_enteritem(x, xpos, ypos,
                                    x->gl_editor->canvas_cnct_outlet_tag);
                            }
                        }
                        // jsarlo (get rid of the cord inspector here since we are
                        // not on a cord)
                        if(x->gl_editor && x->gl_editor->gl_magic_glass)
                        {
                            magicGlass_unbind(x->gl_editor->gl_magic_glass);
                            magicGlass_hide(x->gl_editor->gl_magic_glass);
                        }
                        // end jsarlo
                        canvas_setcursor(x, CURSOR_EDITMODE_CONNECT);
                    }
                    // end jsarlo
                }
                // otherwise we are not hitting any outlet and are making sure
                // the previously highlighted outlets are not anymore highlighted
                else {
                    //post("Comb the Desert!");
                    canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
                    canvas_check_nlet_highlights(x);
                    if (doit)
                        goto nooutletafterall;
                }
            }
            /* look for an inlet (these are colored differently
                since they are not connectable)
                ico@vt.edu: 2020-06-05 added expanded hotspot for
                nlets for easier pinpointing
            */
            else if (ob && (ninlet = obj_ninlets(ob))
                && ypos <= y1+((y2-y1)/2 > IOHOTSPOT ? IOHOTSPOT : (y2-y1)/2-1))
            {
                canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
                int width = x2 - x1;
                int nin1 = (ninlet > 1 ? ninlet - 1 : 1);
                int closest = ((xpos-x1) * (nin1) + width/2)/width;
                int hotspot = x1 +
                    (width - IOWIDTH) * closest / (nin1);
                /* ico@vt.edu: account for enlarged nlet when already
                   highlighted to make it easier to "hit" the nlet */
                // if have found an inlet and are within its range...
                if (closest < ninlet &&
                    xpos >= (hotspot-IOHOTSPOT) && xpos <= (hotspot+IOWIDTH+IOHOTSPOT))
                {
                       t_rtext *yr = glist_findrtext(x, (t_text *)&ob->ob_g);

                    if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
                        canvas_nlet_conf(x,'i');

                    if (yr)
                    {
                        last_inlet_filter =
                            gobj_filter_highlight_behavior((t_text *)&ob->ob_g);
                        sprintf(x->gl_editor->canvas_cnct_inlet_tag, 
                            "%si%d", rtext_gettag(yr), closest);
                        gui_vmess("gui_gobj_highlight_io", "xsi",
                            x,
                            x->gl_editor->canvas_cnct_inlet_tag,
                            0);
                        inlet_issignal = obj_issignalinlet(ob,closest);
                        if (tooltips)
                        {
                            objtooltip = 1;
                            canvas_enteritem(x, xpos, ypos,
                                x->gl_editor->canvas_cnct_inlet_tag);
                        }
                    }
                }
                else
                {
                    if (x->gl_editor->e_onmotion != MA_CONNECT)
                    {
                        canvas_check_nlet_highlights(x);
                    }
                    if (doit)
                        goto nooutletafterall;
                }
            }
                /* not in an outlet; select and move */
            else if (doit)
            {
                    /* check if the box is being text edited */
                nooutletafterall:
                    /* otherwise select and drag to displace */
                if (!glist_isselected(x, y))
                {
                    //t_undo_redo_sel *buf =
                    //    (t_undo_redo_sel *)getbytes(sizeof(*buf));
                    //buf->u_undo =
                    //    (t_undo_sel *)canvas_undo_set_selection(x);

                    glist_noselect(x);
                    glist_select(x, y);
                    //buf->u_redo =
                    //    (t_undo_sel *)canvas_undo_set_selection(x);
                    //canvas_undo_add(x, 11, "selection", buf);
                }
                else
                {
                    canvas_check_nlet_highlights(x);
                }
                //toggle_moving = 1;
                //sys_vgui("pdtk_update_xy_tooltip .x%zx %d %d\n",
                //    x, (int)xpos, (int)ypos);
                //sys_vgui("pdtk_toggle_xy_tooltip .x%zx %d\n", x, 1);
                x->gl_editor->e_onmotion = MA_MOVE;
                /* once the code for creating a new object looks sane
                   we'll leave rendering the tooltips to the GUI. */
                //if (tooltips)
                //    sys_vgui("pdtk_tip .x%x.c 0 0\n", x);
            }
            else
            // jsarlo 
            {
                if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
                {
                    canvas_nlet_conf(x,'i');
                    tooltip_erase(x);
                    x->gl_editor->canvas_cnct_inlet_tag[0] = 0;
                }

                if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
                {
                    canvas_nlet_conf(x,'o');
                    tooltip_erase(x);
                    x->gl_editor->canvas_cnct_outlet_tag[0] = 0;
                }

                if(x->gl_editor && x->gl_editor->gl_magic_glass)
                {              
                    magicGlass_unbind(x->gl_editor->gl_magic_glass);
                    magicGlass_hide(x->gl_editor->gl_magic_glass);
                }
                canvas_setcursor(x, CURSOR_EDITMODE_NOTHING); 

                /* now check for tooltips object tooltips */
                if (tooltips && ob)
                {
                    t_rtext *yr = glist_findrtext(x, (t_text *)&ob->ob_g);
                    objtooltip = 1;
                    canvas_enteritem(x, xpos, ypos, rtext_gettag(yr));
                }
            }
            // end jsarlo
        }
        // we mark last highlighted object here for nlet highlighting below
        //post("Returning from the nlet crusade...");
        return;
    }
    else if (in_text_resizing_hotspot) /* red gop rectangle */
    {
        if (doit)
        {
            x->gl_editor->e_onmotion = MA_RESIZE;
            x->gl_editor->e_xwas = x1;
            x->gl_editor->e_ywas = y1;
            x->gl_editor->e_xnew = xpos;
            x->gl_editor->e_ynew = ypos;
            t_pd *sh = (t_pd *)((t_canvas *)x)->x_handle; // scale handle
            /* Special case-- we abuse the value of h_scale to differentiate
               between clicking the corner of a subcanvas and resizing a
               red gop rectangle. */
            ((t_scalehandle *)sh)->h_scale = 2;
            pd_vmess(sh, gensym("_click"), "fff",
                (t_float)in_text_resizing_hotspot, (t_float)xpos,
                (t_float)ypos);
        }
        else
        {
            canvas_setcursor(x, in_text_resizing_hotspot);
        }
        canvas_check_nlet_highlights(x);
        return;
    }
        /* if right click doesn't hit any boxes, call rightclick
            routine anyway */
    if (rightclick)
        canvas_rightclick(x, xpos, ypos, 0);

        /* if not an editing action, and if we didn't hit a
        box, set cursor and return */
    if (runmode || rightclick)
    {
        canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
        return;
    }
        /* having failed to find a box, we try lines now. */
    if (!runmode && !ctrlmod && !shiftmod)
    {
        t_linetraverser t;
        t_outconnect *oc;
        int parseOutno;
        t_object *parseOb = NULL;
        t_outlet *parseOutlet = NULL;
        t_float fx = xpos, fy = ypos;
        t_glist *glist2 = glist_getcanvas(x);
        linetraverser_start(&t, glist2);
        while (oc = linetraverser_next(&t))
        {
            //fprintf(stderr,"oc_visible %d\n", outconnect_visible(oc));
            //ignore invisible connections
            if (outconnect_visible(oc))
            {

                parseOb = NULL;
                parseOutlet = NULL;
                t_float lx1 = t.tr_lx1, ly1 = t.tr_ly1,
                    lx2 = t.tr_lx2, ly2 = t.tr_ly2;
                t_float area = (lx2 - lx1) * (fy - ly1) -
                    (ly2 - ly1) * (fx - lx1);
                t_float dsquare = (lx2-lx1) * (lx2-lx1) + (ly2-ly1) * (ly2-ly1);
                if (area * area >= 50 * dsquare) continue;
                if ((lx2-lx1) * (fx-lx1) + (ly2-ly1) * (fy-ly1) < 0) continue;
                if ((lx2-lx1) * (lx2-fx) + (ly2-ly1) * (ly2-fy) < 0) continue;
                if (doit)
                {
                    glist_selectline(glist2, oc, 
                        canvas_getindex(glist2, &t.tr_ob->ob_g), t.tr_outno,
                        canvas_getindex(glist2, &t.tr_ob2->ob_g), t.tr_inno);
                }
                // jsarlo
                parseOutno = t.tr_outno;
                parseOb = t.tr_ob;
                for (parseOutlet = parseOb->ob_outlet; 
                     parseOutlet && parseOutno; 
                     parseOutlet = parseOutlet->o_next, parseOutno--);
                if (parseOutlet &&
                    magicGlass_isOn(x->gl_editor->gl_magic_glass))
                {
                    magicGlass_bind(x->gl_editor->gl_magic_glass,
                                    t.tr_ob,
                                    t.tr_outno); 
                    magicGlass_setDsp(x->gl_editor->gl_magic_glass,
                                      obj_issignaloutlet(t.tr_ob, t.tr_outno));
                }
                if (magicGlass_isOn(x->gl_editor->gl_magic_glass))
                {
                    magicGlass_moveText(
                        x->gl_editor->gl_magic_glass, xpos, ypos);
                    magicGlass_show(x->gl_editor->gl_magic_glass);
                }
                if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
                {
                    canvas_nlet_conf(x,'i');
                    tooltip_erase(x);
                    x->gl_editor->canvas_cnct_inlet_tag[0] = 0;                  
                }
                if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
                {
                    canvas_nlet_conf(x,'o');
                    tooltip_erase(x);
                    x->gl_editor->canvas_cnct_outlet_tag[0] = 0;                  
                }
                // end jsarlo
                canvas_setcursor(x, CURSOR_EDITMODE_DISCONNECT);
                return;
            }
        }
    }
    if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
    {
        canvas_nlet_conf(x,'i');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_inlet_tag[0] = 0;                  
    }
    // jsarlo
    if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
    {
        canvas_nlet_conf(x,'o');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_outlet_tag[0] = 0;                  
    }
    if(x->gl_editor && x->gl_editor->gl_magic_glass)
    {
        magicGlass_unbind(x->gl_editor->gl_magic_glass);
        magicGlass_hide(x->gl_editor->gl_magic_glass);
    }
    // end jsarlo
    if (x->gl_editor->e_onmotion != MA_SCROLL)
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
    if (doit)
    {
        if (!shiftmod &&
            (x->gl_editor->e_selection || x->gl_editor->e_selectedline))
        {
            //t_undo_redo_sel *buf = (t_undo_redo_sel *)getbytes(sizeof(*buf));
            //buf->u_undo = (t_undo_sel *)canvas_undo_set_selection(x);

            glist_noselect(x);

            // ico@vt.edu 2021-10-06: reset indices for find back as if we haven't
            // searched yet for anything, effectively restarting the search
            //post("search reset when there is selection");
            canvas_find_index1 = 0;
            canvas_find_index2 = -1;
            canvas_num_found = 0;

            //buf->u_redo = (t_undo_sel *)canvas_undo_set_selection(x);
            //canvas_undo_add(x, 11, "selection", buf);
        }
        gui_vmess("gui_canvas_draw_selection", "xiiii",
            x,
            xpos,
            ypos,
            xpos,
            ypos);
        x->gl_editor->e_xwas = xpos;
        x->gl_editor->e_ywas = ypos;
        x->gl_editor->e_onmotion = MA_REGION;
    }
}

   // Dispatch mouseclick message to receiver (for legacy mouse event externals)
void canvas_dispatch_mouseclick(t_float down, t_float xpos, t_float ypos,
    t_float which)
{
    //post("canvas_dispatch_mouseclick which=%d down=%d", (t_int)which, (t_int)down);
    t_symbol *mouseclicksym = gensym("#legacy_mouseclick");
    if (mouseclicksym->s_thing)
    {
        t_atom at[4];
        SETFLOAT(at, down);
        SETFLOAT(at+1, which);
        SETFLOAT(at+2, xpos);
        SETFLOAT(at+3, ypos);
        pd_list(mouseclicksym->s_thing, &s_list, 4, at);
    }
}

void canvas_mousedown(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg which, t_floatarg mod)
{
    //fprintf(stderr,"canvas_mousedown %d\n", x->gl_editor->e_onmotion);
    canvas_doclick(x, xpos, ypos, which, mod, 1);
    // now dispatch to any listeners
    canvas_dispatch_mouseclick(1., xpos, ypos, which);
}

void canvas_mousewheel(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg zpos)
{
    t_symbol *mousewheelsym = gensym("#legacy_mousewheel");
    if (mousewheelsym->s_thing)
    {
        t_atom at[3];
        SETFLOAT(at, xpos);
        SETFLOAT(at+1, ypos);
        SETFLOAT(at+2, zpos);
        pd_list(mousewheelsym->s_thing, &s_list, 3, at);
    }
}

int canvas_isconnected (t_canvas *x, t_text *ob1, int n1,
    t_text *ob2, int n2)
{
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
        if (t.tr_ob == ob1 && t.tr_outno == n1 &&
            t.tr_ob2 == ob2 && t.tr_inno == n2) 
                return (1);
    return (0);
}

// coordinate grid to mitigate sloppy object alignment
#define PIX_GRID 10

static int grid_coord(int x)
{
    return (int)((x+PIX_GRID/2)/PIX_GRID);
}

static int sel_order = 0; // 0 = vertical, 1 = horizontal
static int sel_order_r = 1; // -1 = reverse rows or columns

static int check_wrap(t_gobj *x, int *last)
{
    t_text *y = (t_text *)x;
    if (grid_coord(sel_order?y->te_ypix:y->te_xpix) < *last) {
        return 1;
    } else {
        *last = grid_coord(sel_order?y->te_ypix:y->te_xpix);
        return 0;
    }
}

typedef struct _sel_map {
    int i;
    t_selection *sel;
} t_sel_map;

static int analyze_order(t_sel_map *sel, int n_selected,
                         t_gobj *y1, t_gobj *y2, int *ip1, int *ip2)
{
    int i1 = -1, i2 = -1, count1 = 0, count2 = 0, skip = 1, last = -1;
    for (int i = 0; i < n_selected; i++)
    {
        t_gobj *x = sel[i].sel->sel_what;
        if (!skip && check_wrap(x, &last)) {
            if (i1 != -1 || i2 != -1) {
                skip = 1;
                last = -1;
            }
        }
        if (x == y1) {
            i1 = i;
            count1 = 1; skip = 0;
        } else if (x == y2) {
            i2 = i;
            count2 = 1; skip = 0;
        } else if (!skip) {
            if (i1 != -1 && i2 < i1)
                count1++;
            if (i2 != -1 && i1 < i2)
                count2++;
        }
    }
    *ip1 = i1; *ip2 = i2;
    return count2 > count1 ? count2 : count1;
}

static int sel_compare(const void *x, const void *y)
{
    t_sel_map *xs = (t_sel_map*)x;
    t_sel_map *ys = (t_sel_map*)y;
    t_text *xt =(t_text*)xs->sel->sel_what;
    t_text *yt =(t_text*)ys->sel->sel_what;
    if (sel_order) {
        if (grid_coord(xt->te_xpix) == grid_coord(yt->te_xpix))
            return grid_coord(xt->te_ypix) - grid_coord(yt->te_ypix);
        else
            return sel_order_r *
                (grid_coord(xt->te_xpix) - grid_coord(yt->te_xpix));
    } else {
        if (grid_coord(xt->te_ypix) == grid_coord(yt->te_ypix))
            return grid_coord(xt->te_xpix) - grid_coord(yt->te_xpix);
        else
            return sel_order_r *
                (grid_coord(xt->te_ypix) - grid_coord(yt->te_ypix));
    }
}

void canvas_sort_selection_according_to_location(t_canvas *x, t_gobj *y1, t_gobj *y2)
{
    int n_selected = glist_selectionindex(x, 0, 1); // get all selected objects
    //fprintf(stderr,"n_selected = %d\n", n_selected);
    t_selection *traverse;
    // vertical order = top-to-bottom, left-to-right
    // horizontal order = left-to-right, top-to-bottom
    t_sel_map sel_v[n_selected], sel_h[n_selected];
    int i = 0;
    for (traverse = x->gl_editor->e_selection; traverse; traverse = traverse->sel_next)
    {
        sel_v[i].sel = traverse;
        sel_v[i].i = i;
        sel_h[i].sel = traverse;
        sel_h[i].i = i;
        i++;
    }
    // compute both orders
    int i1 = -1, i2 = -1;
    sel_order_r = 1;
    sel_order = 0;
    qsort(sel_v, n_selected, sizeof(t_sel_map), sel_compare);
    int count_v = analyze_order(sel_v, n_selected, y1, y2, &i1, &i2);
    int offs_v = i2 - i1;
    sel_order = 1;
    qsort(sel_h, n_selected, sizeof(t_sel_map), sel_compare);
    int count_h = analyze_order(sel_h, n_selected, y1, y2, &i1, &i2);
    int offs_h = i2 - i1;
    // Assess whether the vertical or the horizontal order is most likely,
    // based on the number of columns and rows in the resulting pairing of
    // objects, respectively. Break ties in favor of the vertical order.
    sel_order = 0;
    t_sel_map *sel = sel_v;
    int offs = offs_v;
    if (count_v < count_h) {
        sel_order = 1;
        sel = sel_h;
        offs = offs_h;
    }
    if (offs < 0) {
        // If i2 < i1 then the target y2 comes before source y1 in the
        // computed order. Our algorithm assumes that i1 < i2. We can try to
        // fix up the order on the fly by reversing the primary coordinate.
        // This won't do any good if both y1 and y2 are at the same primary
        // coordinate, but then the whole bipartite pairing of sources and
        // targets won't work anyway, and the algorithm will detect this error
        // condition later.
        sel_order_r = -1;
        qsort(sel, n_selected, sizeof(t_sel_map), sel_compare);
    }
    // apply the constructed order
    x->gl_editor->e_selection = sel[0].sel;
    for (i = 0; i < n_selected-1; i++)
    {
        sel[i].sel->sel_next = sel[i+1].sel;
    }
    sel[n_selected-1].sel->sel_next = 0;

#if 0
    // debug
    i = 0;
    for (traverse = x->gl_editor->e_selection; traverse; traverse = traverse->sel_next) {
        t_text *yt = (t_text *)(traverse->sel_what);
        post("sel[%d/%d] x=%d y=%d", i, sel[i].i, yt->te_xpix, yt->te_ypix);
        binbuf_print(yt->te_binbuf);
        fprintf(stderr,"final: %d/%d x=%d y=%d\n", i, sel[i].i, yt->te_xpix, yt->te_ypix);
        i++;
    }
#endif
}

void canvas_drawconnection(t_canvas *x, int lx1, int ly1, int lx2, int ly2,
    t_int tag, int issignal)
{
    char tagbuf[MAXPDSTRING];
    int ymax = 0;
    int halfx = (lx2 - lx1)/2;
    int halfy = (ly2 - ly1)/2;
    //int yoff = (abs(halfx)+abs(halfy))/2;
    int yoff = abs(halfy);
    //if (yoff < 2) yoff = 2;
    if (halfy >= 0)
    {
        //second object is below the first
        if (abs(halfx) <=10)
        {
            ymax = abs((int)(halfy * pow((halfx/10.0),2)));
            if (ymax > 10) ymax = 10;
        }
        else ymax = 10;
    }
    else
    {
        //second object is above the first
        ymax = 20;
    }
    if (yoff > ymax) yoff = ymax;
    sprintf(tagbuf, "l%zx", (t_int)tag);
    gui_vmess("gui_canvas_line", "xssiiiiiiiiiiii",
        x,
        tagbuf,
        (issignal ? "signal" : "control"),
        sys_curved_cords,
        lx1,
        ly1,
        lx1,
        ly1 + yoff,
        lx1 + halfx,
        ly1 + halfy,
        lx2,
        ly2 - yoff,
        lx2,
        ly2,
        0);
}

void canvas_updateconnection(t_canvas *x, int lx1, int ly1, int lx2, int ly2,
    t_int tag)
{
    char cord_tag[MAXPDSTRING];
    if (glist_isvisible(x) && glist_istoplevel(x))
    {
        int ymax = 0;
        int halfx = (lx2 - lx1)/2;
        int halfy = (ly2 - ly1)/2;
        //int yoff = (abs(halfx)+abs(halfy))/2;
        int yoff = abs(halfy);
        //if (yoff < 2) yoff = 2;
        if (halfy >= 0)
        {
            //second object is below the first
            if (abs(halfx) <=10)
            {
                ymax = abs((int)(halfy * pow((halfx/10.0),2)));
                if (ymax > 10) ymax = 10;
            }
            else ymax = 10;
        }
        else
        {
            //second object is above the first
            ymax = 20;
        }
        //fprintf(stderr,"pow%f halfx%d yoff%d ymax%d\n",
        //    pow((halfx/10.0),2), halfx, yoff, ymax);
        if (yoff > ymax) yoff = ymax;
        if (tag)
        {
            // this is used for existing connections
            // e.g. when moving an object which requries
            // visually updating of its existing connections
            sprintf(cord_tag, "l%zx", (t_int)tag);
            gui_vmess("gui_canvas_update_line", "xsiiiiii",
                x,
                cord_tag,
                sys_curved_cords,
                lx1,
                ly1,
                lx2,
                ly2,
                yoff);
        }
        else
        {
            // this is used for new user-drawn connections
            gui_vmess("gui_canvas_update_line", "xsiiiiii",
                x,
                "newcord",
                sys_curved_cords,
                lx1,
                ly1,
                lx2,
                ly2,
                yoff);
        }
    }
}

int canvas_doconnect_doit(t_canvas *x, t_gobj *y1, t_gobj *y2,
    int closest1, int closest2, int multi, int create_undo)
{
    int x11=0, y11=0, x12=0, y12=0;
    int x21=0, y21=0, x22=0, y22=0;
    int lx1, lx2, ly1, ly2;
    int noutlet1, ninlet2;
    t_object *ob1, *ob2;
    t_outconnect *oc, *oc2;

    ob1 = pd_checkobject(&y1->g_pd);
    ob2 = pd_checkobject(&y2->g_pd);
    noutlet1 = obj_noutlets(ob1);
    ninlet2 = obj_ninlets(ob2);
    gobj_getrect(y1, x, &x11, &y11, &x12, &y12);
    /*if (ob1->te_iemgui)
    {
        //fprintf(stderr,"1 is iemgui\n");
        iemgui_getrect_draw((t_iemgui *)ob1, &x11, &y11, &x12, &y12);
    }*/
    gobj_getrect(y2, x, &x21, &y21, &x22, &y22);
    /*if (ob2->te_iemgui)
    {
        //fprintf(stderr,"2 is iemgui\n");
        iemgui_getrect_draw((t_iemgui *)ob2, &x21, &y21, &x22, &y22);
    }*/

    if (canvas_isconnected (x, ob1, closest1, ob2, closest2))
    {
        if(x->gl_editor && x->gl_editor->gl_magic_glass)
        {                
            magicGlass_unbind(x->gl_editor->gl_magic_glass);
            magicGlass_hide(x->gl_editor->gl_magic_glass);
        }
        if (!multi)
            canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        return(1);
    }
    if (obj_issignaloutlet(ob1, closest1) &&
        !obj_issignalinlet(ob2, closest2))
    {
        error("cannot connect signal outlet to control inlet");
           if(x->gl_editor && x->gl_editor->gl_magic_glass)
           {
            magicGlass_unbind(x->gl_editor->gl_magic_glass);
            magicGlass_hide(x->gl_editor->gl_magic_glass);
        }
        if (!multi)
            canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        return(1);
    }

    // if the first object is preset_node, check if the object
    // we are connecting to is supported. If not, disallow connection
    // but only do so from the first outlet
    
    if (pd_class(&y1->g_pd) == preset_node_class && closest1 == 0)
    {
        if (pd_class(&y2->g_pd) == message_class)
        {
            error("preset_node does not work with messages.");
            return(1);
        }
        if (obj_noutlets(ob2) == 0)
        {
            error("preset_node does not work with objects with zero "
                  "or undefined number of outlets\n");
            return(1);
        }
    }

    // now check if explicit user-made connection into preset_node
    // is other than message.
    // messages may be used to change node's operation
    if (pd_class(&y2->g_pd) == preset_node_class &&
        pd_class(&y1->g_pd) != message_class)
    {
        error("preset node only accepts messages "
              "as input to adjust its settings...\n");
        return(1);
    }

    int issignal = obj_issignaloutlet(ob1, closest1);
    oc = obj_connect(ob1, closest1, ob2, closest2);
    outconnect_setvisible(oc, 1);
    lx1 = x11 + (noutlet1 > 1 ?
            ((x12-x11-IOWIDTH) * closest1)/(noutlet1-1) : 0)
                 + IOMIDDLE;
    ly1 = y12;
    lx2 = x21 + (ninlet2 > 1 ?
            ((x22-x21-IOWIDTH) * closest2)/(ninlet2-1) : 0)
                + IOMIDDLE;
    ly2 = y21;

    canvas_drawconnection(x, lx1, ly1, lx2, ly2, (t_int)oc, issignal);
    if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
    {
        canvas_nlet_conf(x,'i');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_inlet_tag[0] = 0;                  
    }
    if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0 && !glob_shift)
    {
        canvas_nlet_conf(x,'o');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_outlet_tag[0] = 0;                  
    }
    // end jsarlo
    canvas_dirty(x, 1);
    /*canvas_setundo(x, canvas_undo_connect,
        canvas_undo_set_connect(x, 
            canvas_getindex(x, &ob1->ob_g), closest1,
            canvas_getindex(x, &ob2->ob_g), closest2),
            "connect");*/

    if (create_undo)
    {
        canvas_undo_add(x, 1, "connect", canvas_undo_set_connect(x, 
                canvas_getindex(x, &ob1->ob_g), closest1,
                canvas_getindex(x, &ob2->ob_g), closest2));
    }

    // add auto-connect back to preset_node object
    // (by this time we know we are connecting only to legal objects
    // who have at least one outlet)
    if (pd_class(&y1->g_pd) == preset_node_class && closest1 == 0)
    {
        
        //fprintf(stderr,"gotta do auto-connect back to preset_node\n");
        // second check is to make sure we are not connected to the
        // second outlet of the preset_node in which case we should not
        // connect back to it
        if (!canvas_isconnected(x, ob2, 0, ob1, 0))
        {
            oc2 = obj_connect(ob2, 0, ob1, 0);
            outconnect_setvisible(oc2, 0);
        }
        //else
        //    fprintf(stderr, "error: already connected (this happens "
        //                    "when loading from file and is ok)\n");
    }

    return(0);
}

int canvas_trymulticonnect(t_canvas *x, int xpos, int ypos, int which, int doit)
{
    //fprintf(stderr,"canvas_trymulticonnect\n");
    int x11=0, y11=0, x12=0, y12=0;
    t_gobj *y1;
    int x21=0, y21=0, x22=0, y22=0;
    t_gobj *y2;
    int xwas = x->gl_editor->e_xwas,
        ywas = x->gl_editor->e_ywas;
    t_object *ob1, *ob2;
    int noutlet1, ninlet2;
    int i;
    int return_val = 1;

    if (!glob_shift)
    {
        gui_vmess("gui_canvas_delete_line", "xs", x, "newcord");
    }

    if ((y1 = canvas_findhitbox(x, xwas, ywas, &x11, &y11, &x12, &y12))
            && (y2 = canvas_findhitbox(x, xpos, ypos, &x21, &y21, &x22, &y22)))
    {
        /* FIRST OPTION: if two objects are selected and the one that is
           originating is one of the selected objects try multi-connecting
           all outlets into all inlets starting with the user-made one as
           an offset */
        if (!x->gl_editor->e_selection->sel_next->sel_next &&
            glist_isselected(x, y1) && glist_isselected(x, y2))
        {
            canvas_undo_add(x, UNDO_SEQUENCE_START, "multiconnect (mode-1)", 0);
            //fprintf(stderr,"first option\n");
            ob1 = pd_checkobject(&y1->g_pd);
            ob2 = pd_checkobject(&y2->g_pd);
            if (ob1 && ob2 && ob1 != ob2 &&
                (noutlet1 = obj_noutlets(ob1))
                && (ninlet2 = obj_ninlets(ob2)))
            {
                int width1 = x12 - x11, closest1, hotspot1;
                int width2 = x22 - x21, closest2, hotspot2;

                if (noutlet1 > 1)
                {
                    closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                    hotspot1 = x11 +
                        (width1 - IOWIDTH) * closest1 / (noutlet1-1);
                }
                else closest1 = 0, hotspot1 = x11;

                if (ninlet2 > 1)
                {
                    closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                    hotspot2 = x21 +
                        (width2 - IOWIDTH) * closest2 / (ninlet2-1);
                }
                else closest2 = 0, hotspot2 = x21;

                if (closest1 >= noutlet1)
                    closest1 = noutlet1 - 1;
                if (closest2 >= ninlet2)
                    closest2 = ninlet2 - 1;

                int nconnections = ( noutlet1 - closest1 < ninlet2 - closest2 ?
                    noutlet1 - closest1 :
                    ninlet2 - closest2 );
                for (i = 0; i < nconnections; i++)
                {
                    return_val = canvas_doconnect_doit(
                        x, y1, y2, closest1 + i, closest2 + i, 1, 1);
                }
                hotspot1=hotspot1; hotspot2=hotspot2; // silence warnings (unused vars)
            }
            canvas_undo_add(x, UNDO_SEQUENCE_END, "multiconnect (mode-1)", 0);
            return(return_val);
        /* end of FIRST OPTION */
        }
        /* SECOND OPTION: if two or more objects are selected and the one
           that is originating is not one of the selected, connect originating
           to all selected objects' the outlets specified by the first
           connection */
        else if (x->gl_editor->e_selection->sel_next &&
                 !glist_isselected(x, y1) && glist_isselected(x, y2))
        {
            canvas_undo_add(x, UNDO_SEQUENCE_START, "multiconnect (mode-2)", 0);
            //fprintf(stderr,"second option\n");
            ob1 = pd_checkobject(&y1->g_pd);
            ob2 = pd_checkobject(&y2->g_pd);
            int noutlet1, ninlet2;
            if (ob1 && ob2 && ob1 != ob2 &&
                (noutlet1 = obj_noutlets(ob1))
                && (ninlet2 = obj_ninlets(ob2)))
            {
                int width1 = x12 - x11, closest1, hotspot1;
                int width2 = x22 - x21, closest2, hotspot2;

                if (noutlet1 > 1)
                {
                    closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                    hotspot1 = x11 +
                        (width1 - IOWIDTH) * closest1 / (noutlet1-1);
                }
                else closest1 = 0, hotspot1 = x11;

                if (ninlet2 > 1)
                {
                    closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                    hotspot2 = x21 +
                        (width2 - IOWIDTH) * closest2 / (ninlet2-1);
                }
                else closest2 = 0, hotspot2 = x21;

                if (closest1 >= noutlet1)
                    closest1 = noutlet1 - 1;
                if (closest2 >= ninlet2)
                    closest2 = ninlet2 - 1;

                return_val = canvas_doconnect_doit(
                    x, y1, y2, closest1, closest2, 1, 1);

                // now that we made the initial connection and know where
                // to begin and where to connect to, let's connect the rest
                t_selection *sel;
                for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
                {
                    // do this only with objects that have not been
                    // connected as of yet
                    if (sel->sel_what != y1 && sel->sel_what != y2)
                    {
                        ob2 = pd_checkobject(&sel->sel_what->g_pd);
                        ninlet2 = obj_ninlets(ob2);
                        if (closest2 < ninlet2)
                        {
                            return_val = canvas_doconnect_doit(
                                x, y1, sel->sel_what, closest1, closest2, 1, 1);
                        }
                    }
                }    
                hotspot1=hotspot1; hotspot2=hotspot2; // silence warnings (unused vars)
            }
            canvas_undo_add(x, UNDO_SEQUENCE_END, "multiconnect (mode-2)", 0);
            return(return_val);
        /* end of SECOND OPTION */
        }

        /* THIRD OPTION: if two or more objects are selected and the one
           that is receiving connection is one of the selected, and the
           target object is selected, connect nth outlet (as specified by
           the first connection) from all selected objects into the inlet
           of the unselected one */
        else if (x->gl_editor->e_selection->sel_next &&
                 glist_isselected(x, y1) && !glist_isselected(x, y2))
        {
            canvas_undo_add(x, UNDO_SEQUENCE_START, "multiconnect (mode-3)", 0);
            //fprintf(stderr,"third option\n");
            ob1 = pd_checkobject(&y1->g_pd);
            ob2 = pd_checkobject(&y2->g_pd);
            int noutlet1, ninlet2;
            if (ob1 && ob2 && ob1 != ob2 &&
                (noutlet1 = obj_noutlets(ob1))
                && (ninlet2 = obj_ninlets(ob2)))
            {
                int width1 = x12 - x11, closest1, hotspot1;
                int width2 = x22 - x21, closest2, hotspot2;

                if (noutlet1 > 1)
                {
                    closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                    hotspot1 = x11 +
                        (width1 - IOWIDTH) * closest1 / (noutlet1-1);
                }
                else closest1 = 0, hotspot1 = x11;

                if (ninlet2 > 1)
                {
                    closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                    hotspot2 = x21 +
                        (width2 - IOWIDTH) * closest2 / (ninlet2-1);
                }
                else closest2 = 0, hotspot2 = x21;

                if (closest1 >= noutlet1)
                    closest1 = noutlet1 - 1;
                if (closest2 >= ninlet2)
                    closest2 = ninlet2 - 1;

                return_val = canvas_doconnect_doit(
                    x, y1, y2, closest1, closest2, 1, 1);

                // now that we made the initial connection and
                // know where to begin and where to connect to,
                // let's connect the rest
                t_selection *sel;
                for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
                {
                    // do this only with objects that have not been
                    // connected as of yet
                    if (sel->sel_what != y1 && sel->sel_what != y2)
                    {
                        ob2 = pd_checkobject(&sel->sel_what->g_pd);
                        noutlet1 = obj_noutlets(ob2);
                        if (closest1 < noutlet1)
                        {
                            return_val = canvas_doconnect_doit(
                                x, sel->sel_what, y2, closest1, closest2, 1, 1);
                        }
                    }
                }
                hotspot1=hotspot1; hotspot2=hotspot2; // silence warnings (unused vars)
            }
            canvas_undo_add(x, UNDO_SEQUENCE_END, "multiconnect (mode-3)", 0);
            return(return_val);
        /* end of THIRD OPTION */
        }

        /* FOURTH OPTION (1:N a.k.a. fan-out, N:1 a.k.a. fan-in, or N:N
           parallel multi-connect): At least *three* objects are selected
           which include *both* the originating object y1 and the receiving
           object y2. This is the most versatile but also the most complicated
           option. The precise outcome depends on the initial connection and
           the status of the ctrl key when doing the initial connection, see
           options A-C below.

           Since both y1 and y2 are selected, there's no a-priori way to tell
           which of the selected objects will be originating and which will be
           receiving connections, so the selection is sorted based on the
           positions of the objects on the canvas instead. The present
           implementation assumes that the selection can be organized into
           rows or columns with the originating object in one of these, and
           the receiving object in another. A heuristic is used to determine
           which configuration is most likely, based on the layout and the
           positions of y1 and y2. To keep things simple, let's imagine that
           the objects are arranged in two rows a, b as depicted below, where
           y1 = a[i] and y2 = b[j]:

           a[1] ... a[i-1] y1 a[i+1] ... a[m]
           b[1] ... b[j-1] y2 b[j+1] ... b[n]

           Then one of the following options A-C will be taken. Option A and B
           are multi-connect variations of the fan-out and fan-in (mode 2 and
           3) single-connect operations above. Option C is only active when
           holding the ctrl key while doing the connection. It produces N:N
           parallel connections.

           - OPTION A (fan-out): Beginning with the *outlet* of the initial
             connection y1-y2, the outlets of y1 will be connected to the same
             inlet of as many objects b[j], b[j+1], ... as possible, in that
             order, starting with y2=b[j].

           - OPTION B (fan-in): Beginning with the *inlet* of the initial
             connection y1-y2, the inlets of y2 will be connected to the same
             outlet of as many objects a[i], a[i+1], ... as possible, in that
             order, starting with y1=a[i].

           - OPTION C (parallel connections): Beginning with the initial y1-y2
             connection, connect as many pairs y1=a[i] -> y2=b[j], a[i+1] ->
             b[j+1], ... as possible, using the same inlet and outlet numbers
             as in the initial connection.

           In the vertical, row-based order sketched out above, the algorithm
           enforces a strict top-to-bottom, left-to-right order of doing
           connections.  Likewise, in the horizontal, column-based order,
           connections are done from left-to-right, top-to-bottom. This limits
           the possible connections, but makes the results more predictable
           and more likely to resemble the user's intentions.

           Option C is only taken if the ctrl key is pressed while doing the
           initial connection. Otherwise, the operation automatically chooses
           fan-out or fan-in depending on which option gives you the larger
           number of connections, breaking ties in favor of fan-out.

           CAVEAT: This operation is not 100% foolproof. The current
           implementation doesn't really try to arrange the selected objects
           into two neat rows or columns as depicted above, it only goes by
           the precomputed linear selection order. In the real world, patches
           tend to be messy, with objects rarely being lined up perfectly. The
           algorithm tries to account for this by rounding coordinates to a
           grid (see grid_coord() above), but you can also help it along by
           employing the tidy-up operation in the edit menu to line things up
           beforehand.

           It also helps if the positions of the selected objects at least
           roughly follow the row- or column-based layout assumed by the
           algorithm. Moreover, you can avoid ambiguities by selecting minimal
           'a' and 'b' sets of objects which will give you the connections
           that you want. In particular, if the algorithm gives you a fan-out,
           but you actually wanted a fan-in instead, try deselecting all 'b'
           objects except the one that you want to fan into. In the same way,
           a fan-out can be forced by selecting only a single 'a' object. */
        else if (x->gl_editor->e_selection->sel_next->sel_next &&
                 glist_isselected(x, y1) && glist_isselected(x, y2))
        {
            canvas_undo_add(x, UNDO_SEQUENCE_START, "multiconnect (mode-4)", 0);
            //fprintf(stderr,"fourth option\n");
            ob1 = pd_checkobject(&y1->g_pd);
            ob2 = pd_checkobject(&y2->g_pd);
            int noutlet1, ninlet2;
            if (ob1 && ob2 && ob1 != ob2 &&
                (noutlet1 = obj_noutlets(ob1))
                && (ninlet2 = obj_ninlets(ob2)))
            {
                int width1 = x12 - x11, closest1, hotspot1;
                int width2 = x22 - x21, closest2, hotspot2;

                if (noutlet1 > 1)
                {
                    closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                    hotspot1 = x11 +
                        (width1 - IOWIDTH) * closest1 / (noutlet1-1);
                }
                else closest1 = 0, hotspot1 = x11;

                if (ninlet2 > 1)
                {
                    closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                    hotspot2 = x21 +
                        (width2 - IOWIDTH) * closest2 / (ninlet2-1);
                }
                else closest2 = 0, hotspot2 = x21;

                if (closest1 >= noutlet1)
                    closest1 = noutlet1 - 1;
                if (closest2 >= ninlet2)
                    closest2 = ninlet2 - 1;

                return_val = canvas_doconnect_doit(
                    x, y1, y2, closest1, closest2, 1, 1);

                // now that we made the initial connection and know where to
                // begin and where to connect to, let's connect the rest
                t_selection *sel;
                int i, i1 = -1, i2 = -1;
                int last_x = -1;
                // re-order the selection
                canvas_sort_selection_according_to_location(x, y1, y2);

                if (glob_ctrl) {
                    // OPTION C (N:N multi-connect)
                    t_selection *sel1 = NULL;
                    int count = 0, counting = 0;
                    for (sel = x->gl_editor->e_selection, i = 0; sel; sel = sel->sel_next, i++)
                    {
                        // check for wrap-around
                        if (check_wrap(sel->sel_what, &last_x)) {
                            if (counting)
                                counting = 0;
                            else if (i2 != -1)
                                break;
                        }
                        // identify original source and target as we go along
                        if (sel->sel_what == y1) {
                            i1 = i;
                            // found the original source
                            sel1 = sel;
                            // start counting
                            counting = 1;
                        } else if (sel->sel_what == y2) {
                            i2 = i;
                            last_x = -1;
                            // original target comes before source => fail
                            if (i1 == -1) break;
                            // otherwise we found the original target, start
                            // doing connections
                            counting = 0;
                        } else if (i2 != -1) {
                            // still doing connections
                            sel1 = sel1->sel_next;
                            // connected as many sources as we got => done
                            if (!sel1 || --count < 0) break;
                            // connect the current source to the current
                            // target, using the original outlet and inlet
                            // numbers
                            ob1 = pd_checkobject(&sel1->sel_what->g_pd);
                            noutlet1 = obj_noutlets(ob1);
                            ob2 = pd_checkobject(&sel->sel_what->g_pd);
                            ninlet2 = obj_ninlets(ob2);
                            if (closest1 < noutlet1 && closest2 < ninlet2)
                            {
                                return_val = canvas_doconnect_doit(
                                    x, sel1->sel_what, sel->sel_what,
                                    closest1, closest2, 1, 1);
                            }
                        } else if (counting) {
                            count++;
                        }
                    }
                    canvas_undo_add(x, UNDO_SEQUENCE_END, "multiconnect (mode-4)", 0);
                    return(return_val);
                }

                // now check for OPTION A vs. B (see description above)
                int successA = 0;
                int successB = 0;
                int do_count;

                // try option A (fan-out)
                int tmp_closest1 = closest1;
                int tmp_closest2 = closest2;
                t_object *tmp_ob1 = ob1;
                t_object *tmp_ob2 = ob2;
                int tmp_noutlet1 = noutlet1;
                int tmp_ninlet2 = ninlet2;
                for (sel = x->gl_editor->e_selection, i = 0; sel; sel = sel->sel_next, i++)
                {
                    // check for wrap-around
                    if (check_wrap(sel->sel_what, &last_x)) {
                        if (i2 != -1) break;
                    }
                    // identify original source and target as we go along
                    if (sel->sel_what == y1) {
                        i1 = i;
                    } else if (sel->sel_what == y2) {
                        i2 = i;
                        last_x = -1;
                        // original target comes before source => fail
                        if (i1 == -1) break;
                        // otherwise, start doing connections
                    } else if (i2 != -1) {
                        tmp_ob2 = pd_checkobject(&sel->sel_what->g_pd);
                        tmp_ninlet2 = obj_ninlets(tmp_ob2);
                        tmp_closest1++;
                        if (tmp_closest1 >= tmp_noutlet1)
                        {
                            break;
                        }
                        else if (tmp_closest2 < tmp_ninlet2)
                        {
                            do_count = 1;
                            if (canvas_isconnected(
                                x, tmp_ob1, tmp_closest1,
                                tmp_ob2, tmp_closest2))
                            {
                                do_count = 0;
                            }
                            if (obj_issignaloutlet(ob1, closest1) &&
                                !obj_issignalinlet(ob2, closest2))
                            {
                                do_count = 0;
                            }    
                            if ((pd_class(&tmp_ob1->ob_pd)) == preset_node_class)
                            {
                                if ((pd_class(&tmp_ob2->ob_pd)) == message_class)
                                {
                                    do_count = 0;
                                }
                            }
                            if ((pd_class(&tmp_ob2->ob_pd) == preset_node_class)
                                && (pd_class(&tmp_ob1->ob_pd)) != message_class)
                            {
                                do_count = 0;
                            }
                            successA += do_count;
                        }
                    }
                }
                //fprintf(stderr,"successA %d\n", successA);

                // try option B (fan-in)
                tmp_closest1 = closest1;
                tmp_closest2 = closest2;
                tmp_ob1 = ob1;
                tmp_ob2 = ob2;
                tmp_noutlet1 = noutlet1;
                tmp_ninlet2 = ninlet2;
                i1 = -1; i2 = -1;
                last_x = -1;
                for (sel = x->gl_editor->e_selection, i = 0; sel; sel = sel->sel_next, i++)
                {
                    // check for wrap-around
                    if (check_wrap(sel->sel_what, &last_x)) {
                        break;
                    }
                    // identify original source and target as we go along
                    if (sel->sel_what == y1) {
                        i1 = i;
                        // found the original source, start doing connections
                    } else if (sel->sel_what == y2) {
                        i2 = i;
                        // reached the original target, we're done
                        break;
                    } else if (i1 != -1) {
                        tmp_ob1 = pd_checkobject(&sel->sel_what->g_pd);
                        tmp_noutlet1 = obj_noutlets(tmp_ob1);
                        tmp_closest2++;
                        if (tmp_closest2 >= tmp_ninlet2)
                        {
                            break;
                        }
                        else if (tmp_closest1 < tmp_noutlet1)
                        {
                            do_count = 1;
                            if (canvas_isconnected(
                                x, tmp_ob1, tmp_closest1,
                                tmp_ob2, tmp_closest2))
                            {
                                do_count = 0;
                            }
                            if (obj_issignaloutlet(ob1, closest1) &&
                                !obj_issignalinlet(ob2, closest2))
                            {
                                do_count = 0;
                            }    
                            if ((pd_class(&tmp_ob1->ob_pd)) == preset_node_class)
                            {
                                if ((pd_class(&tmp_ob2->ob_pd)) == message_class)
                                {
                                    do_count = 0;
                                }
                            }
                            if ((pd_class(&tmp_ob2->ob_pd) == preset_node_class)
                                && (pd_class(&tmp_ob1->ob_pd)) != message_class)
                            {
                                do_count = 0;
                            }
                            successB += do_count;
                        }
                    }
                }
                //fprintf(stderr,"successB %d\n", successB);
                
                // now decide which one is better
                // (we give preference to option A if both are equal)
                i1 = -1; i2 = -1;
                last_x = -1;
                if (successA >= successB)
                {
                    // OPTION A (fan-out)
                    for (sel = x->gl_editor->e_selection, i = 0; sel; sel = sel->sel_next, i++)
                    {
                        // check for wrap-around
                        if (check_wrap(sel->sel_what, &last_x)) {
                            if (i2 != -1) break;
                        }
                        // identify original source and target as we go along
                        if (sel->sel_what == y1) {
                            i1 = i;
                        } else if (sel->sel_what == y2) {
                            i2 = i;
                            last_x = -1;
                            // original target comes before source => fail
                            if (i1 == -1) break;
                            // otherwise, found the original target, start
                            // doing connections
                        } else if (i2 != -1) {
                            // still doing connections
                            ob2 = pd_checkobject(&sel->sel_what->g_pd);
                            ninlet2 = obj_ninlets(ob2);
                            closest1++;
                            if (closest1 >= noutlet1)
                            {
                                break;
                            }
                            else if (closest2 < ninlet2)
                            {
                                return_val = canvas_doconnect_doit(
                                    x, y1, sel->sel_what,
                                    closest1, closest2, 1, 1);
                            }
                            else
                                closest1--;
                        }
                    }
                }
                else
                {
                    // OPTION B (fan-in)
                    for (sel = x->gl_editor->e_selection, i = 0; sel; sel = sel->sel_next, i++)
                    {
                        // check for wrap-around
                        if (check_wrap(sel->sel_what, &last_x)) {
                            break;
                        }
                        // identify original source and target as we go along
                        if (sel->sel_what == y1) {
                            i1 = i;
                            // found the original source, start doing
                            // connections
                        } else if (sel->sel_what == y2) {
                            i2 = i;
                            // reached the original target, we're done
                            break;
                        } else if (i1 != -1) {
                            // still doing connections
                            ob1 = pd_checkobject(&sel->sel_what->g_pd);
                            noutlet1 = obj_noutlets(ob1);
                            closest2++;
                            if (closest2 >= ninlet2)
                            {
                                break;
                            }
                            else if (closest1 < noutlet1)
                            {
                                return_val = canvas_doconnect_doit(
                                    x, sel->sel_what, y2,
                                    closest1, closest2, 1, 1);
                            }
                            else
                                closest2--;
                        }
                    }
                }            
            }
            canvas_undo_add(x, UNDO_SEQUENCE_END, "multiconnect (mode-4)", 0);
            return(return_val);
        // end of FOURTH OPTION
        }
    }

    return(1);
}

void canvas_doconnect(t_canvas *x, int xpos, int ypos, int which, int doit)
{
    //post("canvas_doconnect");
    if (doit && x->gl_editor->e_selection &&
        x->gl_editor->e_selection->sel_next)
    {
        int result = canvas_trymulticonnect(x, xpos, ypos, which, doit);
        if (!result)
        {
            return;
        }
    }
    int x11=0, y11=0, x12=0, y12=0;
    t_gobj *y1;
    int x21=0, y21=0, x22=0, y22=0;
    t_gobj *y2;
    int xwas = x->gl_editor->e_xwas,
        ywas = x->gl_editor->e_ywas;
    if (doit && !glob_shift)
    {
        gui_vmess("gui_canvas_delete_line", "xs", x, "newcord");
    }
    else
    {
        canvas_updateconnection(x, x->gl_editor->e_xwas, x->gl_editor->e_ywas,
            xpos, ypos, 0);
        //sys_vgui("pdtk_check_scroll_on_motion .x%zx.c 0\n", x);
        /* tried canvas_getscroll here instead, but it doesn't seem to add
           much value. Can revisit later if need be... */
        //canvas_getscroll(x);
    }

    if ((y1 = canvas_findhitbox_w_nlets(x, xwas, ywas, &x11, &y11, &x12, &y12, 1))
        && (y2 = canvas_findhitbox_w_nlets(x, xpos, ypos, &x21, &y21, &x22, &y22, 0)))
    {
        t_object *ob1 = pd_checkobject(&y1->g_pd);
        t_object *ob2 = pd_checkobject(&y2->g_pd);
        int noutlet1, ninlet2;
        if (ob1 && ob2 && ob1 != ob2 &&
            (noutlet1 = obj_noutlets(ob1))
            && (ninlet2 = obj_ninlets(ob2)))
        {
            int width1 = x12 - x11, closest1, hotspot1;
            int width2 = x22 - x21, closest2, hotspot2;

            if (noutlet1 > 1)
            {
                closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                hotspot1 = x11 +
                    (width1 - IOWIDTH) * closest1 / (noutlet1-1);
            }
            else closest1 = 0, hotspot1 = x11;

            if (ninlet2 > 1)
            {
                closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                hotspot2 = x21 +
                    (width2 - IOWIDTH) * closest2 / (ninlet2-1);
            }
            else closest2 = 0, hotspot2 = x21;

            hotspot1=hotspot1; hotspot2=hotspot2; // silence warnings (unused vars)

            if (closest1 >= noutlet1)
                closest1 = noutlet1 - 1;
            if (closest2 >= ninlet2)
                closest2 = ninlet2 - 1;

            if (doit)
            {
                canvas_doconnect_doit(x, y1, y2, closest1, closest2, 0, 1);
            }
            else 
            // jsarlo
            {
                t_rtext *y = glist_findrtext(x, (t_text *)&ob2->ob_g);
                if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
                    canvas_nlet_conf(x,'i');
                if (y)
                {
                    last_inlet_filter =
                        gobj_filter_highlight_behavior((t_text *)&ob2->ob_g);
                    //fprintf(stderr,"last_inlet_filter == %d\n",
                    //    last_inlet_filter);
                    sprintf(x->gl_editor->canvas_cnct_inlet_tag, 
                            "%si%d",
                            rtext_gettag(y),
                            closest2);
                    inlet_issignal = obj_issignalinlet(ob2, closest2);
                    //post("o-sig=%d i-sig=%d", outlet_issignal, inlet_issignal);
                    gui_vmess("gui_gobj_highlight_io", "xsi",
                        x,
                        x->gl_editor->canvas_cnct_inlet_tag,
                        (!outlet_issignal ||
                        (outlet_issignal && inlet_issignal)) ? 1 : 0);

                    /* Didn't I just see this code above? */
                    //sys_vgui(".x%x.c raise %s\n",
                    //         x,
                    //         x->gl_editor->canvas_cnct_inlet_tag);
                    if (tooltips)
                    {
                        objtooltip = 1;
                        canvas_enteritem(x, xpos, ypos,
                            x->gl_editor->canvas_cnct_inlet_tag);
                    }
                }
                canvas_setcursor(x, CURSOR_EDITMODE_CONNECT);
            }
            // end jsarlo
            return;
        }
    }
    // jsarlo
    if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
    {
        canvas_nlet_conf(x,'i');
        tooltip_erase(x);
        x->gl_editor->canvas_cnct_inlet_tag[0] = 0;              
    }
    if(x->gl_editor && x->gl_editor->gl_magic_glass)
    {
        magicGlass_unbind(x->gl_editor->gl_magic_glass);
        magicGlass_hide(x->gl_editor->gl_magic_glass);
    }
    // end jsarlo
    if (x->gl_editor->e_onmotion != MA_CONNECT)
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
}

void canvas_selectinrect(t_canvas *x, int lox, int loy, int hix, int hiy)
{
    //fprintf(stderr,"canvas_selectinrect\n");
    t_gobj *y;
    //t_undo_redo_sel *buf=NULL;
    int selection_changed = 0;
    for (y = x->gl_list; y; y = y->g_next)
    {
        int x1, y1, x2, y2;
        gobj_getrect(y, x, &x1, &y1, &x2, &y2);
        //if (((t_text *)y)->te_iemgui)
        //    iemgui_getrect_mouse(y, &x1, &y1, &x2, &y2);
        if (hix >= x1 && lox <= x2 && hiy >= y1 && loy <= y2)
        {
            if (!selection_changed)
            {
                //buf = (t_undo_redo_sel *)getbytes(sizeof(*buf));
                //buf->u_undo = (t_undo_sel *)canvas_undo_set_selection(x);
                selection_changed = 1;
            }
            if (!glist_isselected(x, y))
                glist_select(x, y);
            else glist_deselect(x, y);
        }
    }
    /*if (buf) {
        buf->u_redo = (t_undo_sel *)canvas_undo_set_selection(x);
        canvas_undo_add(x, 11, "selection", buf);
    }*/
}

static void canvas_doregion(t_canvas *x, int xpos, int ypos, int doit)
{
    if (doit)
    {
        int lox, loy, hix, hiy;
        if (x->gl_editor->e_xwas < xpos)
            lox = x->gl_editor->e_xwas, hix = xpos;
        else hix = x->gl_editor->e_xwas, lox = xpos;
        if (x->gl_editor->e_ywas < ypos)
            loy = x->gl_editor->e_ywas, hiy = ypos;
        else hiy = x->gl_editor->e_ywas, loy = ypos;
        canvas_selectinrect(x, lox, loy, hix, hiy);
        gui_vmess("gui_canvas_hide_selection", "x", x);
        x->gl_editor->e_onmotion = MA_NONE;
    }
    else
    {
        gui_vmess("gui_canvas_move_selection", "xiiii",
            x,
            x->gl_editor->e_xwas,
            x->gl_editor->e_ywas,
            xpos,
            ypos);
    }
}


/* ico@vt.edu 2021-10-10: mousedown can be three options
     1 = mousedown
     0 = motion (neither click nor release)
    -1 = mouseup
*/ 
void canvas_passthrough_click(t_canvas *x, t_int xpos, t_int ypos, t_int click)
{
    //post("canvas_passthrough_click %d %d | %d", xpos, ypos, click);
    // ico@vt.edu 2021-10-08: lastly look for any passthrough objects that should
    // catch the mouseup event even though they have not grabbed the mouse focus

    /*
    // don't do redundant runs in case we are using keyboard, which also triggers
    // motion and possibly doclick calls
    if (x == canvas_last_glist && canvas_last_glist_x == xpos &&
        canvas_last_glist_y == ypos && canvas_last_glist_click == click)
    {
        post("...passthrough returning");
        return;
    }*/
    //post("...continuing");
    if (x->gl_list)
    {

        t_gobj *y;
        t_object *ob;
        // instantiate total number of gobj in gl_list and a number of
        // passthrough objects, plus a counting variable
        int numgobj = 0, numptgobj = 0, i;
        int x1, x2, y1, y2, passthroughclick;

        // find out how many objects there are in the gl_list
        // we use this to allocate an array of pointers, so that
        // we can distribute clicks in order from topmost to
        // bottommost (or stop at a non-passthrough object)
        // see mode 3 for iemgui in m_pd.h and ggee/image
        for (y = x->gl_list; y; y = y->g_next)
            numgobj++;
        t_gobj *ypt[numgobj];

        for (y = x->gl_list; y; y = y->g_next)
        {
            ob = pd_checkobject(&y->g_pd);
            // if we are a canvas class with a GOP enabled
            // do the same function inside it
            if (ob)
            {
                if (ob->ob_pd == canvas_class && ((t_canvas *)ob)->gl_isgraph)
                {
                    if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2))
                    {
                        //post("...traversing GOP subpatch...");
                        canvas_passthrough_click(
                            (t_canvas *)y, xpos, ypos, click);
                    }
                }
                else if (((t_text *)y)->te_iemgui == 3)
                {
                    ypt[numptgobj] = y;
                    numptgobj++;
                    //post("clicking on a pass-through image %lx", y);
                }
            }
        }
        if (numptgobj)
        {
            //post("...numptgobj=%d", numptgobj);
            for(i = numptgobj-1; i >= 0; i--)
            {
                // check if we are above the object and if so send it
                // a mouseup event by using the dbl = - 1 hack
                if (canvas_hitbox(x, ypt[i], xpos, ypos, &x1, &y1, &x2, &y2))
                {
                    //post("...hitbox click %d", click);
                    passthroughclick = gobj_click(ypt[i], x, xpos, ypos,
                        shiftmod, (altmod && (!x->gl_edit)) || ctrlmod,
                            (click >= 0 ? 0 : -1), (click > 0 ? click : 0));
                }/* else if (click == -1) {
                    //post("...mouseup off-hitbox click %d", click);
                    // otherwise send them the dbl = -2 mouseup which is
                    // used to reset internal mousedown variable for the mode 3
                    passthroughclick = gobj_click(ypt[i], x, xpos, ypos,
                        shiftmod, (altmod && (!x->gl_edit)) || ctrlmod, -2, 0);
                }*/
            }
        }
    }
}

//void gobj_activate_after_scroll() TODO

void canvas_mouseup(t_canvas *x,
    t_floatarg fxpos, t_floatarg fypos, t_floatarg fwhich)
{
    //post("canvas_mouseup %d %d", (int)fwhich, x->gl_editor->e_onmotion);
    //if (toggle_moving == 1) {
    //    toggle_moving = 0;
    //    sys_vgui("pdtk_toggle_xy_tooltip .x%zx %d\n", x, 0);
    //}
    int xpos = fxpos, ypos = fypos, which = fwhich;

    t_gobj *y;
    t_object *ob;
    //post("mouseup %d %d %d", xpos, ypos, x->gl_editor->e_onmotion);
    if (!x->gl_editor)
    {
        bug("editor");
        return;
    }

    canvas_upclicktime = sys_getrealtime();
    canvas_upx = xpos;
    canvas_upy = ypos;
    glob_lmclick = 0;

    if (x->gl_editor->e_onmotion == MA_CONNECT) {
        canvas_doconnect(x, xpos, ypos, which, 1);
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
    }
    else if (x->gl_editor->e_onmotion == MA_REGION)
        canvas_doregion(x, xpos, ypos, 1);
    else if (x->gl_editor->e_onmotion == MA_MOVE)
    {
        //post("...MA_MOVE");
            /* after motion or resizing, if there's only one text item
                selected, activate the text */
    	canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        scrollbar_synchronous_update(x);
        if (x->gl_editor->e_selection &&
            !(x->gl_editor->e_selection->sel_next))
        {
            gobj_activate(x->gl_editor->e_selection->sel_what, x, 1);
        }
    }
    else if (x->gl_editor->e_onmotion == MA_SCROLL)
    {
        /* Let's try to do this exclusively in the GUI... */
        //sys_vgui("pdtk_canvas_scroll_xy_click .x%zx %d %d 0\n",
        //    (t_int)x, (int)fxpos, (int)fypos);
        x->gl_editor->e_onmotion = MA_NONE;
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);        
    }
    // ico@vt.edu 2021-08-09: added this condition to account
    // for scrollbar refresh when resizing a GOP-enabled subpatch
    // or an abstraction
    else if (x->gl_editor->e_onmotion == MA_RESIZE)
    {
        //post("mouseup MA_RESIZE");
        // check if we are letting go of canvas resize handle
        // and report to it mouse up, so that if it has an array
        // inside it (which is expensive to redraw, so we don't
        // redraw it with each mouse drag update), we can
        // inform the graph to redraw its contents.
        if (resized_gobj && 
            pd_class(&resized_gobj->g_pd) == canvas_class &&
            canvas_hasarray((t_canvas *)resized_gobj))
        {
            t_canvas *c = (t_canvas *)resized_gobj;
            c->gl_disablecontentredraw = 0;
            //post("mouseup MA_RESIZE detected canvas (graph)...");
            // this redraws both the redrect in the canvas itself
            // (if that window is open), and the canvas on its parent
            glist_redraw(c);
        }
        scrollbar_synchronous_update(x);
        resized_gobj = NULL;   
    }
    
    if ((x->gl_editor->e_onmotion != MA_CONNECT &&
         x->gl_editor->e_onmotion != MA_PASSOUT) ||
        x->gl_editor->e_onmotion == MA_CONNECT && !glob_shift)
    {
        //fprintf(stderr,"releasing shift during connect without "
        //               "the button pressed\n");
        if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
            canvas_nlet_conf(x,'o');
        if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
        {
            canvas_nlet_conf(x,'i');
            x->gl_editor->canvas_cnct_inlet_tag[0] = 0;                  
        }

        x->gl_editor->e_onmotion = MA_NONE;
    }

    if (x->gl_editor->e_onmotion == MA_PASSOUT)
    {
        //post("canvas_mouseup MA_PASSOUT");
        // first do the mouseup from the topmost, potentially passthrough
        // objects, then do the release of the grab of an object below
        canvas_passthrough_click(x, xpos, ypos, -1);
        // here we borrow the double-click flag and make it -1 which signifies
        // mouse up since otherwise doit (the last argument) value of 0 is
        // shared between mouse up and mouse motion, making this unclear
        if (x->gl_editor->e_grab)
        {
            int clickreturned = gobj_click(
                x->gl_editor->e_grab, x, xpos, ypos, glob_shift, glob_alt, -1, 0);
        }
        // however, in the case of an array which does not have an e_grab declared
        // we have to let go of the glist_grab instead
        else
            glist_grab(x, 0, 0, 0, 0, 0, 0, 0);
        x->gl_editor->e_onmotion = MA_NONE;
    } else {
        // if we are not doing MA_PASSOUT, we still need to inform
        // passthrough objects of a mouse release to let them update
        // their internal variables
        canvas_passthrough_click(x, xpos, ypos, -1);
    }

    //fprintf(stderr,"canvas_mouseup -> canvas_doclick %d\n", which);
    /* this is to ignore scrollbar clicks from within tcl and is
       unused within nw.js 2.x implementation and onward. since 
       canvas_last_glist_mod is never set to -1 */
    //if (canvas_last_glist_mod == -1)
    //    canvas_doclick(x, xpos, ypos, 0,
    //        (glob_shift + glob_ctrl*2 + glob_alt*4), 0);

    // now dispatch to any click listeners
    canvas_dispatch_mouseclick(0., xpos, ypos, which);
}

/* Cheap hack to simulate mouseup at the last x/y coord. We use this in
   the GUI in case the window gets a blur event before a mouseup */
void canvas_mouseup_fake(t_canvas *x)
{
    if (x->gl_editor && x->gl_edit)
        canvas_mouseup(x, x->gl_editor->e_xwas, x->gl_editor->e_ywas, 0);
}


/* This entire function is made superfluous in the GUI port-- we get middle-
   click pasting for free by default. */
void canvas_mousedown_middle(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg which, t_floatarg mod)
{
    int middleclick;
    tooltip_erase(x);
    
    // read key and mouse button states
    x->gl_editor->e_xwas = (int)xpos;
    x->gl_editor->e_ywas = (int)ypos;
    middleclick = (which == 2 ? 1 : 0);

        /* let's check if we are not middle-clicking */
    //fprintf(stderr,"middleclick=%d doit=%d\n", middleclick, doit);
    if (middleclick)
    {
        if (x->gl_editor && x->gl_editor->e_textedfor)
        {
            canvas_mousedown(x, xpos, ypos, which, mod);
            canvas_mouseup(x, xpos, ypos, which);
            //sys_vgui("pdtk_pastetext\n");
        }
        else
        {
            //sys_vgui("pdtk_canvas_scroll_xy_click .x%zx %d %d 3\n",
            //    (t_int)x, (int)xpos, (int)ypos);
            x->gl_editor->e_onmotion = MA_SCROLL;
            canvas_setcursor(x, CURSOR_SCROLL);
        }
    }
}

    /* displace the selection by (dx, dy) pixels */
void canvas_displaceselection(t_canvas *x, int dx, int dy)
{
    //fprintf(stderr,"canvas_displaceselection %d %d\n", dx, dy);
    t_selection *y;
    char *tag = NULL;
    int resortin = 0, resortout = 0;
    old_displace = 0;
    if (!we_are_undoing &&
        !canvas_undo_already_set_move &&
        x->gl_editor->e_selection)
    {
        //canvas_setundo(x, canvas_undo_move, canvas_undo_set_move(x, 1),
        //    "motion");
        canvas_undo_add(x, 4, "motion", canvas_undo_set_move(x, 1));
        canvas_undo_already_set_move = 1;
    }
    for (y = x->gl_editor->e_selection; y; y = y->sel_next)
    {
        /* for the time being let's discern from vanilla objects
           and those that don't conform */
        if (y->sel_what->g_pd->c_wb &&
            y->sel_what->g_pd->c_wb->w_displacefnwtag)
        {
            /* this is a vanilla object */
            gobj_displace_withtag(y->sel_what, x, dx, dy);
            //fprintf(stderr, "displaceselection with tag\n");
        }
        else {
            /* we will move the non-conforming objects the old way
               THIS SHOULD GO AWAY SOON*/
            gobj_displace(y->sel_what, x, dx, dy);
            t_object *ob = pd_checkobject(&y->sel_what->g_pd);
            t_rtext *yyyy = glist_findrtext(x, (t_text *)&ob->ob_g);
            if (yyyy) tag = rtext_gettag(yyyy);
            if (tag)
                canvas_restore_original_position(x, y->sel_what, tag, -1); 
            else
                old_displace = 1;
            tag = NULL; 
            //fprintf(stderr, "displaceselection old_displace=%d\n",
            //    old_displace);
        }
        t_class *cl = pd_class(&y->sel_what->g_pd);
        if (cl == vinlet_class) resortin = 1;
        else if (cl == voutlet_class) resortout = 1;
    }
    if (dx || dy)
    {
        gui_vmess("gui_canvas_displace_withtag", "xii", x, dx, dy);
        if (resortin) canvas_resortinlets(x);
        if (resortout) canvas_resortoutlets(x);
        //scrollbar_update(x);
        if (x->gl_editor->e_selection)
            canvas_dirty(x, 1);
    }
    // if we have old_displace, legacy displaced objects won't conform
    // to proper ordering of objects as they have been redrawn on top
    // of everything else rather than where they were supposed to be
    // (e.g. possibly in the middle or at the bottom)
    if (old_displace) canvas_redraw(x);
    old_displace = 0;
}

    /* this routine is called whenever a key is pressed or released.  "x"
    may be zero if there's no current canvas.
    Arguments:
      0) 0 = key up, nonzero = key down
      1) either a symbolic key name (e.g., "Right") or an Ascii key number
      2) shift key: nonzero = pressed
      3) focus (not sure what this does)
      4) autorepeat: 0 = off, nonzero = on
    In Pd-L2Ork additional argument is to determine whether we're pasting
    to ignore unnecessary getscroll calls at paste time */
void canvas_key(t_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    static t_symbol *keynumsym, *keyupsym, *keynamesym,
        *keynumsym_a, *keyupsym_a, *keynamesym_a;
    t_symbol *gotkeysym;
    int keynum, down, shift, focus = 1, autorepeat = 0;

    tooltip_erase(x);
    //post("canvas_key ac=%d", ac);
    
    if (ac < 5)
        return;
    /*
    post ("... %f %s %f %f %f",
        atom_getfloat(av), atom_getsymbol(av+1)->s_name,
        atom_getfloat(av+2), atom_getfloat(av+3), atom_getfloat(av+4));
    */
    canvas_undo_already_set_move = 0;
    down = (atom_getfloat(av) != 0);  /* nonzero if it's a key down */
    shift = (atom_getfloat(av+2) != 0);  /* nonzero if shift-ed */
    autorepeat = (int)(atom_getfloat(av+4));
    //fprintf(stderr,"canvas_key autorepeat=%d\n", autorepeat);
    glob_shift = shift;
    //fprintf(stderr,"%d %d %d %d\n", (x->gl_editor != NULL ? 1 : 0),
    //    (x->gl_editor->e_onmotion == MA_CONNECT ? 1 : 0),
    //    glob_shift, glob_lmclick);

    // check if user released shift while trying manual multi-connect
    if (x && x->gl_editor &&
        x->gl_editor->e_onmotion == MA_CONNECT && !glob_shift && !glob_lmclick)
    {
        //fprintf(stderr,"shift released during connect\n");
        gui_vmess("gui_canvas_delete_line", "xs", x, "newcord");
        canvas_mouseup(x, canvas_last_glist_x, canvas_last_glist_y, 0);
    }

    // check if user released shift while dragging inside an object
    if (x && x->gl_editor &&
        x->gl_editor->e_onmotion == MA_PASSOUT && !glob_shift && !glob_lmclick)
    {
        //fprintf(stderr,"shift released during button+shift drag\n");
        canvas_mouseup(x, x->gl_editor->e_xwas, x->gl_editor->e_ywas, 0);
    }

    if (av[1].a_type == A_SYMBOL)
    {
        gotkeysym = av[1].a_w.w_symbol;
        //fprintf(stderr,"gotkeysym=%s\n", gotkeysym->s_name);
        //post("key: %d %s", keynum, gotkeysym->s_name);
    }
    else if (av[1].a_type == A_FLOAT)
    {
        char buf[3];
        switch((int)(av[1].a_w.w_float))
        {
        case 8:  gotkeysym = gensym("BackSpace"); break;
        case 9:  gotkeysym = gensym("Tab"); break;
        case 10: gotkeysym = gensym("Return"); break;
        case 27: gotkeysym = gensym("Escape"); break;
        case 32: gotkeysym = gensym("Space"); break;
        case 127:gotkeysym = gensym("Delete"); break;
        default:
            sprintf(buf, "%c", (int)(av[1].a_w.w_float));
            /*-- moo: assume keynum is a Unicode codepoint; encode as UTF-8 --*/
            char buf[UTF8_MAXBYTES1];
            u8_wc_toutf8_nul(buf, (UCS4)(av[1].a_w.w_float));
            gotkeysym = gensym(buf);
        }
    }
    else gotkeysym = gensym("?");
    //fflag = (av[0].a_type == A_FLOAT ? av[0].a_w.w_float : 0);
    keynum = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float : 0);
    if (keynum == '\\')
    {
        post("keycode %d: dropped", (int)keynum);
        return;
    }
#if 0
    post("keynum %d, down %d, gotkeysym %s",
        (int)keynum, down, gotkeysym->s_name);
#endif
    if (ac == 4) focus = (int)(av[3].a_w.w_float);
    if (keynum == '\r') keynum = '\n';
    if (av[1].a_type == A_SYMBOL &&
        !strcmp(av[1].a_w.w_symbol->s_name, "Return"))
            keynum = '\n';
    if (!keynumsym)
    {
        keynumsym = gensym("#key");
        keyupsym = gensym("#keyup");
        keynamesym = gensym("#keyname");

        keynumsym_a = gensym("#key_a");
        keyupsym_a = gensym("#keyup_a");
        keynamesym_a = gensym("#keyname_a");
    }
#ifdef __APPLE__
        if (keynum == 30)
            keynum = 0, gotkeysym = gensym("Up");
        else if (keynum == 31)
            keynum = 0, gotkeysym = gensym("Down");
        else if (keynum == 28)
            keynum = 0, gotkeysym = gensym("Left");
        else if (keynum == 29)
            keynum = 0, gotkeysym = gensym("Right");
#endif
    // set the shared variable for broadcasting of keypresses to key et al. objects
    t_atom at[2];

    // 2020-10-04 ico@vt.edu: we are here changing the order of broadcasting
    // key presses to grabbed objects first, and only then to all bound objects
    // this change should be heavily scrutinized as it may introduce subtle
    // and not so subtle regressions. I am here introducing it becuase doing so
    // allows us to have some really nice flexibility in respect to grabbed objects.
    // For instance, by allowing grabbed events to propagate first, objects like
    // iemgui numbox can grab exclusive hold of key presses in select cases
    // before such events propagate to other bound elements. So, here
    // we do grabbed keynameafn for key presses and releases
    if (x && x->gl_editor && x->gl_editor->e_grab)
    {
        // as per vanilla behavior, we first send keynums, then keynames
        if (x->gl_editor->e_keyfn && keynum && focus && down)
            (* x->gl_editor->e_keyfn)
                (x->gl_editor->e_grab, (t_float)keynum);
        // this takes care of both keyname presses and releases
        // because we don't discriminate between the down states
        // as we used to...
        if (x->gl_editor->e_keynameafn && gotkeysym && focus)
        {
            at[0] = av[0];
            SETFLOAT(at, down);
            SETSYMBOL(at+1, gotkeysym);
            (* x->gl_editor->e_keynameafn) (x->gl_editor->e_grab, 0, 2, at);
        }
    }

    // debug:
    //post("exclusive=%d", (x && x->gl_editor ? x->gl_editor->exclusive : 0));

    // now broadcast key press to key et al. objects
    // 2020-10-05 ico@vt.edu: only do so if we do not have an object
    // that has grabbed the keyboard exclusively, such as gatom or iemgui numbox
    //post("canvas_key exclusive=%d", (x  && x->gl_editor ? x->gl_editor->exclusive : 0));
    if (!x || !x->gl_editor || !x->gl_editor->exclusive)
    {
        if (!autorepeat)
        {
            if (keynumsym->s_thing && down)
                pd_float(keynumsym->s_thing, (t_float)keynum);
            if (keyupsym->s_thing && !down)
                pd_float(keyupsym->s_thing, (t_float)keynum);
            if (keynamesym->s_thing)
            {
                at[0] = av[0];
                SETFLOAT(at, down);
                SETSYMBOL(at+1, gotkeysym);
                pd_list(keynamesym->s_thing, 0, 2, at);
            }
        }

        // now do the same for autorepeat-enabled objects (key et al. alternative behavior)
        if (keynumsym_a->s_thing && down)
            pd_float(keynumsym_a->s_thing, (t_float)keynum);
        if (keyupsym_a->s_thing && !down)
            pd_float(keyupsym_a->s_thing, (t_float)keynum);
        if (keynamesym_a->s_thing)
        {
            at[0] = av[0];
            SETFLOAT(at, down);
            SETSYMBOL(at+1, gotkeysym);
            pd_list(keynamesym_a->s_thing, 0, 2, at);
        }
    }

    // now process delayed removal of exclusivity which is here, so that when an object
    // releases exclusivity via keyboard key (e.g. iemgui numbox does this using
    // Esc key), that key is not immediately passed to bound objects
    if (x && x->gl_editor && x->gl_editor->exclusive == -1)
    {
    	x->gl_editor->exclusive = 0;
    }

    if (!x || !x->gl_editor)
        return;

    if (x && down)
    {
            /* cancel any dragging action */
        if (x->gl_editor->e_onmotion == MA_MOVE)
            x->gl_editor->e_onmotion = MA_NONE;

            /* if a text editor is open send the key on, as long as
            it is either "real" (has a key number) or else is an arrow key. */
        if (!x->gl_editor->e_grab && x->gl_editor->e_textedfor && focus && (keynum
            || !strcmp(gotkeysym->s_name, "Up")
            || !strcmp(gotkeysym->s_name, "Down")
            || !strcmp(gotkeysym->s_name, "Left")
            || !strcmp(gotkeysym->s_name, "Right")
            || !strcmp(gotkeysym->s_name, "Home")
            || !strcmp(gotkeysym->s_name, "End")
            || !strncmp("Ctrl", gotkeysym->s_name, 4)
            || !strncmp("CtrlShift", gotkeysym->s_name, 9)
            || !strncmp("Shift", gotkeysym->s_name, 5)))
        {
                /* send the key to the box's editor */
            /*if (!x->gl_editor->e_textdirty)
            {
                //canvas_setundo(x, canvas_undo_cut,
                //    canvas_undo_set_cut(x, UCUT_TEXT), "typing");
                canvas_undo_add(x, 3, "typing",
                    canvas_undo_set_cut(x, UCUT_TEXT));
            }*/
            rtext_key(x->gl_editor->e_textedfor,
                (int)keynum, gotkeysym);
            canvas_fixlinesfor(x,
                (t_text *)(x->gl_editor->e_selection->sel_what));
            if (x->gl_editor->e_textdirty)
                canvas_dirty(x, 1);
        }
            /* check for backspace or clear */
        else if ((keynum == 8 || keynum == 127) && focus)
        {
            //fprintf(stderr,"backspace or clear\n");
            if (x->gl_editor->e_selectedline)
                canvas_clearline(x);
            else if (x->gl_editor->e_selection)
            {
                //canvas_setundo(x, canvas_undo_cut,
                //    canvas_undo_set_cut(x, UCUT_CLEAR), "clear");
                canvas_undo_add(x, 3, "clear",
                    canvas_undo_set_cut(x, UCUT_CLEAR));
                canvas_doclear(x);
                glob_preset_node_list_check_loc_and_update();
            }
        }
                /* check for arrow keys */
        else if (x->gl_editor->e_selection)
        {
            if (!strcmp(gotkeysym->s_name, "Up") ||
                !strcmp(gotkeysym->s_name, "ShiftUp"))
            {
                canvas_displaceselection(x, 0, shift ? -10 : -1);
                scrollbar_update(x);
            }
            else if (!strcmp(gotkeysym->s_name, "Down") ||
                !strcmp(gotkeysym->s_name, "ShiftDown"))
            {
                canvas_displaceselection(x, 0, shift ? 10 : 1);
                scrollbar_update(x);
            }
            else if (!strcmp(gotkeysym->s_name, "Left") ||
                     !strcmp(gotkeysym->s_name, "ShiftLeft"))
            {
                canvas_displaceselection(x, shift ? -10 : -1, 0);
                scrollbar_update(x);
            }
            else if (!strcmp(gotkeysym->s_name, "Right") ||
                     !strcmp(gotkeysym->s_name, "ShiftRight"))
            {
                canvas_displaceselection(x, shift ? 10 : 1, 0);
                scrollbar_update(x);
            }
        }
    }

    if (x && keynum == 0 && x->gl_edit &&
        !strncmp(gotkeysym->s_name, "Control", 7))
    {
        glob_ctrl = down;
    }

        /* if alt key goes up or down, and if we're in edit mode, change
        cursor to indicate how the click action changes
        NEW: do so only if not doing anything else in edit mode */
    if (x && keynum == 0 &&
        !strncmp(gotkeysym->s_name, "Alt", 3) && !autorepeat)
    {
        //fprintf(stderr,"ctrl\n");
        glob_alt = down;
        //post("glob_alt=%d", down);
        /* ico@vt.edu: commenting MA_NONE part as that prevents the patch 
           from assuming editmode after it has had an object added via 
           a ctrl+(1-5) shortcut while not in edit mode
        */
        if (x->gl_edit /*&& x->gl_editor->e_onmotion == MA_NONE*/ ||
            x->gl_edit_save)
        {
            canvas_setcursor(x, down ?
                CURSOR_RUNMODE_NOTHING : CURSOR_EDITMODE_NOTHING);
            x->gl_edit = down ? 0 : 1;
            x->gl_edit_save = !x->gl_edit;
            //post("canvas_key... %d", x->gl_edit);
            gui_vmess("gui_canvas_set_editmode", "xi",
                x,
                x->gl_edit);
            if(x->gl_editor && x->gl_editor->gl_magic_glass)
            {
                if (down)
                {
                    magicGlass_hide(x->gl_editor->gl_magic_glass);                    
                }
            }
        }
    }
    //fprintf(stderr," %d %d %d %s %d %d\n",
    //    glob_shift, glob_ctrl, glob_alt, gotkeysym->s_name, keynum, down);
    //canvas_motion(x, canvas_last_glist_x, canvas_last_glist_y, canvas_last_glist_mod);
    // call canvas_motion, so that we can update modifiers...
    pd_vmess(&x->gl_pd, gensym("motion"), "fff",
        (double)canvas_last_glist_x,
        (double)canvas_last_glist_y,
        (double)(glob_shift + glob_ctrl * 2 + glob_alt * 4));
}

extern void graph_checkgop_rect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);

    /* We get the bbox for the object under the mouse the first time around,
       then cache its offset from the mouse for future motion messages.
       Since there are cases where snapping to a grid can move the object
       relative to the mouse pointer, we can't rely on our "anchor" object to
       always be directly under the mouse coordinates. */
static void snap_get_anchor_xy(t_canvas *x, int *gobj_x, int *gobj_y)
{
    t_selection *s = x->gl_editor->e_selection;
    int x1, y1, x2, y2;
    while (s)
    {
        if (canvas_hitbox(x, s->sel_what, x->gl_editor->e_xwas,
            x->gl_editor->e_ywas, &x1, &y1, &x2, &y2))
        {
            *gobj_x = x1;
            *gobj_y = y1;
            return;
        }
	s = s->sel_next;
    }
    bug("canvas_get_snap_offset");
}

int anchor_xoff;
int anchor_yoff;

static void canvas_snap_to_grid(t_canvas *x, int xwas, int ywas, int xnew,
    int ynew, int *dx, int *dy)
{
    int gsize = sys_gridsize;
        /* If we're snapping to grid, we need an initial delta to align
           the object under the mouse to the given gridlines. We keep
           that in the variables below, which will have no affect after
           our initial grid adjustment. */
    int snap_dx = 0, snap_dy = 0;
    if (!snap_got_anchor)
    {
        int obx = xnew, oby = ynew, xsign, ysign;
        snap_get_anchor_xy(x, &obx, &oby);
            /* First, get the distance the selection should be displaced
               in order to align the anchor object with a grid line. */

        snap_dx = ((obx + gsize / 2 * (obx < 0 ? -1 : 1)) / gsize) * gsize - obx;
        snap_dy = ((oby + gsize / 2 * (oby < 0 ? -1 : 1)) / gsize) * gsize - oby;
        obx = obx / gsize * gsize;
        oby = oby / gsize * gsize;
        anchor_xoff = xnew - obx;
        anchor_yoff = ynew - oby;
        snap_got_anchor = 1;
    }
    *dx = ((xnew - anchor_xoff + gsize / 2) / gsize) * gsize -
        ((xwas - anchor_xoff + gsize / 2) / gsize) * gsize + snap_dx;
    *dy = ((ynew - anchor_yoff + gsize / 2) / gsize) * gsize -
        ((ywas - anchor_yoff + gsize / 2) / gsize) * gsize + snap_dy;
}

static void delay_move(t_canvas *x)
{
    // ico@vt.edu 2022-12-13:
    // since this is a delayed action, here we check if we still
    // have a canvas, editor, and selection to work with. otherwise bail.
    if (!x || !x->gl_editor || !x->gl_editor->e_selection)
        return;
    int dx, dy;
    int xwas = x->gl_editor->e_xwas, ywas = x->gl_editor->e_ywas,
        xnew = x->gl_editor->e_xnew, ynew = x->gl_editor->e_ynew;
    if (sys_snaptogrid)
        canvas_snap_to_grid(x, xwas, ywas, xnew, ynew, &dx, &dy);
    else
    {
        dx = xnew - xwas;
        dy = ynew - ywas;
    }
    canvas_displaceselection(x, dx, dy);
    x->gl_editor->e_xwas = xnew;
    x->gl_editor->e_ywas = ynew;
}

/* ico@vt.edu 2020-10-21: added simple motion that keeps updating last
   xy position without all the other actions. This is useful when we just
   let go of the object that was just created and which followed the mouse
   until user clicked to let it go. This is where this function comes into
   play in order to update variables in setlastxymod
*/
void canvas_text_activated_motion(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg fmod)
{
	//post("canvas_text_activated_motion");
	int mod = fmod;
	glist_setlastxymod(x, xpos, ypos, mod);
}

void canvas_motion(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg fmod)
{
    static t_symbol *mousemotionsym;
    //post("motion %d %d %d %d",
    //    (int)xpos, (int)ypos, (int)fmod, canvas_last_glist_mod);
    //fprintf(stderr,"canvas_motion=%d\n",x->gl_editor->e_onmotion);
    int mod = fmod;
    if (!x->gl_editor)
    {
        bug("editor");
        return;
    }
    /* the following is not being used since this was used by tcl/tk
       with nw.js we never issue a mod lesser than 0
    if (canvas_last_glist_mod == -1 && mod != -1)
    {
        //fprintf(stderr,"revert the cursor %d\n", x->gl_edit);
        if (x->gl_edit)
            canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        else
            canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
    }*/
    glist_setlastxymod(x, xpos, ypos, mod);
    if (x->gl_editor->e_onmotion == MA_MOVE)
    {
        //fprintf(stderr,"x-was=%g y-was=%g xwas=%d ywas=%d x=%g y=%g\n",
        //    xpos - x->gl_editor->e_xwas, ypos - x->gl_editor->e_ywas,
        //    x->gl_editor->e_xwas, x->gl_editor->e_ywas, xpos, ypos);
        if (!x->gl_editor->e_clock)
            x->gl_editor->e_clock = clock_new(x, (t_method)delay_move);
        clock_unset(x->gl_editor->e_clock);
        clock_delay(x->gl_editor->e_clock, 5);
        x->gl_editor->e_xnew = xpos;
        x->gl_editor->e_ynew = ypos;
        //    scrollbar_update(x);
        //sys_vgui("pdtk_check_scroll_on_motion .x%zx.c 20\n", x);  
    }
    else if (x->gl_editor->e_onmotion == MA_REGION)
    {
        canvas_doregion(x, xpos, ypos, 0);
        //sys_vgui("pdtk_check_scroll_on_motion .x%zx.c 0\n", x);
        /* This turns out not to be very useful so it's commented.
           Can revisit later... */
        //canvas_getscroll(x);
    }
    else if (x->gl_editor->e_onmotion == MA_CONNECT)
    {
        //post("MA_CONNECT");
        canvas_doconnect(x, xpos, ypos, 0, 0);
    }
    else if (x->gl_editor->e_onmotion == MA_PASSOUT)
    {
        //post("canvas_motion HERE");
        if (!x->gl_editor->e_motionfn)
            bug("e_motionfn");
        // ico@vt.edu 2021-10-08: if we are in passout mode, which
        // means mouse pointer has been grabbed by an object, thus
        // preventing other objects from accessing the pointer info,
        // here we look for any passthrough objects that should
        // catch the motion event. we do this before sending the
        // motionfn to a grabbed object since passthrough objects
        // will be always on top of it (non-passthrough objects
        // never pass pointer data below them--it always stops
        // with them)
        canvas_passthrough_click(x, xpos, ypos, 0);
        (*x->gl_editor->e_motionfn)(&x->gl_editor->e_grab->g_pd,
            xpos - x->gl_editor->e_xwas,
            ypos - x->gl_editor->e_ywas);
        x->gl_editor->e_xwas = xpos;
        x->gl_editor->e_ywas = ypos;
    }
    else if (x->gl_editor->e_onmotion == MA_RESIZE)
    {
        //post("MA_RESIZE motion... %d %d", x->gl_editor->e_xwas, x->gl_editor->e_ywas);
        int x11=0, y11=0, x12=0, y12=0; 
        //t_gobj *y1;
        /*if (y1 = canvas_findhitbox(x,
            x->gl_editor->e_xwas, x->gl_editor->e_ywas,
                &x11, &y11, &x12, &y12))*/
        if (resized_gobj)
        {
            canvas_hitbox(x, resized_gobj,
                x->gl_editor->e_xwas, x->gl_editor->e_ywas,
                &x11, &y11, &x12, &y12);
            int wantwidth = xpos - x11;
            t_object *ob = pd_checkobject(&resized_gobj->g_pd);
            if (ob && (ob->te_pd->c_wb == &text_widgetbehavior ||
                       ob->te_type == T_ATOM ||
                       (ob->ob_pd == canvas_class &&
                        !((t_canvas *)ob)->gl_isgraph)))
            {
                //post("...object");
                // ico@vt.edu 2022-12-14: check for minimum allowable
                // width based on how many nlets we have (in or out,
                // whichever are the largest number). this should be
                // in sync with text_getrect inside g_text.c. this is
                // why there we add one character width to squash
                // any decimal difference because number of nlets and
                // spaces may generate a number that is in between
                // the width of two characters.
                int minwidth = 1;
                int no = obj_noutlets(ob);
                int ni = obj_ninlets(ob);
                int m = ( ni > no ? ni : no);
                if (minwidth < (((IOWIDTH * m) * 2 - IOWIDTH)
                    / sys_fontwidth(glist_getfont(x))))
                {
                    // object's minimum width is less than its required
                    // width due to the number of nlets, so we change
                    // the minimum allowable width
                    minwidth = (((IOWIDTH * m) * 2 - IOWIDTH)
                        / sys_fontwidth(glist_getfont(x)));
                }
                wantwidth = wantwidth / sys_fontwidth(glist_getfont(x));

                if (wantwidth < minwidth)
                    wantwidth = minwidth;
                ob->te_width = wantwidth;
                gobj_vis(resized_gobj, x, 0);
                canvas_fixlinesfor(x, ob);
                gobj_vis(resized_gobj, x, 1);
                // object vis function should check if the object is still
                // selected, so as to draw the outline in the right color
                // it should also tag all aspects with selected tag
                // fprintf(stderr,"MA_RESIZE gobj=%zx\n", y1);
                canvas_dirty(x, 1);
            }
            else if (ob && ob->ob_pd == canvas_class)
            {
                //post("...canvas");
                t_pd *sh = (t_pd *)((t_canvas *)ob)->x_handle;
                pd_vmess(sh, gensym("_motion"), "ff", (t_float)xpos,
                    (t_float)ypos);
            }
            else if (ob && ob->te_iemgui)
            {
                //post("...iemgui");
                t_pd *sh = (t_pd *)((t_iemgui *)ob)->x_handle;
                pd_vmess(sh, gensym("_motion"), "ff", (t_float)xpos, (t_float)ypos);
                //pd_vmess(sh, gensym("_click"), "fff", 0, xpos, ypos);
            }
            else if (ob && (pd_class(&ob->te_pd)->c_name == gensym("Scope~")
                            || pd_class(&ob->te_pd)->c_name == gensym("grid")))
            {
                //post("...scope/grid");
                pd_vmess((t_pd *)ob, gensym("_motion_for_resizing"),
                    "ff", (t_float)xpos, (t_float)ypos);
            }
            else post("...not resizable");
        }
        else // resizing a gop rectangle
        {
            //post("...gop rectangle");
            t_pd *sh = (t_pd *)x->x_handle;
            pd_vmess(sh, gensym("_motion"), "ff", (t_float)xpos, (t_float)ypos);
            //post("moving a gop rect");
        }
    }
    else if (x->gl_editor->e_onmotion == MA_SCROLL || mod == -1)
    {
        // we use bogus mod from tcl to let editor know we are scrolling
        if (mod == -1)
            canvas_setcursor(x, CURSOR_RUNMODE_CLICKME);
        //fprintf(stderr,"canvas_motion MA_SCROLL\n");
    }
    else {
        //post("canvas_motion -> doclick %d %d\n", \
            x->gl_editor->e_onmotion, mod);
        //canvas_getscroll( x);
        canvas_doclick(x, xpos, ypos, 0, mod, 0);
        //pd_vmess(&x->gl_pd, gensym("mouse"), "ffff",
        //    (double)xpos, (double)ypos, 0, (double)mod);
    }
    //if (toggle_moving == 1) {
    //    sys_vgui("pdtk_update_xy_tooltip .x%zx %d %d\n",
    //        x, (int)xpos, (int)ypos);
    //}
    x->gl_editor->e_lastmoved = 1;
    // Dispatch to any listeners for the motion message
    if (!mousemotionsym)
        mousemotionsym = gensym("#legacy_mousemotion");
    if (mousemotionsym->s_thing)
    {
        t_atom at[2];
        SETFLOAT(at, xpos);
        SETFLOAT(at+1, ypos);
        pd_list(mousemotionsym->s_thing, &s_list, 2, at);
    }
}

void canvas_startmotion(t_canvas *x)
{
    //fprintf(stderr,"canvas_startmotion\n");
    int xval, yval;
    if (!x->gl_editor) return;
    glist_getnextxy(x, &xval, &yval);
    //if (xval == 0 && yval == 0) return;
    canvas_setcursor(x, CURSOR_EDITMODE_FLOATING);
    x->gl_editor->e_onmotion = MA_MOVE;
    x->gl_editor->e_xwas = xval;
    x->gl_editor->e_ywas = yval;
    t_selection *sel;
    for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
    {
        if (sel && ((t_text *)sel->sel_what)->te_iemgui > 0)
        {
            // iemgui exception to hide all handles that may interfere
            // with the mouse cursor and its ability to move/deselect
            // object(s) in question
            gobj_select(sel->sel_what, x, 2);
        }
    }
}

/* ----------------------------- window stuff ----------------------- */
extern int sys_perf;

void canvas_print(t_canvas *x, t_symbol *s)
{
    /* Could go a few different ways in porting this:
       1) Use window.print(), which gives a clunky (non-native) interface
          (Also suggests to save to Google cloud, which I don't like at all)
       2) Look into node modules
       3) Replace with an svg export menu item
       4) Nothing
    */
    //sys_vgui(".x%zx.c postscript -file %s\n", x,
    //    *s->s_name ? s->s_name : "x.ps");
}

    /* find a dirty sub-glist, if any, of this one (including itself) */
static t_glist *glist_finddirty(t_glist *x)
{
    t_gobj *g;
    t_glist *g2;
    if (x->gl_env && x->gl_dirty)
        return (x);
    for (g = x->gl_list; g; g = g->g_next)
        if (pd_class(&g->g_pd) == canvas_class &&
            (g2 = glist_finddirty((t_glist *)g)))
                return (g2);
    return (0);
}

void canvas_menuclose(t_canvas *x, t_floatarg fforce);
/* properly close all open root canvases */
void glob_closeall(void*dummy, t_floatarg fforce)
{
  t_canvas*x, *y;
  for (x = pd_this->pd_canvaslist; x; )
    {
      y=x->gl_next;
      canvas_menuclose(x, fforce); /* forced closing of this root canvas */
      x=y;
    }
}

    /* quit, after calling glist_finddirty() on all toplevels and verifying
    the user really wants to discard changes  */
void glob_verifyquit(void *dummy, t_floatarg f)
{
    //fprintf(stderr, "glob_verifyquit %f\n", f);
    t_glist *g, *g2;
        /* find all root canvases */
    for (g = pd_this->pd_canvaslist; g; g = g->gl_next)
        if (g2 = glist_finddirty(g))
        {
            /* first open window */
            if (!glist_istoplevel(g2) && g2->gl_env)
            {
                /* if this is an abstraction */
                vmess(&g2->gl_pd, gensym("menu-open"), "");
            }
            else
            {
                /* is this even necessary? */
                canvas_vis(g2, 1);
            }
            if (!glist_istoplevel(g2) && g->gl_env)
            {
                /* if this is an abstraction */
                gui_vmess("gui_canvas_menuclose", "xxi",
                    g2,
                    g2,
                    3);
            }
            else
            {
                gui_vmess("gui_canvas_menuclose", "xxi",
                    canvas_getrootfor(g2),
                    g2,
                    3);
            }
            //canvas_vis(g2, 1);
            //sys_vgui("pdtk_canvas_menuclose .x%zx {.x%zx menuclose 3;\n}\n",
            //         canvas_getrootfor(g2), g2);
        return;
    }
    if (f == 0 && sys_perf)
    {
        gui_vmess("gui_quit_dialog", "");
    }
    else
    {
        post("Quitting Pd-L2Ork...");
        glob_quit(0, 0);
    }
}

//void canvas_dofree(t_gobj *dummy, t_glist *x)
//{
    //int dspstate = canvas_suspend_dsp();
    //sys_flushqueue();
    //pd_free(&x->gl_pd);
    //canvas_resume_dsp(dspstate);
//}

    /* close a window (or possibly quit Pd), checking for dirty flags.
    The "force" parameter is interpreted as follows:
        0 - request from GUI to close, verifying whether clean or dirty
        1 - request from GUI to close, no verification
        2 - verified - mark this one clean, then continue as in 1
        3 - verified - mark this one clean, then verify-and-quit
    */
void canvas_menuclose(t_canvas *x, t_floatarg fforce)
{
    //fprintf(stderr,"canvas_menuclose %zx %f\n", (t_int)x, fforce);
    int force = fforce;
    t_glist *g;
    if ((x->gl_owner || x->gl_isclone) && (force == 0 || force == 1))
    {
        //fprintf(stderr,"    invis\n");
        canvas_vis(x, 0);   /* if subpatch, just invis it */
    }
    else if (force == 0)    
    {
        g = glist_finddirty(x);
        if (g)
        {
            /* first open window */
            if (!glist_istoplevel(g) && g->gl_env)
            {
                /* if this is an abstraction */
                vmess(&g->gl_pd, gensym("menu-open"), "");
            }
            else
            {
                // is this even necessary?
                canvas_vis(g, 1);
            }
            if (!glist_istoplevel(g) && g->gl_env)
            {
                /* if this is an abstraction */
                gui_vmess("gui_canvas_menuclose", "xxi",
                    g,
                    g,
                    2);
            }
            else
            {
                gui_vmess("gui_canvas_menuclose", "xxi",
                    canvas_getrootfor(g),
                    g,
                    2);
            }
            return;
        }
        else pd_free(&x->gl_pd);
            //sys_queuegui(x, x, canvas_dofree);
            //clock_delay(x->gl_destroy, 0);
    }
    else if (force == 1)
    {
        //sys_vgui("pd {.x%zx menuclose -1;}\n", x);
        //sys_vgui("menu_close .x%zx\n", x);
        //sys_queuegui(x, x, canvas_dofree);
        //canvas_vis(x, 0);
        //canvas_free(x);
        pd_free(&x->gl_pd);
        //fprintf(stderr,"pd_free queued------------\n");
        //clock_delay(x->gl_destroy, 0);
    }
    else if (force == 2)
    {
        canvas_dirty(x, 0);
        while (x->gl_owner)
            x = x->gl_owner;
        g = glist_finddirty(x);
        if (g)
        {
            if (!glist_istoplevel(g) && g->gl_env)
            {
                /* if this is an abstraction */
                vmess(&g->gl_pd, gensym("menu-open"), "");
            }
            else
            {
                /* is this even necessary? */
                canvas_vis(g, 1);
            }
            //vmess(&g->gl_pd, gensym("menu-open"), "");
            if (!glist_istoplevel(g) && g->gl_env)
            {
                /* if this is an abstraction */
                gui_vmess("gui_canvas_menuclose", "xxi",
                    g,
                    g,
                    2);
            }
            else
            {
                gui_vmess("gui_canvas_menuclose", "xxi",
                    canvas_getrootfor(g),
                    g,
                    2);
            }
            //sys_vgui("pdtk_canvas_menuclose .x%zx {.x%zx menuclose 2;\n}\n",
            //         canvas_getrootfor(x), g);
            return;
        }
        else pd_free(&x->gl_pd);
            //sys_vgui("pd {.x%zx menuclose -1;}\n", x);
            //sys_queuegui(x, x, canvas_dofree);
            //clock_delay(x->gl_destroy, 0);
    }
    else if (force == 3)
    {
        canvas_dirty(x, 0);
        glob_verifyquit(0, 1);
    }
}

    /* put up a dialog which may call canvas_font back to do the work */
static void canvas_menufont(t_canvas *x, t_floatarg newsize)
{
    t_canvas *x2 = canvas_getrootfor(x);
    //gfxstub_deleteforkey(x2);
    //char *gfxstub = gfxstub_new2(&x2->gl_pd, &x2->gl_pd);
    if (newsize != x2->gl_font)
    {
        // additional args copied from dialog_font.html
        canvas_font(x2, newsize, x2->gl_font, 100.0, 0.0);
    }
    /*{
        gui_vmess("gui_font_dialog", "xi",
            x2,
            //gfxstub,
            x2->gl_font);
    }*/
}

    /* find an atom or string of atoms */
static int canvas_dofind(t_canvas *x, int *myindex1p)
{
    // Debugging stuff
    //char *b;
    //int i;
    //binbuf_gettext(canvas_findbuf, &b, &i);
    //post("canvas_dofind %lx myindex1p=%d canvas_find_index1=%d \
    //    canvas_findbuf=<%s>", x, *myindex1p, canvas_find_index1, b);
    t_binbuf *b = binbuf_new();
    int iscanvas = 0;

    t_gobj *y;
    int myindex1 = *myindex1p, myindex2, delayed=0;
    if (myindex1 >= canvas_find_index1)
    {
        for (y = x->gl_list, myindex2 = 0; y;
            y = y->g_next, myindex2++)
        {
            t_object *ob = 0;
            if (ob = pd_checkobject(&y->g_pd))
            {
                //binbuf_print(ob->ob_binbuf);
                if (pd_class(&y->g_pd) == canvas_class)
                    iscanvas = 1;
                gobj_save(y, b);
                //binbuf_print(b);
                /* 2021-11-06 ico@vt.edu: before b was ob->ob_binbuf
                   however, this caused 2 problems:
                   1) newly created objects that had their sends/receives
                      changed, did not have their binbuf reflect anything
                      other than what was entered at creation (e.g.
                      numbox2 only had nbx). the find of a matching send
                      or receive in the newly created and edited object
                      would not work until the patch was saved, closed,
                      .and reopened.
                   2) some objects, like gatom, whose sends and receives
                      may match the search phrase, would never work.
                   2021-12-01 ico@vt.edu update: it appears for canvas
                   objects we do want the ob->ob_binbuf approach. otherwise
                   b contains all its objects inside it, resulting in a
                   false positive (highlighting the canvas containing an
                   object).
                */
                if (binbuf_match((iscanvas ? ob->ob_binbuf : b), canvas_findbuf,
                    canvas_find_wholeword))
                {
                    if (myindex1 > canvas_find_index1 ||
                        myindex1 == canvas_find_index1 &&
                            myindex2 > canvas_find_index2)
                    {
                        canvas_find_index1 = myindex1;
                        canvas_find_index2 = myindex2;
                        glist_noselect(x);
                        if (!x->gl_mapped)
                        {
                            delayed = 1;
                        }
                        // this needs to be outside previous if statement
                        // to ensure that window is always brought to the
                        // top and refocused
                        vmess(&x->gl_pd, gensym("menu-open"), "");
                        canvas_editmode(x, 1.);
                        glist_select(x, y);
                        //post("find A delayed=%d", delayed);
                        t_rtext *yrt = glist_findrtext(x, (t_text *)y);
                        if (delayed)
                        {
                            gui_vmess("gui_canvas_delayed_scroll_to_gobj",
                                "xsi", x, rtext_gettag(yrt), 1);
                        }
                        else
                        {
                            if (canvas_whichfind != x)
                                gui_vmess("gui_close_find_bar_on_new_window_focus",
                                    "x", x);
                            gui_vmess("gui_canvas_scroll_to_gobj", "xsi",
                                x, rtext_gettag(yrt), 1);
                        }
                        binbuf_free(b);
                        return (1);
                    }
                }
                // we clear the binbuf to prevent false positives on
                // the next search due to binbuf_save simply appending
                // to the existing content
                binbuf_clear(b);
            }
        }
    }
    for (y = x->gl_list, myindex2 = 0; y; y = y->g_next, myindex2++)
    {
        if (pd_class(&y->g_pd) == canvas_class)
        {
            (*myindex1p)++;
            //post("find B");
            if (canvas_dofind((t_canvas *)y, myindex1p))
            {
                t_rtext *yrt = glist_findrtext(x, (t_text *)y);
                gui_vmess("gui_canvas_scroll_to_gobj", "xsi",
                    x, rtext_gettag(yrt), 1);
                binbuf_free(b);
                return (1);
            }
        }
    }
    binbuf_free(b);
    return (0);
}

static void canvas_find(t_canvas *x, t_symbol *s, t_floatarg wholeword)
{
    //post("canvas_find");
    int myindex1 = 0;
    t_symbol *decodedsym = sys_decodedialog(s);
    //post("...decodedsym=<%s>", decodedsym->s_name);
    if (!canvas_findbuf)
        canvas_findbuf = binbuf_new();
    binbuf_text(canvas_findbuf, decodedsym->s_name, strlen(decodedsym->s_name));
    canvas_find_index1 = 0;
    canvas_find_index2 = -1;
    canvas_num_found = 0;
    canvas_find_wholeword = wholeword;
    canvas_whichfind = x;
    if (!canvas_dofind(x, &myindex1))
    {
        binbuf_print(canvas_findbuf);
        post("... couldn't find");
    } else {
        canvas_num_found++;
    }
}

static void canvas_find_again(t_canvas *x)
{
    //post("canvas_find_again");
    int myindex1 = 0;
    if (!canvas_findbuf || !canvas_whichfind)
        return;
    if (!canvas_dofind(canvas_whichfind, &myindex1))
    {
        binbuf_print(canvas_findbuf);
        if (canvas_num_found > 0)
            post("... couldn't find. will restart on the next find again request.");
        else
            post("... couldn't find");
        // now that we've notified the user that there are no more objects
        // that have the search term, reset the search, so that the next
        // find again restarts from the beginning
        canvas_find_index1 = 0;
        canvas_find_index2 = -1;
        canvas_num_found = 0;
    } else {
        canvas_num_found++;
    }
}

/* following function serves mainly as a helper function for tcl/tk
   f = 0: show immediate parent
   f = 1: show visible parent */
static void canvas_find_parent(t_canvas *x, t_floatarg f)
{
    if (x->gl_owner)
    {
        if (f != 0) /* find visible parent */
        {
            t_glist *owner = x->gl_owner;
            while (!glist_isvisible(owner) &&
                   !owner->gl_havewindow && owner->gl_owner)
                owner = owner->gl_owner;
            if (glist_isvisible(owner) && owner->gl_havewindow)
                canvas_vis(owner, 1);
        }
        else /* find immediate parent */
        {
            canvas_vis(glist_getcanvas(x->gl_owner), 1);
        }
    }
    else
    {
        gui_vmess("gui_raise_pd_window", "");
    }
}

/*
   ico@vt.edu 2021-10-19:
   finds a number of instances on canvas of a particular class. this
   is useful in situations where an object may have loaded bunch of
   stuff on the GUI side of things (e.g. moonlib/image that can batch
   load a bunch of images), and once all instances are gone, we will
   want to dispose of all those loaded images. the same may be also
   used by the ggee/image. Currently disabled.
*/
/*
int canvas_find_number_of_class_instances(t_canvas *x, void *objclass)
{
    int result = 0;
    t_gobj *y;
    for(y = x->gl_list; y; y = y->g_next)
    {
        if (pd_class(&y->g_pd) == objclass)
            result++;
        if (pd_class(&y->g_pd) == canvas_class)
            result = result + 
                canvas_find_number_of_class_instances((t_canvas *)y, objclass);
    }
    return result;
}

int find_total_number_of_class_instances(t_canvas *x, void *objclass)
{
    int result = 0;
    // get the root window
    t_glist *root = x;
    while (root->gl_owner)
        root = root->gl_owner;
    // now navigate all the windows using a subfunction
    result = canvas_find_number_of_class_instances(root, objclass);
    post("result=%d", result);
}
*/

    /* tell the gui to bring a gobj into view, possibly with an animation */
static void gobj_emphasize(t_glist *g, t_gobj *x)
{
    t_rtext *y = glist_findrtext(g, (t_text *)x);
    gui_vmess("gui_gobj_emphasize", "xs", g, rtext_gettag(y));
}

    /* tell the gui to mark a gobj as dirty (change border color) */
void gobj_dirty(t_gobj *x, t_glist *g, int state)
{
    t_rtext *y = glist_findrtext(g, (t_text *)x);
    gui_vmess("gui_gobj_dirty", "xsi", glist_getcanvas(g), rtext_gettag(y), state);
}

    /* tell the gui to display a specific message in the
        top right corner */
void canvas_warning(t_canvas *x, int warid)
{
    gui_vmess("gui_canvas_warning", "xi", x, warid);
}

static int glist_dofinderror(t_glist *gl, void *error_object)
{
    t_gobj *g;
    for (g = gl->gl_list; g; g = g->g_next)
    {
        if ((void *)g == error_object)
        {
            /* got it... now show it. */
            glist_noselect(gl);
            canvas_vis(gl, 1);
            canvas_editmode(gl, 1.);
            glist_select(gl, g);
            gobj_emphasize(gl, g);
            return (1);
        }
        else if (g->g_pd == canvas_class)
        {
            if (glist_dofinderror((t_canvas *)g, error_object))
                return (1);
        }
    }
    return (0);
}

void canvas_finderror(void *error_object)
{
    t_canvas *x;
        /* find all root canvases */
    for (x = pd_this->pd_canvaslist; x; x = x->gl_next)
    {
        if ((void *)x == error_object)
        {
            /* If the error is associated with a toplevel canvas, we
               we raise it to the top and do a quick-and-dirty background
               animation for visual feedback to the user */
            glist_noselect(x);
            gui_vmess("gui_canvas_emphasize", "x", glist_getcanvas(x));
            return;
        }
        if (glist_dofinderror(x, error_object))
            return;
    }
    post("... sorry, I couldn't find the source of that message.");
}

void canvas_stowconnections(t_canvas *x)
{
    //fprintf(stderr,"canvas_stowconnections\n");
    t_gobj *selhead = 0, *seltail = 0, *nonhead = 0, *nontail = 0, *y, *y2;
    t_linetraverser t;
    t_outconnect *oc;
    if (!x->gl_editor) return;
        /* split list to "selected" and "unselected" parts */ 
    for (y = x->gl_list; y; y = y2)
    {
        y2 = y->g_next;
        if (glist_isselected(x, y))
        {
            if (seltail)
            {
                seltail->g_next = y;
                seltail = y;
                y->g_next = 0;
            }
            else
            {
                selhead = seltail = y;
                seltail->g_next = 0;
            }
        }
        else
        {
            if (nontail)
            {
                nontail->g_next = y;
                nontail = y;
                y->g_next = 0;
            }
            else
            {
                nonhead = nontail = y;
                nontail->g_next = 0;
            }
        }
    }
        /* move the selected part to the end */
    if (!nonhead) x->gl_list = selhead;
    else x->gl_list = nonhead, nontail->g_next = selhead;

        /* add connections to binbuf */
    binbuf_clear(x->gl_editor->e_connectbuf);
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        int s1 = glist_isselected(x, &t.tr_ob->ob_g);
        int s2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (s1 != s2)
            binbuf_addv(x->gl_editor->e_connectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                    glist_getindex(x, &t.tr_ob->ob_g), t.tr_outno,
                        glist_getindex(x, &t.tr_ob2->ob_g), t.tr_inno);
    }
}

void canvas_restoreconnections(t_canvas *x)
{
    //fprintf(stderr,"canvas_restoreconnections\n");
    pd_bind(&x->gl_pd, gensym("#X"));
    binbuf_eval(x->gl_editor->e_connectbuf, 0, 0, 0);
    pd_unbind(&x->gl_pd, gensym("#X"));
}

static t_binbuf *canvas_docopy(t_canvas *x)
{
    //fprintf(stderr,"canvas_docopy\n");
    t_gobj *y;
    t_linetraverser t;
    t_outconnect *oc;
    t_binbuf *b = binbuf_new();
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (glist_isselected(x, y))
        {
            //fprintf(stderr,"saving object\n");
            gobj_save(y, b);
        }
    }
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        //fprintf(stderr,"found some lines %d %d\n",
        //    glist_isselected(x, &t.tr_ob->ob_g),
        //    glist_isselected(x, &t.tr_ob2->ob_g));
        if (glist_isselected(x, &t.tr_ob->ob_g)
            && glist_isselected(x, &t.tr_ob2->ob_g))
        {
            //fprintf(stderr,"saving lines leading into selected object\n");
            binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
                glist_selectionindex(x, &t.tr_ob->ob_g, 1), t.tr_outno,
                glist_selectionindex(x, &t.tr_ob2->ob_g, 1), t.tr_inno);
        }
    }
    return (b);
}

static void canvas_reset_copyfromexternalbuffer(t_canvas *x)
{
    copyfromexternalbuffer = 0;
}

int abort_when_pasting_from_external_buffer = 0;

static void canvas_copyfromexternalbuffer(t_canvas *x, t_symbol *s,
    int ac, t_atom *av)
{
    static int level, line;
    if (!x->gl_editor)
        return;

    if (!ac && !copyfromexternalbuffer)
    {
        //fprintf(stderr,"init\n");
        copyfromexternalbuffer = 1;
        screenx1 = 0;
        screeny1 = 0;
        screenx2 = 0;
        screeny2 = 0;
        copiedfont = 0;
        binbuf_free(copy_binbuf);
        copy_binbuf = binbuf_new();
        line = level = 0;
    }
    else if (ac && copyfromexternalbuffer)
    {
        int begin_patch = av[0].a_type == A_SYMBOL &&
            !strcmp(av[0].a_w.w_symbol->s_name, "#N");
        int end_patch = av[0].a_type == A_SYMBOL &&
            !strcmp(av[0].a_w.w_symbol->s_name, "#X") &&
            av[1].a_type == A_SYMBOL &&
            !strcmp(av[1].a_w.w_symbol->s_name, "restore");
        line++;
        // Keep track of the nesting of (sub)patches. Improperly nested
        // patches will make Pd crash and burn if we just paste them, so we
        // rather report such conditions as errors instead.
        if (end_patch && --level < 0) {
            post("paste error: unmatched end of subpatch at line %d",
                line);
            copyfromexternalbuffer = 0;
            binbuf_clear(copy_binbuf);
            return;
        }
        //fprintf(stderr,"fill %d\n", ac);
        if (copyfromexternalbuffer != 1 || !begin_patch || ac != 7)
        {
        // not a patch header, just copy
            if (begin_patch) level++;
            binbuf_add(copy_binbuf, ac, av);
            binbuf_addsemi(copy_binbuf);
            copyfromexternalbuffer++;
        }
        else if (copyfromexternalbuffer == 1 &&
            begin_patch && ac == 7)
        {
        // patch header, if the canvas is empty adjust window size and
        // position here...
            int check = 0;
            //fprintf(stderr,
            //    "copying canvas properties for copyfromexternalbuffer\n");
            if (av[2].a_type == A_FLOAT)
            {
                screenx1 = av[2].a_w.w_float;
                check++;
            }
            if (av[3].a_type == A_FLOAT)
            {
                screeny1 = av[3].a_w.w_float;
                check++;
            }
            if (av[4].a_type == A_FLOAT)
            {
                screenx2 = av[4].a_w.w_float;
                check++;
            }
            if (av[5].a_type == A_FLOAT)
            {
                screeny2 = av[5].a_w.w_float;
                check++;
            }
            if (av[6].a_type == A_FLOAT)
            {
                copiedfont = av[6].a_w.w_float;
                check++;
            }
            if (check != 5)
            {
                post("paste error: canvas info has invalid data at line %d",
                    line);
                copyfromexternalbuffer = 0;
                binbuf_clear(copy_binbuf);
            }
            else
            {
                copyfromexternalbuffer++;
            }
        }
    }
    else if (!ac && copyfromexternalbuffer)
    {
        // here we can do things after the copying process has been completed.
        // in particular, we use this to check whether there's an incomplete
        // subpatch definition
        if (level > 0) {
            post("paste error: unmatched beginning of subpatch at line %d",
                line);
            copyfromexternalbuffer = 0;
            binbuf_clear(copy_binbuf);
        }
    }
}

void glob_clipboard_text(t_pd *dummy, float f)
{
    clipboard_istext = (int)f;
}

static void canvas_copy(t_canvas *x)
{
    if (!x->gl_editor || !x->gl_editor->e_selection)
        return;
    copyfromexternalbuffer = 0;
    screenx1 = 0;
    screeny1 = 0;
    screenx2 = 0;
    screeny2 = 0;
    copiedfont = 0;
    binbuf_free(copy_binbuf);
    clipboard_istext = 0;
    //fprintf(stderr, "canvas_copy\n");
    /* We're not replacing the following sys_vgui call because nw.js's
       paste mechanism works a bit differently and doesn't require this.
       But if I missed some functionality this-- as well as the rest of the
       insanely complicated externalbuffer logic-- should be revisited. */

    //sys_vgui("pdtk_canvas_reset_last_clipboard\n");
    copy_binbuf = canvas_docopy(x);
    if (!x->gl_editor->e_selection)
    {
        /* Ok, this makes no sense-- if we return above when there's no
           e_selection, then how could the following possibly be true? */

        //sys_vgui("pdtk_canvas_update_edit_menu .x%zx 0\n", x);
    }
    else
    {
        /* Still not exactly sure what this is doing.  If it's just
           disabling menu items related to the clipboard I think we can
           do without it. */
        //sys_vgui("pdtk_canvas_update_edit_menu .x%zx 1\n", x);
    }
    paste_xyoffset = 1;
    if (x->gl_editor->e_textedfor)
    {
        char *buf;
        int bufsize;
        clipboard_istext = 1;
        rtext_getseltext(x->gl_editor->e_textedfor, &buf, &bufsize);
    }
}

static void canvas_clearline(t_canvas *x)
{
    if (x->gl_editor->e_selectedline)
    {
        canvas_disconnect(x, x->gl_editor->e_selectline_index1,
             x->gl_editor->e_selectline_outno,
             x->gl_editor->e_selectline_index2,
             x->gl_editor->e_selectline_inno);
        canvas_dirty(x, 1);
        canvas_undo_add(x, 2, "disconnect", canvas_undo_set_disconnect(x,
                x->gl_editor->e_selectline_index1,
                x->gl_editor->e_selectline_outno,
                x->gl_editor->e_selectline_index2,
                x->gl_editor->e_selectline_inno));
    }
}

static void canvas_doclear(t_canvas *x)
{
    t_gobj *y, *y2;
    int dspstate;

    dspstate = canvas_suspend_dsp();
    if (x->gl_editor->e_selectedline)
    {
        canvas_disconnect(x, x->gl_editor->e_selectline_index1,
             x->gl_editor->e_selectline_outno,
             x->gl_editor->e_selectline_index2,
             x->gl_editor->e_selectline_inno);
        /*canvas_setundo(x, canvas_undo_disconnect,
            canvas_undo_set_disconnect(x,
                x->gl_editor->e_selectline_index1,
                x->gl_editor->e_selectline_outno,
                x->gl_editor->e_selectline_index2,
                x->gl_editor->e_selectline_inno),
            "disconnect");*/
        canvas_undo_add(x, 2, "disconnect", canvas_undo_set_disconnect(x,
                x->gl_editor->e_selectline_index1,
                x->gl_editor->e_selectline_outno,
                x->gl_editor->e_selectline_index2,
                x->gl_editor->e_selectline_inno));
    }
        /* if text is selected, deselecting it might remake the
        object. So we deselect it and hunt for a "new" object on
        the glist to reselect. */
    if (x->gl_editor->e_textedfor)
    {
        pd_this->pd_newest = 0;
        glist_noselect(x);
        if (pd_this->pd_newest)
        {
            for (y = x->gl_list; y; y = y->g_next)
                if (&y->g_pd == pd_this->pd_newest) glist_select(x, y);
        }
    }
    while (1)   /* this is pretty weird...  should rewrite it */
    {
        for (y = x->gl_list; y; y = y2)
        {
            y2 = y->g_next;
            if (glist_isselected(x, y))
            {
                /* if it is a graph and the window is open, destroy it first
                   this will avoid leaving stale gop rectangle and name */
                if (pd_class(&y->g_pd) == canvas_class &&
                    ((t_glist *)y)->gl_havewindow)
                {
                    canvas_menuclose((t_glist *)y, 0);
                }

                /* delete any stale visual cords */
                canvas_eraselinesfor(x, (t_text *)y);

                /* now destroy the object */
                glist_delete(x, y);
#if 0
                if (y2) post("cut 5 %zx %zx", y2, y2->g_next);
                else post("cut 6");
#endif
                goto next;
            }
        }
        goto restore;
    next: ;
    }
restore:
    canvas_dirty(x, 1);
    //canvas_redraw(x);
    scrollbar_update(x);
    canvas_resume_dsp(dspstate);
}

static void canvas_cut(t_canvas *x)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    if (x->gl_editor && x->gl_editor->e_selectedline)
        canvas_clearline(x);
    /* if we are cutting text */
    else if (x->gl_editor && x->gl_editor->e_textedfor)
    {
        //fprintf(stderr,"canvas_cut textedfor\n");
        char *buf;
        int bufsize;
        rtext_getseltext(x->gl_editor->e_textedfor, &buf, &bufsize);
        if (!bufsize)
            return;
        canvas_copy(x);
        //rtext_key(x->gl_editor->e_textedfor, 127, &s_);
        canvas_fixlinesfor(x,(t_text*) x->gl_editor->e_selection->sel_what);
        canvas_dirty(x, 1);
    }
    /* else we are cutting objects */
    else if (x->gl_editor && x->gl_editor->e_selection)
    {
        //canvas_setundo(x, canvas_undo_cut,
        //    canvas_undo_set_cut(x, UCUT_CUT), "cut");
        canvas_undo_add(x, 3, "cut", canvas_undo_set_cut(x, UCUT_CUT));
        canvas_copy(x);
        canvas_doclear(x);
        glob_preset_node_list_check_loc_and_update();
        paste_xyoffset = 0;
        scrollbar_update(x);
    }
}

/* ------------------------- encapsulate ------------------------ */

typedef struct _xletholder
{
    void *xlh_addr;
    int xlh_seln;
    int xlh_xletn;
    int xlh_type;
    int xlh_x;
    int xlh_y;
    t_binbuf *xlh_conn;
    struct _xletholder *xlh_next;
} t_xletholder;

#define X_PLAOFF 40
#define Y_PLAOFF 80
#define XLETSGAP 10

static void canvas_dofancycopy(t_canvas *x, t_binbuf **object, t_binbuf **connections)
{
    t_gobj *y;
    t_linetraverser t;
    t_outconnect *oc;
    t_binbuf *b = binbuf_new();
    int maxx, maxy, minx, miny, numobj = 0;

    binbuf_addv(b, "ssiiiisi;", gensym("#N"), gensym("canvas"),
        0, 0, 450, 300, gensym("subpatch"), 0);

    gobj_getrect(x->gl_editor->e_selection->sel_what, x,
                    &minx, &miny, &maxx, &maxy);

    /* compute global rect */
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (glist_isselected(x, y))
        {
            int tminx, tmaxx, tminy, tmaxy;
            gobj_getrect(y, x, &tminx, &tminy, &tmaxx, &tmaxy);
            if(tminx < minx) minx = tminx;
            if(tmaxx > maxx) maxx = tmaxx;
            if(tminy < miny) miny = tminy;
            if(tmaxy > maxy) maxy = tmaxy;
        }
    }

    /* save selected objects into binbbuf */
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (glist_isselected(x, y))
        {
            gobj_displace(y, x, X_PLAOFF-minx, Y_PLAOFF-miny);
            gobj_save(y, b);
            gobj_displace(y, x, minx-X_PLAOFF, miny-Y_PLAOFF);
            numobj++;
        }
    }

    /* traverse all the lines */
    t_xletholder *inlets = 0, *outlets = 0, *lastoutlet = 0;
    linetraverser_start(&t, x);
    while (oc = linetraverser_next(&t))
    {
        int s1 = glist_isselected(x, &t.tr_ob->ob_g);
        int s2 = glist_isselected(x, &t.tr_ob2->ob_g);
        /* if inner connection */
        if (s1 && s2)
        {
            binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
                glist_selectionindex(x, &t.tr_ob->ob_g, 1), t.tr_outno,
                glist_selectionindex(x, &t.tr_ob2->ob_g, 1), t.tr_inno);
        }
        /* if outer outlet connection */
        else if(s1 && !s2)
        {
            /* if we continue in the same outlet, we add the object and
               inlet number to the list and skip */
            if (lastoutlet && lastoutlet->xlh_addr == (void *)t.tr_outlet)
            {
                binbuf_addv(lastoutlet->xlh_conn, "ii",
                    glist_selectionindex(x, &t.tr_ob2->ob_g, 0), t.tr_inno);
                continue;
            }

            /* create and fill outlet handling structure */
            t_xletholder *newoutlet = (t_xletholder *)getbytes(sizeof(t_xletholder));
            newoutlet->xlh_addr = (void *)t.tr_outlet;
            newoutlet->xlh_seln = glist_selectionindex(x, &t.tr_ob->ob_g, 1);
            newoutlet->xlh_xletn = t.tr_outno;
            newoutlet->xlh_type = obj_issignaloutlet(t.tr_ob, t.tr_outno);
            newoutlet->xlh_x = t.tr_lx1;
            newoutlet->xlh_y = t.tr_ly1;
            newoutlet->xlh_conn = binbuf_new();
            binbuf_addv(newoutlet->xlh_conn, "ii",
                glist_selectionindex(x, &t.tr_ob2->ob_g, 0), t.tr_inno);
            /* insert structure regarding its coords */
            t_xletholder *prev = 0, *curr = outlets;
            while(curr && (t.tr_lx1 > curr->xlh_x ||
                (t.tr_lx1 == curr->xlh_x && t.tr_ly1 > curr->xlh_y)))
            {
                prev = curr;
                curr = curr->xlh_next;
            }
            if(prev) prev->xlh_next = newoutlet;
            else outlets = newoutlet;
            newoutlet->xlh_next = curr;

            lastoutlet = newoutlet;
        }
        /* if outer inlet connection */
        else if(!s1 && s2)
        {
            /* check if inlet is already visited */
            int alr = 0;
            t_xletholder *it;
            for(it = inlets; it && !alr; )
            {
                alr = ((t.tr_inno ? (void *)t.tr_inlet : (void *)t.tr_ob2) == it->xlh_addr);
                if(!alr) it = it->xlh_next;
            }

            int type = obj_issignaloutlet(t.tr_ob, t.tr_outno);
            /* if so, we add the object and outlet number to the list and skip */
            if(alr)
            {
                binbuf_addv(it->xlh_conn, "ii",
                    glist_selectionindex(x, &t.tr_ob->ob_g, 0), t.tr_outno);
                if(it->xlh_type >= 0 && it->xlh_type != type) it->xlh_type = -1;
                continue;
            }

            /* create and fill inlet handling structure */
            t_xletholder *newinlet = (t_xletholder *)getbytes(sizeof(t_xletholder));
            newinlet->xlh_addr = (t.tr_inno ? (void *)t.tr_inlet : (void *)t.tr_ob2);
            newinlet->xlh_seln = glist_selectionindex(x, &t.tr_ob2->ob_g, 1);
            newinlet->xlh_xletn = t.tr_inno;
            newinlet->xlh_type = type;
            newinlet->xlh_x = t.tr_lx2;
            newinlet->xlh_y = t.tr_ly2;
            newinlet->xlh_conn = binbuf_new();
            binbuf_addv(newinlet->xlh_conn, "ii",
                glist_selectionindex(x, &t.tr_ob->ob_g, 0), t.tr_outno);
            /* insert structure regarding its coords */
            t_xletholder *prev = 0, *curr = inlets;
            while(curr && (t.tr_lx2 > curr->xlh_x ||
                (t.tr_lx2 == curr->xlh_x && t.tr_ly2 > curr->xlh_y)))
            {
                prev = curr;
                curr = curr->xlh_next;
            }
            if(prev) prev->xlh_next = newinlet;
            else inlets = newinlet;
            newinlet->xlh_next = curr;
        }
    }

    /* store outer connections in a binbuf */
    t_binbuf *conns = binbuf_new();
    int subnum = glist_selectionindex(x, 0, 0), nin = 0, nout = 0, xplac = 0;
    while(inlets)
    {
        if(inlets->xlh_type >= 0)
        {
            binbuf_addv(b, "ssiis;", gensym("#X"), gensym("obj"),
                X_PLAOFF/3 + xplac, Y_PLAOFF/3, (inlets->xlh_type ?
                    gensym("inlet~") : gensym("inlet")));

            xplac += (inlets->xlh_type ? 46 : 40) + XLETSGAP; //hardcoded
        }
        else
        {
            binbuf_addv(b, "ssiiss;", gensym("#X"), gensym("obj"),
                X_PLAOFF/3 + xplac, Y_PLAOFF/3, gensym("inlet~"), gensym("fwd"));

            xplac += 64 + XLETSGAP; //hardcoded

            binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
            numobj+nin, 1,
            inlets->xlh_seln, inlets->xlh_xletn);
        }

        binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
            numobj+nin, 0,
            inlets->xlh_seln, inlets->xlh_xletn);


        int nvec = binbuf_getnatom(inlets->xlh_conn);
        t_atom *vec = binbuf_getvec(inlets->xlh_conn);
        for(int i = 0; i < nvec/2; i++)
        {
            binbuf_addv(conns, "ssffii;", gensym("#X"), gensym("connect"),
                atom_getfloat(vec+(2*i)), atom_getfloat(vec+(2*i+1)), subnum, nin);
        }
        binbuf_free(inlets->xlh_conn);
        t_xletholder *tmp = inlets->xlh_next;
        freebytes(inlets, sizeof(t_xletholder));
        inlets = tmp;
        nin++;
    }
    xplac = 0;
    while(outlets)
    {
        binbuf_addv(b, "ssiis;", gensym("#X"), gensym("obj"),
            X_PLAOFF/3 + xplac, (maxy-miny) + Y_PLAOFF*5/3, (outlets->xlh_type ?
                gensym("outlet~") : gensym("outlet")));

        xplac += (outlets->xlh_type ? 52 : 46) + XLETSGAP; //hardcoded

        binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
            outlets->xlh_seln, outlets->xlh_xletn,
            numobj+nin+nout, 0);

        int nvec = binbuf_getnatom(outlets->xlh_conn);
        t_atom *vec = binbuf_getvec(outlets->xlh_conn);
        for(int i = 0; i < nvec/2; i++)
        {
            binbuf_addv(conns, "ssiiff;", gensym("#X"), gensym("connect"),
                subnum, nout, atom_getfloat(vec+(2*i)), atom_getfloat(vec+(2*i+1)));
        }
        binbuf_free(outlets->xlh_conn);
        t_xletholder *tmp = outlets->xlh_next;
        freebytes(outlets, sizeof(t_xletholder));
        outlets = tmp;
        nout++;
    }

    int m = (nin > nout ? nin : nout);
    int width = (IOWIDTH * m) * 2 - IOWIDTH;

    binbuf_addv(b, "ssiiss;", gensym("#X"), gensym("restore"),
        (minx+maxx)/2 - width/2, (miny+maxy)/2, gensym("pd"), gensym("[name]"));

    *object = b;
    *connections = conns;
}

static void canvas_encapsulate(t_canvas *x)
{
    /* check preconditions */
    // num selected objects > 0
    if (!x->gl_editor || !x->gl_editor->e_selection)
        return;

    canvas_undo_add(x, UNDO_SEQUENCE_START, "encapsulate", 0);
    /* cut selected objects using special copy method, based on canvas_cut */
    t_binbuf *object, *connections;
    canvas_undo_add(x, UNDO_CUT, "cut", canvas_undo_set_cut(x, UCUT_CUT));
    canvas_dofancycopy(x, &object, &connections);
    canvas_doclear(x);
    glob_preset_node_list_check_loc_and_update();
    scrollbar_update(x);

    /* subcanvas creation */
    canvas_dopaste(x, object);
    /* trace connections between unselectd objects and patch object */
    pd_bind(&x->gl_pd, gensym("#X"));
    binbuf_eval(connections, 0, 0, 0);
    pd_unbind(&x->gl_pd, gensym("#X"));

    canvas_undo_add(x, UNDO_CREATE, "create", canvas_undo_set_create(x));

    binbuf_free(object);
    binbuf_free(connections);

    /* select subpatch object */
    t_gobj *sub = glist_nth(x, glist_getindex(x, 0)-1);
    gobj_activate(sub, x, (0b1 << 31) | (0x0003 << 16) | 0x0009);

    canvas_undo_add(x, UNDO_SEQUENCE_END, "encapsulate", 0);
}

/* ------------------------------------------------- */

static int paste_onset;
static t_canvas *paste_canvas;

static void glist_donewloadbangs(t_glist *x)
{
    if (!sys_noloadbang && x->gl_editor)
    {
        t_selection *sel;
        for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            if (pd_class(&sel->sel_what->g_pd) == canvas_class)
                canvas_loadbang((t_canvas *)(&sel->sel_what->g_pd));
            /*else if (zgetfn(&sel->sel_what->g_pd, gensym("loadbang")))
                vmess(&sel->sel_what->g_pd, gensym("loadbang"), "f", LB_LOAD);*/
    }
}

static void canvas_paste_xyoffset(t_canvas *x)
{
    //t_selection *sel;
    //t_class *cl;
    //int resortin = 0;
    //int resortout = 0;

    //for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
    //{
        //gobj_displace(sel->sel_what, x, paste_xyoffset*10, paste_xyoffset*10);
        //cl = pd_class(&sel->sel_what->g_pd);
        //if (cl == vinlet_class) resortin = 1;
        //if (cl == voutlet_class) resortout = 1;
    //}
    canvas_displaceselection(x, paste_xyoffset*10, paste_xyoffset*10);

    //if (resortin) canvas_resortinlets(x);
    //if (resortout) canvas_resortoutlets(x);

    // alternative one-line implementation that
    // replaces the entire function
    //canvas_displaceselection(x, 10, 10);

    //paste_xyoffset++; //a part of original way
}

static void canvas_paste_atmouse(t_canvas *x)
{
    t_selection *sel;
    //fprintf(stderr,"paste_atmouse\n");
    /* use safe values for x1 and y1 which are essentially the same as
       xyoffset */
    int x1 = x->gl_editor->e_xwas+10, y1 = x->gl_editor->e_ywas+10, init = 0;
    t_float sx = 0.0, sy = 0.0;
    t_glist *g;
    t_text *t;

    /* find the initial offset--we use leftmost object as our reference */
    for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
    {
        //fprintf(stderr,"got selection\n");
        g = (t_glist *)sel->sel_what;
        if (pd_class(&((t_gobj *)g)->g_pd) == canvas_class && g->gl_isgraph)
        {
            /* hack: if any objects are GOPs re-select them, otherwise
               we may get stray nlets due to networked nature between
               the gui and the engine, so we select it explicitly here
               once again to prevent that from being a problem */
            gobj_select((t_gobj *)g, x, 1);
        }
        if (pd_class(&((t_gobj *)g)->g_pd) == scalar_class)
        {
            scalar_getbasexy((t_scalar *)g, &sx, &sy);
            x1 = (int)sx;
            y1 = (int)sy;            
        }
        else
        {
            t = (t_text *)g;
            if (!init)
            {
                x1 = t->te_xpix;
                y1 = t->te_ypix;
                init = 1;
            }
            else if ( t->te_xpix < x1 )
            {
                x1 = t->te_xpix;
            }
        }
    }
    /* redraw objects */
    canvas_undo_already_set_move = 1;
    canvas_displaceselection(x,
        (x->gl_editor->e_xwas)-x1-8, (x->gl_editor->e_ywas)-y1-8);
    //glist_setlastxy(x, (int)((x->gl_editor->e_xwas)+5-x1), (int)((x->gl_editor->e_ywas)-y1));
    canvas_startmotion(x);
}

extern void canvas_obj(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_howputnew(t_canvas *x, int *connectp, int *xpixp, int *ypixp,
    int *indexp, int *totalp);
extern int we_are_undoing;

static void canvas_dopaste(t_canvas *x, t_binbuf *b)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    //fprintf(stderr,"start dopaste\n");
    // ico@vt.edu: paste is enabled at startup (GUI-end problem),
    // so until we figure out why that is so, here we double-check the
    // sanity of the paste command
    if (binbuf_getnatom(b) == 0)
        return;
    do_not_redraw += 1;
    int was_dnr = do_not_redraw;
    
    t_gobj *g2;
    int dspstate = canvas_suspend_dsp(), nbox, count;
    int canvas_empty = 0;
    int offset = 1;

    /* first let's see if we are pasting into an empty canvas.
       this will be used below when pasting from copyfromexternalbuffer,
       usually text editor */
    if (!x->gl_list) canvas_empty = 1;

    //autopatching variables
    int connectme, xpix, ypix, indx, nobj;
    connectme = 0;

    canvas_editmode(x, 1.);
    /*  abolish potential displacing of object that may have been
        created with the first new object on canvas, but now we are
        pasting and therefore MA_MOVE should not apply to new objects
    */
    x->gl_editor->e_onmotion = MA_NONE;

    if (copyfromexternalbuffer && canvas_empty)
    {
        if (screenx2 && screeny2 && copiedfont)
        {
            x->gl_screenx1 = screenx1;
            x->gl_screenx2 = screenx1 + screenx2;
            x->gl_screeny1 = screeny1;
            x->gl_screeny2 = screeny1 + screeny2;
            gui_vmess("gui_canvas_change_geometry", "xiiii",
                x,
                (int)(x->gl_screenx2 - x->gl_screenx1),
                (int)(x->gl_screeny2 - x->gl_screeny1),
                (int)(x->gl_screenx1),
                (int)(x->gl_screeny1));
            // hardwired stretchval and whichstretch
            // until we figure out proper resizing
            canvas_dofont(x, copiedfont, 1, 1);
            //sys_vgui("pdtk_canvas_checkgeometry .x%zx\n", x);
            canvas_redraw(x);
        }
    }

    //if we have something selected in another canvas
    if (c_selection && c_selection != x)
    {
        offset = 0;
        glist_noselect(c_selection);
    }
    //if we are undoing, offset should be also 0
    if (we_are_undoing) offset = 0;

    //if we are pasting see if we can autopatch
    if (canvas_undo_name && !strcmp(canvas_undo_name, "paste"))
    {
        canvas_howputnew(x, &connectme, &xpix, &ypix, &indx, &nobj);
        //glist_noselect(x);
    }
    //else we are duplicating
    else glist_noselect(x);

    for (g2 = x->gl_list, nbox = 0; g2; g2 = g2->g_next) nbox++;
    
    /* found the end of the queue */
    paste_onset = nbox;
    paste_canvas = x;

    do_not_redraw = 0;
    pd_bind(&x->gl_pd, gensym("#X"));
    binbuf_eval(b, 0, 0, 0);
    pd_unbind(&x->gl_pd, gensym("#X"));
    do_not_redraw = was_dnr;

    /* select newly created objects */
    for (g2 = x->gl_list, count = 0; g2; g2 = g2->g_next, count++)
        if (count >= nbox)
            glist_select(x, g2);

    paste_canvas = 0;

    //fprintf(stderr,"dopaste autopatching? %d==%d %d\n",
    //    count, nbox, connectme);
    do_not_redraw -= 1;

    /* TODO: Ico: because figuring out exact position/size for a scalar
       is not simple to assess and besides, I am not even sure if we can
       have a scalar with an inlet, we currently ignore scalar autopatching */
    if (connectme == 1 &&
        pd_class(&(x->gl_editor->e_selection->sel_what)->g_pd) == scalar_class)
    {
        connectme = 0;
    }

    //if we are pasting only one object autoposition it below our selection
    if (count == nbox+1 && connectme == 1)
    {
        canvas_connect(x, indx, 0, nobj, 0);

        //is this universally safe? Not for scalars
        t_text *z = (t_text *)x->gl_editor->e_selection->sel_what;
        //fprintf(stderr,"%d %d %d %d\n", z->te_xpix, z->te_ypix, xpix, ypix);

        //calculate delta (since displace is always relative)
        int delta_x = xpix - z->te_xpix;
        int delta_y = ypix - z->te_ypix;

        //now displace it but without undo
        //(by spoofing canvas_undo_already_set_move)
        canvas_undo_already_set_move = 1;
        canvas_displaceselection(x, delta_x, delta_y);
        //reset canvas_undo_already_set_move
        canvas_undo_already_set_move = 0;
    }
    /* if we are pasting into a new window and this is not copied from external
       buffer OR if we are copying from external buffer and the current canvas
       is not empty */
    else if (canvas_undo_name && !strcmp(canvas_undo_name, "paste") &&
             !copyfromexternalbuffer ||
             copyfromexternalbuffer && !canvas_empty)
    {
        //if (!copyfromexternalbuffer) canvas_paste_xyoffset(x);
        if (!we_are_undoing) canvas_paste_atmouse(x);
        //fprintf(stderr,"doing a paste\n");
    }
    //else let's provide courtesy offset
    else if (!copyfromexternalbuffer && offset)
    {
        canvas_paste_xyoffset(x);
    }

    canvas_dirty(x, 1);
    //fprintf(stderr,"dopaste redraw %d\n", do_not_redraw);
    //if (!canvas_undo_name || !strcmp(canvas_undo_name, "duplicate")) {
        // need to redraw duplicated objects as
        // they need to be drawn with an offset
        // fprintf(stderr,"canvas_dopaste redraw objects\n");
        // canvas_redraw(x);
    //}

    scrollbar_update(x);
    if (!abort_when_pasting_from_external_buffer)
    {
        glist_donewloadbangs(x);
    }
    else
    {
        error("failed pasting correctly from external buffer, "
              "likely due to incomplete text selection. hopefully "
              "you saved your work... please get ready to crash...");
    }
    canvas_resume_dsp(dspstate);
    abort_when_pasting_from_external_buffer = 0;
    glob_preset_node_list_check_loc_and_update();
    //fprintf(stderr,"end dopaste\n");
}

static void canvas_paste(t_canvas *x)
{
    if (!x->gl_editor)
        return;

    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    // ico@vt.edu: prevent pasting in a toplevel garray window
    if (x->gl_list && glist_istoplevel(x) && canvas_hasarray(x))
        return;
    if (x->gl_editor->e_textedfor)
    {
            /* simulate keystrokes as if the copy buffer were typed in. */
//#if defined(MSW) || defined(__APPLE__)
            /* for Mac or Windows,  ask the GUI to send the clipboard down */
        //sys_gui("pdtk_pastetext\n");
//#else
            /* in X windows we kept the text in our own copy buffer */
/*        int i;
        for (i = 0; i < canvas_textcopybufsize; i++)
        {
            pd_vmess(&x->gl_gobj.g_pd, gensym("key"), "iii",
                1, canvas_textcopybuf[i]&0xff, 0);
        }*/
//#endif
    }
    // ico@vt.edu: need to check that the copy_binbuf is not null. This
    // currently prevents the crash when opening a new patch with nothing
    // in the buffer and pressing paste since the paste menu item is not
    // being properly initialized. This is probably a good safety check
    // anyhow. We will also have to investigate why the undo/paste menus
    // are not being properly initialized and fix them accordingly.
    else if (!clipboard_istext && copy_binbuf != NULL)
    {
        //canvas_setundo(x, canvas_undo_paste, canvas_undo_set_paste(x),
        //    "paste");
        canvas_undo_add(x, 5, "paste",
            (void *)canvas_undo_set_paste(x, 0, 0, 0));
        canvas_dopaste(x, copy_binbuf);
        //canvas_paste_xyoffset(x);
    }
}

static void canvas_duplicate(t_canvas *x)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    //if (x->gl_editor->e_onmotion == MA_NONE && x->gl_editor->e_selection)
    if (x->gl_editor->e_onmotion == MA_NONE && c_selection &&
        c_selection->gl_editor->e_selection)
    {
        /* Check if we are trying to duplicate an object that we just
           typed in and which has not been instantiated yet. If so,
           let's deselect it to instatiate it and then reselect it again.
           If this is the case, only one object will be selected. We
           temporarily borrow g object for this operation. */
        t_gobj *g;

        g = c_selection->gl_editor->e_selection->sel_what;
        if (!c_selection->gl_editor->e_selection->sel_next &&
            c_selection->gl_editor->e_textedfor &&
            pd_class(&g->g_pd) == text_class &&
            ((t_text *)x)->te_type == T_OBJECT)
        {
            //fprintf(stderr,
            //    "got uninitiated object we are trying to duplicate...\n");
            glist_deselect(x, g);
            // now we need to find the newly instantiated object and reselect it
            g = x->gl_list;
            if (g)
                while (g->g_next)
                    g = g->g_next;

        }

        //canvas_copy(x);
        //canvas_setundo(x, canvas_undo_paste, canvas_undo_set_paste(x),
        //    "duplicate");
        //canvas_dopaste(x, copy_binbuf);
        //canvas_paste_xyoffset(x);
        //canvas_dirty(x, 1);
        g = x->gl_list;
        if (g)
            while (g->g_next)
                g = g->g_next;
        canvas_copy(c_selection);
        canvas_undo_add(x, 5, "duplicate",
            (void *)canvas_undo_set_paste(x, 0, 1, (c_selection == x ? 1 : 0)));
        canvas_dopaste(x, copy_binbuf);
        //if (c_selection == x) //{
            /* we are in the same window */
            //canvas_setundo(x, canvas_undo_paste, canvas_undo_set_paste(x),
            //    "duplicate");
            //canvas_paste_xyoffset(x);
            //canvas_dirty(x, 1);
        //} else {
            //canvas_setundo(x, canvas_undo_paste, canvas_undo_set_paste(x),
            //    "duplicate");
            //canvas_dopaste(x, copy_binbuf);
            //canvas_paste_xyoffset(x);  
        //}
        canvas_dirty(x, 1);
        /* if we already have objects on the newly duplicated canvas,
           this will be invoked */
        if (g)
            g = g->g_next;
        else
        {
            /* this is if the duplicated object is the first one
               on the new canvas */
            g = x->gl_list;
        }
        while (g)
        {
            if (pd_class(&g->g_pd) == canvas_class &&
                ((t_canvas *)g)->gl_isgraph)
            {
                /* hack: if any objects are GOPs re-select them, otherwise
                   we may get stray unselected objects due to networked
                   nature between the gui and the engine, so we select it
                   explicitly here once again to prevent that from being
                   a problem */
                //fprintf(stderr,"post-duplicate reselect hack for gop objects\n");
                gobj_select(g, x, 1);
            }
            g = g->g_next;
        }
    }
}

static void canvas_selectall(t_canvas *x)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;
    
    t_gobj *y;
    if (!x->gl_edit)
        canvas_editmode(x, 1);

    if (x->gl_editor && x->gl_editor->e_textedfor)
    {
        /* only do this if exactly one item is selected. */
        if (x->gl_editor->e_selection->sel_what &&
            !x->gl_editor->e_selection->sel_next)
        {
            t_gobj *z = x->gl_editor->e_selection->sel_what;
            /* reactivate */
            gobj_activate(z, x, 1);
            /* make canvas dirty in case this was done at creation time */
            x->gl_editor->e_textdirty = 1;
        }
    }
    else
    {

        /* if everyone is already selected deselect everyone */
        if (!glist_selectionindex(x, 0, 0))
        {
            glist_noselect(x);
        }
        else
        {
            for (y = x->gl_list; y; y = y->g_next)
            {
                if (!glist_isselected(x, y))
                    glist_select(x, y);
            }
        }
    }
}

static void canvas_reselect(t_canvas *x)
{
    t_gobj *g, *gwas;
        /* if someone is text editing, and if only one object is 
        selected,  deselect everyone and reselect.  */
    if (x->gl_editor->e_textedfor)
    {
            /* only do this if exactly one item is selected. */
        if ((gwas = x->gl_editor->e_selection->sel_what) &&
            !x->gl_editor->e_selection->sel_next)
        {
            //int nobjwas = glist_getindex(x, 0);
            //int indx = canvas_getindex(x, x->gl_editor->e_selection->sel_what);
            glist_noselect(x);
            for (g = x->gl_list; g; g = g->g_next)
                if (g == gwas)
            {
                glist_select(x, g);
                return;
            }
                /* "gwas" must have disappeared; just search to the last
                object and select it */
            for (g = x->gl_list; g; g = g->g_next)
                if (!g->g_next)
                    glist_select(x, g);
        }
    }
    else if (x->gl_editor->e_selection &&
        !x->gl_editor->e_selection->sel_next)
            /* otherwise activate first item in selection */
            gobj_activate(x->gl_editor->e_selection->sel_what, x, 1);
}

void canvas_connect(t_canvas *x, t_floatarg fwhoout, t_floatarg foutno,
    t_floatarg fwhoin, t_floatarg finno)
{
    //fprintf(stderr,"canvas_connect\n");
    if (!x->gl_list)
    {
        post("paste error: no objects to connect, "
             "probably incomplete clipboard copy from an external source "
             "(e.g. from a text editor)");
        return;        
    }
    int whoout = fwhoout, outno = foutno, whoin = fwhoin, inno = finno;
    t_gobj *src = 0, *sink = 0;
    t_object *objsrc, *objsink;
    t_outconnect *oc, *oc2;
    int nin = whoin, nout = whoout;
    if (paste_canvas == x) whoout += paste_onset, whoin += paste_onset;
    for (src = x->gl_list; whoout; src = src->g_next, whoout--)
        if (!src->g_next) goto bad; /* bug fix thanks to Hannes */
    for (sink = x->gl_list; whoin; sink = sink->g_next, whoin--)
        if (!sink->g_next) goto bad;
    
        /* check they're both patchable objects */
    if (!(objsrc = pd_checkobject(&src->g_pd)) ||
        !(objsink = pd_checkobject(&sink->g_pd)))
            goto bad;

        /* check signal outlets don't try to connect to non-signal inlet,
           but only do so when there is no exception active due to
           autopatching */
    if (!connect_exception &&
        obj_issignaloutlet(objsrc, outno) &&
        !obj_issignalinlet(objsink, inno))
    {
        error("cannot connect signal outlet to control inlet");
        goto bad;
    }
        /* now check for illegal connections between preset_node object
           and other non-supported objects from node's first outlet
           (node's second outlet is for status info) */
    if (pd_class(&src->g_pd) == preset_node_class && outno == 0)
    {
        if (pd_class(&sink->g_pd) == message_class)
        {
            error("preset_node does not work with messages.");
            goto bad;
        }
        if (obj_noutlets(pd_checkobject(&sink->g_pd)) == 0)
        {
            error("preset_node does not work with objects with zero or "
                  "undefined number of outlets\n");
            goto bad;
        }
    }

        /* if object creation failed, make dummy inlets or outlets
        as needed */ 
    if (pd_class(&src->g_pd) == text_class && objsrc->te_type == T_OBJECT)
    {
        while (outno >= obj_noutlets(objsrc))
            outlet_new(objsrc, 0);
        //fprintf(stderr,"canvas_connect got fake outlets\n");
    }
    if (pd_class(&sink->g_pd) == text_class && objsink->te_type == T_OBJECT)
    {
        while (inno >= obj_ninlets(objsink))
            inlet_new(objsink, &objsink->ob_pd, 0, 0);
        //fprintf(stderr,"canvas_connect got fake inlets\n");
    }

    if (!canvas_isconnected(x, objsrc, outno, objsink, inno))
    {
        if (!(oc = obj_connect(objsrc, outno, objsink, inno))) goto bad;
        outconnect_setvisible(oc, 1);
        /* add auto-connect back to preset_node object
           (by this time we know we are connecting only to legal objects
           who have at least one outlet) */
        if (pd_class(&objsrc->ob_pd) == preset_node_class && outno == 0)
        {
            //fprintf(stderr,
            //   "canvas_connect: gotta do auto-connect back to preset_node\n");
            if (!canvas_isconnected(x, objsink, 0, objsrc, 0))
            {
                oc2 = obj_connect(objsink, 0, objsrc, 0);
                outconnect_setvisible(oc2, 0);
            }
        }
        if (glist_isvisible(x) &&
            (pd_class(&sink->g_pd) != preset_node_class ||
            (pd_class(&sink->g_pd) == preset_node_class &&
            pd_class(&src->g_pd) == message_class)))
        {
            //fprintf(stderr,"draw line\n");
            canvas_drawconnection(x, 0, 0, 0, 0, (t_int)oc, obj_issignaloutlet(objsrc, outno));
            /*sys_vgui(".x%zx.c create polyline %d %d %d %d -strokewidth %s "
                       "-stroke %s -tags {l%zx all_cords}\n",
                  glist_getcanvas(x), 0, 0, 0, 0,
                  (obj_issignaloutlet(objsrc, outno) ?
                      "$pd_colors(signal_cord_width)" :
                      "$pd_colors(control_cord_width)"),
                  (obj_issignaloutlet(objsrc, outno) ?
                      "$pd_colors(signal_cord)" :
                      "$pd_colors(control_cord)"), oc);*/
            canvas_fixlinesfor(x, objsrc);
        }
    }
    return;

bad:
    post("%s %d %d %d %d (%s->%s) connection failed", 
        x->gl_name->s_name, nout, outno, nin, inno,
            (src? class_getname(pd_class(&src->g_pd)) : "???"),
            (sink? class_getname(pd_class(&sink->g_pd)) : "???"));
}

/* new implementation works in such a way that it first tries to line up all
 * objects in the same line depending on the minimal distance between values
 * (e.g. if objects' y values are closer than x values the alignment will
 * happen vertically and vice-versa). If the objects already exhibit 0
 * difference across one axis, it will pick the top/left-most two objects and
 * use them as a reference for spatialization between the remaining selected
 * objects. any further tidy calls will be ignored */

// struct for storing spatially aware list of selected gobjects
typedef struct _sgobj
{
    t_gobj *s_g;
    int s_x1;
    int s_x2;
    int s_y1;
    int s_y2;
    struct _sgobj *s_next;
} t_sgobj;

static int sgobj_already_processed(t_gobj *y, t_sgobj *sg)
{
    while (sg)
    {
        if (sg->s_g == y)
            return(1);
        sg = sg->s_next;
    }
    return(0);
}

static int canvas_tidy_gobj_width(t_canvas *x, t_gobj *y)
{

    int w = 0;
    int x1, y1, x2, y2;
    gobj_getrect(y, x, &x1, &y1, &x2, &y2);
    w = x2 - x1;
    //fprintf(stderr,"width = %d\n", w);
    return(w);
}

static int canvas_tidy_gobj_height(t_canvas *x, t_gobj *y)
{

    int h = 0;
    int x1, y1, x2, y2;
    gobj_getrect(y, x, &x1, &y1, &x2, &y2);
    h = y2 - y1;
    //fprintf(stderr,"height = %d\n", h);
    return(h);
}

static void canvas_tidy(t_canvas *x)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    // if we have no editor, no selection, or only one object selected, return
    if (!x->gl_editor || !x->gl_editor->e_selection || !x->gl_editor->e_selection->sel_next) return;

    //fprintf(stderr,"canvas_tidy\n");
    t_gobj *y;
    t_text *yt, *rightmost_t = NULL, *topmost_t = NULL;
    int h, v;   // horizontal, vertical
    int hs, vs; // horizontal respacing, vertical respacing
    t_gobj *leftmost, *rightmost, *topmost, *bottommost;
    int x1, x2, y1, y2;
    //int cox1, cox2, coy1, coy2; // comparing object x and y dimensions
    int dx, dy; // displacement variables
    int i; // generic counter var
    t_sgobj *sg, *tmpsg; // list of objects ordered spatially
    int spacing = 0; // spacing between objects on respacing (adjustable)
    int delta = 0;

    dx = dy = h = v = hs = vs = 0;

    t_selection *sel = x->gl_editor->e_selection;
    y = sel->sel_what;
    yt = (t_text *)y;
    x1 = yt->te_xpix;
    x2 = x1;
    y1 = yt->te_ypix;
    y2 = y1;

    leftmost = y;
    topmost = y;
    rightmost = y;
    bottommost = y;

    sel = sel->sel_next;

    // first find out whether we are dealing with horizontal or vertical
    // alignment or spatialization
    while (sel)
    {
        y = sel->sel_what;
        yt = (t_text *)y;

        if (yt->te_xpix < x1)
        {
            x1 = yt->te_xpix;
        }
        
        if (yt->te_xpix > x2)
        {
            x2 = yt->te_xpix;
        }

        if (yt->te_ypix < y1)
        {
            y1 = yt->te_ypix;
        }
        
        if (yt->te_ypix > y2)
        {
            y2 = yt->te_ypix;
        }

        sel = sel->sel_next;
    }
    if (x2-x1 != 0 && x2-x1 < y2-y1)
        v = 1; //horizontal
    else if (y2-y1 != 0 && y2-y1 <= x2-x1)
        h = 1; //vertical (takes precedence over vertical if two are equal)
    else if (x2-x1 == 0)
        vs = 1; //vertically aligned respacing
    else if (y2-y1 == 0)
        hs = 1; //horizontally aligned respacing

    //fprintf(stderr,"h=%d v=%d hs=%d vs=%d\n", h, v, hs, vs);

    // now find leftmost, topmost, rightmost, and bottommost object
    sel = x->gl_editor->e_selection;
    while (sel)
    {
        y = sel->sel_what;
        yt = (t_text *)y;
        if(yt->te_xpix == x1)
        {
            leftmost = y;
            //fprintf(stderr,"leftmost %d\n", x1);
        }
        if(yt->te_xpix == x2)
        {
            rightmost = y;
            rightmost_t = (t_text *)y;
            //fprintf(stderr,"rightmost %d\n", x2);
        }
        if(yt->te_ypix == y2)
        {
            topmost = y;
            topmost_t = (t_text *)y;
            //fprintf(stderr,"topmost %d\n", y2);
        }
        if(yt->te_ypix == y1)
        {
            bottommost = y;
            //fprintf(stderr,"bottommost %d\n", y1);
        }
        sel = sel->sel_next;    
    }

    if (h == 1)
    {
        // horizontal tidy (everyone lines up to the y of the leftmost object)
        canvas_undo_add(x, 4, "motion", canvas_undo_set_move(x, 1));
        sel = x->gl_editor->e_selection;
        yt = (t_text *)leftmost;
        dy = yt->te_ypix;

        while (sel)
        {
            y = sel->sel_what;
            yt = (t_text *)y;

            //fprintf(stderr,"displace %d\n", dy - yt->te_ypix);
            gobj_displace(y, x, 0, dy - yt->te_ypix);

            sel = sel->sel_next;
        }
    }
    else if (v == 1)
    {
        // vertical tidy (everyone lines up to the x of the bottommost object,
        // since y axis is inverted)
        canvas_undo_add(x, 4, "motion", canvas_undo_set_move(x, 1));
        sel = x->gl_editor->e_selection;
        yt = (t_text *)bottommost;
        dx = yt->te_xpix;

        while (sel)
        {
            y = sel->sel_what;
            yt = (t_text *)y;

            //fprintf(stderr,"displace %d\n", dx - yt->te_xpix);
            gobj_displace(y, x, dx - yt->te_xpix, 0);

            sel = sel->sel_next;
        }
    }
    else
    {
        // first check if we have more than 2 objects selected
        // (otherwise there is no point in doing this
        sel = x->gl_editor->e_selection;
        i = 1;
        while (sel->sel_next)
        {
            i++;
            sel = sel->sel_next;
        }

        // we now know we will do a respace which means we need to
        // first order objects according to their physical location
        // (horizontal or vertical)
        if (hs == 1)
        {

            t_gobj *next_right = NULL;
            t_text *next_right_t = NULL;

            yt = (t_text *)rightmost;
            sg = (t_sgobj *)getbytes(sizeof(*sg));
            sg->s_g = rightmost;
            sg->s_x1 = yt->te_xpix;
            sg->s_x2 = yt->te_xpix + canvas_tidy_gobj_width(x, y);
            sg->s_y1 = yt->te_ypix;
            sg->s_y2 = yt->te_ypix + canvas_tidy_gobj_height(x, y);
            sg->s_next = NULL;

            //fprintf(stderr,"%d: x=%d y=%d width=%d height=%d\n",
            //    i, yt->te_xpix, yt->te_ypix, canvas_tidy_gobj_width(x, y),
            //    canvas_tidy_gobj_height(x, y)); 

            i--;

            while (i)
            {
                //fprintf(stderr,"i=%d\n", i);
                sel = x->gl_editor->e_selection;
                while (sel)
                {
                    y = sel->sel_what;
                    yt = (t_text *)y;

                    //fprintf(stderr, "already processed ? %d ... x=%d y=%d\n",
                    //    sgobj_already_processed(y, sg),
                    //    yt->te_xpix, yt->te_ypix);

                    // we need to avoid duplicates
                    if (!sgobj_already_processed(y, sg))
                    {
                        if (!next_right && yt->te_xpix <= rightmost_t->te_xpix)
                        {
                            next_right = y;
                            next_right_t = yt;
                        }
                        else if (next_right &&
                                 yt->te_xpix >= next_right_t->te_xpix &&
                                 yt->te_xpix <= rightmost_t->te_xpix)
                        {
                            next_right = y;
                            next_right_t = yt;
                        }
                    }

                    sel = sel->sel_next;
                }

                tmpsg = (t_sgobj *)getbytes(sizeof(*sg));
                tmpsg->s_g = next_right;
                tmpsg->s_x1 = next_right_t->te_xpix;
                tmpsg->s_x2 = next_right_t->te_xpix + canvas_tidy_gobj_width(x, next_right);
                tmpsg->s_y1 = next_right_t->te_ypix;
                tmpsg->s_y2 = next_right_t->te_ypix + canvas_tidy_gobj_height(x, next_right);
                tmpsg->s_next = sg;
                sg = tmpsg;

                //fprintf(stderr,"%d: x=%d y=%d width=%d height=%d\n",
                //    i, next_right_t->te_xpix, next_right_t->te_ypix,
                //    canvas_tidy_gobj_width(x, next_right),
                //    canvas_tidy_gobj_height(x, next_right)); 

                rightmost = next_right;
                rightmost_t = next_right_t;
                next_right = NULL;    

                i--;
            }

            //fprintf(stderr,"got this far\n");
            /* now let's traverse the new list and find minimal spacing and
               use that as our reference if spacing is anywhere less than 0
               (meaning objects overlap), use next legal value one that is
               greater than 0. If all values are < 0 then use default value
               (10). */
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                //fprintf(stderr,"calculating spacing from: %d and %d\n",
                //    tmpsg->s_next->s_x1, tmpsg->s_x2);
                if (tmpsg->s_next->s_x1 > tmpsg->s_x2 &&
                    (spacing <= 0 ||
                     (spacing > 0 && tmpsg->s_next->s_x1 -
                         tmpsg->s_x2 < spacing)))
                {
                    spacing = tmpsg->s_next->s_x1 - tmpsg->s_x2;
                }
                //fprintf(stderr,"spacing = %d\n", spacing);
                tmpsg = tmpsg->s_next;
            }
            if (spacing <= 0)
            {
#ifdef PDL2ORK
                if (sys_k12_mode)
                    spacing = 25;
                else
#endif
                spacing = 5;
            }

            //fprintf(stderr,"final spacing = %d\n", spacing);

            //fprintf(stderr,"0...\n");

            // now change all values in the list to their target values
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                //fprintf(stderr,"adjusting %d to %d + %d\n",
                //    tmpsg->s_next->s_x1, tmpsg->s_x2, spacing);
                delta = tmpsg->s_next->s_x1 - (tmpsg->s_x2 + spacing);
                tmpsg->s_next->s_x1 = tmpsg->s_next->s_x1 - delta;
                tmpsg->s_next->s_x2 = tmpsg->s_next->s_x2 - delta;
                tmpsg = tmpsg->s_next;
            }

            //fprintf(stderr,"1...\n");

            // create an undo checkpoint
            canvas_undo_add(x, 4, "motion", canvas_undo_set_move(x, 1));

            //fprintf(stderr,"2...\n");

            // reposition all objects
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                yt = (t_text *)tmpsg->s_next->s_g;
                //fprintf(stderr,"displace: %d %d\n",
                //    tmpsg->s_next->s_x1, yt->te_xpix);
                gobj_displace(tmpsg->s_next->s_g, x,
                    tmpsg->s_next->s_x1 - yt->te_xpix, 0);
                tmpsg = tmpsg->s_next;
            }

            //fprintf(stderr,"3...\n");

            // free the temporary list of spatialized objects
            while (sg)
            {
                tmpsg = sg->s_next;
                freebytes(sg, sizeof(*sg));
                sg = tmpsg;
            }
        }
        else if (vs == 1)
        {

            t_gobj *next_top = NULL;
            t_text *next_top_t = NULL;

            yt = (t_text *)topmost;
            sg = (t_sgobj *)getbytes(sizeof(*sg));
            sg->s_g = topmost;
            sg->s_x1 = yt->te_xpix;
            sg->s_x2 = yt->te_xpix + canvas_tidy_gobj_width(x, y);
            sg->s_y1 = yt->te_ypix;
            sg->s_y2 = yt->te_ypix + canvas_tidy_gobj_height(x, y);
            sg->s_next = NULL;

            //fprintf(stderr,"%d: x=%d y=%d width=%d height=%d\n",
            //    i, yt->te_xpix, yt->te_ypix, canvas_tidy_gobj_width(x, y),
            //    canvas_tidy_gobj_height(x, y)); 

            i--;

            while (i)
            {
                //fprintf(stderr,"i=%d\n", i);
                sel = x->gl_editor->e_selection;
                while (sel)
                {
                    y = sel->sel_what;
                    yt = (t_text *)y;

                    //fprintf(stderr, "already processed ? %d ... x=%d y=%d\n",
                    //    sgobj_already_processed(y, sg),
                    //    yt->te_xpix, yt->te_ypix);

                    // we need to avoid duplicates
                    if (!sgobj_already_processed(y, sg))
                    {
                        if (!next_top && yt->te_ypix <= topmost_t->te_ypix)
                        {
                            next_top = y;
                            next_top_t = yt;
                        }
                        else if (next_top &&
                                 yt->te_ypix >= next_top_t->te_ypix &&
                                 yt->te_ypix <= topmost_t->te_ypix)
                        {
                            next_top = y;
                            next_top_t = yt;                        
                        }
                    }

                    sel = sel->sel_next;
                }

                tmpsg = (t_sgobj *)getbytes(sizeof(*sg));
                tmpsg->s_g = next_top;
                tmpsg->s_x1 = next_top_t->te_xpix;
                tmpsg->s_x2 = next_top_t->te_xpix + canvas_tidy_gobj_width(x, next_top);
                tmpsg->s_y1 = next_top_t->te_ypix;
                tmpsg->s_y2 = next_top_t->te_ypix + canvas_tidy_gobj_height(x, next_top);
                tmpsg->s_next = sg;
                sg = tmpsg;

                //fprintf(stderr,"%d: x=%d y=%d width=%d height=%d\n",
                //    i, next_top_t->te_xpix, next_top_t->te_ypix,
                //    canvas_tidy_gobj_width(x, next_top),
                //    canvas_tidy_gobj_height(x, next_top)); 

                topmost = next_top;
                topmost_t = next_top_t;
                next_top = NULL;    

                i--;
            }

            //fprintf(stderr,"got this far\n");
            /* now let's traverse the new list and find minimal spacing
               and use that as our reference if spacing is anywhere less
               than 0 (meaning objects overlap), use next legal value one
               that is greater than 0. If all values are < 0 then use
               default value (10). */
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                //fprintf(stderr,"calculating spacing from: %d and %d\n",
                //    tmpsg->s_next->s_y1, tmpsg->s_y2);
                if (tmpsg->s_next->s_y1 > tmpsg->s_y2 &&
                    (spacing <= 0 ||
                     (spacing > 0 && tmpsg->s_next->s_y1 -
                         tmpsg->s_y2 < spacing)))
                {
                    spacing = tmpsg->s_next->s_y1 - tmpsg->s_y2;
                }
                //fprintf(stderr,"spacing = %d\n", spacing);
                tmpsg = tmpsg->s_next;
            }
            if (spacing <= 0)
            {
#ifdef PDL2ORK
                if (sys_k12_mode)
                    spacing = 25;
                else
#endif
                spacing = 5;
            }

            //fprintf(stderr,"final spacing = %d\n", spacing);

            //fprintf(stderr,"0...\n");

            // now change all values in the list to their target values
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                //fprintf(stderr,"adjusting %d to %d + %d\n",
                //    tmpsg->s_next->s_y1, tmpsg->s_y2, spacing);
                delta = tmpsg->s_next->s_y1 - (tmpsg->s_y2 + spacing);
                tmpsg->s_next->s_y1 = tmpsg->s_next->s_y1 - delta;
                tmpsg->s_next->s_y2 = tmpsg->s_next->s_y2 - delta;
                tmpsg = tmpsg->s_next;
            }

            //fprintf(stderr,"1...\n");

            // create an undo checkpoint
            canvas_undo_add(x, 4, "motion", canvas_undo_set_move(x, 1));

            //fprintf(stderr,"2...\n");

            // reposition all objects
            tmpsg = sg;
            while (tmpsg->s_next)
            {
                yt = (t_text *)tmpsg->s_next->s_g;
                //fprintf(stderr,"displace: %d %d\n",
                //    tmpsg->s_next->s_y1, yt->te_ypix);
                gobj_displace(tmpsg->s_next->s_g, x, 0,
                    tmpsg->s_next->s_y1 - yt->te_ypix);
                tmpsg = tmpsg->s_next;
            }

            //fprintf(stderr,"3...\n");

            // free the temporary list of spatialized objects
            while (sg)
            {
                tmpsg = sg->s_next;
                freebytes(sg, sizeof(*sg));
                sg = tmpsg;
            }
        }

    }
    canvas_dirty(x, 1);
    scrollbar_update(x);
}

void glob_key(void *dummy, t_symbol *s, int ac, t_atom *av)
{
        /* canvas_editing can be zero; canvas_key checks for that */
    canvas_key(canvas_editing, s, ac, av);
}

void glob_pastetext(void *dummy, t_symbol *s, int ac, t_atom *av)
{
    //fprintf(stderr,"glob_pastetext %s\n", s->s_name);
    canvas_key(canvas_editing, s, ac-1, av+1);
    if ((int)atom_getfloat(av) == 1)
    {
        //fprintf(stderr,"force getscroll\n");
        scrollbar_update(canvas_editing);
    }
}

void canvas_editmode(t_canvas *x, t_floatarg fyesplease)
{
    //post("canvas_editmode %lx %f", x, fyesplease);

    // ico@vt.edu 2022-11-14: first check if we have a parent
    // canvas that is not editable (in which case we are not
    // editable either, and therefore bail)
    if (!canvas_is_editable(x))
        return;

    /* now, check if this is a canvas hosting an array and if so,
       refuse to add any further objects. we allow edit mode on the
       subpatches that have only scalars, as that allows for their
       repositioning/deletion/etc.
    */
    if (canvas_hasarray(x) || x->gl_obj.ob_pd == array_define_class)
    {
        //post("canvas_editmode returning (hasarray or array define)");
        return;
    }

    int yesplease = fyesplease;
    if (yesplease && x->gl_edit)
    {
        //if (x->gl_edit && glist_isvisible(x) && glist_istoplevel(x))
        //    canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        return;
    }
    x->gl_edit = !x->gl_edit;
    // make sure to exit temporary run mode here
    x->gl_edit_save = 0;
    if (x->gl_edit && glist_isvisible(x) && glist_istoplevel(x)) {
        //dpsaha@vt.edu add the resize blobs on GOP
        t_gobj *g;
        t_object *ob;
        if (x->gl_goprect)    canvas_draw_gop_resize_hooks(x);
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        // ico@vt.edu 2022-12-03: disabled drawing of the comment
        // border in edit mode, as that is unnecessary and results
        // in redundant borders being drawn every time we (re)enter
        // the edit mode. instead we draw them once at vis time,
        // and then handle visibility inside the css file. 
        /*for (g = x->gl_list; g; g = g->g_next)
            if ((ob = pd_checkobject(&g->g_pd)) && ob->te_type == T_TEXT)
        {
            t_rtext *y = glist_findrtext(x, ob);
            text_drawborder(ob, x,
                rtext_gettag(y), rtext_width(y), rtext_height(y), 1);
        }*/
    }
    else
    {
        //fprintf(stderr,"we are out of edit\n");
        glist_noselect(x);
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
            /* Don't need this anymore, as we can control comment appearance
               with CSS. */
            //sys_vgui(".x%zx.c delete commentbar\n", glist_getcanvas(x));
            // jsarlo
            if (x->gl_editor->canvas_cnct_inlet_tag[0] != 0)
            {
                canvas_nlet_conf(x,'i');
                x->gl_editor->canvas_cnct_inlet_tag[0] = 0;                  
            }
            if (x->gl_editor->canvas_cnct_outlet_tag[0] != 0)
            {
                canvas_nlet_conf(x,'o');
                x->gl_editor->canvas_cnct_outlet_tag[0] = 0;                  
            }
            if(x->gl_editor && x->gl_editor->gl_magic_glass)
            {
                magicGlass_unbind(x->gl_editor->gl_magic_glass);
                magicGlass_hide(x->gl_editor->gl_magic_glass);
            }
                // end jsarlo
            //dpsaha@vt.edu called to delete the GOP_blob
            if (x->gl_goprect)        canvas_draw_gop_resize_hooks(x);
        }
        if (glist_isvisible(x))
            canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
    }
    if (glist_isvisible(x))
    {
        //post("canvas_editmode... %lx %d", x, x->gl_edit);
        int edit = /*!glob_ctrl && */x->gl_edit;
        gui_vmess("gui_canvas_set_editmode", "xi",
            glist_getcanvas(x),
            edit);
    }
}

void canvas_query_editmode(t_canvas *x)
{
    //post("canvas_query_editmode... %d", x->gl_edit);
    int edit = /*!glob_ctrl && */x->gl_edit;
    gui_vmess("gui_canvas_set_editmode", "xi",
        glist_getcanvas(x),
        edit);
}

// jsarlo
void canvas_magicglass(t_canvas *x, t_floatarg fyesplease)
{
    // ico@vt.edu 2022-11-14: check if we are editable, otherwise bail
    if (!canvas_is_editable(x))
        return;

    int yesplease = fyesplease;
    if (yesplease && x->gl_editor &&
        magicGlass_isOn(x->gl_editor->gl_magic_glass))
        return;
    if (!magicGlass_isOn(x->gl_editor->gl_magic_glass))
    {
        canvas_editmode(x, 1.);
        magicGlass_setOn(x->gl_editor->gl_magic_glass, 1);
        if (magicGlass_bound(x->gl_editor->gl_magic_glass))
        {
            magicGlass_show(x->gl_editor->gl_magic_glass);
        }
    }
    else {
        magicGlass_setOn(x->gl_editor->gl_magic_glass, 0);
        magicGlass_hide(x->gl_editor->gl_magic_glass);
    }
    gui_vmess("gui_canvas_set_cordinspector", "xi",
        glist_getcanvas(x),
        magicGlass_isOn(x->gl_editor->gl_magic_glass));
}
// end jsarlo

/* Not porting this... will do tooltips GUI-side instead */
void canvas_tooltips(t_canvas *x, t_floatarg fyesplease)
{
    //fprintf(stderr,"canvas_tooltips %f\n", fyesplease);
    int yesplease = fyesplease;
    if (yesplease && tooltips)
        return;
    if (!tooltips)
    {
        if (!sys_k12_mode) canvas_editmode(x, 1.);
        tooltips = 1;
    }
    else {
        tooltips = 0;
        tooltip_erase(x);
    }
    //sys_vgui("pdtk_canvas_tooltips .x%zx %d\n",
    //    x, tooltips);
}

    /* called by canvas_font below */
static void canvas_dofont(t_canvas *x, t_floatarg font, t_floatarg xresize,
    t_floatarg yresize)
{
    t_gobj *y;
    x->gl_font = font;
    /*if (x->gl_isgraph && !canvas_isabstraction(x) &&
        (xresize != 1 || yresize != 1) && !glist_istoplevel(x))
    {
        vmess(&x->gl_pd, gensym("menu-open"), "");
    }*/
    if (xresize != 1 || yresize != 1)
    {
        for (y = x->gl_list; y; y = y->g_next)
        {
            int x1, x2, y1, y2, nx1, ny1;
            gobj_getrect(y, x, &x1, &y1, &x2, &y2);
            nx1 = x1 * xresize + 0.5;
            ny1 = y1 * yresize + 0.5;
            if (pd_class(&y->g_pd) != scalar_class)
            {
                //fprintf(stderr,"dofont gobj displace %zx %d %d %d %d : "
                //               "%f %f : %d %d\n",
                //    y, x1, x2, y1, y2, xresize, yresize, nx1, ny1);
                gobj_displace(y, x, nx1-x1, ny1-y1);
            }
        }
    }
    if (glist_isvisible(x))
    {
        //fprintf(stderr,"glist_redraw %zx\n", x);
        if (x->gl_editor && magicGlass_isOn(x->gl_editor->gl_magic_glass))
            magicGlass_hide(x->gl_editor->gl_magic_glass);
        glist_redraw(x);
    }
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == canvas_class
            && !canvas_isabstraction((t_canvas *)y))
                canvas_dofont((t_canvas *)y, font, xresize, yresize);
    if (glist_isvisible(x))
        scrollbar_update(x);
}

    /* canvas_menufont calls up a dialog which calls this back */
static void canvas_font(t_canvas *x, t_floatarg font, t_floatarg oldfont,
    t_floatarg resize, t_floatarg noundo)
{
    t_float realresize;
    t_canvas *x2 = canvas_getrootfor(x);
    if (!resize)
        realresize = 1;
    else
        realresize = (t_float)sys_fontwidth(font) /
            (t_float)sys_fontwidth(x2->gl_font);

    if (!noundo)
    {
        if (!oldfont && font != x2->gl_font)
            canvas_undo_add(x, 11, "font",
                canvas_undo_set_font(x, x2->gl_font));
        else if (oldfont != font)
            canvas_undo_add(x, 11, "font", canvas_undo_set_font(x, oldfont));
    }

    canvas_dirty(x2, 1);

    canvas_dofont(x2, font, realresize, realresize);
}

static void canvas_zoom(t_canvas *x, t_floatarg zoom)
{
    x->gl_zoom = zoom;
    //post("store zoom level: %d", x->gl_zoom);
}

void glist_getnextxy(t_glist *gl, int *xpix, int *ypix)
{
    if (canvas_last_glist == gl)
        *xpix = canvas_last_glist_x, *ypix = canvas_last_glist_y;
    else *xpix = *ypix = 20;
}

void glist_setlastxy(t_glist *gl, int xval, int yval)
{
    canvas_last_glist = gl;
    canvas_last_glist_x = xval;
    canvas_last_glist_y = yval;
}

void glist_setlastxymod(t_glist *gl, int xval, int yval, int mod)
{
	//post("setlastxymod %d %d", xval, yval);
    canvas_last_glist = gl;
    canvas_last_glist_x = xval;
    canvas_last_glist_y = yval;
    canvas_last_glist_mod = mod;
}

/* Not porting... will do tooltips GUI-side... */
static void canvas_enterobj(t_canvas *x, t_symbol *item, t_floatarg xpos,
    t_floatarg ypos, t_floatarg xletno)
{
    if (x->gl_editor->e_onmotion == MA_MOVE) { return; }
    //t_symbol *name = 0, *helpname, *dir;
    int yoffset = 0, xoffset = 0;
    if (item == gensym("inlet"))
    {
        yoffset = 1;
        xoffset = xletno==0 ? 1 : -1;
    }
    else if (item == gensym("outlet"))
    {
        yoffset = -1;
        xoffset = xletno== 0 ? 1 : -1;
    }
    int x1, y1, x2, y2;
    t_gobj *g;
    if (g = canvas_findhitbox(x, xpos+xoffset, ypos+yoffset,
    &x1, &y1, &x2, &y2))
    {
        if (pd_class((t_pd *)g)==canvas_class ?
            canvas_isabstraction((t_canvas *)g) : 0)
        {
            //t_canvas *z = (t_canvas *)g;
            //name = z->gl_name;
            //helpname = z->gl_name;
            //dir = canvas_getdir(z);
        }
        else
        {
            //name = g->g_pd->c_name;
            //helpname = g->g_pd->c_helpname;
            //dir = g->g_pd->c_externdir;
        }
        //sys_vgui("pdtk_gettip .x%zx.c %s %d "
        //"[list %s] [list %s] [list %s]\n",
        //x, item->s_name, (int)xletno,
        //name->s_name, helpname->s_name, dir->s_name);
    }
}

/* Not porting... will do GUI-side */
static void canvas_tip(t_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    if (s == gensym("echo"))
    return;
    if (argv->a_type != A_FLOAT)
        error("canvas_tip: bad argument");
    else
    {
        //sys_vgui("pdtk_tip .x%zx.c 1", x);
        //t_atom *at = argv;
        int i;
        for (i=0; i<argc; i++)
        {
        //if (at[i].a_type == A_FLOAT)
        //    sys_vgui(" %g", at[i].a_w.w_float);
        //else if (at[i].a_type == A_SYMBOL)
        //    sys_vgui(" %s", at[i].a_w.w_symbol->s_name);
        }
        //sys_gui("\n");
    }
}

t_binbuf *newobjbuf;
static void canvas_addtobuf(t_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!newobjbuf)
        newobjbuf = binbuf_new();
    binbuf_add(newobjbuf, argc, argv);
}

static void canvas_buftotext(t_canvas *x)
{
    t_gobj *y;
    t_rtext *rtext;
    if (!x->gl_editor) return;
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (glist_isselected(x, y) && (rtext = glist_findrtext(x, (t_text *)y)))
        {
            int length;
            char *buf;
            t_binbuf *b = binbuf_new();
            binbuf_restore(b, binbuf_getnatom(newobjbuf), binbuf_getvec(newobjbuf));
            if (((t_text *)y)->te_type == T_TEXT)
                binbuf_getrawtext(b, &buf, &length);
            else
                binbuf_gettext(b, &buf, &length);
            /* We hand off "buf" to rtext as its x_buf member, and "length"
               as x_bufsize. Pd will handle deallocation of those members
               automatically, so we don't need to free the "buf" here. */
            rtext_settext(rtext, buf, length);
            /*
            post("canvas_buftotext check dirty:");
            binbuf_print(b);
            post("-----");
            binbuf_print(((t_text *)y)->te_binbuf);
            */
            /* Here we are abusing binbuf_match-- it was written only to see
               if a subset of a binbuf matches a larger one. So we have to
               also compare the size of both binbufs to tell if it is an
               exact match. */
            if (binbuf_match(((t_text *)y)->te_binbuf, b, 1) &&
                binbuf_getnatom(((t_text *)y)->te_binbuf) == binbuf_getnatom(b))
                x->gl_editor->e_textdirty = 0;
            else
                x->gl_editor->e_textdirty = 1;
            binbuf_free(b);
            // Set the dirty flag only if we've changed the rtext content...
            if (x->gl_editor->e_textdirty == 1)
                canvas_dirty(x, 1);
            x->gl_editor->e_onmotion = MA_NONE; // undo any mouse actions
            break;
        }
    }
    /* Clear the buffer */
    binbuf_clear(newobjbuf);
}

void g_editor_setup(void)
{
/* ------------------------ events ---------------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_mousedown, gensym("mouse"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mouseup, gensym("mouseup"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mouseup_fake,
        gensym("mouseup_fake"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mousedown_middle,
        gensym("mouse-2"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mousewheel,
        gensym("legacy_mousewheel"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_key, gensym("key"),
        A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_text_activated_motion,
    	gensym("text_activated_motion"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_noselect,
        gensym("noselect"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_enterobj, gensym("enter"),
        A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_tip, gensym("tip"),
    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_tip, gensym("echo"),
        A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_addtobuf,
        gensym("obj_addtobuf"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_buftotext,
        gensym("obj_buftotext"), A_NULL);
/* ------------------------ menu actions ---------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_menuclose,
        gensym("menuclose"), A_DEFFLOAT, 0);
    class_addmethod(canvas_class, (t_method)canvas_cut,
        gensym("cut"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_copy,
        gensym("copy"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_paste,
        gensym("paste"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_duplicate,
        gensym("duplicate"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_selectall,
        gensym("selectall"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_reselect,
        gensym("reselect"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_undo_undo,
        gensym("undo"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_undo_redo,
        gensym("redo"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_tidy,
        gensym("tidy"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_encapsulate,
        gensym("encapsulate"), A_NULL);
    //class_addmethod(canvas_class, (t_method)canvas_texteditor,
    //    gensym("texteditor"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_editmode,
        gensym("editmode"), A_DEFFLOAT, A_NULL);
    // jsarlo
    class_addmethod(canvas_class, (t_method)canvas_magicglass,
        gensym("magicglass"), A_DEFFLOAT, A_NULL);
    //end jsarlo
    class_addmethod(canvas_class, (t_method)canvas_tooltips,
        gensym("tooltips"), A_DEFFLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_print,
        gensym("print"), A_SYMBOL, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_menufont,
        gensym("menufont"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_init_menu,
        gensym("updatemenu"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_font,
        gensym("font"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_zoom,
        gensym("zoom"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find,
        gensym("find"), A_SYMBOL, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find_again,
        gensym("findagain"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find_parent,
        gensym("findparent"), A_DEFFLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_done_popup,
        gensym("done-popup"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_donecanvasdialog,
        gensym("donecanvasdialog"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_arraydialog,
        gensym("arraydialog"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_copyfromexternalbuffer,
        gensym("copyfromexternalbuffer"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_reset_copyfromexternalbuffer,
        gensym("reset_copyfromexternalbuffer"), A_NULL);
/* -------------- connect method used in reading files ------------------ */
    class_addmethod(canvas_class, (t_method)canvas_connect,
        gensym("connect"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);

    class_addmethod(canvas_class, (t_method)canvas_disconnect,
        gensym("disconnect"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
/* -------------- copy buffer ------------------ */
    copy_binbuf = binbuf_new();

    abstracthandler_setup();
}

void canvas_editor_for_class(t_class *c)
{
    class_addmethod(c, (t_method)canvas_mousedown, gensym("mouse"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_mouseup, gensym("mouseup"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_key, gensym("key"),
        A_GIMME, A_NULL);
    class_addmethod(c, (t_method)canvas_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);

/* ------------------------ menu actions ---------------------------- */
    class_addmethod(c, (t_method)canvas_menuclose,
        gensym("menuclose"), A_DEFFLOAT, 0);
    class_addmethod(c, (t_method)canvas_find_parent,
        gensym("findparent"), A_NULL);
}

void g_editor_newpdinstance(void)
{
    // Have not ported multi-instance editor from pure-data
}

void g_editor_freepdinstance(void)
{
    // Have not ported multi-instance editor from pure-data
}