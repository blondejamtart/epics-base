/*  $Id$
 *	Author: Marty Kraimer
 *	Date:	9/28/95
 *	Replacement for old bldCvtTable
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	28SEP95	mrk	Replace old bldCvtTable
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dbBase.h>
#include <ellLib.h>
#include <cvtTable.h>

#define MAX_LINE_SIZE 160
#define MAX_BREAKS 100
struct brkCreateInfo {
    float           engLow;	/* Lowest value desired: engineering units */
    float           engHigh;	/* Highest value desired: engineering units */
    float            rawLow;	/* Raw value for EngLow			 */
    float            rawHigh;	/* Raw value for EngHigh		 */
    float           accuracy;	/* accuracy desired in engineering units */
    float           tblEngFirst;/* First table value: engineering units */
    float           tblEngLast;	/* Last table value: engineering units	 */
    float           tblEngDelta;/* Change per table entry: eng units	 */
    long            nTable;	/* number of table entries 		 */
    /* (last-first)/delta + 1		 */
    float          *pTable;	/* addr of data table			 */
} brkCreateInfo;

brkInt brkint[MAX_BREAKS];

static int create_break(struct brkCreateInfo *pbci, brkInt *pabrkInt,
	int max_breaks, int *n_breaks);
static char inbuf[MAX_LINE_SIZE];
static int linenum=0;

typedef struct dataList{
	struct dataList *next;
	float		value;
}dataList;

static int getNumber(char **pbeg, float *value)
{
    int	 nchars=0;

    while(isspace(**pbeg) && **pbeg!= '\0') (*pbeg)++;
    if(**pbeg == '!' || **pbeg == '\0') return(-1);
    if(sscanf(*pbeg,"%f%n",value,&nchars)!=1) return(-1);
    *pbeg += nchars;
    return(0);
}
	
static void errExit(char *pmessage)
{
    fprintf(stderr,pmessage);
    fprintf(stderr,"\n");
    exit(-1);
}

int main(argc, argv)
    int             argc;
    char          **argv;
{
    char	*pbeg;
    char	*pend;
    float	value;
    char	*pname;
    dataList	*phead;
    dataList	*pdataList;
    dataList	*pnext;
    float	*pdata;
    long	ndata;
    int		nBreak,len,n;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    FILE	*inFile;
    char	*plastSlash;
    

    if(argc!=2) {
	fprintf(stderr,"usage: makeBpt file.data\n");
	exit(-1);
    }
    plastSlash = strrchr(argv[1],'/');
    plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
    outFilename = calloc(1,strlen(plastSlash)+2);
    strcpy(outFilename,plastSlash);
    pext = strstr(outFilename,".data");
    if(!pext) {
	fprintf(stderr,"Input file MUST have .data  extension\n");
	exit(-1);
    }
    strcpy(pext,".dbd");
    inFile = fopen(argv[1],"r");
    if(!inFile) {
	fprintf(stderr,"Error opening %s\n",argv[1]);
	exit(-1);
    }
    outFile = fopen(outFilename,"w");
    if(!outFile) {
	fprintf(stderr,"Error opening %s\n",outFilename);
	exit(-1);
    }
    while(fgets(inbuf,MAX_LINE_SIZE,inFile)) {
	linenum++;
	inbuf[strlen(inbuf)] = '\0'; /* remove newline*/
	pbeg = inbuf;
	while(isspace(*pbeg) && *pbeg!= '\0') pbeg++;
	if(*pbeg == '!' || *pbeg == '\0') continue;
	while(*pbeg!='"' && *pbeg!= '\0') pbeg++;
	if(*pbeg!='"' ) errExit("Illegal Header");
	pbeg++; pend = pbeg;
	while(*pend!='"' && *pend!= '\0') pend++;
	if(*pend!='"') errExit("Illegal Header");
	len = pend - pbeg;
	if(len<=1) errExit("Illegal Header");
	pname = calloc(len,sizeof(char));
	strncpy(pname,pbeg,len);
	pbeg = pend + 1;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.engLow = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.rawLow = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.engHigh = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.rawHigh = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.accuracy = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngFirst = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngLast = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngDelta = value;
	goto got_header;
    }
    errExit("Illegal Header");
got_header:
    phead = pnext = 0;
    ndata = 0;
    errno = 0;
    while(fgets(inbuf,MAX_LINE_SIZE,inFile)) {
	float	value;

	inbuf[strlen(inbuf)] = '\0'; /* remove newline*/
	pbeg = inbuf;
	while(!getNumber(&pbeg,&value)) {
	    ndata++;
	    pdataList = (dataList *)calloc(1,sizeof(dataList));
	    if(!phead) 
		phead = pdataList;
	    else
		pnext->next = pdataList;
	    pdataList->value = value;
	    pnext = pdataList;
	}
    }
    brkCreateInfo.nTable = ndata;
    pdata = (float *)calloc(brkCreateInfo.nTable,sizeof(float));
    pnext = phead;
    for(n=0; n<brkCreateInfo.nTable; n++) {
	pdata[n] = pnext->value;
	pdataList = pnext;
	pnext = pnext->next;
	free((void *)pdataList);
    }
    brkCreateInfo.pTable = pdata;
    if(create_break(&brkCreateInfo,&brkint[0],MAX_BREAKS,&nBreak)) 
	errExit("create_break failed\n");
    fprintf(outFile,"breaktable(%s) {\n",pname);
    for(n=0; n<nBreak; n++) {
	fprintf(outFile,"\t%f %f\n",brkint[n].raw,brkint[n].eng);
    }
    fprintf(outFile,"}\n");
    fclose(inFile);
    fclose(outFile);
    return(0);
}

