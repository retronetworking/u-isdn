/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"


#ifdef DO_DEBUG_MALLOC
void
tr_freehook (ptr)
     __ptr_t ptr;
{
  fprintf (mallstream, "- %p,%p,%p %p\n", BI(2),BI(3),BI(4), ptr);	/* Be sure to print it first.  */
  __free_hook = tr_old_free_hook;
  free (ptr);
  __free_hook = tr_freehook;
}

__ptr_t
tr_mallochook (size)
     size_t size;
{
  __ptr_t hdr;

  __malloc_hook = tr_old_malloc_hook;
  hdr = (__ptr_t) xmalloc (size);
  __malloc_hook = tr_mallochook;

  /* We could be printing a NULL here; that's OK.  */
  fprintf (mallstream, "+ %p,%p,%p %p %x\n", BI(2),BI(3),BI(4), hdr, size);

  return hdr;
}

__ptr_t
tr_reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  __ptr_t hdr;

  __free_hook = tr_old_free_hook;
  __malloc_hook = tr_old_malloc_hook;
  __realloc_hook = tr_old_realloc_hook;
  hdr = (__ptr_t) realloc (ptr, size);
  __free_hook = tr_freehook;
  __malloc_hook = tr_mallochook;
  __realloc_hook = tr_reallochook;
  if (hdr == NULL)
    /* Failed realloc.  */
    fprintf (mallstream, "! %p,%p,%p %p %x\n", __builtin_return_address(2),BI(3),BI(4), ptr, size);
  else
    fprintf (mallstream, "< %p,%p,%p %p\n> %p,%p,%p %p %x\n", BI(2),BI(3),BI(4), ptr, BI(2),BI(3),BI(4), hdr, size);

  return hdr;
}

void
mmtrace ()
{
	char *mallfile;

	mallfile = getenv (mallenv);
	if (mallfile != NULL) {
    	mallstream = fopen (mallfile != NULL ? mallfile : "/dev/null", "a");
		if (mallstream != NULL) {
	  		/* Be sure it doesn't xmalloc its buffer!  */
	  		setbuf (mallstream, mallbuf);
	  		fprintf (mallstream, "= Start\n");
	  		tr_old_free_hook = __free_hook;
	  		__free_hook = tr_freehook;
	  		tr_old_malloc_hook = __malloc_hook;
	  		__malloc_hook = tr_mallochook;
	  		tr_old_realloc_hook = __realloc_hook;
	  		__realloc_hook = tr_reallochook;
		}
    }
}



void checkhdr (hdr)
     __const struct hdr *hdr;
{
	if (hdr->magic != MAGICWORD || ((char *) &hdr[1])[hdr->size] != MAGICBYTE)
		(*abortfunc) ();
	if(hdr->prevb != &thelist) {
		if (hdr->prevb->magic != MAGICWORD || ((char *) &hdr->prevb[1])[hdr->prevb->size] != MAGICBYTE)
			(*abortfunc) ();
	}
	if(hdr->nextb != &thelist) {
		if (hdr->nextb->magic != MAGICWORD || ((char *) &hdr->nextb[1])[hdr->nextb->size] != MAGICBYTE)
			(*abortfunc) ();
	}
}

int mallcheck(void *foo) {
	int found = 0;
	struct hdr *h = &thelist;
	while((h = h->nextb) != &thelist) {
		checkhdr(h);
		if(h+1 == foo)
			found++;
	}
	return found;
}

void
freehook (ptr)
     __ptr_t ptr;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  checkhdr (hdr);
  hdr->magic = 0;
  hdr->prevb->nextb = hdr->nextb;
  hdr->nextb->prevb = hdr->prevb;
  __free_hook = old_free_hook;
  free (hdr);
  __free_hook = freehook;
}

__ptr_t
mallochook (size)
     size_t size;
{
  struct hdr *hdr;

  __malloc_hook = old_malloc_hook;
  hdr = (struct hdr *) xmalloc (sizeof (struct hdr) + size + 1);
  __malloc_hook = mallochook;
  if (hdr == NULL)
    return NULL;

  hdr->size = size;
  hdr->magic = MAGICWORD;
  hdr->prevb = &thelist;
  hdr->nextb = thelist.nextb;
  hdr->prevb->nextb = hdr;
  hdr->nextb->prevb = hdr;

  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

__ptr_t
reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;

  checkhdr (hdr);
  hdr->prevb->nextb = hdr->nextb;
  hdr->nextb->prevb = hdr->prevb;

  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
  __realloc_hook = old_realloc_hook;
  hdr = (struct hdr *) realloc ((__ptr_t) hdr, sizeof (struct hdr) + size + 1);
  __free_hook = freehook;
  __malloc_hook = mallochook;
  __realloc_hook = reallochook;
  if (hdr == NULL)
    return NULL;

  hdr->magic = MAGICWORD;
  hdr->size = size;
  hdr->prevb = &thelist;
  hdr->nextb = thelist.nextb;
  hdr->prevb->nextb = hdr;
  hdr->nextb->prevb = hdr;
  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

void
mcheck (func)
     void (*func) __P ((void));
{
  static int mcheck_used = 0;

  if (func != NULL)
    abortfunc = func;

  /* These hooks may not be safely inserted if xmalloc is already in use.  */
  if (!__malloc_initialized && !mcheck_used)
    {
      old_free_hook = __free_hook;
      __free_hook = freehook;
      old_malloc_hook = __malloc_hook;
      __malloc_hook = mallochook;
      old_realloc_hook = __realloc_hook;
      __realloc_hook = reallochook;
      mcheck_used = 1;
    }
}

void chkall(void);
void chkone(void *foo);
#else /* not DO_DEBUG_MALLOC */
#undef chkone
#undef chkall
void chkone(void *foo) { }
void chkall(void) { }
#define chkall() do { } while (0)
#define chkone(f) do { } while (0)
#endif



#ifdef DO_DEBUG_MALLOC

void chkcf(cf conf)
{
	for(;conf != NULL; conf = conf->next) 
		chkone(conf);
}

void chkone(void *foo)
{
	if(foo == NULL) return;
	checkhdr(((struct hdr *)foo)-1);
}


void chkall(void)
{
	struct conninfo *conn;
	for(conn = isdn4_conn; conn != NULL; conn = conn->next)  {
		chkone(conn); chkone(conn->cg);
	}
#if 0 /* takes much too long */
	chkcf(cf_P);
	chkcf(cf_ML);
	chkcf(cf_MP);
	chkcf(cf_D);
	chkcf(cf_DL);
	chkcf(cf_DP);
	chkcf(cf_R);
	chkcf(cf_RP);
	chkcf(cf_LF);
	chkcf(cf_C);
	chkcf(cf_CM);
	chkcf(cf_CL);
	mallcheck(NULL);
#endif
}
#endif /* DO_DEBUG_MALLOC */

