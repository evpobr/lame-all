/*
 *      Command line parsing related functions
 *
 *      Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifdef __OS2__
#include <os2.h>
#define PRTYC_IDLE 1
#define PRTYC_REGULAR 2
#define PRTYD_MINIMUM -31 
#define PRTYD_MAXIMUM 31 
#endif

#include "lame.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


/* GLOBAL VARIABLES.  set by parse_args() */
/* we need to clean this up */
sound_file_format input_format;   
int swapbytes;              /* force byte swapping   default=0*/
int silent;
int brhist;
float update_interval;      /* to use Frank's time status display */
int mp3_delay;              /* to adjust the number of samples truncated
                               during decode */
int mp3_delay_set;          /* user specified the value of the mp3 encoder 
                               delay to assume for decoding */

int enc_delay;
int enc_padding;
int disable_wav_header;
mp3data_struct mp3input_data; /* used by Ogg and MP3 */

int in_signed=1;
int in_unsigned=0;
int in_endian=order_littleEndian;
int in_bitwidth=16;


/**  
 *  Long Filename support for the WIN32 platform
 *  
 */
#ifdef WIN32
#include <winbase.h>
static void  
dosToLongFileName( char *fn )
{
    const int MSIZE = MAX_NAME_SIZE-4;  //  we wanna add ".mp3" later
    WIN32_FIND_DATAA lpFindFileData;
    HANDLE h = FindFirstFileA( fn, &lpFindFileData );
    if ( h != INVALID_HANDLE_VALUE ) {
        int   a;
        char *q;
        FindClose( h );
        for ( a = 0; a < MSIZE; a++ ) {
            if ( '\0' == lpFindFileData.cFileName[a] ) break;
        }
        if ( a >= MSIZE || a == 0 ) return;
        q = strrchr( fn, '\\' );
        if ( q == NULL ) q = strrchr(fn,':');
        if ( q == NULL ) strncpy( fn, lpFindFileData.cFileName, a );
        else {
            a += q-fn +1;
            if ( a >= MSIZE ) return;
            strncpy( ++q, lpFindFileData.cFileName, MSIZE-a );
        }
    }
}
#endif

#if defined(__OS2__)
/* OS/2 priority functions */
static int setOS2Priority( lame_global_flags*  gfp, int Priority )
{
    int rc;

    switch(Priority) {
 
    case 0:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_IDLE,             /* select priority class (idle, regular, etc) */
             0,                      /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 0 (Low priority).\n");
        break;

    case 1:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_IDLE,             /* select priority class (idle, regular, etc) */
             PRTYD_MAXIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 1 (Medium priority).\n");
        break;

    case 2:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             PRTYD_MINIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 2 (Regular priority).\n");
        break;
        
    case 3:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             0,                      /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 3 (High priority).\n");
        break;

    case 4:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             PRTYD_MAXIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 4 (Maximum priority). I hope you enjoy it :)\n");
        break;
     
    default:
        printf("==> Invalid priority specified! Assuming idle priority.\n");
    }
   

    return 0;
}
#endif


/************************************************************************
*
* license
*
* PURPOSE:  Writes version and license to the file specified by fp
*
************************************************************************/

static int  lame_version_print ( FILE* const fp )
{
    const char * v = get_lame_version ();
    const char * u = get_lame_url ();
    const int lenv = strlen (v);
    const int lenu = strlen (u);
    const int lw = 80; /* line width of terminal in characters */
    const int sw = 16; /* static width of text */
    
    if (lw >= lenv+lenu+sw || lw < lenu+2)                
        /* text fits in 80 chars per line, or line even too small for url */
        fprintf ( fp, "LAME version %s (%s)\n\n", v, u );
    else            
        /* text too long, wrap url into next line, right aligned */
        fprintf ( fp, "LAME version %s\n%*s(%s)\n\n", v, lw-2-lenu, "", u );
    
    return 0;
}

int  print_license ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print version & license */
{
    lame_version_print ( fp );
    fprintf ( fp, 
              "Can I use LAME in my commercial program?\n"
              "\n"
              "Yes, you can, under the restrictions of the LGPL.  In particular, you\n"
              "can include a compiled version of the LAME library (for example,\n"
              "lame.dll) with a commercial program.  Some notable requirements of\n"
              "the LGPL:\n"
              "\n"
              "1. In your program, you cannot include any source code from LAME, with\n"
              "   the exception of files whose only purpose is to describe the library\n"
              "   interface (such as lame.h).\n"
              "\n"
              "2. Any modifications of LAME must be released under the LGPL.\n"
              "   The LAME project (www.mp3dev.org) would appreciate being\n"
              "   notified of any modifications.\n"
              "\n"
              "3. You must give prominent notice that your program is:\n"
              "      A. using LAME (including version number)\n"
              "      B. LAME is under the LGPL\n"
              "      C. Provide a copy of the LGPL.  (the file COPYING contains the LGPL)\n"
              "      D. Provide a copy of LAME source, or a pointer where the LAME\n"
              "         source can be obtained (such as www.mp3dev.org)\n"
              "   An example of prominent notice would be an \"About the LAME encoding engine\"\n"
              "   button in some pull down menu within the executable of your program.\n"
              "\n"
              "4. If you determine that distribution of LAME requires a patent license,\n"
              "   you must obtain such license.\n"
              "\n"
              "\n"
              "*** IMPORTANT NOTE ***\n"
              "\n"
              "The decoding functions provided in LAME use the mpglib decoding engine which\n"
              "is under the GPL.  They may not be used by any program not released under the\n"
              "GPL unless you obtain such permission from the MPG123 project (www.mpg123.de).\n"
              "\n" );
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

int  usage ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print general syntax */
{
    lame_version_print ( fp );
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "Try:\n"
              "     \"%s --help\"           for general usage information\n" 
              " or:\n"
              "     \"%s --preset help\"    for information on suggested predefined settings\n"
              "     \"%s --dm-preset help\" for information on the seperate and very highly\n" 
              "                             tuned for quality predefined settings\n"
              " or:\n"
              "     \"%s --longhelp\"\n"
              "  or \"%s -?\"              for a complete options list\n\n",
              ProgramName, ProgramName, ProgramName, ProgramName, ProgramName, ProgramName ); 
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*           but only the most important ones, to fit on a vt100 terminal
*
************************************************************************/

int  short_help ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print short syntax help */
{
    lame_version_print ( fp );
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "RECOMMENDED:\n"
              "    lame -h input.wav output.mp3\n"
              "\n"
              "OPTIONS:\n"
              "    -b bitrate      set the bitrate, default 128 kbps\n"
              "    -f              fast mode (lower quality)\n"
              "    -h              higher quality, but a little slower.  Recommended.\n"
              "    -k              keep ALL frequencies (disables all filters)\n"
              "                    Can cause ringing and twinkling\n"
              "    -m mode         (s)tereo, (j)oint, (m)ono or (a)uto\n"
              "                    default is (j) or (s) depending on bitrate\n"
              "    -V n            quality setting for VBR.  default n=%i\n"
              "\n"
              "    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n"
              "                    \"--preset help\" gives some more infos on these\n"
              "    --dm-preset type type must be \"standard\", \"xtreme\", \"insane\", or a\n"
              "                     value for an average desired bitrate and depending on the\n"                       
              "                     value specified, appropriate quality settings will be used\n"
              "\n"
#if defined(__OS2__)
              "    --priority type  sets the process priority (OS/2 only):\n"
              "                     0 = Low priority\n"
              "                     1 = Medium priority\n"
              "                     2 = Regular priority\n"
              "                     3 = High priority\n"
              "                     4 = Maximum priority\n"
              "\n"
#endif
              "    --longhelp      full list of options\n"
              "\n",
              ProgramName, lame_get_VBR_q(gfp) );
 
    return 0;
}

/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

static void  wait_for ( FILE* const fp, int lessmode )
{
    if ( lessmode ) {
        fflush ( fp ); 
        getchar (); 
    } else {
        fprintf ( fp, "\n" );
    }
    fprintf ( fp, "\n" );
}

int  long_help ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName, int lessmode )  /* print long syntax help */
{
    lame_version_print ( fp );
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "RECOMMENDED:\n"
              "    lame -h input.wav output.mp3\n"
              "\n"
              "OPTIONS:\n"
              "  Input options:\n"
              "    -r              input is raw pcm\n"
              "    -x              force byte-swapping of input\n"
              "    -s sfreq        sampling frequency of input file (kHz) - default 44.1 kHz\n"
              "    --bitwidth w    input bit width is w (default 16)\n"
#ifdef LIBSNDFILE
              "    --signed        input is signed (default)\n"
              "    --unsigned      input is unsigned\n"
              "    --little-endian input is little-endian (default from host)\n"
              "    --big-endian    input is big-endian (default from host)\n"
#endif
              "    --mp1input      input file is a MPEG Layer I   file\n"
              "    --mp2input      input file is a MPEG Layer II  file\n"
              "    --mp3input      input file is a MPEG Layer III file\n"
#if defined(HAVE_VORBIS)
              "    --ogginput      input file is a Ogg Vorbis file\n"
#endif
 	      "    --nogap <file1> <file2> <...>\n"
 	      "                    gapless encoding for a set of contiguous files\n"
 	      "    --nogapout <dir>\n"
 	      "                    output dir for gapless encoding (must precede --nogap)"
              , ProgramName );

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  Operational options:\n"
              "    -m <mode>       (s)tereo, (j)oint, (f)orce, (m)ono or (a)auto  \n"
              "                    default is (s) or (j) depending on bitrate\n"
              "                    force = force ms_stereo on all frames.\n"
              "                    auto = jstereo, with varialbe mid/side threshold\n"
              "    -a              downmix from stereo to mono file for mono encoding\n"
              "    -d              allow channels to have different blocktypes\n"
