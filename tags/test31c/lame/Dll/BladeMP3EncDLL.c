/*
 *	Blade DLL Interface for LAME.
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

#include "BladeMP3EncDLL.h"
#include <assert.h>
#include "version.h"  
#include "VbrTag.h"   
#include "lame.h"
/*
#include "get_audio.h"
#include "globalflags.h"
#include "machine.h"
#include "util.h"
*/

#ifdef _DEBUG
	#define _DEBUGDLL 1
#endif


const int MAJORVERSION=1;
const int MINORVERSION=05;
const int CURRENT_STRUCT_VERSION=1;
const int CURRENT_STRUCT_SIZE=sizeof(BE_CONFIG);





// Local variables
static int		nPsychoModel=2;
static BOOL		bFirstFrame=TRUE;
static DWORD	dwSampleBufferSize=0;


#ifdef _DEBUGDLL
void dump_config( char *inPath, char *outPath);
#endif

lame_global_flags *gfp;

static void InitParams()
{
    bFirstFrame=TRUE;
    gfp=lame_init();

}




#define MAX_ARGV 25

__declspec(dllexport) BE_ERR	beInitStream(PBE_CONFIG pbeConfig, PDWORD dwSamples, PDWORD dwBufferSize, PHBE_STREAM phbeStream)
{
	//char		strTmp[255];
	int			nDllArgC=0;
	//char		DllArgV[20][80];
	//char*		argv[MAX_ARGV];
//	int			i;
	BE_CONFIG	lameConfig;
	//	layer*		pInfo = NULL;


	// clear out structure
	memset(&lameConfig,0x00,CURRENT_STRUCT_SIZE);

	// Check if this is a regular BLADE_ENCODER header
	if (pbeConfig->dwConfig!=BE_CONFIG_LAME)
	{
		int	nCRC=pbeConfig->format.mp3.bCRC;
		int nVBR=(nCRC>>12)&0x0F;

		// Copy parameter from old Blade structure
		lameConfig.format.LHV1.dwSampleRate	=pbeConfig->format.mp3.dwSampleRate;
		//for low bitrates, LAME will automatically downsample for better
		//sound quality.  Forcing output samplerate = input samplerate is not a good idea 
		//unless the user specifically requests it:
		//lameConfig.format.LHV1.dwReSampleRate=pbeConfig->format.mp3.dwSampleRate;
		lameConfig.format.LHV1.nMode		=(pbeConfig->format.mp3.byMode&0x0F);
		lameConfig.format.LHV1.dwBitrate	=pbeConfig->format.mp3.wBitrate;
		lameConfig.format.LHV1.bPrivate		=pbeConfig->format.mp3.bPrivate;
		lameConfig.format.LHV1.bOriginal	=pbeConfig->format.mp3.bOriginal;
		lameConfig.format.LHV1.bCRC			=nCRC&0x01;
		lameConfig.format.LHV1.bCopyright	=pbeConfig->format.mp3.bCopyright;
	
		// Fill out the unknowns
		lameConfig.format.LHV1.dwStructSize=CURRENT_STRUCT_VERSION;
		lameConfig.format.LHV1.dwStructVersion=CURRENT_STRUCT_SIZE;

		// Get VBR setting from fourth nibble
		if (nVBR>0)
		{
			lameConfig.format.LHV1.bWriteVBRHeader=TRUE;
			lameConfig.format.LHV1.bEnableVBR=TRUE;
			lameConfig.format.LHV1.nVBRQuality=nVBR-1;
		}

		// Get Quality from third nibble
		lameConfig.format.LHV1.nQuality=(MPEG_QUALITY)((nCRC>>8)&0x0F);

	}
	else
	{
		// Copy the parameters
		memcpy(&lameConfig,pbeConfig,pbeConfig->format.LHV1.dwStructSize);
	}

	//for (i=0;i<MAX_ARGV;i++)
	//	argv[i]=DllArgV[i];

	// Clear the external and local paramters
	InitParams();

	// Clear argument array
	//memset(&DllArgV[0][0],0x00,sizeof(DllArgV));

	// Not used, always assign stream 1
	*phbeStream=1;

	// Set MP3 buffer size
	*dwBufferSize=BUFFER_SIZE*2;


	// --------------- Set arguments to LAME encoder -------------------------

	// Set zero argument, the filename
	//strcpy(DllArgV[nDllArgC++],"LameDLLEncoder");

  	switch (lameConfig.format.LHV1.nMode)
	{
		case BE_MP3_MODE_STEREO:
			gfp->mode=0;
			gfp->mode_fixed=1;  /* dont allow LAME to change the mode */
		break;
		case BE_MP3_MODE_JSTEREO:
			gfp->mode=1;
			gfp->mode_fixed=1;
		break;
		case BE_MP3_MODE_MONO:
			gfp->mode=3;
			gfp->mode_fixed=1;
		break;
		case BE_MP3_MODE_DUALCHANNEL:
			gfp->force_ms=1;
			gfp->mode=1;
			gfp->mode_fixed=1;
		break;
		default:
		{
			char lpszError[255];
			sprintf(lpszError,"Invalid lameConfig.format.LHV1.nMode, value is %d\n",lameConfig.format.LHV1.nMode);
			OutputDebugString(lpszError);
			return BE_ERR_INVALID_FORMAT_PARAMETERS;
		}
	}

	switch (lameConfig.format.LHV1.nQuality)
	{
		case NORMAL_QUALITY:	// Nothing special
			break;
		case LOW_QUALITY:		// -f flag
			gfp->quality=9;
			break;
		case HIGH_QUALITY:		// -h flag for high qualtiy
			gfp->quality=2;
        break;
		case VOICE_QUALITY:		// --voice flag for experimental voice mode
			gfp->lowpassfreq=12000;
			gfp->VBR_max_bitrate_kbps=160;
			gfp->no_short_blocks=1;
		break;
	}

	if (lameConfig.format.LHV1.bEnableVBR)
	{
		// 0=no vbr 1..10 is VBR quality setting -1
		gfp->VBR=1;
		gfp->VBR_q=lameConfig.format.LHV1.nVBRQuality;
	}

	// Set frequency
	gfp->in_samplerate=lameConfig.format.LHV1.dwSampleRate;

	// Set frequency resampling rate, if specified
	if (lameConfig.format.LHV1.dwReSampleRate>0)
		gfp->out_samplerate=lameConfig.format.LHV1.dwReSampleRate;
		
	
	// Set bitrate.  (CDex users always specify bitrate=Min bitrate when using VBR)
	gfp->brate=lameConfig.format.LHV1.dwBitrate;
	gfp->VBR_min_bitrate_kbps=gfp->brate;
			
	// Set Maxbitrate, if specified
	if (lameConfig.format.LHV1.dwMaxBitrate>0)
		gfp->VBR_max_bitrate_kbps=lameConfig.format.LHV1.dwMaxBitrate;
	
	// Set copyright flag?
    if (lameConfig.format.LHV1.bCopyright)
		gfp->copyright=1;

	// Do we have to tag  it as non original 
    if (!lameConfig.format.LHV1.bOriginal)
		gfp->original=0;

	// Add CRC?
    if (lameConfig.format.LHV1.bCRC)
		gfp->error_protection=1;

	lame_init_params();	

	// Set the encoder variables
	// lame_parse_args(nDllArgC,argv);
	gfp->silent=1;  /* disable status ouput */

	// Set private bit?
	if (lameConfig.format.LHV1.bPrivate)
	{
		gfp->extension = 0;
	}
	else
	{
		gfp->extension = 1;
	}
	

	//LAME encoding call will accept any number of samples.  Lets use 1152
	*dwSamples=1152*gfp->stereo;


	// Set the input sample buffer size, so we know what we can expect
	dwSampleBufferSize=*dwSamples;

#ifdef _DEBUGDLL
	dump_config(gfp->inPath,gfp->outPath);
#endif

	// Everything went OK, thus return SUCCESSFUL
	return BE_ERR_SUCCESSFUL;
}



