#pragma once
/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* Audio and MIDI I/O, and other scheduling and system stuff. */

/* NOTE: this file describes Pd implementation details which may change
in future releases.  The public (stable) API is in m_pd.h. */  

#ifndef __s_stuff_h_
#define __s_stuff_h_

/* in s_path.c */

#include <stdarg.h>

typedef struct _namelist    /* element in a linked list of stored strings */
{
    struct _namelist *nl_next;  /* next in list */
    char *nl_string;            /* the string */
} t_namelist;

EXTERN t_namelist *namelist_append(t_namelist *listwas, const char *s, int allowdup);
EXTERN t_namelist *namelist_append_files(t_namelist *listwas, const char *s);
EXTERN void namelist_free(t_namelist *listwas);
EXTERN const char *namelist_get(const t_namelist *namelist, int n);
int do_open_via_path(const char *dir, const char *name,
    const char *ext, char *dirresult, char **nameresult, unsigned int size,
    int bin, t_namelist *searchpath, int okgui);

void sys_setextrapath(const char *p);
extern int sys_usestdpath;
int sys_open_absolute(const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int *fdp,
    int okgui);
int sys_trytoopenit(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int okgui);
int sys_trytoopenone(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin);
t_symbol *sys_decodedialog(t_symbol *s);

/* s_file.c */
// do not load preferences and recent files if using emscripten
// LATER: distinguish between WebPdL2Ork emscripten and full online editor
#ifndef __EMSCRIPTEN__
void sys_loadpreferences();
void sys_savepreferences();
#endif
extern int sys_defeatrt;
extern t_symbol *sys_gui_preset;
extern t_symbol *sys_flags;

#define MAX_RECENT_FILES 8
// do not load preferences and recent files if using emscripten
// LATER: distinguish between WebPdL2Ork emscripten and full online editor
#ifndef __EMSCRIPTEN__
void sys_load_recent_files(void);
void sys_save_recent_files(void);
#endif
void sys_add_recent_file(const char *s);
void sys_clear_recent_files(void);
extern int sys_n_recent_files;
extern char *sys_recent_files[];

/* s_main.c */
extern int sys_debuglevel;
extern int sys_verbose;
EXTERN int sys_noloadbang;
extern int sys_nogui;
extern int sys_havegui(void);
extern const char *sys_guicmd;
extern int sys_batch;

EXTERN int sys_nearestfontsize(int fontsize);

extern int sys_defaultfont;
EXTERN t_symbol *sys_libdir;    /* library directory for auxiliary files */

/* s_loader.c */
typedef int (*loader_t)(t_canvas *canvas, const char *classname, const char *path); /* callback type */
EXTERN int sys_load_lib(t_canvas *canvas, const char *classname);
EXTERN void sys_register_loader(loader_t loader);
EXTERN const char**sys_get_dllextensions(void);

                        /* s_audio.c */

#define MAXAUDIOINDEV 4
#define MAXAUDIOOUTDEV 4

typedef struct _audiosettings
{
    int a_api;
    int a_nindev;
    int a_indevvec[MAXAUDIOINDEV];
    int a_nchindev;
    int a_chindevvec[MAXAUDIOINDEV];
    int a_noutdev;
    int a_outdevvec[MAXAUDIOOUTDEV];
    int a_nchoutdev;
    int a_choutdevvec[MAXAUDIOOUTDEV];
    int a_srate;
    int a_advance;
    int a_callback;
    int a_blocksize;
} t_audiosettings;

#define SENDDACS_NO 0           /* return values for sys_send_dacs() */
#define SENDDACS_YES 1 
#define SENDDACS_SLEPT 2

#define DEFDACBLKSIZE 64
#define DEFDACSAMPLERATE 48000

                /* s_audio.c */
#define API_NONE 0
#define API_ALSA 1
#define API_OSS 2
#define API_MMIO 3
#define API_PORTAUDIO 4
#define API_JACK 5
#define API_SGI 6           /* gone */
#define API_AUDIOUNIT 7
#define API_ESD 8           /* Defunct Enlightenment Sounds Daemon lib, from the
                               wonderful Enlightenment gnu/linux window manager
                               (enlightenment.org)
                               Apparently an early version of Pulse Audio was
                               compatible with this old API. Just for kicks I
                               tried using the old esd backend code to connect
                               with pulse but it didn't work. */
