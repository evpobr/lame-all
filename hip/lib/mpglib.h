#ifndef _MPGLIB_H_
#define _MPGLIB_H_

/*
#ifndef NOANALYSIS
extern plotting_data *mpg123_pinfo;
#endif
*/

struct buf {
    unsigned char *pnt;
    long size;
    long pos;
    struct buf *next;
    struct buf *prev;
};

struct framebuf {
	struct buf *buf;
	long pos;
	struct frame *next;
	struct frame *prev;
};

typedef struct mpstr_tag {
  struct buf *head,*tail; /* buffer linked list pointers, tail points to oldest buffer */
        int vbr_header;               /* 1 if valid Xing vbr header detected */
        int num_frames;               /* set if vbr header present */
        int enc_delay;                /* set if vbr header present */
        int enc_padding;              /* set if vbr header present */
  /* header_parsed, side_parsed and data_parsed must be all set 1
     before the full frame has been parsed */
  int header_parsed;            /* 1 = header of current frame has been parsed */
  int side_parsed;		/* 1 = header of sideinfo of current frame has been parsed */
        int data_parsed;  
        int free_format;             /* 1 = free format frame */
        int old_free_format;        /* 1 = last frame was free format */
	int bsize;
	int framesize;
  int ssize;                    /* number of bytes used for side information, including 2 bytes for CRC-16 if present */
	int dsize;
  int fsizeold;                 /* size of previous frame, -1 for first */
        int fsizeold_nopadding;
  struct frame fr;              /* holds the parameters decoded from the header */
  unsigned char bsspace[2][MAXFRAMESIZE+1024]; /* bit stream space used ???? */ /* MAXFRAMESIZE */
	real hybrid_block[2][2][SBLIMIT*SSLIMIT];
	int hybrid_blc[2];
	unsigned long header;
	int bsnum;
	real synth_buffs[2][2][0x110];
        int  synth_bo;
  int  sync_bitstream; /* 1 = bitstream is yet to be synchronized */
	
  int bitindex;
  unsigned char *wordpointer;
} MPSTR, *PMPSTR;


#if ( defined(_MSC_VER) || defined(__BORLANDC__) )
	typedef int BOOL; /* windef.h contains the same definition */
#else
	#define BOOL int
#endif

#define MP3_ERR -1
#define MP3_OK  0
#define MP3_NEED_MORE 1



#endif /* _MPGLIB_H_ */