__declspec(dllexport) BE_ERR	beDeinitStream(HBE_STREAM hbeStream, PBYTE pOutput, PDWORD pdwOutput)
{

        *pdwOutput =   lame_encode_finish(pOutput);

	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR	beCloseStream(HBE_STREAM hbeStream)
{
	// DeInit encoder
//	return DeInitEncoder();
	return BE_ERR_SUCCESSFUL;
}



__declspec(dllexport) VOID		beVersion(PBE_VERSION pbeVersion)
{
	// DLL Release date
	char lpszDate[20];
	char lpszTemp[5];


	// Set DLL interface version
	pbeVersion->byDLLMajorVersion=MAJORVERSION;
	pbeVersion->byDLLMinorVersion=MINORVERSION;

	// Set Engine version number (Same as Lame version)
	pbeVersion->byMajorVersion=LAME_MAJOR_VERSION;
	pbeVersion->byMinorVersion=LAME_MINOR_VERSION;

	// Get compilation date
	strcpy(lpszDate,__DATE__);

	// Get the first three character, which is the month
	strncpy(lpszTemp,lpszDate,3);

	// Set month
	if (strcmp(lpszTemp,"Jan")==0)	pbeVersion->byMonth=1;
	if (strcmp(lpszTemp,"Feb")==0)	pbeVersion->byMonth=2;
	if (strcmp(lpszTemp,"Mar")==0)	pbeVersion->byMonth=3;
	if (strcmp(lpszTemp,"Apr")==0)	pbeVersion->byMonth=4;
	if (strcmp(lpszTemp,"May")==0)	pbeVersion->byMonth=5;
	if (strcmp(lpszTemp,"Jun")==0)	pbeVersion->byMonth=6;
	if (strcmp(lpszTemp,"Jul")==0)	pbeVersion->byMonth=7;
	if (strcmp(lpszTemp,"Aug")==0)	pbeVersion->byMonth=8;
	if (strcmp(lpszTemp,"Sep")==0)	pbeVersion->byMonth=9;
	if (strcmp(lpszTemp,"Oct")==0)	pbeVersion->byMonth=10;
	if (strcmp(lpszTemp,"Nov")==0)	pbeVersion->byMonth=11;
	if (strcmp(lpszTemp,"Dec")==0)	pbeVersion->byMonth=12;

	// Get day of month string (char [4..5])
	pbeVersion->byDay=atoi(lpszDate+4);

	// Get year of compilation date (char [7..10])
	pbeVersion->wYear=atoi(lpszDate+7);

	memset(pbeVersion->zHomepage,0x00,BE_MAX_HOMEPAGE);

	strcpy(pbeVersion->zHomepage,"http://www.sulaco.org/mp3/");
}

__declspec(dllexport) BE_ERR	beEncodeChunk(HBE_STREAM hbeStream, DWORD nSamples, 
											  PSHORT pSamples, PBYTE pOutput, PDWORD pdwOutput)
{
	int iSampleIndex;
	int n=nSamples/gfp->stereo;
    PSHORT LBuffer,RBuffer;
	LBuffer=malloc(sizeof(short)*n);
	RBuffer=malloc(sizeof(short)*n);
	
		

	if (gfp->stereo==2)
	{
		for (iSampleIndex=0;iSampleIndex<n;iSampleIndex++)
		{
			// Copy new sample data into InputBuffer
			LBuffer[iSampleIndex]=*pSamples++;
			RBuffer[iSampleIndex]=*pSamples++;
		}
	}
	else
	{
		// Mono, only put it data into buffer[0] (=left channel)
		for (iSampleIndex=0;iSampleIndex<n;iSampleIndex++)
		{
			// Copy new sample data into InputBuffer
			LBuffer[iSampleIndex]=*pSamples++;
		}
	}


	// Encode it
	*pdwOutput=lame_encode_buffer(LBuffer,RBuffer,n,pOutput,1);


	free(LBuffer);
	free(RBuffer);
	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR beWriteVBRHeader(LPCSTR lpszFileName)
{
	if (gfp->bWriteVbrTag)
	{
		// Calculate relative quality of VBR stream 
		// 0=best, 100=worst
		int nQuality=gfp->VBR_q*100/9;

		// Write Xing header again
		return PutVbrTag((LPSTR)lpszFileName,nQuality);
	}
	return BE_ERR_INVALID_FORMAT_PARAMETERS;
}


BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
    switch( ul_reason_for_call )
	{
		case DLL_PROCESS_ATTACH:
#ifdef _DEBUGDLL
			OutputDebugString("Attach Process \n");
#endif
		break;
		case DLL_THREAD_ATTACH:
#ifdef _DEBUGDLL
			OutputDebugString("Attach Thread \n");
#endif
		break;
		case DLL_THREAD_DETACH:
#ifdef _DEBUGDLL
			OutputDebugString("Detach Thread \n");
#endif
		break;
		case DLL_PROCESS_DETACH:
#ifdef _DEBUGDLL
			OutputDebugString("Detach Process \n");
#endif
		break;
    }
    return TRUE;
}


#ifdef _DEBUGDLL
void dump_config( char *inPath, char *outPath)
{
  	char strTmp[255];

	OutputDebugString("Encoding configuration:\n");


	sprintf(strTmp,"Write VBR Header=%s\n",(gfp->bWriteVbrTag)?"Yes":"No");
	OutputDebugString(strTmp);

	sprintf(strTmp,"version=%d\n",gfp->version);
	OutputDebugString(strTmp);


	sprintf(strTmp,"Layer=3   mode=%d  \n",gfp->mode);
	OutputDebugString(strTmp);


	sprintf(strTmp,"samp frq=%.1f kHz   total bitrate=%d kbps\n",gfp->in_samplerate/1000.0);
	OutputDebugString(strTmp);

	sprintf(strTmp,"de-emph=%d   c/right=%d   orig=%d   errprot=%s\n",gfp->emphasis, gfp->copyright, gfp->original,((gfp->error_protection) ? "on" : "off"));
	OutputDebugString(strTmp);

//	sprintf(strTmp,"16 Khz cut off is %s\n",(0)?"enabled":"disabled");
//	OutputDebugString(strTmp);

	sprintf(strTmp,"Fast mode is %s\n",(gfp->quality==9)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"Force ms %s\n",(gfp->force_ms)?"enabled":"disabled");
	OutputDebugString(strTmp);

//	sprintf(strTmp,"GPsycho acoustic model is %s\n",(gpsycho)?"enabled":"disabled");
//	OutputDebugString(strTmp);

	sprintf(strTmp,"VRB is %s, VBR_q value is  %d\n",(gfp->VBR)?"enabled":"disabled",gfp->VBR_q);
	OutputDebugString(strTmp);

	sprintf(strTmp,"input file: '%s'   output file: '%s'\n", inPath, outPath);
	OutputDebugString(strTmp);

//	sprintf(strTmp,"Voice mode %s\n",(voice_mode)?"enabled":"disabled");
//	OutputDebugString(strTmp);

	sprintf(strTmp,"Encoding as %.1f kHz %d kbps %d MPEG-%d LayerIII file\n",gfp->out_samplerate/1000.0,gfp->brate,gfp->mode,3 - gfp->mode_gr);
	OutputDebugString(strTmp);
}


void DispErr(LPSTR strErr)
{
	MessageBox(NULL,strErr,"",MB_OK);
}

#endif