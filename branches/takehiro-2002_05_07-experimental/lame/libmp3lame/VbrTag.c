/*
 *	Xing VBR tagging for LAME.
 *
 *	Copyright (c) 1999 A.L. Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "machine.h"
#include "bitstream.h"
#include "lame.h"
#include "VbrTag.h"
#include "version.h"
#include "id3tag.h"

#include	<assert.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#ifdef __sun__
/* woraround for SunOS 4.x, it has SEEK_* defined here */
#include <unistd.h>
#endif

/*
 *    4 bytes for Header Tag
 *    4 bytes for Header Flags
 *  100 bytes for entry (NUMTOCENTRIES)
 *    4 bytes for FRAME SIZE
 *    4 bytes for STREAM_SIZE
 *    4 bytes for VBR SCALE. a VBR quality indicator: 0=best 100=worst
 *   20 bytes for LAME tag.  for example, "LAME3.12 (beta 6)"
 * ___________
 *  140 bytes
*/
#define VBRHEADERSIZE (NUMTOCENTRIES+4+4+4+4+4)

#define LAMEHEADERSIZE (VBRHEADERSIZE + 9 + 1 + 1 + 8 + 1 + 1 + 3 + 1 + 1 + 2 + 4 + 2 + 2)

/* the size of the Xing header (MPEG1 and MPEG2) in kbps */
#define XING_BITRATE1 128
#define XING_BITRATE2  64
#define XING_BITRATE25 32

static const char	VBRTag[]={"Xing"};
static const char	VBRTag2[]={"Info"};




/* Lookup table for fast CRC computation
 * See 'CRC_update_lookup'
 * Uses the polynomial x^16+x^15+x^2+1 */

unsigned int crc16_lookup[256] =
{
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40, 
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41, 
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441, 
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640, 
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041, 
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241, 
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};





/*-------------------------------------------------------------*/
static int ExtractI4(unsigned char *buf)
{
    int x;
    /* big endian extract */
    x = buf[0];
    x <<= 8;
    x |= buf[1];
    x <<= 8;
    x |= buf[2];
    x <<= 8;
    x |= buf[3];
    return x;
}

int
GetVbrTag(VBRTAGDATA *pTagData,  unsigned char *buf)
{
    int	i, head_flags, h_bitrate,h_id, h_mode, h_sr_index;
    int enc_delay, enc_padding; 

    /* get selected MPEG header data */
    h_id       = (buf[1] >> 3) & 1;
    h_sr_index = (buf[2] >> 2) & 3;
    h_mode     = (buf[3] >> 6) & 3;
    h_bitrate  = ((buf[2]>>4)&0xf);
    h_bitrate = bitrate_table[h_id][h_bitrate];

    /* get Vbr header data */
    pTagData->flags = 0;

    /* check for FFE syncword */
    if ((buf[1]>>4)==0xE) 
	pTagData->samprate = samplerate_table[2][h_sr_index];
    else
	pTagData->samprate = samplerate_table[h_id][h_sr_index];

    /*  determine offset of header */
    if (h_id) {
	/* mpeg1 */
	if (h_mode != 3)	buf+=(32+4);
	else				buf+=(17+4);
    }
    else
    {
	/* mpeg2 */
	if (h_mode != 3) buf+=(17+4);
	else             buf+=(9+4);
    }

    if (strncmp(buf, VBRTag, 4) && strncmp(buf, VBRTag2, 4))
	return 0;
    buf+=4;

    pTagData->h_id = h_id;
    head_flags = pTagData->flags = ExtractI4(buf); buf+=4;      /* get flags */

    if (head_flags & FRAMES_FLAG)
	pTagData->frames   = ExtractI4(buf); buf+=4;

    if (head_flags & BYTES_FLAG)
	pTagData->bytes = ExtractI4(buf); buf+=4;

    if (head_flags & TOC_FLAG){
	if(pTagData->toc)
	{
	    for(i=0;i<NUMTOCENTRIES;i++)
		pTagData->toc[i] = buf[i];
	}
	buf+=NUMTOCENTRIES;
    }

    pTagData->vbr_scale = -1;

    if (head_flags & VBR_SCALE_FLAG)
	pTagData->vbr_scale = ExtractI4(buf); buf+=4;

    pTagData->headersize = ((h_id+1)*72000*h_bitrate) / pTagData->samprate;

    buf+=21;
    enc_delay = buf[0] << 4;
    enc_delay += buf[1] >> 4;
    enc_padding= (buf[1] & 0x0F)<<8;
    enc_padding += buf[2];
    /* check for reasonable values (this may be an old Xing header, */
    /* not a INFO tag) */
    if (enc_delay<0 || enc_delay > 3000) enc_delay=-1;
    if (enc_padding<0 || enc_padding > 3000) enc_padding=-1;

    pTagData->enc_delay=enc_delay;
    pTagData->enc_padding=enc_padding;

    return 1;       /* success */
}