#if defined(HAVE_VORBIS_ENCODER)
              "    --ogg           encode to Ogg Vorbis instead of MP3\n"
#endif
              "    --freeformat    produce a free format bitstream\n"
              "    --decode        input=mp3 file, output=wav\n"
              "    -t              disable writing wav header when using --decode\n"
              "    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n"
              "    --scale <arg>   scale input (multiply PCM data) by <arg>\n"
	      "    --scale-l <arg> scale channel 0 (left) input (multiply PCM data) by <arg>\n"
              "    --scale-r <arg> scale channel 1 (right) input (multiply PCM data) by <arg>\n"
              "    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n"
              "                    \"--preset help\" gives some more infos on these\n" 
              "    --dm-preset type type must be \"standard\", \"xtreme\", \"insane\", or a\n"
              "                     value for an average desired bitrate and depending on the\n"                       
              "                     value specified, appropriate quality settings will be used\n"
 	      "    --r3mix         use high-quality VBR preset"
              );

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  Verbosity:\n"
              "    --disptime <arg>print progress report every arg seconds\n"
              "    -S              don't print progress report, VBR histograms\n"
              "    --nohist        disable VBR histogram display\n"
              "    --silent        don't print anything on screen\n"
              "    --quiet         don't print anything on screen\n"
              "    --verbose       print a lot of useful information\n"
              "\n"
              "  Noise shaping & psycho acoustic algorithms:\n"
              "    -q <arg>        <arg> = 0...9.  Default  -q 5 \n"
              "                    -q 0:  Highest quality, very slow \n"
              "                    -q 9:  Poor quality, but fast \n"
              "    -h              Same as -q 2.   Recommended.\n"
              "    -f              Same as -q 7.   Fast, ok quality\n" 
              );

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  CBR (constant bitrate, the default) options:\n"
              "    -b <bitrate>    set the bitrate in kbps, default 128 kbps\n"
              "\n"
              "  ABR options:\n"
              "    --abr <bitrate> specify average bitrate desired (instead of quality)\n"
              "\n"
              "  VBR options:\n"
              "    -v              use variable bitrate (VBR) (--vbr-old)\n"
              "    --vbr-old       use old variable bitrate (VBR) routine\n"
              "    --vbr-new       use new variable bitrate (VBR) routine\n"
              "    -V n            quality setting for VBR.  default n=%i\n"
              "                    0=high quality,bigger files. 9=smaller files\n"
              "    -b <bitrate>    specify minimum allowed bitrate, default  32 kbps\n"
              "    -B <bitrate>    specify maximum allowed bitrate, default 320 kbps\n"
              "    -F              strictly enforce the -b option, for use with players that\n"
              "                    do not support low bitrate mp3\n"
              "    -t              disable writing LAME Tag\n"
              , lame_get_VBR_q(gfp) );
  
    wait_for ( fp, lessmode );  
    fprintf ( fp,
              "  ATH related:\n"
              "    --noath         turns ATH down to a flat noise floor\n"
              "    --athshort      ignore GPSYCHO for short blocks, use ATH only\n"
              "    --athonly       ignore GPSYCHO completely, use ATH only\n"
              "    --athtype n     selects between different ATH types [0-5]\n"
              "    --athlower x    lowers ATH by x dB\n"
              "    --athaa-type n  ATH auto adjust types 1-3, else no adjustment\n"
              "    --athaa-loudapprox n   n=1 total energy or n=2 equal loudness curve\n"
              "    --athaa-sensitivity x  activation offset in -/+ dB for ATH auto-adjustment\n" 
              "\n"
              "  PSY related:\n"
              "    --short         use short blocks when appropriate\n"
              "    --noshort       do not use short blocks\n"
              "    --allshort      use only short blocks\n"
              "    --cwlimit <freq>  compute tonality up to freq (in kHz) default 8.8717\n"
#if 0
/* this is redundant, we already have --notemp */
              "    --temporal-masking n  use temporal masking effect n=0:no n=1:yes\n"
#endif
              "    --notemp        disable temporal masking effect\n"
              "    --nspsytune     experimental PSY tunings by Naoki Shibata\n"
              "    --nssafejoint   M/S switching criterion\n"
              "    --nsmsfix <arg> M/S switching tuning [effective 0-3.5]\n"
              "    --ns-bass x     adjust masking for sfbs  0 -  6 (long)  0 -  5 (short)\n"
              "    --ns-alto x     adjust masking for sfbs  7 - 13 (long)  6 - 10 (short)\n"         
              "    --ns-treble x   adjust masking for sfbs 14 - 21 (long) 11 - 12 (short)\n"
              "    --ns-sfb21 x    change ns-treble by x dB for sfb21\n"
            );

    wait_for ( fp, lessmode );  

    fprintf ( fp,
              "  experimental switches:\n"
              "    -X n            selects between different noise measurements\n"
              "    -Y              lets LAME ignore noise in sfb21, like in CBR\n"
              "    -Z              toggles the scalefac feature on\n"
            );

    wait_for ( fp, lessmode );  

    fprintf ( fp,
              "  MP3 header/stream options:\n"
              "    -e <emp>        de-emphasis n/5/c  (obsolete)\n"
              "    -c              mark as copyright\n"
              "    -o              mark as non-original\n"
              "    -p              error protection.  adds 16 bit checksum to every frame\n"
              "                    (the checksum is computed correctly)\n"
              "    --nores         disable the bit reservoir\n"
              "    --strictly-enforce-ISO   comply as much as possible to ISO MPEG spec\n"
              "\n"
              "  Filter options:\n"
              "    -k              keep ALL frequencies (disables all filters),\n"
              "                    Can cause ringing and twinkling\n"
              "  --lowpass <freq>        frequency(kHz), lowpass filter cutoff above freq\n"
              "  --lowpass-width <freq>  frequency(kHz) - default 15%% of lowpass freq\n"
              "  --highpass <freq>       frequency(kHz), highpass filter cutoff below freq\n"
              "  --highpass-width <freq> frequency(kHz) - default 15%% of highpass freq\n"
              "  --resample <sfreq>  sampling frequency of output file(kHz)- default=automatic\n"
               );
  
    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  ID3 tag options:\n"
              "    --tt <title>    audio/song title (max 30 chars for version 1 tag)\n"
              "    --ta <artist>   audio/song artist (max 30 chars for version 1 tag)\n"
              "    --tl <album>    audio/song album (max 30 chars for version 1 tag)\n"
              "    --ty <year>     audio/song year of issue (1 to 9999)\n"
              "    --tc <comment>  user-defined text (max 30 chars for v1 tag, 28 for v1.1)\n"
              "    --tn <track>    audio/song track number (1 to 255, creates v1.1 tag)\n"
              "    --tg <genre>    audio/song genre (name or number in list)\n"
              "    --add-id3v2     force addition of version 2 tag\n"
              "    --id3v1-only    add only a version 1 tag\n"
              "    --id3v2-only    add only a version 2 tag\n"
              "    --space-id3v1   pad version 1 tag with spaces instead of nulls\n"
              "    --pad-id3v2     pad version 2 tag with extra 128 bytes\n"
              "    --genre-list    print alphabetically sorted ID3 genre list and exit\n"
              "\n"
              "    Note: A version 2 tag will NOT be added unless one of the input fields\n"
              "    won't fit in a version 1 tag (e.g. the title string is longer than 30\n"
              "    characters), or the '--add-id3v2' or '--id3v2-only' options are used,\n"
              "    or output is redirected to stdout."
#if defined(HAVE_VORBIS_ENCODER)
              "\n\n"
              "    Note: All '--t*' options (except those for track and genre) work for Ogg\n"
              "    Vorbis output, but other ID3-specific options are ignored."
#endif              
#if defined(__OS2__)
              "\n\nOS/2-specific options:\n"
              "    --priority <type>     sets the process priority:\n"
              "                               0 = Low priority (IDLE, delta = 0)\n"
              "                               1 = Medium priority (IDLE, delta = +31)\n"
              "                               2 = Regular priority (REGULAR, delta = -31)\n"
              "                               3 = High priority (REGULAR, delta = 0)\n"
              "                               4 = Maximum priority (REGULAR, delta = +31)\n"
              "    Note: Calling '--priority' without a parameter will select priority 0.\n"
#endif

              );


    wait_for ( fp, lessmode );  
    display_bitrates ( fp );

    return 0;
}