#define API_DUMMY 9

    /* figure out which API should be the default.  The one we judge most
    likely to offer a working device takes precedence so that if you
    start up Pd for the first time there's a reasonable chance you'll have
    sound.  (You'd think portaudio would be best but it seems to default
    to jack on linux, and on Windows we only use it for ASIO). 
    If nobody shows up, define DUMMY and make it the default.*/
#if defined(USEAPI_ALSA)
# define API_DEFAULT API_ALSA
# define API_DEFSTRING "ALSA"
#elif defined(USEAPI_PORTAUDIO)
# define API_DEFAULT API_PORTAUDIO
# define API_DEFSTRING "portaudio"
#elif defined(USEAPI_OSS)
# define API_DEFAULT API_OSS
# define API_DEFSTRING "OSS"
#elif defined(USEAPI_AUDIOUNIT)
# define API_DEFAULT API_AUDIOUNIT
# define API_DEFSTRING "AudioUnit"
#elif defined(USEAPI_ESD)
# define API_DEFAULT API_ESD
# define API_DEFSTRING "ESD (?)"
#elif defined(USEAPI_JACK)
# define API_DEFAULT API_JACK
# define API_DEFSTRING "Jack audio connection kit"
#elif defined(USEAPI_MMIO)
# define API_DEFAULT API_MMIO
# define API_DEFSTRING "MMIO"
#else 
# ifndef USEAPI_DUMMY   /* we need at least one so bring in the dummy */
# define USEAPI_DUMMY
# endif /* USEAPI_DUMMY */
# define API_DEFAULT API_DUMMY
# define API_DEFSTRING "dummy audio"
#endif 
#define DEFAULTAUDIODEV 0

#define DEFMIDIDEV 0

#define DEFAULTSRATE 44100
#if defined(_WIN32)
#define DEFAULTADVANCE 80
#define MMIODEFBLOCKSIZE 512
#elif defined(__linux__)
#define DEFAULTADVANCE 20
#elif defined(__APPLE__)
#define DEFAULTADVANCE 5
#else
#define DEFAULTADVANCE 25
#endif

extern int sys_schedadvance;

int sys_send_dacs(void);
void sys_reportidle(void);
void sys_listdevs(void);
EXTERN void sys_set_audio_settings(t_audiosettings *as);
EXTERN void sys_get_audio_settings(t_audiosettings *as);
EXTERN void sys_reopen_audio(void);
EXTERN void sys_close_audio(void);
void sys_xclose_audio( void);
    /* return true if the interface prefers always being open (ala jack) : */
EXTERN int audio_shouldkeepopen(void);
EXTERN int audio_isopen(void);     /* true if audio interface is open */
EXTERN int sys_audiodevnametonumber(int output, const char *name);
EXTERN void sys_audiodevnumbertoname(int output, int devno, char *name,
    int namesize);
EXTERN void sys_get_audio_devs(char *indevlist, int *nindevs,
                          char *outdevlist, int *noutdevs, int *canmulti, int *cancallback,
                          int maxndev, int devdescsize, int api);
EXTERN void sys_get_audio_apis(char *buf);
EXTERN void sys_get_audio_apis2(t_binbuf *buf);


        /* audio API specific functions */

typedef void (*t_audiocallback)(void);

int pa_open_audio(int inchans, int outchans, int rate, t_sample *soundin,
    t_sample *soundout, int framesperbuf, int nbuffers,
    int indeviceno, int outdeviceno, t_audiocallback callback);