/***********************************************************************
 *  Robert Hegemann 2001-01-17
 ***********************************************************************/
static void Xing_seek_table(VBR_seek_info_t * v, unsigned char *t)
{
    int i, index, seek_point;
    if (v->pos <= 0)
        return;

    for (i = 1; i < NUMTOCENTRIES; ++i) {
	float j = i/(float)NUMTOCENTRIES, act, sum;
        index = (int)(floor(j * v->pos));
        if (index > v->pos-1)
            index = v->pos-1;
        act = v->bag[index];
        sum = v->sum;
        seek_point = (int)(256. * act / sum);
        if (seek_point > 255)
            seek_point = 255;
        t[i] = seek_point;
    }
}

/****************************************************************************
 * AddVbrFrame: Add VBR entry, used to fill the VBR the TOC entries
 * Paramters:
 *	nStreamPos: how many bytes did we write to the bitstream so far
 *				(in Bytes NOT Bits)
 ****************************************************************************
*/
void AddVbrFrame(lame_global_flags *gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int i, bitrate = bitrate_table[gfp->version][gfc->bitrate_index];
    assert(gfc->VBR_seek_table.bag);

    gfc->VBR_seek_table.sum += bitrate;
    gfc->VBR_seek_table.seen++;

    if (gfc->VBR_seek_table.seen < gfc->VBR_seek_table.want)
        return;

    if (gfc->VBR_seek_table.pos < gfc->VBR_seek_table.size) {
	gfc->VBR_seek_table.bag[gfc->VBR_seek_table.pos++]
	    = gfc->VBR_seek_table.sum;
        gfc->VBR_seek_table.seen = 0;
    }
    if (gfc->VBR_seek_table.pos == gfc->VBR_seek_table.size) {
        for (i = 1; i < gfc->VBR_seek_table.size; i += 2)
            gfc->VBR_seek_table.bag[i/2] = gfc->VBR_seek_table.bag[i]; 

        gfc->VBR_seek_table.want *= 2;
        gfc->VBR_seek_table.pos  /= 2;
    }
}


