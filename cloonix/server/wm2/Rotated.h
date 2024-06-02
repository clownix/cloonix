/* ************************************************************************ */


/* Header file for the `xvertext' routines.

   Copyright (c) 1992 Alan Richardson (mppa3@uk.ac.sussex.syma) */


/* ************************************************************************ */


#ifndef _XVERTEXT_INCLUDED_ 
#define _XVERTEXT_INCLUDED_


#define XV_VERSION      2.0
#define XV_COPYRIGHT    "xvertext routines Copyright (c) 1992 Alan Richardson"


/* ---------------------------------------------------------------------- */


/* *** The font structures *** */

struct BitmapStruct {
    int			 bit_w;
    int			 bit_h;

    Pixmap bm;
};

struct XRotCharStruct {
    int			 ascent;
    int			 descent;
    int			 lbearing;
    int			 rbearing;
    int			 width;

    BitmapStruct	 glyph;
};

struct XRotFontStruct {
    int			 dir;
    int			 height;
    int			 max_ascent;
    int			 max_descent;
    int			 max_char;
    int			 min_char;
    char 		*name;

    XFontStruct		*xfontstruct;

    XRotCharStruct	 per_char[95];
};


/* ---------------------------------------------------------------------- */


extern float XRotVersion(char *, int);
extern XRotFontStruct *XRotLoadFont(Display *, const char *, float);
extern void XRotUnloadFont(Display *, XRotFontStruct *);
extern int XRotTextWidth(XRotFontStruct *, char *, int);
extern void XRotDrawString(Display *, XRotFontStruct *, Drawable, GC,
			   int, int, char *, int);
extern void XRotDrawAlignedString(Display *, XRotFontStruct *, Drawable, GC,
				  int, int, char *, int);


/* ---------------------------------------------------------------------- */


#define TLEFT	 	 1
#define TCENTRE	 	 2
#define TRIGHT	 	 3
#define MLEFT	 	 4
#define MCENTRE	 	 5
#define MRIGHT	 	 6
#define BLEFT	 	 7
#define BCENTRE	 	 8
#define BRIGHT	 	 9


/* ---------------------------------------------------------------------- */


extern int		 xv_errno;

#define XV_NOFONT	 1  /* no such font on X server */
#define XV_NOMEM	 2  /* couldn't do malloc */
#define XV_NOXIMAGE	 3  /* couldn't create an XImage */


/* ---------------------------------------------------------------------- */
 

#else

extern int		 xv_errno;

#endif 
