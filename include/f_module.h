
#if defined (linux) && defined(__KERNEL__) && defined(MODULE)
#define _HAS_MODULE

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>

#ifndef F_NOCODE
char kernel_version[] = UTS_RELEASE;

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