void pa_close_audio(void);
int pa_send_dacs(void);
int pa_reopen_audio(void);
void pa_listdevs(void);
void pa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int oss_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate, int blocksize);
void oss_close_audio(void);
int oss_send_dacs(void);
void oss_reportidle(void);
void oss_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int alsa_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate, int blocksize);
void alsa_close_audio(void);
int alsa_send_dacs(void);
void alsa_reportidle(void);
void alsa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int jack_open_audio(int inchans, int outchans, t_audiocallback callback);
void jack_close_audio(void);
int jack_send_dacs(void);
int jack_reopen_audio(void);
void jack_reportidle(void);
void jack_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);
void jack_listdevs(void);
void jack_client_name(const char *name);
void jack_autoconnect(int);

int mmio_open_audio(int naudioindev, int *audioindev,
    int nchindev, int *chindev, int naudiooutdev, int *audiooutdev,
    int nchoutdev, int *choutdev, int rate, int blocksize);
void mmio_close_audio(void);
void mmio_reportidle(void);
int mmio_send_dacs(void);
void mmio_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int audiounit_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate);
void audiounit_close_audio(void);
int audiounit_send_dacs(void);
void audiounit_listdevs(void);
void audiounit_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize);
int esd_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate);
void esd_close_audio(void);
int esd_send_dacs(void);
void esd_listdevs(void);
void esd_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize);
int dummy_open_audio(int nin, int nout, int sr);
int dummy_close_audio(void);
int dummy_send_dacs(void);
void dummy_getdevs(char *indevlist, int *nindevs, char *outdevlist,
    int *noutdevs, int *canmulti, int maxndev, int devdescsize);
void dummy_listdevs(void);

/* s_midi.c */
#define MAXMIDIINDEV 16         /* max. number of input ports */
#define MAXMIDIOUTDEV 16        /* max. number of output ports */
EXTERN int sys_midiapi;
extern int sys_nmidiin;
extern int sys_nmidiout;
extern int sys_midiindevlist[];
extern int sys_midioutdevlist[];

EXTERN void sys_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec, int enable);

EXTERN void sys_get_midi_apis(char *buf);
EXTERN void sys_get_midi_apis2(t_binbuf *buf);
EXTERN void sys_get_midi_devs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, 
   int maxndev, int devdescsize);
EXTERN void sys_get_midi_params(int *pnmidiindev, int *pmidiindev,
    int *pnmidioutdev, int *pmidioutdev);
EXTERN int sys_mididevnametonumber(int output, const char *name);
EXTERN void sys_mididevnumbertoname(int output, int devno, char *name,
    int namesize);

void sys_get_midi_apis2(t_binbuf *buf);

EXTERN void sys_reopen_midi(void);
EXTERN void sys_close_midi(void);
EXTERN void sys_xclose_midi(void);
EXTERN void sys_putmidimess(int portno, int a, int b, int c);
EXTERN void sys_putmidibyte(int portno, int a);
EXTERN void sys_poll_midi(void);
EXTERN void sys_setmiditimediff(double inbuftime, double outbuftime);
EXTERN void sys_midibytein(int portno, int byte);

void sys_listmididevs(void);
EXTERN void sys_set_midi_api(int whichapi);
EXTERN int sys_audio_get_blocksize(void);

    /* implemented in the system dependent MIDI code (s_midi_pm.c, etc. ) */
void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize);
void sys_do_open_midi(int nmidiindev, int *midiindev,
    int nmidioutdev, int *midioutdev);

#ifdef USEAPI_ALSA
EXTERN void sys_alsa_putmidimess(int portno, int a, int b, int c);
EXTERN void sys_alsa_putmidibyte(int portno, int a);
EXTERN void sys_alsa_poll_midi(void);
EXTERN void sys_alsa_close_midi(void);

    /* implemented in the system dependent MIDI code (s_midi_pm.c, etc. ) */
void midi_alsa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize);
void sys_alsa_do_open_midi(int nmidiindev, int *midiindev,
    int nmidioutdev, int *midioutdev);
#endif

/* m_sched.c */
EXTERN void sys_log_error(int type);
#define ERR_NOTHING 0
#define ERR_ADCSLEPT 1
#define ERR_DACSLEPT 2
#define ERR_RESYNC 3
#define ERR_DATALATE 4

