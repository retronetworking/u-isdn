/*
   Code to extract Touch-Tones from PCM Sample Data
   sample data is assumed to be signed 8 bit, sampled at 8000 HZ
   written by Johannes Deisenhofer 14.6.95
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#undef DEBUG

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Our timebase, sampling frequency for ISDN */
#define BUFFREQU 8000

/* We check samples of this length */
/* min 128,  max. 512  */
#define CHUNKLEN 512  

/* Soviele aufeinanderfolgende DTMF-Toene muessen uebereinstimmen, mind. 1 */
#define PROOF 1

/* Mindestpegel fuer Erkennung eines Touchtones */
#define MINPEGEL 20000 

/* Minimaler Faktor, um den der Pegel eines Tones hoeher als der naechste */
/* sein muss */
#define MINDIFF  30  

#define DATA_IS_SIGNED


#ifdef DATA_IS_SIGNED
#define DATAFORM signed
#define SUB 0x0
#else
#define DATAFORM unsigned
#define SUB 0x80
#endif


/* Touch-Tone Frequencies in Hz  */
/* from CCC Hackerbibel #1       */

int frequencies[] =
    {
          1209, 1336, 1477 ,        /* Rows */
  697 , /*  1     2     3  */
  770 , /*  4     5     6  */
  852 , /*  7     8     9  */
  941   /*  *     0     #  */
     };

/* Key mapping */
static char taste[] =
  {   '1' , '2' , '3' ,
      '4' , '5' , '6' ,
      '7' , '8' , '9' ,
      '*' , '0' , '#' };


/* sin/cos table, calculated for max.chunksize 512 */
extern signed char dtmfpre[7][512*2];

#ifdef PRECALC
/* initialize sine-table */
int fillpre(void)
{
  int k,x;
  for(k=0;k<7;k++)
  {
     char* cmpr=dtmfpre[k];
     for(x=0;x<CHUNKLEN;x++)
     {
         double rad= (double)x*frequencies[k]/BUFFREQU*2*M_PI;
         /* Sinus und Cosinus sind aufeinanderfolgend abgespeichert */
         *(cmpr++) = 127*sin(rad);
         *(cmpr++) = 127*cos(rad);
     }
  }
}

/* generate sine-table for inclusion in c-code*/
int generatepre(void)
{
  int k,x;
  printf("{ ");
  for(k=0;k<7;k++)
  {
     char* cmpr=dtmfpre[k];
     printf(" {");
     for(x=0;x<CHUNKLEN;x++)
     {
         double rad= (double)x*frequencies[k]/BUFFREQU*2*M_PI;
         if (x%8==0) printf("\n ");
         /* Sinus und Cosinus sind aufeinanderfolgend abgespeichert */
         printf("%4d,",*(cmpr++) = 127*sin(rad));
         printf("%4d,",*(cmpr++) = 127*cos(rad));
     }
     printf("\n} , ");
  }
  printf(" }; ");
}
#endif

/* calculate fourier-coefficient (sort of) from buffer buf
/* at frequency with index number frequ_idx */
int fk(DATAFORM char* buf, int frequ_idx, int len )
{
   int  koeff=0,koeff2=0,k;
   char*  compare = dtmfpre[frequ_idx];

   for (k=0;k<len;k+=8)
   {

       register int it;
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
       it=(*buf++)-SUB;
       koeff +=it * (*compare++);
       koeff2+=it * (*compare++);
   }
   koeff/=len;
   koeff2/=len;
   /* Diese Wurzel muss noch raus! */
   return (koeff*koeff+koeff2*koeff2)/8;
}

struct st { int num; int val; };

void swap(struct st* s1, struct st *s2)
{
  struct st stx;
  if (s1->val<s2->val)
  {
    stx = *s2;
    *s2=*s1;
    *s1=stx;
  }
}


int checkchunk(char* buf, int len)
{
    int row,col,k,valid;
    struct st tbl[7];
    for (k= 0; k<7;k++)
    {
        tbl[k].num = k;
        tbl[k].val = fk(buf,k,len);
#ifdef DEBUG
	/*  If you want to play with the heuristics below, you probably */ 
	/*  need to know the koefficients. Uncomment this */
        fprintf(stderr,"%6d ",tbl[k].val);  
#endif
    }

    /* Hard-coded bubble-Sort */
    /* First the rows */
    swap(tbl,tbl+1);
    swap(tbl+1,tbl+2);
    swap(tbl,tbl+1);

    /* Then columns */
    swap(tbl+3,tbl+4);
    swap(tbl+4,tbl+5);
    swap(tbl+5,tbl+6);
    swap(tbl+3,tbl+4);
    swap(tbl+4,tbl+5);
    swap(tbl+3,tbl+4);

    /* Highest Coeff. is row or col */
    row=tbl[0].num;
    col=tbl[3].num-3;

    /* Diese (heuristische) Regel bestimmt, ob es sich um einen */
    /* DTMF-Ton handelt oder nicht. Abhaengig vom Pegel der Toene */
    /* Und der gewuenschten Erkennungssicherheit */
 
    valid =  (tbl[0].val>tbl[1].val*MINDIFF)
           && (tbl[3].val>tbl[4].val*MINDIFF)
           && (tbl[0].val>MINPEGEL)
           && (tbl[3].val>MINPEGEL);

    #ifdef DEBUG
    fprintf(stderr,"Taste: %c\n",valid?taste[row+col*3]:'?');
    #endif 

    if (!valid) return -1;
    else return row+col*3;
}


/* 
  Main-Routine: von stdin lesen und auf DTMF testen,
  Erkannte Tasten nach stderr
*/

char buffer[CHUNKLEN];

int main(int argc, char**argv)
{
   int val, l1val=-1,same=0;
   int inbuf;
   int buflen;
   int count=0;
   do {
     /* Fill input buffer */
      inbuf=0;
      do { 
        buflen=fread(buffer,1,sizeof(buffer)-inbuf,stdin);
        inbuf+=buflen;
      } while (!feof(stdin) && inbuf < sizeof(buffer) ); 
      if (inbuf>=CHUNKLEN  ) 
      {
         val=checkchunk(buffer,CHUNKLEN);
         if (val==l1val ) same++;
         else same=0;
         if (same==(PROOF-1) && l1val!=-1)
         {
            fprintf(stderr,"Taste %c\n",taste[l1val]);
         }
         l1val=val;
      }
  } while(!feof(stdin));
}


