
#if defined (linux) && defined(__KERNEL__) && defined(MODULE)
#define _HAS_MODULE

#ifdef F_NOCODE
#define __NO_VERSION__
#endif

#include <linux/config.h>
#ifdef __GENKSYMS__
#include <linux/module.h>
#endif
#ifdef CONFIG_MODVERSIONS
#define MODVERSIONS
#include <linux/modversions.h>

#ifndef CONFIG_MODVERSIONS /* why does modversions clear this?? */
#define CONFIG_MODVERSIONS
#endif

#endif
#ifndef __GENKSYMS__
#include <linux/module.h>
#endif
#include "symtables"
#include <linux/kernel.h>

#ifndef F_NOCODE

static int do_init_module(void);
static int do_exit_module(void);

int init_module(void)
{
	return do_init_module();
}

void cleanup_module( void) {
	do_exit_module();
}
#endif
#define MORE_USE MOD_INC_USE_COUNT
#define LESS_USE MOD_DEC_USE_COUNT

#endif /* linux */


#if defined (sun) && defined(MODULE)
#define _HAS_MODULE
static int nr_use = 0;

#define MORE_USE nr_use++;
#define LESS_USE nr_use--;

static int do_init_module(struct vddrv *vdp);
static int do_exit_module(struct vddrv *vdp);

modinit (fc, vdp, vdi, vds)
	unsigned int fc;
	struct vddrv *vdp;
	addr_t vdi;
	struct vdstat *vds;
{
	int err;

	switch (fc) {
	case VDLOAD:
		return do_init_module(vdp);
	case VDUNLOAD:
		if (nr_use)
			return (EIO);
		return do_exit_module(vdp);
	case VDSTAT:
		return 0;
	default:
		return EIO;
	}
	return 0;
}

#endif



#ifndef _HAS_MODULE
#define MORE_USE do { } while(0)
#define LESS_USE do { } while(0)
#endif
