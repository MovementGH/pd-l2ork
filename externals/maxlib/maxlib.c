/* --------------------------  maxlib  ---------------------------------------- */
/*                                                                              */
/* maxlib :: music analysis extensions library.                                 */
/* Written by Olaf Matthes <olaf.matthes@gmx.de>                                */
/* Get source at http://www.akustische-kunst.org/puredata/maxlib/               */
/*                                                                              */
/* This program is free software; you can redistribute it and/or                */
/* modify it under the terms of the GNU General Public License                  */
/* as published by the Free Software Foundation; either version 2               */
/* of the License, or (at your option) any later version.                       */
/*                                                                              */
/* This program is distributed in the hope that it will be useful,              */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/* GNU General Public License for more details.                                 */
/*                                                                              */
/* You should have received a copy of the GNU General Public License            */
/* along with this program; if not, write to the Free Software                  */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.  */
/*                                                                              */
/* Based on PureData by Miller Puckette and others.                             */
/*                                                                              */
/* ---------------------------------------------------------------------------- */
#ifndef VERSION
#define VERSION "1.5.4"
#endif

#include "m_pd.h"


#ifndef __DATE__ 
#define __DATE__ "without using a gnu compiler"
#endif

typedef struct _maxlib
{
     t_object x_obj;
} t_maxlib;

static t_class* maxlib_class;

	/* objects */
#ifndef __EMSCRIPTEN__
void maxlib_allow_setup();
void maxlib_arbran_setup();
void maxlib_arraycopy_setup();
void maxlib_average_setup();
void maxlib_beat_setup();
void maxlib_beta_setup();
void maxlib_bilex_setup();
void maxlib_borax_setup();
void maxlib_cauchy_setup();
void maxlib_chord_setup();
void maxlib_delta_setup();
void maxlib_deny_setup();
void maxlib_dist_setup();
void maxlib_divide_setup();
void maxlib_divmod_setup();
void maxlib_edge_setup();
void maxlib_expo_setup();
void maxlib_fifo_setup();
void maxlib_gauss_setup();
void maxlib_gestalt_setup();
void maxlib_history_setup();
void maxlib_ignore_setup();
void maxlib_iso_setup();
void maxlib_lifo_setup();
void maxlib_limit_setup();
void maxlib_linear_setup();
void maxlib_listfifo_setup();
void maxlib_listfunnel_setup();
void maxlib_match_setup();
void maxlib_minus_setup();
void maxlib_mlife_setup();
void maxlib_multi_setup();
void maxlib_nchange_setup();
void maxlib_netclient_setup();
void maxlib_netdist_setup();
void maxlib_netrec_setup();
void maxlib_netserver_setup();
void maxlib_nroute_setup();
void maxlib_pitch_setup();
void maxlib_plus_setup();
void maxlib_poisson_setup();
void maxlib_pong_setup();
void maxlib_pulse_setup();
void maxlib_remote_setup();
void maxlib_rewrap_setup();
void maxlib_rhythm_setup();
void maxlib_scale_setup();
void maxlib_score_setup();
void maxlib_speedlim_setup();
void maxlib_split_setup();
void maxlib_step_setup();
void maxlib_subst_setup();
void maxlib_sync_setup();
void maxlib_temperature_setup();
void maxlib_tilt_setup();
void maxlib_timebang_setup();
void maxlib_triang_setup();
void maxlib_unroute_setup();
void maxlib_urn_setup();
void maxlib_velocity_setup();
void maxlib_weibull_setup();
void maxlib_wrap_setup();
#else
void allow_setup();
void arbran_setup();
void arraycopy_setup();
void average_setup();
void beat_setup();
void beta_setup();
void bilex_setup();
void borax_setup();
void cauchy_setup();
void chord_setup();
void delta_setup();
void deny_setup();
void dist_setup();
void divide_setup();
void divmod_setup();
void edge_setup();
void expo_setup();
void fifo_setup();
void gauss_setup();
void gestalt_setup();
void history_setup();
void ignore_setup();
void iso_setup();
void lifo_setup();
void limit_setup();
void linear_setup();
void listfifo_setup();
void listfunnel_setup();
void match_setup();
void minus_setup();
void mlife_setup();
void multi_setup();
void nchange_setup();
void netclient_setup();
void netdist_setup();
void netrec_setup();
void netserver_setup();
void nroute_setup();
void pitch_setup();
void plus_setup();
void poisson_setup();
void pong_setup();
void pulse_setup();
void remote_setup();
void rewrap_setup();
void rhythm_setup();
void scale_setup();
void score_setup();
void speedlim_setup();
void split_setup();
void step_setup();
void subst_setup();
void sync_setup();
void temperature_setup();
void tilt_setup();
void timebang_setup();
void triang_setup();
void unroute_setup();
void urn_setup();
void velocity_setup();
void weibull_setup();
void wrap_setup();
#endif /* __EMSCRIPTEN__ */