/****************************************************************************
 * InitVbrTag: Initializes the header, and write empty frame to stream
 * Paramters:
 *			fpStream: pointer to output file stream
 ****************************************************************************
*/
int InitVbrTag(lame_global_flags *gfp)
{
#define MAXFRAMESIZE 2880 /* the max framesize freeformat 640 32kHz */
    lame_internal_flags *gfc = gfp->internal_flags;
    int i, header_bitrate, tot;

    /* we shold not count the vbr tag itself, because it breaks some player */
    gfp->nVbrNumFrames=0;

    /*
     * Xing VBR pretends to be a 48kbs layer III frame.  (at 44.1kHz).
     * (at 48kHz they use 56kbs since 48kbs frame not big enough for
     * table of contents)
     * let's always embed Xing header inside a 64kbs layer III frame.
     * this gives us enough room for a LAME version string too.
     * size determined by sampling frequency (MPEG1)
     * 32kHz:    216 bytes@48kbs    288bytes@ 64kbs
     * 44.1kHz:  156 bytes          208bytes@64kbs     (+1 if padding = 1)
     * 48kHz:    144 bytes          192
     *
     * MPEG 2 values are the same since the framesize and samplerate
     * are each reduced by a factor of 2.
     */
    if (1==gfp->version) {
	header_bitrate = XING_BITRATE1;
    } else {
	if (gfp->out_samplerate < 16000)
	    header_bitrate = XING_BITRATE25;
	else
	    header_bitrate = XING_BITRATE2;
    }

    if (gfp->VBR == cbr)
	header_bitrate = gfp->mean_bitrate_kbps;

    gfp->TotalFrameSize
	= ((gfp->version+1)*72000*header_bitrate) / gfp->out_samplerate;

    tot = (gfc->l3_side.sideinfo_len+LAMEHEADERSIZE);
    if (!(tot <= gfp->TotalFrameSize && gfp->TotalFrameSize <= MAXFRAMESIZE)) {
	/* disable tag, it wont fit */
	gfp->bWriteVbrTag = 0;
	return 0;
    }

    /* write dummy VBR tag of all 0's into bitstream */
    for (i=0; i<gfp->TotalFrameSize; ++i)
	add_dummy_byte(gfc, 0);


    /*TOC shouldn't take into account the size of the VBR header itself, too*/
    gfc->VBR_seek_table.sum  = 0;
    gfc->VBR_seek_table.seen = 0;
    gfc->VBR_seek_table.want = 1;
    gfc->VBR_seek_table.pos  = 0;
    if (!gfc->VBR_seek_table.bag) {
	gfc->VBR_seek_table.bag  = malloc (400*sizeof(int));
	if (gfc->VBR_seek_table.bag) {
	    gfc->VBR_seek_table.size = 400;
	}
	else {
	    gfc->VBR_seek_table.size = 0;
	    ERRORF (gfc,"Error: can't allocate VbrFrames buffer\n");
	    return -1;
	}   
    }
    /* Success */
    return 0;
}



/* fast CRC-16 computation - uses table crc16_lookup 8*/
int CRC_update_lookup(int value, int crc)
{
    int tmp;
    tmp=crc^value;
    crc=(crc>>8)^crc16_lookup[tmp & 0xff];
    return crc;
}

void UpdateMusicCRC(uint16_t *crc,unsigned char *buffer, int size){
    int i;
    for (i=0; i<size; ++i) 
        *crc = CRC_update_lookup(buffer[i],*crc);
}




void ReportLameTagProgress(lame_global_flags *gfp,int nStart)
{
    if (!gfp->bWriteVbrTag)
	return;

    if (nStart)
	MSGF( gfp->internal_flags, "Writing LAME Tag...");
    else
	MSGF( gfp->internal_flags, "done\n");
}


/****************************************************************************
 * Jonathan Dee 2001/08/31
 *
 * PutLameVBR: Write LAME info: mini version + info on various switches used
 * Paramters:
 *				pbtStreamBuffer	: pointer to output buffer  
 *				id3v2size		: size of id3v2 tag in bytes
 *				crc				: computation of crc-16 of Lame Tag so far (starting at frame sync)
 *				
 ****************************************************************************
*/
static void CreateI4(unsigned char *buf, int nValue)
{
    /* big endian create */
    buf[0] = nValue >> 24;
    buf[1] = nValue >> 16;
    buf[2] = nValue >>  8;
    buf[3] = nValue;
}

static void CreateI2(unsigned char *buf, int nValue)
{
    /* big endian create */
    buf[0] = nValue >> 8;
    buf[1] = nValue;
}