static void  display_bitrate ( FILE* const fp, const char* const version, const int div, const int index )
{
    int  i;
    
    fprintf ( fp,
              "\nMPEG-%-3s layer III sample frequencies (kHz):  %2d  %2d  %g\n"
              "bitrates (kbps):", 
              version, 32/div, 48/div, 44.1/div );
    for (i = 1; i <= 14; i++ )          // 14 numbers are printed, not 15, and it was a bug of me
        fprintf ( fp, " %2i", bitrate_table [index] [i] );
    fprintf ( fp, "\n" );
}

int  display_bitrates ( FILE* const fp )
{
    display_bitrate ( fp, "1"  , 1, 1 );
    display_bitrate ( fp, "2"  , 2, 0 );
    display_bitrate ( fp, "2.5", 4, 0 );
    fprintf ( fp, "\n" );
    fflush  ( fp );
    return 0;
}


/*  note: for presets it would be better to externalize them in a file.
    suggestion:  lame --preset <file-name> ...
            or:  lame --preset my-setting  ... and my-setting is defined in lame.ini
 */
 
/************************************************************************
*
* usage
*
* PURPOSE:  Writes presetting info to #stdout#
*
************************************************************************/


typedef struct {
    const char* name;                   // name of preset
    long        resample;               // resample frequency in Hz, or -1 for no resampling
    short       highpass_freq;          // highpass frequency in Hz, or -1 for no highpass filtering
    short       lowpass_freq;           // lowpass frequency in Hz, or -1 for no lowpass filtering
    short       lowpass_width;          // lowpass width in Hz
    signed char no_short_blocks;        // use of short blocks, 1: no, 0: yes
    signed char quality;                // quality, the same as -f or -h
    MPEG_mode   mode;                   // channel mode (mono, stereo, joint)
    short       cbr;                    // CBR data rate in kbps (8...320)
    signed char vbr_mode;               // VBR mode (0...9)
    short       vbr_min;                // minimum VBR rate in kbps ( 8...256)
    short       vbr_max;                // maximum VBR rate in kbps (16...320)
} preset_t;

const preset_t Presets [] = {
   // name       fs     fu    fo    dfo shrt qual  mode              cbr vbr_mode/min/max
    { "phone" ,  8000, 125,  3400,    0,  1,  5, MONO        ,  16,  6,   8,  24 },  // phone standard 300-3400
    { "phon+" , 11025, 100,  4000,    0,  1,  5, MONO        ,  24,  4,  16,  32 },  // phone theoretical limits
    { "lw"    , 11025,  -1,  4000,    0,  0,  5, MONO        ,  24,  3,  16,  56 },  // LW
    { "mw-eu" , 11025,  -1,  4000,    0,  0,  5, MONO        ,  24,  3,  16,  56 },  // MW in europe
    { "mw-us" , 16000,  -1,  7500,    0,  0,  5, MONO        ,  40,  3,  24, 112 },  // MW in U.S.A.
    { "sw"    , 11025,  -1,  4000,    0,  0,  5, MONO        ,  24,  3,  16,  56 },  // SW
    { "fm"    , 32000,  -1, 15000,    0,  0,  3, JOINT_STEREO, 112,  3,  80, 256 },
    { "voice" , 24000,  -1, 12000,    0,  1,  5, MONO        ,  56,  4,  40, 112 },
    { "radio" ,    -1,  -1, 15000,    0,  0,  3, JOINT_STEREO, 128,  3,  96, 256 },
    { "tape"  ,    -1,  -1, 18000,  900,  0,  3, JOINT_STEREO, 128,  3,  96, 256 },
    { "hifi"  ,    -1,  -1, 18000,  900,  0,  2, JOINT_STEREO, 160,  2, 112, 320 },
    { "cd"    ,    -1,  -1,    -1,   -1,  0,  2, STEREO      , 192,  1, 128, 320 },
    { "studio",    -1,  -1,    -1,   -1,  0,  1, STEREO      , 256,  0, 160, 320 },
};


static int  presets_longinfo_basic ( FILE* const fp )  /* print possible combination */
{
    int i;

    fprintf ( fp, "\n"
              "Basic- presets covering a wide span of qualities and bit rates\n"
              "       These presets may be combined with -v for VBR MP3s.\n");
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  strlen(Presets[i].name) <= 4 ? "%5s " : " %-5s", Presets[i].name );
    fprintf ( fp, "\n=================");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "======" );
    fprintf ( fp, "\n--resample      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].resample < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].resample*1.e-3 );
    fprintf ( fp, "\n--highpass      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].highpass_freq < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].highpass_freq*1.e-3 );
    fprintf ( fp, "\n--lowpass       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_freq < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].lowpass_freq*1.e-3 );
    fprintf ( fp, "\n--lowpass-width ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_width < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].lowpass_width*1.e-3 );
    fprintf ( fp, "\n--noshort       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].no_short_blocks ) {
        case  1: fprintf ( fp,  "   yes" ); break;
        case  0: fprintf ( fp,  "    no" ); break;
        case -1: fprintf ( fp,  "      " ); break;
        default: assert (0);                break;
        }
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].mode ) {
        case MONO:         fprintf ( fp, "   -mm"); break;
        case JOINT_STEREO: fprintf ( fp, "   -mj"); break;
        case STEREO:       fprintf ( fp, "   -ms"); break;
        case -1:                  fprintf ( fp, "      "); break;
        default:                  assert (0);              break;
        }
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].quality ) {
        case -1: fprintf ( fp, "      "); break;
        case  2: fprintf ( fp, "    -h"); break;
        case  3: fprintf ( fp, "   -q3"); break;
        case  5: fprintf ( fp, "      "); break;
        case  7: fprintf ( fp, "    -f"); break;
        default: assert (0);              break;
    }
    fprintf ( fp, "\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].cbr );
    fprintf ( fp, "\n-- PLUS WITH -v ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "------" );
    fprintf ( fp, "-\n-V              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_mode );
    fprintf ( fp, "\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_min );
    fprintf ( fp, "\n-B              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_max );
    fprintf ( fp, "\n----------------");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "------" );
  
    fprintf ( fp, "-\nEXAMPLES:\n");
    fprintf ( fp, " a) --preset fm\n");
    fprintf ( fp, "    equal to: -mj -b112 --resample 32 --lowpass 15 --lowpass-width 0\n");
    fprintf ( fp, " b) -v --preset studio\n");
    fprintf ( fp, "    equals to: -h -ms -V0 -b160 -B320\n");
    
    return 0;
}


static void  presets_info_basic ( FILE* msgfp )
{
    int i;
    int j;
    int k;
    int p_cnt = sizeof(Presets)/sizeof(*Presets);
    int rows;

    fprintf ( msgfp, "\n" 
              "basic- presets covering a wide span (fixed rates unless used with -v or VBR):\n"
        );
    rows = ((p_cnt - 1) / 4) + 1;
    for ( i = 0; i < rows; ++i) {
        for( j = 0; j < 4; ++j ) {
            k = i + j * 4;
            if( k < p_cnt ) {
                fprintf( msgfp, "    %-5s (%2d kbps)", 
                         Presets[k].name, Presets[k].cbr );
            }
        }
        fputc( '\n', msgfp );
    }

}


static void  presets_info_dm ( FILE* msgfp )
{
    fprintf( msgfp, "\n"
             "dm- presets highly tuned for utmost quality via blind listening tests:\n"
             "  VBR presets for steady quality\n"
             "    --dm-preset standard\n"
             "    --dm-preset xtreme\n"
             "    --dm-preset insane\n"
             "  ABR presets for best quality at a given average bitrate:\n"
             "    --dm-preset <bitrate value>\n"
        );
}


static void  presets_info_r3mix ( FILE* msgfp )
{
    fprintf( msgfp, "\n"
             "r3mix- VBR preset for steady quality with little excess:\n"
             "    --preset r3mix\n" );
}


static void  presets_info_head ( FILE* msgfp )
{
    fputc( '\n', msgfp );
    lame_version_print( msgfp );
    fprintf( msgfp, 
             "Presets are shortcuts for common or carefully tuned settings.\n"
             "Several separate collections of preset profiles are available.\n"
        );
}


/* briefly and concisely display all available presets
*/
static void  presets_info ( FILE* msgfp )
{
    fprintf( msgfp, "For more preset details, refer to --preset longhelp.\n" );
    presets_info_basic( msgfp );
    presets_info_dm( msgfp );
    presets_info_r3mix( msgfp );
}


/* elaborate on details for all available presets
*/
static void  presets_longinfo ( FILE* msgfp )
{
    const char hr[] = "\n----------------------------------------------------------------\n";

    fprintf( msgfp, hr );
    presets_longinfo_basic( msgfp );
    fprintf( msgfp, hr );
    presets_info_dm( msgfp );
    fprintf( msgfp, hr );
    presets_info_r3mix( msgfp );
}


/* preset alias translation
 */
static const char* presets_alias( const char* preset_name )
{
    int i;
    unsigned char p0;

    struct ps_alias {
        const char *in;
        const char *out;
    } ps_xlat[] = {             /* alphabetically ordered alias table  */
        {"cd",       "basic-cd"},
        {"fm",       "basic-fm"},
        {"hifi",     "basic-hifi"},
        {"insane",   "dm-insane"},
        {"lw",       "basic-lw"},
        {"metal",    "dm-metal"},
        {"mw-eu",    "basic-mw-eu"},
        {"mw-us",    "basic-mw-us"},
        {"phon+",    "basic-phon+"},
        {"phone",    "basic-phone"},
        {"radio",    "basic-radio"},
        {"standard", "dm-standard"},
        {"studio",   "basic-studio"},
        {"sw",       "basic-sw"},
        {"tape",     "basic-tape"},
        {"voice",    "basic-voice"},
        {"xtreme",   "dm-xtreme"},
        {"\xff", ""}
    };

    p0 = preset_name[0];
    if( p0 < 0xff ) {
        for ( i = 0; ps_xlat[i].in[0] < p0; ++i )
            ;
        for ( ; ps_xlat[i].in[0] == p0; ++i ) {
            if( strcmp( preset_name, ps_xlat[i].in ) == 0 ) {
                return( ps_xlat[i].out );
            }
        }
    }

    return( preset_name );
}

static const char presets_set_err_unk[] = "ERROR, unknown --preset profile: ";

static int  presets_set_basic( lame_t gfp, const char* preset_name, 
                               FILE *msgfp )
{
    int i;

    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++ )
        if ( 0 == strncmp (preset_name, Presets[i].name, strlen (preset_name) ) ) {
            if ( Presets[i].resample >= 0 )
                (void) lame_set_out_samplerate( gfp, Presets[i].resample );
            if ( Presets[i].highpass_freq >= 0 )
                lame_set_highpassfreq(gfp,Presets[i].highpass_freq),
                lame_set_highpasswidth(gfp,0);
            lame_set_lowpassfreq(gfp,Presets[i].lowpass_freq);
            lame_set_lowpasswidth(gfp,Presets[i].lowpass_width);
            lame_set_no_short_blocks( gfp, Presets[i].no_short_blocks );
            lame_set_quality        ( gfp, Presets[i].quality         );
            lame_set_mode           ( gfp, Presets[i].mode            );
            lame_set_brate(gfp,Presets[i].cbr);
            lame_set_VBR_q(gfp,Presets[i].vbr_mode);
            lame_set_VBR_min_bitrate_kbps(gfp,Presets[i].vbr_min);
            lame_set_VBR_max_bitrate_kbps(gfp,Presets[i].vbr_max);
            return 0;
        }

    fprintf( msgfp, "%sbasic-%s\n", presets_set_err_unk, preset_name );
    return -1;
}


/* presets courtesy of r3mix
 */
static int  presets_set_r3mix( lame_t gfp, const char* preset_name, 
                               FILE *msgfp )
{
    if (preset_name[0] == '\0') {
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1); /*nspsytune*/
        /*  lame_set_experimentalX(gfp,1); (test CVS) */

        (void) lame_set_scale( gfp, 0.98 ); /* --scale 0.98*/

        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (8 << 20));

        lame_set_VBR(gfp,vbr_mtrh); 
        lame_set_VBR_q(gfp,1);
        lame_set_quality( gfp, 2 );
        lame_set_lowpassfreq(gfp,19500);
        lame_set_mode( gfp, JOINT_STEREO );
        lame_set_ATHtype( gfp, 3 );
        lame_set_VBR_min_bitrate_kbps(gfp,96);
    
        return 0;
    }
/*  else if( strcmp(preset_name, "-...") == 0 ) {
        ;
        return 0;
    }
*/
    fprintf( msgfp, "%sr3mix%s\n", presets_set_err_unk, preset_name );
    presets_info_r3mix( msgfp );
    return -1;
}

