#include "primitives.h"
#include <stdio.h>

#define MALLOCPTRTYPE void
#define DEBUGGING
#define safemalloc
#define debug (0xffff &~128 )

#ifdef DEBUGGING
#define RCHECK
#endif

#ifdef malloc
#undef malloc
#endif
#ifdef realloc
#undef realloc
#endif
#ifdef free
#undef free
#endif
/*
 * malloc.c (Caltech) 2/21/82 Chris Kingsley, kingsley@cit-20.
 * 
 * This is a very fast storage allocator.  It allocates blocks of a small number
 * of different sizes, and keeps free lists of each size.  Blocks that don't
 * exactly fit are passed up to the next larger size.  In this implementation,
 * the available sizes are 2^n-4 (or 2^n-12) bytes long. This is designed for
 * use in a program that uses vast quantities of memory, but bombs when it runs
 * out.
 */

/*
 * The overhead on a block is at least 4 bytes.  When free, this space contains
 * a pointer to the next free block, and the bottom two bits must be zero. When
 * in use, the first byte is set to MAGIC, and the second byte is the size
 * index.  The remaining bytes are for alignment. If range checking is enabled
 * and the size of the block fits in two bytes, then the top two bytes hold the
 * size of the requested block plus the range checking words, and the header
 * word MINUS ONE.
 */
union overhead {
#if ALIGNBYTES > 4
	double strut;				  /* alignment problems */
#endif
	struct {
	union overhead *ovu_next;	  /* when free */
		uchar_t ovu_magic;		  /* magic number */
		uchar_t ovu_index;		  /* bucket # */
#ifdef RCHECK
		ushort_t ovu_size;		  /* actual block size */
		ulong_t ovu_rmagic;		  /* range magic number */
#endif
	} ovu;
#define	ov_next	ovu.ovu_next
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_size		ovu.ovu_size
#define	ov_rmagic	ovu.ovu_rmagic
};

static short findbucket (union overhead *, int );
static void morecore (int);

#define	MAGIC		0xff		  /* magic # on accounting info */
#define OLDMAGIC	0x7f		  /* same after a free() */
#define RMAGIC		0x55555555	  /* magic # on range info */
#ifdef RCHECK
#define	RSLOP		sizeof (ulong_t)
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information precedes
 * the data area returned to the user.
 */
#define	NBUCKETS 30
static union overhead *nextf[NBUCKETS];

#ifdef MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees for a
 * given block size.
 */
static ulong_t nmalloc[NBUCKETS];

#endif

#ifdef debug
#define	ASSERT(p)   do { if (!(p)) botch("p"); } while(0)
static void
botch (char *s)
{

	fprintf (stderr,"assertion botched: %s\n", s);
	abort ();
}

#else
#define	ASSERT(p)
#endif

#ifdef safemalloc
static int an = 0;
#endif

MALLOCPTRTYPE *
malloc (size_t nbytes)
{
	register union overhead *p;
	register int bucket = 0;
	register unsigned shiftr;

#ifdef safemalloc
#ifdef DEBUGGING
	int size = nbytes;

#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
               fprintf (stderr, "Allocation too large: %lx\n",
                       (unsigned long)nbytes);
		exit (1);
	}
#endif							/* MSDOS */
#ifdef DEBUGGING
	if ((long) nbytes < 0 || (long) nbytes > 65536)
		botch ("panic: malloc");
#endif
#endif							/* safemalloc */

	/*
	 * Convert amount of memory requested into closest block size stored in
	 * hash buckets which satisfies request.  Account for space used per block
	 * for accounting.
	 */
	nbytes += sizeof (union overhead) + RSLOP;

	nbytes = (nbytes + 3) & ~3;
	shiftr = (nbytes - 1) >> 2;
	/* apart from this loop, this is O(1) */
	while (shiftr >>= 1)
		bucket++;
	/*
	 * If nothing in hash bucket right now, request more memory from the
	 * system.
	 */
	if (nextf[bucket] == NULL)
		morecore (bucket);
	if ((p = (union overhead *) nextf[bucket]) == NULL) {
#ifdef safemalloc
		fputs ("Out of memory!\n", stderr);
		exit (1);
#else
		return (NULL);
#endif
	}