static void* maxlib_new(t_symbol* s)
{
    t_maxlib *x = (t_maxlib *)pd_new(maxlib_class);
    return (x);
}

void maxlib_setup(void) 
{
	maxlib_class = class_new(gensym("maxlib"), (t_newmethod)maxlib_new, 0,
    	sizeof(t_maxlib), 0,0);

#ifndef __EMSCRIPTEN__
	maxlib_allow_setup();
	maxlib_arbran_setup();
	maxlib_arraycopy_setup();
	maxlib_average_setup();
	maxlib_beat_setup();
	maxlib_beta_setup();
	maxlib_bilex_setup();
	maxlib_borax_setup();
	maxlib_cauchy_setup();
	maxlib_chord_setup();
	maxlib_delta_setup();
	maxlib_deny_setup();
	maxlib_dist_setup();
	maxlib_divide_setup();
	maxlib_divmod_setup();
	maxlib_edge_setup();
	maxlib_expo_setup();
	maxlib_fifo_setup();
	maxlib_gauss_setup();
	maxlib_gestalt_setup();
	maxlib_history_setup();
	maxlib_ignore_setup();
	maxlib_iso_setup();
	maxlib_lifo_setup();
	maxlib_limit_setup();
	maxlib_linear_setup();
	maxlib_listfifo_setup();
	maxlib_listfunnel_setup();
	maxlib_match_setup();
	maxlib_minus_setup();
    maxlib_mlife_setup();
	maxlib_multi_setup();
	maxlib_nchange_setup();
	maxlib_netclient_setup();
	maxlib_netdist_setup();
	maxlib_netrec_setup();
	maxlib_netserver_setup();
	maxlib_nroute_setup();
	maxlib_pitch_setup();
	maxlib_plus_setup();
	maxlib_poisson_setup();
	maxlib_pong_setup();
	maxlib_pulse_setup();
	maxlib_remote_setup();
	maxlib_rewrap_setup();
	maxlib_rhythm_setup();
	maxlib_scale_setup();
	maxlib_score_setup();
	maxlib_speedlim_setup();
	maxlib_split_setup();
	maxlib_step_setup();
    maxlib_subst_setup();
	maxlib_sync_setup();
	maxlib_temperature_setup();
	maxlib_tilt_setup();
	maxlib_timebang_setup();
	maxlib_triang_setup();
	maxlib_unroute_setup();
	maxlib_urn_setup();
	maxlib_velocity_setup();
	maxlib_weibull_setup();
	maxlib_wrap_setup();
#else
    allow_setup();
    arbran_setup();
    arraycopy_setup();
    average_setup();
    beat_setup();
    beta_setup();
    bilex_setup();
    borax_setup();
    cauchy_setup();
    chord_setup();
    delta_setup();
    deny_setup();
    dist_setup();
    divide_setup();
    divmod_setup();
    edge_setup();
    expo_setup();
    fifo_setup();
    gauss_setup();
    gestalt_setup();
    history_setup();
    ignore_setup();
    iso_setup();
    lifo_setup();
    limit_setup();
    linear_setup();
    listfifo_setup();
    listfunnel_setup();
    match_setup();
    minus_setup();
    mlife_setup();
    multi_setup();
    nchange_setup();
    netclient_setup();
    netdist_setup();
    netrec_setup();
    netserver_setup();
    nroute_setup();
    pitch_setup();
    plus_setup();
    poisson_setup();
    pong_setup();
    pulse_setup();
    remote_setup();
    rewrap_setup();
    rhythm_setup();
    scale_setup();
    score_setup();
    speedlim_setup();
    split_setup();
    step_setup();
    subst_setup();
    sync_setup();
    temperature_setup();
    tilt_setup();
    timebang_setup();
    triang_setup();
    unroute_setup();
    urn_setup();
    velocity_setup();
    weibull_setup();
    wrap_setup();
#endif /* __EMSCRIPTEN__ */

	post("\n       maxlib :: Music Analysis eXtensions LIBrary");
	post("       written by Olaf Matthes <olaf.matthes@gmx.de>");
	logpost(NULL, 4, "       version "VERSION);
	post("       compiled "__DATE__);
	logpost(NULL, 4, "       latest version at http://www.akustische-kunst.org/puredata/maxlib/");
	post("       objects: allow arbran arraycopy average beat beta bilex borax cauchy ");
	post("                chord delta deny dist divide divmod edge expo fifo gauss ");
	post("                gestalt history ignore iso lifo linear listfifo listfunnel ");
	post("                match minus mlife multi nchange netclient netdist netrec ");
	post("                netserver nroute pitch plus poisson pong pulse remote rewrap ");
	post("                rhythm scale score speedlim split step subst sync temperature ");
	post("                tilt timebang triang unroute urn velocity weibull wrap\n");
}