extern void lame_set_msfix( lame_t gfp, double msfix );

/*  some presets thanks to Dibrom
 */
static int  dm_presets( lame_t gfp, int fast, int cbr, const char* preset_name, const char* ProgramName )
{
    int k; 

    typedef struct {
        int    abr_kbps;
        int    expZ;
        int    expX;
        int    lowpass;
        int    safejoint;
        double nsmsfix;
        double nsbass;
        double scale;
        int    kbps_compensate;
    } dm_abr_presets_t;


    // Switch mappings for ABR mode
    const dm_abr_presets_t abr_switch_map [] = {
        // kbps Z  X  lowpass safejoint nsmsfix ns-bass scale  kbps_compensate
        {  80,  1, 1, 13500,  0,        0   ,   -3,      0.85, 8    }, //  80
        {  96,  1, 1, 15300,  0,        0   ,   -4,      0.85, 9    }, //  96
        { 112,  1, 1, 16000,  0,        0   ,   -5,      0.87, 8    }, // 112
        { 128,  1, 1, 17500,  0,        0   ,   -6,      0.93, 9    }, // 128
        { 160,  1, 1, 18000,  0,        0   ,   -4,      0.95, 7    }, // 160
        { 192,  1, 1, 19500,  1,        1.7 ,   -2,      0.97, 6    }, // 192
        { 224,  1, 1, 20000,  1,        1.25,    0,      0.98, 3    }, // 224
        { 256,  0, 3, 20500,  1,        0   ,    0,      1.00, 4    }, // 256
        { 320,  0, 3, 21000,  1,        0   ,    0,      1.00, 4    }  // 320
                                       };

    // Variables for the ABR stuff
    int actual_bitrate = atoi(preset_name);

    int lower_range = 0, lower_range_kbps = 0,
        upper_range = 0, upper_range_kbps = 0;
    
    int r; // r = resolved range

    int b;

    // Standard Preset == --nspsytune -V2 -mj -h --lowpass 19.5 -b112 --nssafejoint --athtype 4 --ns-sfb21 3
    if (strcmp(preset_name, "standard") == 0) {

        if (fast > 0)
           lame_set_VBR(gfp, vbr_mtrh);
        else
           lame_set_VBR(gfp, vbr_rh);

        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);
        lame_set_experimentalZ(gfp, 1);
        lame_set_experimentalX(gfp, 1);
        lame_set_VBR_q(gfp, 2);
        lame_set_quality(gfp, 2);
        lame_set_lowpassfreq(gfp, 19500);
        lame_set_mode(gfp, JOINT_STEREO);
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2); // safejoint
        lame_set_ATHtype(gfp, 4);
        lame_set_VBR_min_bitrate_kbps(gfp, 112);

        // modify sfb21 by 3 dB plus ns-treble=0                 
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (12 << 20));

        return 0;
    }
    
    // Xtreme Preset == --nspsytune -V2 -mj -h --lowpass 19.5 -b128 --nssafejoint --athtype 2 --ns-sfb21 3 -X3
    else if (strcmp(preset_name, "xtreme") == 0){

        if (fast > 0) {
           lame_set_VBR(gfp, vbr_mtrh);
           lame_set_experimentalX(gfp, 2);
        }
        else {
           lame_set_VBR(gfp, vbr_rh);
           lame_set_experimentalX(gfp, 3);
        }

        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);
        lame_set_experimentalZ(gfp, 1);
        lame_set_VBR_q(gfp, 2);
        lame_set_quality(gfp, 2);
        lame_set_lowpassfreq(gfp, 19500);
        lame_set_mode(gfp, JOINT_STEREO);
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2); // safejoint
        lame_set_ATHtype(gfp, 2);
        lame_set_VBR_min_bitrate_kbps(gfp, 128);					

        // modify sfb21 by 3 dB plus ns-treble=0                 
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (12 << 20));

        return 0;
    }
    					
    // Insane Preset == --nspsytune -b320 -h -mj --nssafejoint --lowpass 20 --athtype 2 -X3
    else if (((strcmp(preset_name, "insane") == 0) || 
              (strcmp(preset_name, "320"   ) == 0))   && (fast < 1)) {

        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);
        lame_set_experimentalX(gfp, 3);
        lame_set_brate(gfp, 320);
        lame_set_quality(gfp, 2);
        lame_set_mode(gfp, JOINT_STEREO);
        lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2); // safejoint
        lame_set_lowpassfreq(gfp, 20500);
        lame_set_ATHtype(gfp, 2);

        return 0;
    }

    // Generic ABR Preset
    else if (((atoi(preset_name)) > 0) &&  (fast < 1)) {
        if ((atoi(preset_name)) >= 80 && (atoi(preset_name)) <= 320) {

            // We assume specified bitrate will be 320kbps
            upper_range_kbps = abr_switch_map[8].abr_kbps;
            upper_range = 8;
            lower_range_kbps = abr_switch_map[8].abr_kbps;
            lower_range = 8;
 
            // Determine which significant bitrates the value specified falls between,
            // if loop ends without breaking then we were correct above that the value was 320
            for (b = 1; b < 9; b++) {
                if ((Max(actual_bitrate, abr_switch_map[b].abr_kbps)) != actual_bitrate) {
                      upper_range_kbps = abr_switch_map[b].abr_kbps;
                      upper_range = b;
                      lower_range_kbps = abr_switch_map[b-1].abr_kbps;
                      lower_range = (b-1);
                      break; // We found upper range 
                }
            }

            // Determine which range the value specified is closer to
            if ((upper_range_kbps - actual_bitrate) > (actual_bitrate - lower_range_kbps))
                r = lower_range;
            else
                r = upper_range;

            // If the user specified the CBR option, use that instead of ABR
            if (cbr > 0) {
                if ((lower_range_kbps == actual_bitrate)  ||
                    (upper_range_kbps == actual_bitrate)) {
                    lame_set_brate(gfp, actual_bitrate);
                }
                else {
                    lame_version_print ( stdout );
                    fprintf(stdout,"Error: The \"cbr\" option was used but the bitrate does not match allowed values.\n"
                                   "\n"
                                   "Allowed values are:\n"
                                   "\" 80\"\n"
                                   "\" 96\"\n"
                                   "\"112\"\n"
                                   "\"128\"\n"
                                   "\"160\"\n"
                                   "\"192\"\n"
                                   "\"224\"\n"
                                   "\"256\"\n"
                                   "\"320\"\n"
                                   "\n"
                                   "For further information try: \"%s --dm-preset help\"\n"
                                   , ProgramName
                           );
                    return -1;
                }
            }
            else {
                lame_set_VBR(gfp, vbr_abr); 
                lame_set_VBR_mean_bitrate_kbps(gfp, (actual_bitrate + abr_switch_map[r].kbps_compensate));
                lame_set_VBR_mean_bitrate_kbps(gfp, Min(lame_get_VBR_mean_bitrate_kbps(gfp), 320)); 
                lame_set_VBR_mean_bitrate_kbps(gfp, Max(lame_get_VBR_mean_bitrate_kbps(gfp), 8)); 
            }

            lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);
            lame_set_experimentalZ(gfp, abr_switch_map[r].expZ);
            lame_set_experimentalX(gfp, abr_switch_map[r].expX);
            lame_set_quality(gfp, 2);
            lame_set_lowpassfreq(gfp, abr_switch_map[r].lowpass);
            lame_set_mode(gfp, JOINT_STEREO);

            if (abr_switch_map[r].safejoint > 0)
                lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 2); // safejoint

            if (abr_switch_map[r].nsmsfix > 0)
                    (void) lame_set_msfix( gfp, abr_switch_map[r].nsmsfix );

            // ns-bass tweaks
            if (abr_switch_map[r].nsbass != 0) {
                k = (int)(abr_switch_map[r].nsbass * 4);
                if (k < 0) k += 64;
                lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (k << 2));
            }

            // ABR seems to have big problems with clipping, especially at low bitrates
            // so we compensate for that here by using a scale value depending on bitrate
            if (abr_switch_map[r].scale != 1)
                (void) lame_set_scale( gfp, abr_switch_map[r].scale );

            lame_set_ATHtype(gfp, 2);

            return 0;
    
        }
        else {
            lame_version_print ( stdout );
            fprintf(stdout,"Error: The bitrate specified is out of the valid range for this preset\n"
                           "\n"
                           "When using this mode you must enter a value between \"80\" and \"320\"\n"
                           "\n"
                           "For further information try: \"%s --dm-preset help\"\n"                  

                           , ProgramName
                   );
            return -1;
        }
    }

    lame_version_print ( stdout );
    fprintf(stdout,"Error: You did not enter a valid profile and/or options with --dm-preset\n"
                   "\n"
                   "Available profiles are:\n"
                   "\n"
                   "   <fast>        standard\n"
                   "   <fast>        xtreme\n"
                   "                 insane\n"
                   "          <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
                   "                             simply specify a bitrate. For example:\n"
                   "                                         \"--dm-preset 185\" activates this\n"
                   "                             preset and uses 185 as an average kbps.\n" 
                   "\n"
                   "    Some examples:\n"
                   "\n"
                   " or \"%s --dm-preset fast standard <input file> <output file>\"\n"
                   " or \"%s --dm-preset cbr 192 <input file> <output file>\"\n"
                   " or \"%s --dm-preset 172 <input file> <output file>\"\n"
                   " or \"%s --dm-preset xtreme <input file> <output file>\"\n"                   
                   "\n"
                   "For further information try: \"%s --dm-preset help\"\n"                  

                   , ProgramName, ProgramName, ProgramName, ProgramName, ProgramName
           );
    return -1;
}