#ifdef safemalloc
#ifdef DEBUGGING
#ifndef I286
	if (debug & 128)
               fprintf (stderr, "0x%x: mall %03d -> %x\n",
                       (unsigned int)p + 1, size, (unsigned int)p->ov_next);
#else
	if (debug & 128)
               fprintf (stderr, "0x%lx: (%05d) malloc %d bytes\n",
                       (unsigned long)p + 1, an++, size);
#endif
#endif
#endif							/* safemalloc */

	/* remove from linked list */
#ifdef RCHECK
	if (*((int *) p) & (sizeof (union overhead) - 1)) {
#ifndef I286
                fprintf (stderr, "Corrupt malloc ptr 0x%x at 0x%x\n",
                       *((int *) p), (unsigned int)p);

#else
                fprintf (stderr, "Corrupt malloc ptr 0x%lx at 0x%lx\n",
                       *((int *) p), (unsigned long)p);

#endif
		botch("Corrupt");
	}
#endif
	nextf[bucket] = p->ov_next;
	p->ov_magic = MAGIC;
	p->ov_index = bucket;
#ifdef MSTATS
	nmalloc[bucket]++;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and bound space with magic numbers.
	 */
	if (nbytes <= 0x10000)
		p->ov_size = size;
	else
		p->ov_size = 0;
	p->ov_rmagic = RMAGIC;
	*(ulong_t *) ((caddr_t) p + size + sizeof (union overhead)) = RMAGIC;

#endif
	return ((MALLOCPTRTYPE *) (p + 1));
}

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore (int bucket)
{
	register union overhead *op;
	register int rnu;			  /* 2^rnu bytes will be requested */
	register int nblks;			  /* become nblks blocks of the desired size */
	register int siz;

	if (nextf[bucket])
		return;
	/*
	 * Insure memory is allocated on a page boundary.  Should make getpageize
	 * call?
	 */
	op = (union overhead *) sbrk (0);
#ifndef I286
	if ((int) op & 0x3ff)
		(void) sbrk (1024 - ((int) op & 0x3ff));
#else
	/* The sbrk(0) call on the I286 always returns the next segment */
#endif

#ifndef I286
	/* take 2k unless the block is bigger than that */
	rnu = (bucket <= 8) ? 11 : bucket + 3;
#else
	/*
	 * take 16k unless the block is bigger than that (80286s like large
	 * segments!)
	 */
	rnu = (bucket <= 11) ? 14 : bucket + 3;
#endif
	nblks = 1 << (rnu - (bucket + 3));	/* how many blocks to get */
	if (rnu < bucket)
		rnu = bucket;
	op = (union overhead *) sbrk (1 << rnu);
	/* no more room! */
	if ((int) op == -1)
		return;
	/*
	 * Round up to minimum allocation size boundary and deduct from block count
	 * to reflect.
	 */
#ifndef I286
	if ((int) op & 7) {
		op = (union overhead *) (((int) op + 8) & ~7);
		nblks--;
	}
#else
	/* Again, this should always be ok on an 80286 */
#endif
	/*
	 * Add new memory allocated to that on free list for this hash bucket.
	 */
	nextf[bucket] = op;
	siz = 1 << (bucket + 3);
	while (--nblks > 0) {
		op->ov_magic = OLDMAGIC;
		op->ov_rmagic = RMAGIC;
		op->ov_index = bucket;
		op->ov_next = (union overhead *) ((caddr_t) op + siz);
		op = (union overhead *) ((caddr_t) op + siz);
	}
	op->ov_magic = OLDMAGIC;
	op->ov_rmagic = RMAGIC;
	op->ov_index = bucket;
}