static int
PutLameVBR(lame_global_flags *gfp, FILE *fpStream,
	   uint8_t *pbtStreamBuffer, uint32_t id3v2size,  uint16_t crc)
{
    lame_internal_flags *gfc = gfp->internal_flags;

    int nBytesWritten = 0;
    int nFilesize	  = 0;		/*size of fpStream. Will be equal to size after process finishes. */
    int i;

    int enc_delay=lame_get_encoder_delay(gfp);       /* encoder delay */
    int enc_padding=lame_get_encoder_padding(gfp);   /* encoder padding  */

    /*recall:	gfp->VBR_q is for example set by the switch -V */
    /*			gfp->quality by -q, -h, -f, etc */
    int nQuality		= (100 - 10 * gfp->VBR_q - gfp->quality);

    const char *szVersion	= get_lame_very_short_version();
    uint8_t nVBR;
    uint8_t nRevision = 0x00;
    uint8_t nRevMethod;
    uint8_t vbr_type_translator[] = {1,5,3};		/*numbering different in vbr_mode vs. Lame tag */

    uint8_t nLowpass		= ( ((gfp->lowpassfreq / 100.0)+.5) > 255 ? 255 : (gfp->lowpassfreq / 100.0)+.5);

    ieee754_float32_t fPeakSignalAmplitude	= 0;				/*TODO... */
    uint16_t nRadioReplayGain		= 0;				/*TODO... */
    uint16_t nAudioPhileReplayGain  = 0;				/*TODO... */

    uint8_t nNoiseShaping	= gfp->internal_flags->noise_shaping_amp >= 0;
    uint8_t nStereoMode				= 0;
    int		bNonOptimal				= 0;
    uint8_t nSourceFreq				= 0;
    uint8_t nMisc					= 0;
    uint32_t nMusicLength			= 0;
    int		bId3v1Present
	= ((gfp->internal_flags->tag_spec.flags & CHANGED_FLAG)
	   && !(gfp->internal_flags->tag_spec.flags & V2_ONLY_FLAG));
    uint16_t nMusicCRC				= 0;

    /*psy model type:*/
    unsigned char bExpNPsyTune = 1;
    unsigned char bSafeJoint   = 0;

    unsigned char bNoGapMore		= 0;
    unsigned char bNoGapPrevious	= 0;

    int		 nNoGapCount	= gfp->internal_flags->nogap_total;
    int		 nNoGapCurr		= gfp->internal_flags->nogap_current;

    uint8_t  nFlags			= 0;

    /* if ABR, {store bitrate <=255} else { store "-b"} */
    int nABRBitrate	= gfp->mean_bitrate_kbps;

    /*revision and vbr method */
    nVBR = 0x00;		/*unknown. */
    if (gfp->VBR < sizeof(vbr_type_translator))
	nVBR = vbr_type_translator[gfp->VBR];
    nRevMethod = 0x10 * nRevision + nVBR; 

    /*nogap */
    if (nNoGapCount != -1) {
	if (nNoGapCurr > 0)
	    bNoGapPrevious = 1;

	if (nNoGapCurr < nNoGapCount-1)
	    bNoGapMore = 1;
    }

    /*flags */
    nFlags = (((int)gfp->ATHcurve) & 15) /*nAthType*/
	+ (bExpNPsyTune		<< 4)
	+ (bSafeJoint		<< 5)
	+ (bNoGapMore		<< 6)
	+ (bNoGapPrevious	<< 7);

    if (nQuality < 0)
	nQuality = 0;

    /*stereo mode field... a bit ugly.*/
    switch(gfp->mode)
    {
    case MONO:
	nStereoMode = 0;
	break;
    case STEREO:
	nStereoMode = 1;
	break;
    case DUAL_CHANNEL:
	nStereoMode = 2;
	break;
    case JOINT_STEREO:
	if (gfp->force_ms)
	    nStereoMode = 4;
	else
	    nStereoMode = 3;
	break;
    case NOT_SET:
	/* FALLTHROUGH */
    default:
	nStereoMode = 7;
	break;
    }
    if (gfp->use_istereo)
	nStereoMode = 6;

    if (gfp->in_samplerate <= 32000)
	nSourceFreq = 0x00;
    else if (gfp->in_samplerate ==48000)
	nSourceFreq = 0x02;
    else if (gfp->in_samplerate > 48000)
	nSourceFreq = 0x03;
    else
	nSourceFreq = 0x01;  /*default is 44100Hz. */

    /*Check if the user overrided the default LAME behaviour with some nasty options */

    if ((gfp->lowpassfreq == -1 && gfp->highpassfreq == -1)
	|| (gfp->scale_left != gfp->scale_right)
	|| gfp->disable_reservoir
	|| gfp->noATH
	|| gfp->ATHonly
	|| gfp->in_samplerate <= 32000)
	bNonOptimal = 1;

    nMisc
	=	nNoiseShaping
	+	(nStereoMode << 2)
	+	(bNonOptimal << 5)
	+	(nSourceFreq << 6);

    /*get filesize */
    fseek(fpStream, 0, SEEK_END);
    nFilesize = ftell(fpStream);

    nMusicLength = nFilesize - id3v2size;		/*omit current frame */
    if (bId3v1Present)
	nMusicLength-=128;                     /*id3v1 present. */
    nMusicCRC = gfc->nMusicCRC;

    /*Write all this information into the stream*/
    CreateI4(&pbtStreamBuffer[nBytesWritten], nQuality);
    nBytesWritten+=4;

    strncpy(&pbtStreamBuffer[nBytesWritten], szVersion, 9);
    nBytesWritten+=9;

    pbtStreamBuffer[nBytesWritten++] = nRevMethod ;
    pbtStreamBuffer[nBytesWritten++] = nLowpass;
    memmove(&pbtStreamBuffer[nBytesWritten], &fPeakSignalAmplitude, 4);
    nBytesWritten+=4;

    CreateI2(&pbtStreamBuffer[nBytesWritten],nRadioReplayGain);
    nBytesWritten+=2;

    CreateI2(&pbtStreamBuffer[nBytesWritten],nAudioPhileReplayGain);
    nBytesWritten+=2;

    pbtStreamBuffer[nBytesWritten++] = nFlags;

    if (nABRBitrate >= 255)
	pbtStreamBuffer[nBytesWritten++] = 0xFF;
    else
	pbtStreamBuffer[nBytesWritten++] = nABRBitrate;

    pbtStreamBuffer[nBytesWritten++] = enc_delay >> 4;
    pbtStreamBuffer[nBytesWritten++] = (enc_delay << 4) + (enc_padding >> 8);
    pbtStreamBuffer[nBytesWritten++] = enc_padding;

    pbtStreamBuffer[nBytesWritten++] = nMisc;
    pbtStreamBuffer[nBytesWritten++] = 0;	/*unused in rev1 */

    /* preset value for LAME4 is always 0 */
    CreateI2(&pbtStreamBuffer[nBytesWritten], 0);
    nBytesWritten+=2;

    CreateI4(&pbtStreamBuffer[nBytesWritten], nMusicLength);
    nBytesWritten+=4;

    CreateI2(&pbtStreamBuffer[nBytesWritten], nMusicCRC);
    nBytesWritten+=2;

    /*Calculate tag CRC.... must be done here, since it includes
     *previous information*/
    for (i = 0;i<nBytesWritten;i++)
	crc = CRC_update_lookup(pbtStreamBuffer[i], crc);

    CreateI2(&pbtStreamBuffer[nBytesWritten], crc);
    nBytesWritten+=2;

    return nBytesWritten;
}