static void genre_list_handler (int num,const char *name,void *cookie)
{
    printf ("%3d %s\n", num, name);
}


/************************************************************************
*
* parse_args
*
* PURPOSE:  Sets encoding parameters to the specifications of the
* command line.  Default settings are used for parameters
* not specified in the command line.
*
* If the input file is in WAVE or AIFF format, the sampling frequency is read
* from the AIFF header.
*
* The input and output filenames are read into #inpath# and #outpath#.
*
************************************************************************/

/* would use real "strcasecmp" but it isn't portable */
static int local_strcasecmp ( const char* s1, const char* s2 )
{
    unsigned char c1;
    unsigned char c2;
    
    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if (!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while (c1 == c2);
    return c1 - c2;
}


// LAME is a simple frontend which just uses the file extension
// to determine the file type.  Trying to analyze the file
// contents is well beyond the scope of LAME and should not be added.
static int filename_to_type ( const char* FileName )
{
    int len = strlen (FileName);
    
    if ( len < 4 ) return sf_unknown;

    FileName += len-4;
    if ( 0 == local_strcasecmp ( FileName, ".mpg" ) ) return sf_mp1;
    if ( 0 == local_strcasecmp ( FileName, ".mp1" ) ) return sf_mp1;
    if ( 0 == local_strcasecmp ( FileName, ".mp2" ) ) return sf_mp2;
    if ( 0 == local_strcasecmp ( FileName, ".mp3" ) ) return sf_mp3;
#if defined(HAVE_VORBIS)
    if ( 0 == local_strcasecmp ( FileName, ".ogg" ) ) return sf_ogg;
#endif
    if ( 0 == local_strcasecmp ( FileName, ".wav" ) ) return sf_wave;
    if ( 0 == local_strcasecmp ( FileName, ".aif" ) ) return sf_aiff;
    if ( 0 == local_strcasecmp ( FileName, ".raw" ) ) return sf_raw;
    return sf_unknown;
}

static int resample_rate ( double freq )
{
    if (freq >= 1.e3) freq *= 1.e-3;

    switch ( (int)freq ) {
    case  8: return  8000;
    case 11: return 11025;
    case 12: return 12000;
    case 16: return 16000;
    case 22: return 22050;
    case 24: return 24000;
    case 32: return 32000;
    case 44: return 44100;
    case 48: return 48000;
    default: fprintf(stderr,"Illegal resample frequency: %.3f kHz\n", freq );
             return 0;
    }
}

/* Ugly, NOT final version */

#define T_IF(str)          if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF(str)        } else if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF2(str1,str2) } else if ( 0 == local_strcasecmp (token,str1)  ||  0 == local_strcasecmp (token,str2) ) {
#define T_ELSE             } else {
#define T_END              }

int  parse_args ( lame_global_flags* gfp, int argc, char** argv, 
char* const inPath, char* const outPath, char **nogap_inPath, int *num_nogap)
{
    int         err;
    int         input_file=0;  // set to 1 if we parse an input file name 
    int         i;
    int         autoconvert  = 0;
    double      val;
    int         nogap=0;
    const char* ProgramName  = argv[0]; 
    int count_nogap=0;

    inPath [0] = '\0';   
    outPath[0] = '\0';
    /* turn on display options. user settings may turn them off below */
    silent   = 0;
    brhist   = 1;
    enc_padding=-1;
    enc_delay=-1;
    mp3_delay = 0;   
    mp3_delay_set=0;
    disable_wav_header=0;
    id3tag_init (gfp);

    /* process args */
    for ( i = 0, err = 0; ++i < argc  &&  !err; ) {
        char   c;
        char*  token;
        char*  arg;
        char*  nextArg;
        int    argUsed;
    
        token = argv[i];
        if ( *token++ == '-' ) {
            argUsed = 0;
            nextArg = i+1 < argc  ?  argv[i+1]  :  "";
            
            if (! *token) { /* The user wants to use stdin and/or stdout. */
                input_file = 1;
                if (inPath [0] == '\0')
                    strncpy (inPath, argv[i],MAX_NAME_SIZE);
                else 
                if (outPath[0] == '\0') 
                    strncpy (outPath, argv[i],MAX_NAME_SIZE);
            } 
            if (*token == '-') { /* GNU style */
                token++;

                T_IF ("resample")
                    argUsed = 1;
                    (void) lame_set_out_samplerate( gfp,
                        resample_rate ( atof (nextArg) ) );
                
                T_ELIF ("vbr-old")
                    lame_set_VBR(gfp,vbr_rh); 
                
                T_ELIF ("vbr-new")
                    lame_set_VBR(gfp,vbr_mtrh); 
                
                T_ELIF ("vbr-mtrh")
                    lame_set_VBR(gfp,vbr_mtrh); 

                T_ELIF ("r3mix")
		    presets_set_r3mix(gfp, "", stderr);
                                    
                /**
                 *  please, do *not* DOCUMENT this one
                 *  it is a developers only switch (rh)
                 */
                T_ELIF ("tune")
                    argUsed=1;
                    {extern void lame_set_tune(lame_t gfp, float val);
                    lame_set_tune(gfp,atof(nextArg));}    
                    
                T_ELIF ("abr")
                    argUsed=1;
                    lame_set_VBR(gfp,vbr_abr); 
                    lame_set_VBR_mean_bitrate_kbps(gfp,atoi(nextArg));
                    /* values larger than 8000 are bps (like Fraunhofer), so it's strange to get 320000 bps MP3 when specifying 8000 bps MP3 */
                    if ( lame_get_VBR_mean_bitrate_kbps(gfp) >= 8000 )
                        lame_set_VBR_mean_bitrate_kbps(gfp,( lame_get_VBR_mean_bitrate_kbps(gfp) + 500 ) / 1000);

                    lame_set_VBR_mean_bitrate_kbps(gfp,Min(lame_get_VBR_mean_bitrate_kbps(gfp), 320)); 
                    lame_set_VBR_mean_bitrate_kbps(gfp,Max(lame_get_VBR_mean_bitrate_kbps(gfp),8)); 
                
                T_ELIF ("bitwidth")
                    argUsed=1;
                    in_bitwidth=atoi(nextArg);

                T_ELIF ("signed")
		    in_signed=1;

                T_ELIF ("unsigned")
		    in_signed=0;

                T_ELIF ("little-endian")
		    in_endian=order_littleEndian;

                T_ELIF ("big-endian")
		    in_endian=order_bigEndian;

                T_ELIF ("mp1input")
                    input_format=sf_mp1;
                
                T_ELIF ("mp2input")
                    input_format=sf_mp2;
                
                T_ELIF ("mp3input")
                    input_format=sf_mp3;
                
                T_ELIF ("ogginput")
#if defined(HAVE_VORBIS)
                    input_format=sf_ogg;
#else
                    fprintf(stderr,"Error: LAME not compiled with Vorbis support\n");
                    return -1;
#endif
                T_ELIF ("ogg")
#if defined(HAVE_VORBIS_ENCODER)
                    (void) lame_set_ogg( gfp, 1 );
#else
                    fprintf(stderr,"Error: LAME not compiled with Vorbis Encoder support\n");
                    return -1;
#endif
                T_ELIF ("phone")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                    fprintf(stderr, "Warning: --phone is deprecated, use --preset phone instead!");
                    
                T_ELIF ("voice")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                    fprintf(stderr, "Warning: --voice is deprecated, use --preset voice instead!");
                    
                T_ELIF ("radio")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                    fprintf(stderr, "Warning: --radio is deprecated, use --preset radio instead!");

                T_ELIF ("tape")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                fprintf(stderr, "Warning: --tape is deprecated, use --preset tape instead!");
                    
                T_ELIF ("cd")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                fprintf(stderr, "Warning: --cd is deprecated, use --preset cd instead!");
                    
                T_ELIF ("studio")
                    if (presets_set_basic( gfp, token, stderr ) < 0)
                        return -1;
                fprintf(stderr, "Warning: --studio is deprecated, use --preset studio instead!");
                    
                T_ELIF ("noshort")
                    (void) lame_set_no_short_blocks( gfp, 1 );
                
                T_ELIF ("short")
                    (void) lame_set_no_short_blocks( gfp, 0 );
                
                T_ELIF ("allshort")
                    (void) lame_set_force_short_blocks( gfp, 1 );
                
                T_ELIF ("decode")
                    (void) lame_set_decode_only( gfp, 1 );

                T_ELIF ("decode-mp3delay")
                    mp3_delay = atoi( nextArg );
                    mp3_delay_set=1;
                    argUsed=1;
                
                T_ELIF ("noath")
                    (void) lame_set_noATH( gfp, 1 );
                
                T_ELIF ("nores")
                    lame_set_disable_reservoir(gfp,1);
                    lame_set_padding_type(gfp, PAD_NO);
                
                T_ELIF ("strictly-enforce-ISO")
                    lame_set_strict_ISO(gfp,1);
                
                T_ELIF ("athonly")
                    (void) lame_set_ATHonly( gfp, 1 );
                
                T_ELIF ("athlower")
                    argUsed=1;
                    (void) lame_set_ATHlower( gfp, atof( nextArg ) );
                
                T_ELIF ("athtype")
                    argUsed=1;
                    (void) lame_set_ATHtype( gfp, atoi( nextArg ) );

                T_ELIF ("athaa-type")   //  switch for developing, no DOCU
                    argUsed=1;          //  1:Gaby, 2:Robert, 3:Jon, else:off
                    lame_set_athaa_type( gfp, atoi(nextArg) );

                T_ELIF ("athaa-loudapprox")
                    argUsed=1;
                    lame_set_athaa_loudapprox( gfp, atoi(nextArg) );

                T_ELIF ("athaa-sensitivity")
                    argUsed=1;
                    lame_set_athaa_sensitivity( gfp, atof(nextArg) );

                T_ELIF ("scale")
                    argUsed=1;
                    (void) lame_set_scale( gfp, atof(nextArg) );

		T_ELIF ("scale-l")
                    argUsed=1;
                    (void) lame_set_scale_left( gfp, atof(nextArg) );

                T_ELIF ("scale-r")
                    argUsed=1;
                    (void) lame_set_scale_right( gfp, atof(nextArg) );
                
                T_ELIF ("freeformat")
                    lame_set_free_format(gfp,1);
                
                T_ELIF ("athshort")
                    (void) lame_set_ATHshort( gfp, 1 );
                
                T_ELIF ("nohist")
                    brhist = 0;
                
#if defined(__OS2__)
      		T_ELIF ("priority")
      		    argUsed=1;
      		    setOS2Priority(gfp, atoi(nextArg));
#endif

                /* options for ID3 tag */
                T_ELIF ("tt")
                    argUsed=1;
                    id3tag_set_title(gfp, nextArg);
                
                T_ELIF ("ta")
                    argUsed=1;
                    id3tag_set_artist(gfp, nextArg);
                
                T_ELIF ("tl")
                    argUsed=1;
                    id3tag_set_album(gfp, nextArg);
                
                T_ELIF ("ty")
                    argUsed=1;
                    id3tag_set_year(gfp, nextArg);
                
                T_ELIF ("tc")
                    argUsed=1;
                    id3tag_set_comment(gfp, nextArg);
                
                T_ELIF ("tn")
                    argUsed=1;
                    id3tag_set_track(gfp, nextArg);
                
                T_ELIF ("tg")
                    argUsed=1;
                    if (id3tag_set_genre(gfp, nextArg)) {
                        fprintf(stderr,"Unknown genre: %s.  Specify genre name or number\n", nextArg);
                        return -1;
                    }
                
                T_ELIF ("add-id3v2")
                    id3tag_add_v2(gfp);
                
                T_ELIF ("id3v1-only")
                    id3tag_v1_only(gfp);
                
                T_ELIF ("id3v2-only")
                    id3tag_v2_only(gfp);
                
                T_ELIF ("space-id3v1")
                    id3tag_space_v1(gfp);
                
                T_ELIF ("pad-id3v2")
                    id3tag_pad_v2(gfp);
                
                T_ELIF ("genre-list")
                    id3tag_genre_list(genre_list_handler, NULL);
                    return -2;
                
                T_ELIF ("lowpass")
                    val     = atof( nextArg );
                    argUsed = 1;
                    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
                    if ( val < 0.001 || val > 50000. ) {
                        fprintf(stderr,"Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
                        return -1;
                    }
                    lame_set_lowpassfreq(gfp,val * (val < 50. ? 1.e3 : 1.e0 ) + 0.5);
                
                T_ELIF ("lowpass-width")
                    argUsed=1;
                    val = 1000.0 * atof( nextArg ) + 0.5;
                    if (val  < 0) {
                        fprintf(stderr,"Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
                        return -1;
                    }
                    lame_set_lowpasswidth(gfp,val); 
                
                T_ELIF ("highpass")
                    val = atof( nextArg );
                    argUsed=1;
                    /* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
                    if ( val < 0.001 || val > 50000. ) {
                        fprintf(stderr,"Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
                        return -1;
                    }
                    lame_set_highpassfreq(gfp, val * (val < 16. ? 1.e3 : 1.e0 ) + 0.5);
                
                T_ELIF ("highpass-width")
                    argUsed=1;
                    val = 1000.0 * atof( nextArg ) + 0.5;
                    if (val   < 0) {
                        fprintf(stderr,"Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
                        return -1;
                    }
                    lame_set_highpasswidth(gfp,val);
                
                T_ELIF ("cwlimit")
                    val = atof (nextArg);
                    argUsed=1;
                    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
                    {
                        int my_cwlimit = val * ( val <= 50. ? 1.e3 : 1.e0 );
                        lame_set_cwlimit( gfp, my_cwlimit );
                        if ( my_cwlimit <= 0 ) {
                            fprintf( stderr,
              "Must specify cwlimit with --cwlimit freq, freq >= 0.001 kHz\n" );
                            return -1;
                        }
                    }
                 
                T_ELIF ("comp")
                    argUsed=1;
                    val = atof( nextArg );
                    if (val  < 1.0 ) {
                        fprintf(stderr,"Must specify compression ratio >= 1.0\n");
                        return -1;
                    }
                    lame_set_compression_ratio(gfp,val);
                
                T_ELIF ("notemp")
                    (void) lame_set_useTemporal( gfp, 0 );

                T_ELIF ("temporal-masking")
                    argUsed = 1;
                    (void) lame_set_useTemporal( gfp, atoi(nextArg)?1:0 );

                T_ELIF ("nspsytune")
                    lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1);
                    lame_set_experimentalZ(gfp,1);
                    lame_set_experimentalX(gfp,1);
                
                T_ELIF ("nssafejoint")
                    lame_set_exp_nspsytune(gfp,lame_get_exp_nspsytune(gfp) | 2);

                T_ELIF ("nsmsfix")
                    argUsed=1;
                    (void) lame_set_msfix( gfp, atof(nextArg) );
                
                T_ELIF ("ns-bass")
                    argUsed=1;
                    {
                      double d;
                      int k;
                      d = atof( nextArg );
                      k = (int)(d * 4);
                      if (k < -32) k = -32;
                      if (k >  31) k =  31;
                      if (k < 0) k += 64;
                      lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (k << 2));
                    }
                
                T_ELIF ("ns-alto")
                    argUsed=1;
                    {
                      double d;
                      int k;
                      d = atof( nextArg );
                      k = (int)(d * 4);
                      if (k < -32) k = -32;
                      if (k >  31) k =  31;
                      if (k < 0) k += 64;
                      lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (k << 8));
                    }
                
                T_ELIF ("ns-treble")
                    argUsed=1;
                    {
                      double d;
                      int k;
                      d = atof( nextArg );
                      k = (int)(d * 4);
                      if (k < -32) k = -32;
                      if (k >  31) k =  31;
                      if (k < 0) k += 64;
                      lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (k << 14));
                    }
                
                T_ELIF ("ns-sfb21")
                    /*  to be compatible with Naoki's original code, 
                     *  ns-sfb21 specifies how to change ns-treble for sfb21 */
                    argUsed=1;
                    {
                      double d;
                      int k;
                      d = atof( nextArg );
                      k = (int)(d * 4);
                      if (k < -32) k = -32;
                      if (k >  31) k =  31;
                      if (k < 0) k += 64;
                      lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (k << 20));
                    }
                
                /* some more GNU-ish options could be added
                 * brief         => few messages on screen (name, status report)
                 * o/output file => specifies output filename
                 * O             => stdout
                 * i/input file  => specifies input filename
                 * I             => stdin
                 */
                T_ELIF2 ("quiet", "silent")
                    silent = 10;    /* on a scale from 1 to 10 be very silent */
                
                T_ELIF ("verbose")
                    silent = -10;    /* print a lot on screen, the default */
                    
                T_ELIF2 ("version", "license")
                    print_license ( gfp, stdout, ProgramName );
                    return -2;
                
                T_ELIF2 ("help", "usage")
                    short_help ( gfp, stdout, ProgramName );
                    return -2;
                
                T_ELIF ("longhelp")
                    long_help ( gfp, stdout, ProgramName, 0 /* lessmode=NO */ );
                    return -2;
                    
                T_ELIF ("?")
#ifdef __unix__
                    FILE* fp = popen ("less -Mqc", "w");
                    long_help ( gfp, fp, ProgramName, 0 /* lessmode=NO */ );
                    pclose (fp);
#else           
                    long_help ( gfp, stdout, ProgramName, 1 /* lessmode=YES */ );
#endif              
                    return -2;
                
                T_ELIF ("preset") {
                    const char *xlatArg;
                    int rval = -1;
                    argUsed = 1;
                    xlatArg = presets_alias( nextArg );
                    
                    if( strncmp( xlatArg, "basic-", 6 ) == 0 ) {
                        rval = presets_set_basic( gfp, xlatArg + 6, stderr );
                    }
//                  else if (strncmp( xlatArg, "dm-", 3 ) == 0 ) {
//                      rval = dm_presets( gfp, xlatArg + 3, stderr );
//                  } 
                    else if( strncmp( xlatArg, "r3mix", 5 ) == 0 ) {
                        rval = presets_set_r3mix( gfp, xlatArg + 5, stderr );
                    }
                    else if( strcmp( xlatArg, "longhelp" ) == 0 ) {
                                /* send explicitly requested help to stdout */
                        presets_info_head( stdout );
                        presets_longinfo( stdout );
                    }
                    else {
                        if( strcmp( xlatArg, "help" ) == 0 ) {
                            presets_info_head( stdout );
                            presets_info( stdout );
                        }
                        else {
                            fprintf( stderr, "\n%s%s\n", 
                                     presets_set_err_unk, xlatArg );
                            presets_info( stderr );
                        }
                    }
                    if( rval < 0 ) {
                        return -1;
                    }
                }

                T_ELIF ("dm-preset")
                    argUsed = 1;
                    {
                    int fast = 0, cbr = 0;

                    if (strcmp(nextArg, "help") == 0) {
                        lame_version_print ( stdout );
                        fprintf ( stdout,
                        "The --dm-preset switches are designed to provide the highest possible quality.\n"
                        "\n"
                        "They have for the most part been subject to and tuned via rigorous double blind\n"
                        "listening tests to verify and achieve this objective.\n"
                        "\n"
                        "These are continually updated to coincide with the latest developments that\n"
                        "occur and as a result should provide you with nearly the best quality\n"
                        "currently possible from LAME.\n"
                        "\n"
                        "To activate these presets:\n"
                        "\n"
                        "   For VBR modes (generally highest quality):\n"
                        "\n"
                        "     \"--dm-preset standard\" This preset should generally be transparent\n"
                        "                            to most people on most music and is already\n"
                        "                            quite high in quality.\n"
                        "\n"
                        "     \"--dm-preset xtreme\"   If you have extremely good hearing and similar\n"
                        "                            equipment, this preset will generally provide\n"
                        "                            slightly higher quality than the \"standard\"\n"
                        "                            mode. This mode will also provide much higher\n"
                        "                            quality on certain clips heavy on synthetic\n"
                        "                            impulse sounds, such as those in fatboy.wav.\n"
                        "\n"
                        "   For CBR 320kbps (highest quality possible from the --dm-preset switches):\n"
                        "\n"
                        "     \"--dm-preset insane\"   This preset will usually be overkill for most\n"
                        "                            people and most situations, but if you must\n"
                        "                            have the absolute highest quality with no\n"
                        "                            regard to filesize, this is the way to go.\n"
                        "\n"
                        "   For ABR modes (high quality per given bitrate but not as high as VBR):\n"
                        "\n"
                        "     \"--dm-preset <kbps>\"   Using this preset will usually give you good\n"
                        "                            quality at a specified bitrate. Depending on the\n"
                        "                            bitrate entered, this preset will determine the\n"
                        "                            optimal settings for that particular situation.\n"
                        "                            While this approach works, it is not nearly as\n"
                        "                            flexible as VBR, and usually will not attain the\n"
                        "                            same level of quality as VBR at higher bitrates.\n"  
                        "\n"
                        "The following options are also available for the corresponding profiles:\n"
                        "\n"
                        "   <fast>        standard\n"
                        "   <fast>        xtreme\n"
                        "                 insane\n"
                        "          <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
                        "                             simply specify a bitrate. For example:n"
                        "                                         \"--dm-preset 185\" activates this\n"
                        "                             preset and uses 185 as an average kbps.\n" 
                        "\n"
                        "   \"fast\" - Enables the new fast VBR for a particular profile. The\n"
                        "            disadvantage to the speed switch is that often times the\n"
                        "            bitrate will be slightly higher than with the normal mode\n"
                        "            and quality may be slightly lower also.\n"
                        "\n"
                        "   \"cbr\"  - If you use the ABR mode (read above) with a significant\n"
                        "            bitrate such as 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"
                        "            you can use the \"cbr\" option to force CBR mode encoding\n"
                        "            instead of the standard abr mode. ABR does provide higher\n"
                        "            quality but CBR may be useful in situations such as when\n"
                        "            streaming an mp3 over the internet may be important.\n"
                        "\n"
                        "    For example:\n"
                        "\n"
                        " or \"%s --dm-preset fast standard <input file> <output file>\"\n"
                        " or \"%s --dm-preset cbr 192 <input file> <output file>\"\n"
                        " or \"%s --dm-preset 172 <input file> <output file>\"\n"
                        " or \"%s --dm-preset xtreme <input file> <output file>\"\n"
                        , ProgramName, ProgramName, ProgramName, ProgramName);
                       return -1;
                    }

                    while ((strcmp(nextArg, "fast")  == 0) ||
                           (strcmp(nextArg, "cbr")   == 0))  {

                        if ((strcmp(nextArg, "fast") == 0) && (fast < 1))
                            fast = 1;
                        if ((strcmp(nextArg, "cbr")  == 0) && (cbr  < 1))
                            cbr = 1;

                        argUsed++;
                        nextArg = i+argUsed < argc  ?  argv[i+argUsed]  :  "";
                    }

                    if (dm_presets ( gfp, fast, cbr, nextArg, ProgramName ) < 0)
                    return -1;
                }

                T_ELIF ("disptime")
                    argUsed = 1;
                    update_interval = atof (nextArg);

		T_ELIF ("nogapout")
		    strcpy(outPath, nextArg);
		    argUsed = 1;
                
                T_ELIF ("nogap")
                    nogap=1;
                    
                T_ELSE
                    fprintf(stderr,"%s: unrec option --%s\n", ProgramName, token);
                    
                T_END
                
                i += argUsed;
                
            } else {
                while ( (c = *token++) != '\0' ) {
                    arg = *token ? token : nextArg;
                    switch (c) {
                    case 'm':        
                        argUsed           = 1;   
                        
                        switch ( *arg ) {
                        case 's': (void) lame_set_mode( gfp, STEREO       );
                                  break;
                        case 'd': (void) lame_set_mode( gfp, DUAL_CHANNEL );
                                  fprintf( stderr,
                                           "%s: dual channel is not supported yet, the result (perhaps stereo)\n"
                                           "  may not be what you expect\n",
                                           ProgramName );
                                  break;
                        case 'f': lame_set_force_ms(gfp,1);
                                 /* FALLTHROUGH */
                        case 'j': (void) lame_set_mode( gfp, JOINT_STEREO );
                                  break;
                        case 'm': (void) lame_set_mode( gfp, MONO         );
                                  break;
                        case 'a': (void) lame_set_mode_automs( gfp, 1 );
                                  /* lame picks mode and uses variable MS
                                     threshold */
                                  break; 
                        default : fprintf(stderr,"%s: -m mode must be s/d/j/f/m not %s\n", ProgramName, arg);
                            err = 1;
                            break;
                        }
                        break;
            
                    case 'V':        argUsed = 1;
                        /* to change VBR default look in lame.h */
                        if (lame_get_VBR(gfp) == vbr_off) lame_set_VBR(gfp,vbr_default);  
                        lame_set_VBR_q(gfp,atoi(arg));
                        if (lame_get_VBR_q(gfp) <0) lame_set_VBR_q(gfp,0);
                        if (lame_get_VBR_q(gfp) >9) lame_set_VBR_q(gfp,9);
                        break;
                    case 'v': 
                        /* to change VBR default look in lame.h */
                        if (lame_get_VBR(gfp) == vbr_off)
                            lame_set_VBR(gfp,vbr_default); 
                        break;

                    case 'q':        argUsed = 1;
                        {
                            int tmp_quality = atoi( arg );

                            /* XXX should we move this into lame_set_quality()? */
                            if( tmp_quality < 0 ) tmp_quality = 0;
                            if( tmp_quality > 9 ) tmp_quality = 9;

                            (void) lame_set_quality( gfp, tmp_quality );
                        }
                        break;
                    case 'f': 
                        (void) lame_set_quality( gfp, 7 );
                        break;
                    case 'h': 
                        (void) lame_set_quality( gfp, 2 );
                        break;

                    case 's':
                        argUsed = 1;
                        val = atof( arg );
                        (void) lame_set_in_samplerate( gfp,
                            val * ( val <= 192 ? 1.e3 : 1.e0 ) + 0.5 );
                        break;
                    case 'b':        
                        argUsed = 1;
                        lame_set_brate(gfp,atoi(arg)); 
                        lame_set_VBR_min_bitrate_kbps(gfp,lame_get_brate(gfp));
                        break;
                    case 'B':        
                        argUsed = 1;
                        lame_set_VBR_max_bitrate_kbps(gfp,atoi(arg)); 
                        break;  
                    case 'F':        
                        lame_set_VBR_hard_min(gfp,1);
                        break;  
                    case 't':  /* dont write VBR tag */
                        (void) lame_set_bWriteVbrTag( gfp, 0 );
                        disable_wav_header=1;
                        break;
                    case 'r':  /* force raw pcm input file */
#if defined(LIBSNDFILE)
                        fprintf(stderr,"WARNING: libsndfile may ignore -r and perform fseek's on the input.\n"
                               "Compile without libsndfile if this is a problem.\n");
#endif
                        input_format=sf_raw;
                        break;
                    case 'x':  /* force byte swapping */
                        swapbytes=1;
                        break;
                    case 'p': /* (jo) error_protection: add crc16 information to stream */
                        lame_set_error_protection(gfp,1);
                        break;
                    case 'a': /* autoconvert input file from stereo to mono - for mono mp3 encoding */
                        autoconvert=1;
                        (void) lame_set_mode( gfp, MONO );
                        break;
                    case 'k': 
                        lame_set_lowpassfreq(gfp,-1);
                        lame_set_highpassfreq(gfp,-1);
                        break;
                    case 'd': 
                        (void) lame_set_allow_diff_short( gfp, 1 );
                        break;
                    case 'S': 
                        silent = 1;
                        break;
                    case 'X':        
                        argUsed = 1;   
                        lame_set_experimentalX(gfp,atoi(arg)); 
                        break;
                    case 'Y': 
                        lame_set_experimentalY(gfp,1);
                        break;
                    case 'Z': 
                        lame_set_experimentalZ(gfp,1);
                        break;
                    case 'e':        
                        argUsed = 1;
                        
                        switch (*arg) {
                        case 'n': lame_set_emphasis( gfp, 0 ); break;
                        case '5': lame_set_emphasis( gfp, 1 ); break;
                        case 'c': lame_set_emphasis( gfp, 3 ); break;
                        default : fprintf(stderr,"%s: -e emp must be n/5/c not %s\n", ProgramName, arg );
                            err = 1;
                            break;
                        }
                        break;
                    case 'c':   
                        lame_set_copyright(gfp,1);
                        break;
                    case 'o':   
                        lame_set_original(gfp,0);
                        break;
                        
                    case '?':   
                        long_help ( gfp, stderr, ProgramName, 0 /* LESSMODE=NO */);
                        return -1;
                        
                    default:    
                        fprintf(stderr,"%s: unrec option %c\n", ProgramName, c);
                        err = 1; 
                        break;
                    }
                    if (argUsed) {
                        if(arg == token)    token = "";   /* no more from token */
                        else                ++i;          /* skip arg we used */
                        arg = ""; argUsed = 0;
                    }
                }
            }   
        } else {
            if (nogap) {
                if ((num_nogap != NULL) && (count_nogap < *num_nogap)) {
                    strncpy(nogap_inPath[count_nogap++],argv[i],MAX_NAME_SIZE);
                    input_file=1;
                } else {
                    /* sorry, calling program did not allocate enough space */
                    fprintf(stderr,"Error: 'nogap option'.  Calling program does not allow nogap option, or\nyou have exceeded maximum number of input files for the nogap option\n");
                    *num_nogap=-1;
                    return -1;
                }
            }else{
                /* normal options:   inputfile  [outputfile], and
                   either one can be a '-' for stdin/stdout */
                if (inPath [0] == '\0') {     
                    strncpy(inPath , argv[i], MAX_NAME_SIZE);
                    input_file=1;
                } else {
                    if (outPath[0] == '\0') 
                        strncpy(outPath, argv[i], MAX_NAME_SIZE);
                    else {
                        fprintf(stderr,"%s: excess arg %s\n", ProgramName, argv[i]);
                        err = 1;
                    }
                }
            }
        }
    }  /* loop over args */
    
    if ( err  ||  !input_file ) {
        usage ( gfp, stderr, ProgramName );
        return -1;
    }
        
    if ( inPath[0] == '-' ) 
        silent = 1;  /* turn off status - it's broken for stdin */
#ifdef WIN32
    else
        dosToLongFileName( inPath );
#endif
    
    if ( outPath[0] == '\0' && count_nogap == 0) {
        if ( inPath[0] == '-' ) {
            /* if input is stdin, default output is stdout */
            strcpy(outPath,"-");
        } else {
            strncpy(outPath, inPath, MAX_NAME_SIZE - 4);
            if ( lame_get_decode_only( gfp ) ) {
                strncat (outPath, ".wav", 4 );
            } else if( lame_get_ogg( gfp ) ) {
                strncat (outPath, ".ogg", 4 );
            } else {
                strncat (outPath, ".mp3", 4 );
            }
        }
    }
    
    /* some file options not allowed with stdout */
    if (outPath[0]=='-') {
        (void) lame_set_bWriteVbrTag( gfp, 0 ); /* turn off VBR tag */
    }
    
    /* if user did not explicitly specify input is mp3, check file name */
    if (input_format == sf_unknown)
        input_format = filename_to_type ( inPath );
    
#if !(defined HAVE_MPGLIB || defined AMIGA_MPEGA)
    if ( input_format == sf_mp1 ||
         input_format == sf_mp2 ||
         input_format == sf_mp3) {
        fprintf(stderr,"Error: libmp3lame not compiled with mpg123 *decoding* support \n");
        return -1;
    }
#endif

#if !(defined HAVE_VORBIS)
    if ( input_format == sf_ogg ) {
        fprintf(stderr,"Error: LAME not compiled with Vorbis support\n");
        return -1;
    }
#endif
    /* default guess for number of channels */
    if (autoconvert) 
        (void) lame_set_num_channels( gfp, 2 ); 
    else if( MONO == lame_get_mode( gfp ) ) 
        (void) lame_set_num_channels( gfp, 1 );
    else 
        (void) lame_set_num_channels( gfp, 2 );
    
    if ( lame_get_free_format(gfp) ) {
        if ( lame_get_brate(gfp) < 8  ||  lame_get_brate(gfp) > 640 ) {
            fprintf(stderr,"For free format, specify a bitrate between 8 and 640 kbps\n");
            return -1;
        }
    }
    if (num_nogap!=NULL) *num_nogap=count_nogap;
    return 0;
}

/* end of parse.c */