void
chkfree (MALLOCPTRTYPE *cp)
{
       /* register int size; */
	register union overhead *op;

	ASSERT (cp != NULL);

	op = (union overhead *) ((caddr_t) cp - sizeof (union overhead));

#ifdef safemalloc
#ifdef DEBUGGING
#ifndef I286
	if (debug & 128)
               fprintf (stderr, "0x%x: free %03d -> %x\n",
                       (unsigned int)cp, op->ov_size,
                       (unsigned int)nextf[op->ov_index]);
#else
	if (debug & 128)
               fprintf (stderr, "0x%lx: (%05d) free\n",
                       (unsigned long)cp, an++);
#endif
#endif
#endif							/* safemalloc */

	if (cp == NULL)
		return;
#ifdef debug
	ASSERT (op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC) {
		warn ("%s free() ignored",
				op->ov_magic == OLDMAGIC ? "Duplicate" : "Bad");
		return;
	}
#endif
	return;
}

void
free (MALLOCPTRTYPE *cp)
{
	register int size;
	register union overhead *op;

	if(cp == NULL) return;

	op = (union overhead *) ((caddr_t) cp - sizeof (union overhead));

#ifdef safemalloc
#ifdef DEBUGGING
#ifndef I286
	if (debug & 128)
               fprintf (stderr, "0x%x: free %03d -> %x\n",
               (unsigned int)cp, op->ov_size,
               (unsigned int)nextf[op->ov_index]);
#else
	if (debug & 128)
               fprintf (stderr, "0x%lx: (%05d) free\n",
               (unsigned long)cp, an++);
#endif
#endif
#endif							/* safemalloc */

	if (cp == NULL)
		return;
#ifdef debug
	ASSERT (op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC) {
		warn ("%s free() ignored",
				op->ov_magic == OLDMAGIC ? "Duplicate" : "Bad");
		return;
	}
#endif
	op->ov_magic = OLDMAGIC;
#ifdef RCHECK
	ASSERT (op->ov_rmagic == RMAGIC);
	if (op->ov_index <= 13)
		ASSERT (*(ulong_t *) ((caddr_t) cp + op->ov_size) == RMAGIC);
#endif
	ASSERT (op->ov_index < NBUCKETS);
#ifdef safemalloc
	{
		int x = op->ov_size;

		while (x-- > 0)
			*((char *) cp)++ = 0x33;
	}
#endif
	size = op->ov_index;
	op->ov_next = nextf[size];
	nextf[size] = op;
#ifdef MSTATS
	nmalloc[size]--;
#endif
}

/*
 * When a program attempts "storage compaction" as mentioned in the old malloc
 * man page, it realloc's an already freed block.  Usually this is the last
 * block it freed; occasionally it might be farther back.  We have to search
 * all the free lists for the block in order to determine its bucket: 1st we
 * make one pass thru the lists checking only the first block in each; if that
 * fails we search ``reall_srchlen'' blocks in each list for a match (the
 * variable is extern so the caller can modify it).  If that fails we just copy
 * however many bytes was given to realloc() and hope it's not huge.
 */
int reall_srchlen = 4;			  /* 4 should be plenty, -1 =>'s whole list */

MALLOCPTRTYPE *
realloc (MALLOCPTRTYPE *mp, size_t nbytes)
{
	register ulong_t onb;
	union overhead *op;
	char *res;
	register int i;
	int was_alloced = 0;
	char *cp = (char *) mp;

#ifdef safemalloc
#ifdef DEBUGGING
	int size = nbytes;

#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
               fprintf (stderr, "Reallocation too large: %lx\n",
                       (unsigned long)size);
		exit (1);
	}
#endif							/* MSDOS */
	if (!cp)
		return malloc(nbytes);
#ifdef DEBUGGING
	if ((long) nbytes < 0)
		botch ("panic: realloc");