/***********************************************************************
 * 
 * PutVbrTag: Write final VBR tag to the file
 * Paramters:
 *				lpszFileName: filename of MP3 bit stream
 ****************************************************************************
*/
int PutVbrTag(lame_global_flags *gfp, FILE *fpStream)
{
    lame_internal_flags * gfc = gfp->internal_flags;
    long lFileSize;
    char abyte;
    uint8_t pbtStreamBuffer[MAXFRAMESIZE] = {0}, *p;
    int i;
    uint16_t crc = 0x00;
    unsigned char id3v2Header[10];
    size_t id3v2TagSize;
    int bitrate;

    if (gfc->VBR_seek_table.pos <= 0 && gfp->VBR != cbr)
	return -1;

    /* Get file size */
    fseek(fpStream,0,SEEK_END);
    if ((lFileSize=ftell(fpStream)) == 0)
	return -1;

    /*
     * The VBR tag may NOT be located at the beginning of the stream.
     * If an ID3 version 2 tag was added, then it must be skipped to write
     * the VBR tag data.
     */

    /* seek to the beginning of the stream */
    fseek(fpStream, 0, SEEK_SET);

    /* read 10 bytes in case there's an ID3 version 2 header here */
    fread(id3v2Header, 1, sizeof id3v2Header, fpStream);

    /* does the stream begin with the ID3 version 2 file identifier? */
    id3v2TagSize=0;
    if (!strncmp(id3v2Header, "ID3", 3)) {
	/* the tag size (minus the 10-byte header) is encoded into four
	 * bytes where the most significant bit is clear in each byte */
	id3v2TagSize=(((id3v2Header[6] & 0x7f)<<21)
		      | ((id3v2Header[7] & 0x7f)<<14)
		      | ((id3v2Header[8] & 0x7f)<<7)
		      | (id3v2Header[9] & 0x7f))
	    + sizeof id3v2Header;
    }

    /* Seek to first real frame */
    fseek(fpStream, id3v2TagSize+gfp->TotalFrameSize, SEEK_SET);

    /* Read the header (first valid frame) */
    fread(pbtStreamBuffer,4,1,fpStream);

    /* the default VBR header. 48 kbps layer III, no padding, no crc */
    /* but sampling freq, mode andy copyright/copy protection taken */
    /* from first valid frame */
    bitrate = XING_BITRATE1;
    if (gfp->version != 1) {
	bitrate = XING_BITRATE2;
	if (gfp->out_samplerate < 16000)
	    bitrate = XING_BITRATE25;
    }
    if (gfp->VBR == cbr)
	bitrate = gfp->mean_bitrate_kbps;

    /* Use as much of the info from the real frames in the
     * Xing header:  samplerate, channels, crc, etc...
     */
    pbtStreamBuffer[0] = 0xff;
    abyte = (pbtStreamBuffer[1] & 0xf1) | 2;
    if (gfp->version == 1)
	abyte |= 8;
    pbtStreamBuffer[1] = abyte;

    abyte = 0;
    if (!gfp->free_format)
	abyte = 16*BitrateIndex(bitrate, gfp->version);
    pbtStreamBuffer[2] = abyte | (pbtStreamBuffer[2] & 0x0d);

    /* note! Xing header specifies that Xing data goes in the
     * ancillary data with NO ERROR PROTECTION.  If error protecton
     * in enabled, the Xing data still starts at the same offset,
     * and now it is in sideinfo data block, and thus will not
     * decode correctly by non-Xing tag aware players */
    p = &pbtStreamBuffer[gfc->l3_side.sideinfo_len];
    if (gfp->error_protection)
	p -= 2;

    /* Put Vbr tag */
    if (gfp->VBR == cbr)
	memcpy(p, VBRTag2, 4);
    else
	memcpy(p, VBRTag, 4);
    p += 4;

    CreateI4(p, FRAMES_FLAG+BYTES_FLAG+TOC_FLAG+VBR_SCALE_FLAG); p += 4;
    CreateI4(p, gfp->nVbrNumFrames); p += 4;
    CreateI4(p, lFileSize); p += 4;

    /* Put TOC */
    if (gfp->VBR == cbr) {
	for (i = 0; i < NUMTOCENTRIES; ++i)
	    *p++ = 255*i/100;
    } else {
	Xing_seek_table(&gfc->VBR_seek_table, p);
	p += NUMTOCENTRIES;
    }

    /* error_protection: add crc16 information to header !? */
    if (gfp->error_protection)
	CRC_writeheader(pbtStreamBuffer, gfc->l3_side.sideinfo_len);

    /* work out CRC so far: initially crc = 0 */
    for (i = 0; i < p - pbtStreamBuffer; i++)
	crc = CRC_update_lookup(pbtStreamBuffer[i], crc);

    /* Put LAME VBR info */
    PutLameVBR(gfp, fpStream, p, id3v2TagSize, crc);

    fseek(fpStream, id3v2TagSize, SEEK_SET);
    if (fwrite(pbtStreamBuffer, gfp->TotalFrameSize, 1, fpStream) != 1)
	return -1;
    return 0;
}