static int create_break( struct brkCreateInfo *pbci, brkInt *pabrkInt,
    int max_breaks, int *n_breaks)
{
    brkInt  *pbrkInt;
    float          *table = pbci->pTable;
    long            ntable = pbci->nTable;
    double          ilow,
                    ihigh,
                    tbllow,
                    tblhigh,
                    slope,
                    offset;
    int             ibeg,
                    iend,
                    i,
                    inc,
                    imax,
                    n;
    double          rawBeg,
                    engBeg,
                    rawEnd,
                    engEnd,
                    engCalc,
                    engActual,
                    error;
    int             valid,
                    all_ok,
                    expanding;
    /* make checks to ensure that brkCreateInfo makes sense */
    if (pbci->engLow >= pbci->engHigh) {
	errExit("create_break: engLow >= engHigh");
	return (-1);
    }
    if ((pbci->engLow < pbci->tblEngFirst)
	    || (pbci->engHigh > pbci->tblEngLast)) {
	errExit("create_break: engLow > engHigh");
	return (-1);
    }
    if (pbci->tblEngDelta <= 0.0) {
	errExit("create_break: tblEngDelta <= 0.0");
	return (-1);
    }
    if (ntable < 3) {
	errExit("raw data must have at least 3 elements");
	return (-1);
    }
/***************************************************************************
			 Convert Table to raw values
 *
 * raw and table values are assumed to be related by an equation of the form:
 *
 * raw = slope*table + offset
 *
 * The following algorithm converts each table value to raw units
 *
 * 1) Finds the locations in Table corresponding to engLow and engHigh
 *    Note that these locations need not be exact integers
 * 2) Interpolates to obtain table values corresponding to engLow and enghigh
 *    we now have the equations:
 *    rawLow = slope*tblLow + offset
 *    rawHigh = slope*tblHigh + offset
 * 4) Solving these equations for slope and offset gives:
 *    slope=(rawHigh-rawLow)/(tblHigh-tblLow)
 *    offset=rawHigh-slope*tblHigh
 * 5) for each table value set table[i]=table[i]*slope+offset
 *************************************************************************/
    /* Find engLow in Table  and then compute tblLow */
    ilow = (pbci->engLow - pbci->tblEngFirst) / (pbci->tblEngDelta);
    i = (int) ilow;
    if (i >= ntable - 1)
	i = ntable - 2;
    tbllow = table[i] + (table[i + 1] - table[i]) * (ilow - (float) i);
    /* Find engHigh in Table and then compute tblHigh */
    ihigh = (pbci->engHigh - pbci->tblEngFirst) / (pbci->tblEngDelta);
    i = (int) ihigh;
    if (i >= ntable - 1)
	i = ntable - 2;
    tblhigh = table[i] + (table[i + 1] - table[i]) * (ihigh - (float) i);
    /* compute slope and offset */
    slope = (pbci->rawHigh - pbci->rawLow) / (tblhigh - tbllow);
    offset = pbci->rawHigh - slope * tblhigh;
    /* convert table to raw units */
    for (i = 0; i < ntable; i++)
	table[i] = table[i] * slope + offset;

/*****************************************************************************
 *		Now create break point table
 *
 * The algorithm does the following:
 *
 * It finds one breakpoint interval at a time. For each it does the following:
 *
 * 1) Use a relatively large portion of the remaining table as an interval
 * 2) It attempts to use the entire interval as a breakpoint interval
 *	Success is determined by the following algorithm:
 *	  a) compute the slope using the entire interval
 *        b) for each table entry in the interval determine the eng value
 *	     using the slope just determined.
 *	  c) compare the computed value with eng value associated with table
 *        d) if all table entries are within the accuracy desired then success.
 * 3) If successful then attempt to expand the interval and try again.
 *    Note that it is expanded by up to 1/10 of the table size.
 * 4) If not successful reduce the interval by 1 and try again.
 *    Once the interval is being decreased it will never be increased again.
 * 5) The algorithm will ultimately fail or will have determined the optimum
 *    breakpoint interval
 *************************************************************************/

    /* Must start with table entry corresponding to engLow; */
    i = ilow;
    if (i >= ntable - 1)
	i = ntable - 2;
    rawBeg = table[i] + (table[i + 1] - table[i]) * (ilow - (float) i);
    engBeg = pbci->engLow;
    ibeg = (int) (ilow);       /* Make sure that ibeg > ilow */
    if( ibeg < ilow ) ibeg = ibeg + 1;
    /* start first breakpoint interval */
    n = 1;
    pbrkInt = pabrkInt;
    pbrkInt->raw = rawBeg;
    pbrkInt->eng = engBeg;
    /* determine next breakpoint interval */
    while ((engBeg <= pbci->engHigh) && (ibeg < ntable - 1)) {
	/* determine next interval to try. Up to 1/10 full range */
	iend = ibeg;
	inc = (int) ((ihigh - ilow) / 10.0);
	if (inc < 1)
	    inc = 1;
	valid = TRUE;
	/* keep trying intervals until cant do better */
	expanding = TRUE;	/* originally we are trying larger and larger
				 * intervals */
	while (valid) {
	    imax = iend + inc;
	    if (imax >= ntable) {
		/* don't go past end of table */
		imax = ntable - 1;
		inc = ntable - iend - 1;
		expanding = FALSE;
	    }
	    if (imax > (int) (ihigh + 1.0)) {	/* Don't go to far past
						 * engHigh */
		imax = (int) (ihigh + 1.0);
		inc = (int) (ihigh + 1.0) - iend;
		expanding = FALSE;
	    }
	    if (imax <= ibeg)
		break;		/* failure */
	    rawEnd = table[imax];
	    engEnd = pbci->tblEngFirst + (float) imax *(pbci->tblEngDelta);
	    slope = (engEnd - engBeg) / (rawEnd - rawBeg);
	    all_ok = TRUE;
	    for (i = ibeg + 1; i <= imax; i++) {
		engCalc = engBeg + slope * (table[i] - rawBeg);
		engActual = pbci->tblEngFirst + ((float) i) * (pbci->tblEngDelta);
		error = engCalc - engActual;
		if (error < 0.0)
		    error = -error;
		if (error >= pbci->accuracy) {
		    /* we will be trying smaller intervals */
		    expanding = FALSE;
		    /* just decrease inc and let while(valid) try again */
		    inc--;
		    all_ok = FALSE;
		    break;
		}
	    }			/* end for */
	    if (all_ok) {
		iend = imax;
		/* if not expanding we found interval */
		if (!expanding)
		    break;
		/* will automatically try larger interval */
	    }
	}			/* end while(valid) */
	/* either we failed or optimal interval has been found */
	if ((iend <= ibeg) && (iend < (int) ihigh)) {
	    errExit("Could not meet accuracy criteria");
	    return (-1);
	}
	pbrkInt->slope = slope;
	/* get ready for next breakpoint interval */
	if (n++ >= max_breaks) {
	    errExit("Break point table too large");
	    return (-1);
	}
	ibeg = iend;
	pbrkInt++;
	rawBeg = rawEnd;
	engBeg = engEnd;
	pbrkInt->raw = rawBeg;
	pbrkInt->eng = engBeg + (pbrkInt->raw - rawBeg) * slope;
    }
    pbrkInt->slope = 0.0;
    *n_breaks = n;
    return (0);
}