#endif
#endif							/* safemalloc */

	if (cp == NULL)
		return (malloc (nbytes));
	op = (union overhead *) ((caddr_t) cp - sizeof (union overhead));

	if (op->ov_magic == MAGIC) {
		was_alloced++;
		i = op->ov_index;
	} else {
		/*
		 * Already free, doing "compaction".
		 * 
		 * Search for the old block of memory on the free list.  First, check the
		 * most common case (last element free'd), then (this failing) the last
		 * ``reall_srchlen'' items free'd. If all lookups fail, then assume the
		 * size of the memory block being realloc'd is the smallest possible.
		 */
		if ((i = findbucket (op, 1)) < 0 &&
				(i = findbucket (op, reall_srchlen)) < 0)
			i = 0;
	}
	onb = (1 << (i + 3)) - sizeof (*op) - RSLOP;
	/* avoid the copy if same size block */
	if (was_alloced &&
			nbytes <= onb && nbytes > (onb >> 1) - sizeof (*op) - RSLOP) {
#ifdef RCHECK
		/*
		 * Record new allocated size of block and bound space with magic
		 * numbers.
		 */
		if (op->ov_index <= 13) {
			/*
			 * Convert amount of memory requested into closest block size
			 * stored in hash buckets which satisfies request.  Account for
			 * space used per block for accounting.
			 */
			nbytes += sizeof (union overhead) + RSLOP;

			nbytes = (nbytes + 3) & ~3;
			op->ov_size = size;
			*((ulong_t *) ((caddr_t) cp + size)) = RMAGIC;
		}
#endif
		res = cp;
	} else {
		if ((res = (char *) malloc (nbytes)) == NULL)
			return (NULL);
		if (cp != res)			  /* common optimization */
			bcopy (cp, res, (int) (nbytes < onb ? nbytes : onb));
		if (was_alloced)
			free (cp);
	}

#ifdef safemalloc
#ifdef DEBUGGING
#ifndef I286
	if (debug & 128) {
               fprintf (stderr, "0x%x: (%05d) rfree\n",
                       (unsigned int)res, an++);
               fprintf (stderr, "0x%x: (%05d) realloc %d bytes\n",
                       (unsigned int)res, an++, size);
	}
#else
	if (debug & 128) {
               fprintf (stderr, "0x%lx: (%05d) rfree\n",
                       (unsigned long)res, an++);
               fprintf (stderr, "0x%lx: (%05d) realloc %d bytes\n",
                       (unsigned long)res, an++, size);
	}
#endif
#endif
#endif							/* safemalloc */
	return ((MALLOCPTRTYPE *) res);
}

/*
 * Search ``srchlen'' elements of each free list for a block whose header
 * starts at ``freep''.  If srchlen is -1 search the whole list. Return bucket
 * number, or -1 if not found.
 */
static short
findbucket (union overhead *freep, int srchlen)
{
	register union overhead *p;
	register int i, j;

	for (i = 0; i < NBUCKETS; i++) {
		j = 0;
		for (p = nextf[i]; p && j != srchlen; p = p->ov_next) {
			if (p == freep)
				return (i);
			j++;
		}
	}
	return (-1);
}

#ifdef MSTATS
/*
 * mstats - print out statistics about malloc
 * 
 * Prints two lines of numbers, one showing the length of the free list for each
 * size category, the second showing the number of mallocs - frees for each
 * size category.
 */
mstats (char *s)
{
	register int i, j;
	register union overhead *p;
	int totfree = 0, totused = 0;

	fprintf (stderr, "Memory allocation statistics %s\nfree:\t", s);
	for (i = 0; i < NBUCKETS; i++) {
		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++) ;
		fprintf (stderr, " %d", j);
		totfree += j * (1 << (i + 3));
	}
	fprintf (stderr, "\nused:\t");
	for (i = 0; i < NBUCKETS; i++) {
		fprintf (stderr, " %d", nmalloc[i]);
		totused += nmalloc[i] * (1 << (i + 3));
	}
	fprintf (stderr, "\n\tTotal in use: %d, total free: %d\n",
			totused, totfree);
}

#endif


#ifdef CONFIG_DEBUG_ISDN
void mallchk(void)
{
	union overhead *p;
	int bucket = 0;
       /* unsigned shiftr; */

	for(bucket=0; bucket < NBUCKETS; bucket++) {
		for(p = (union overhead *) nextf[bucket]; p != NULL; p = p->ov_next) {
			ASSERT (p->ov_magic == OLDMAGIC);
			ASSERT (p->ov_rmagic == RMAGIC);
			ASSERT (p->ov_index < NBUCKETS);
		}
	}
}
#endif