#define SCHED_AUDIO_NONE 0
#define SCHED_AUDIO_POLL 1
#define SCHED_AUDIO_CALLBACK 2
void sched_set_using_audio(int flag);
int sched_get_using_audio(void);
extern int sys_sleepgrain;      /* override value set in command line */
EXTERN int sched_get_sleepgrain( void);     /* returns actual value */

/* s_inter.c */

EXTERN void sys_microsleep( void);
EXTERN void sys_init_fdpoll(void);

EXTERN void sys_bail(int exitcode);
EXTERN int sys_pollgui(void);

EXTERN_STRUCT _socketreceiver;
#define t_socketreceiver struct _socketreceiver

typedef void (*t_socketnotifier)(void *x, int n);
typedef void (*t_socketreceivefn)(void *x, t_binbuf *b);
    /* from addr sockaddr_storage struct, optional */
typedef void (*t_socketfromaddrfn)(void *x, const void *fromaddr);

EXTERN t_socketreceiver *socketreceiver_new(void *owner,
    t_socketnotifier notifier, t_socketreceivefn socketreceivefn, int udp);
EXTERN void socketreceiver_read(t_socketreceiver *x, int fd);
EXTERN void socketreceiver_set_fromaddrfn(t_socketreceiver *x,
    t_socketfromaddrfn fromaddrfn);
EXTERN void sys_sockerror(const char *s);
EXTERN void sys_closesocket(int fd);
EXTERN unsigned char *sys_getrecvbuf(unsigned int *size);

typedef void (*t_fdpollfn)(void *ptr, int fd);
EXTERN void sys_addpollfn(int fd, t_fdpollfn fn, void *ptr);
EXTERN void sys_rmpollfn(int fd);
#if defined(USEAPI_OSS) || defined(USEAPI_ALSA)
void sys_setalarm(int microsec);
#endif

void sys_set_priority(int higher);
EXTERN int sys_hipriority;      /* real-time flag, true if priority boosted */

/* s_print.c */

typedef void (*t_printhook)(const char *s);
/* set this to override printing; used as default for STUFF->st_printhook */
extern t_printhook sys_printhook;
extern int sys_printtostderr;
extern int sys_k12_mode;

/* jsarlo { */

EXTERN int sys_externalschedlib;

EXTERN t_sample* get_sys_soundout(void);
EXTERN t_sample* get_sys_soundin(void);
EXTERN int* get_sys_main_advance(void);
EXTERN double* get_sys_time_per_dsp_tick(void);
EXTERN int* get_sys_schedblocksize(void);
EXTERN double* get_sys_time(void);
EXTERN t_float* get_sys_dacsr(void);
EXTERN int* get_sys_sleepgrain(void);
EXTERN int* get_sys_schedadvance(void);

EXTERN void sys_initmidiqueue(void);
EXTERN void sy_setmiditimediff(double inbuftime, double outbuftime);
EXTERN void sched_tick(void);
EXTERN void sys_pollmidiqueue(void);
EXTERN void sys_setchsr(int chin, int chout, int sr);

EXTERN void inmidi_realtimein(int portno, int cmd);
EXTERN void inmidi_byte(int portno, int byte);
EXTERN void inmidi_sysex(int portno, int byte);
EXTERN void inmidi_noteon(int portno, int channel, int pitch, int velo);
EXTERN void inmidi_controlchange(int portno,
                                 int channel,
                                 int ctlnumber,
                                 int value);
EXTERN void inmidi_programchange(int portno, int channel, int value);
EXTERN void inmidi_pitchbend(int portno, int channel, int value);
EXTERN void inmidi_aftertouch(int portno, int channel, int value);
EXTERN void inmidi_polyaftertouch(int portno,
                                  int channel,
                                  int pitch,
                                  int value);
/* } jsarlo */

struct _instancestuff
{
    t_namelist *st_externlist;
    t_namelist *st_searchpath;
    t_namelist *st_staticpath;
    t_namelist *st_helppath;
    t_namelist *st_temppath;    /* temp search paths ie. -path on commandline */
    int st_schedblocksize;      /* audio block size for scheduler */
    int st_blocksize;           /* audio I/O block size in sample frames */
    t_float st_dacsr;           /* I/O sample rate */
    int st_inchannels;
    int st_outchannels;
    t_sample *st_soundout;
    t_sample *st_soundin;
    double st_time_per_dsp_tick;    /* obsolete - included for GEM?? */
    t_printhook st_printhook;   /* set this to override per-instance printing */
    t_printhook st_printhook_error;
    void *st_impdata; /* optional implementation-specific data for libpd, etc */
};

#define STUFF (pd_this->pd_stuff)

/* escape characters for tcl/tk
 * escapes special characters ("{}\") in the string 'src', which
 * has a maximum length of 'srclen' and might be 0-terminated,
 * and writes them into the 'dstlen' sized output buffer 'dst'
 * the result is zero-terminated; if the 'dst' buffer cannot hold the
 * fully escaped 'src' string, the result might be incomplete.
 * 'srclen' can be 0, in which case the 'src' string must be 0-terminated.
 */
EXTERN char*pdgui_strnescape(char* dst, size_t dstlen, const char*src, size_t srclen);

/* format non-trivial data when sending it from core->gui (and vice versa)
 */
/* make sure that an object-id is always a string (even on windows, where %p
 * does not prefix '0x'
 */
#define PDGUI_FORMAT__OBJECT "obj:%p"

/* safe cross-platform alternatives to snprintf and vsnprintf. */
EXTERN int pd_snprintf(char *buf, size_t size, const char *fmt, ...);
EXTERN int pd_vsnprintf(char *buf, size_t size, const char *fmt,
    va_list argptr);

EXTERN const char *pd_extraflags;     /* a place to stick an extra startup arg */
 /* this is used by 'stdout' but could be useful elsewhere perhaps. */

/* in x_list.c */
    /* List element for storage.  Keep an atom and, in case it's a pointer,
        an associated 'gpointer' to protect against stale pointers. */
typedef struct _listelem
{
    t_atom l_a;
    t_gpointer l_p;
} t_listelem;

struct _alist
{
    t_pd l_pd;          /* object to point inlets to */
    int l_n;            /* number of items */
    int l_npointer;     /* number of pointers */
    t_listelem *l_vec;  /* pointer to items */
};

#ifndef t_alist
#define t_alist struct _alist
#endif

#if 0 /* probably won't use this version... */
#ifdef HAVE_ALLOCA
#define LIST_ALLOCA(x, n) ( \
    (x).l_n = (n), \
    (x).l_vec = (t_listelem *)((n) < LIST_NGETBYTE ?  \
        alloca((n) * sizeof(t_listelem)) : getbytes((n) * sizeof(t_listelem))))     \
#define LIST_FREEA(x) ( \
    ((x).l_n < LIST_NGETBYTE ||
        (freebytes((x).l_vec, (x).l_n * sizeof(t_listelem)), 0)))

#else
#define LIST_ALLOCA(x, n) ( \
    (x).l_n = (n), \
    (x).l_vec = (t_listelem *)getbytes((n) * sizeof(t_listelem))) 
#define LIST_FREEA(x) (freebytes((x).l_vec, (x).l_n * sizeof(t_listelem)))
#endif
#endif

#if HAVE_ALLOCA
#define XL_ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < LIST_NGETBYTE ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define XL_ATOMS_FREEA(x, n) ( \
    ((n) < LIST_NGETBYTE || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define XL_ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define XL_ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

EXTERN void atoms_copy(int argc, t_atom *from, t_atom *to);
EXTERN t_class *alist_class;
EXTERN void alist_init(t_alist *x);
EXTERN void alist_clear(t_alist *x);
EXTERN void alist_list(t_alist *x, t_symbol *s, int argc, t_atom *argv);
EXTERN void alist_anything(t_alist *x, t_symbol *s, int argc, t_atom *argv);
EXTERN void alist_toatoms(t_alist *x, t_atom *to, int onset, int count);
EXTERN void alist_clone(t_alist *x, t_alist *y, int onset, int count);

#endif /* __s_stuff_h_ */
