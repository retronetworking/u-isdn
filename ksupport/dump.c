#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "isdn_23.h"
#ifdef KERNEL
#define putchar(x) printf("%c",(x))
#else
#include <stdio.h>
#include <syslog.h>
#endif

void
dumphex (uchar_t * buf, ushort_t len)
{
	putchar ('<');
	while (len--) {
		printf ("%02x", *buf++);
		if (len)
			putchar (' ');
	}
	putchar ('>');
}


void
dumpaschex (uchar_t * buf, ushort_t len)
{
	putchar ('<');
	while (len--) {
		printf ("%02x%c", *buf, (*buf >= ' ' && *buf < 0x7F) ? *buf : '.');
		buf++;
		if (len)
			putchar (' ');
	}
	putchar ('>');
}

void
dumpascii (uchar_t * buf, ushort_t len)
{
	putchar ('<');
	while (len--) {
		printf ("%c", (*buf >= ' ' && *buf < 0x7F) ? *buf : '.');
		buf++;
	}
	putchar ('>');
}


void
dumpblock (char *name, uchar_t * buf, ushort_t len)
{
#define BLOCKSIZE 0x10
	ushort_t i;
	uchar_t *dp;

	if (name != NULL)
		printf (" ** %s", name);
	printf ("\n    ");

	for (i = 0, dp = buf; i < len; dp += BLOCKSIZE, i += BLOCKSIZE) {
		int k;
		int l = len - i;

		for (k = 0; k < BLOCKSIZE && k < l; k++)
			printf ("%x%x%s", dp[k] >> 4, dp[k] & 0x0F, (k & 0x01) ? " " : "");
		for (; k < BLOCKSIZE - 1; k += 2)
			printf ("     ");
		if (k < BLOCKSIZE)
			printf ("   ");
		for (k = 0; k < BLOCKSIZE && k < l; k++)
			if (dp[k] > 31 && dp[k] < 127)
				printf ("%c", dp[k]);
			else
				printf (".");
		if (k < l)
			printf (" +\n    ");
		else
			printf ("\n");
	}
}

uchar_t
hexc (uchar_t x)
{
	switch (x) {
	default:
		return 255;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return x - '0';
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		return x - 'A' + 10;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		return x - 'a' + 10;
	}
}

void
hexm (uchar_t * tr, int *len)
{
	uchar_t *tw = tr;
	int nlen = 0, olen = *len;

	while (olen > 1) {
		uchar_t hexn1, hexn2;

		if (*tr == ' ' || *tr == '\t') {
			olen++;
			tr++;
			continue;
		}
		hexn1 = hexc (*tr++);
		if (hexn1 > 15) {
			*len = nlen;
			return;
		}
		hexn2 = hexc (*tr++);
		if (hexn2 > 15) {
			*len = nlen;
			return;
		}
		*tw++ = (hexn1 << 4) | hexn2;
		olen -= 2;
		nlen++;
	}
	*len = nlen;
}

char *
conv_ind (unsigned char xx)
{
	switch(xx) {
	case DL_ESTABLISH_REQ:		return "DL_ESTABLISH_REQ";
	case DL_ESTABLISH_IND:		return "DL_ESTABLISH_IND";
	case DL_ESTABLISH_CONF:		return "DL_ESTABLISH_CONF";
	case PH_ACTIVATE_IND:		return "PH_ACTIVATE_IND";
	case PH_ACTIVATE_REQ:		return "PH_ACTIVATE_REQ";
	case PH_ACTIVATE_CONF:		return "PH_ACTIVATE_CONF";
	case PH_ACTIVATE_NOTE:		return "PH_ACTIVATE_NOTE";
	case DL_RELEASE_REQ:		return "DL_RELEASE_REQ";
	case DL_RELEASE_IND:		return "DL_RELEASE_IND";
	case DL_RELEASE_CONF:		return "DL_RELEASE_CONF";
	case PH_DEACTIVATE_REQ:		return "PH_DEACTIVATE_REQ";
	case PH_DEACTIVATE_IND:		return "PH_DEACTIVATE_IND";
	case PH_DEACTIVATE_CONF:	return "PH_DEACTIVATE_CONF";
	case PH_DISCONNECT_IND:		return "PH_DISCONNECT_IND";
	case MDL_ASSIGN_REQ:		return "MDL_ASSIGN_REQ";
	case MDL_REMOVE_REQ:		return "MDL_REMOVE_REQ";
	case MDL_ERROR_IND:			return "MDL_ERROR_IND";
	default:
		{
			static char sb[30];
			sprintf(sb,"<?? 0x%02x>",xx);
			return sb;
		}
	}
}

static void
dump_one_hdr (isdn23_hdr hdr)
{
	printf("=%x_",hdr->seqnum);
	if(hdr->key & HDR_NOERROR)
		printf("NoErr_");
	switch (hdr->key & ~HDR_FLAGS) {
	default:
#ifdef KERNEL
		printf("??? Unknown header ID %d\n",hdr->key);
#else
		syslog (LOG_ERR, "Unknown header ID %d", hdr->key);
		printf("Unknown header ID %d", hdr->key);
#endif
		break;
	case HDR_ATCMD:
		printf ("ATcmd from %d", hdr->hdr_atcmd.minor);
		break;
	case HDR_PROTOCMD:
		if(hdr->hdr_protocmd.minor != 0)
			printf ("ProtocolCmd from minor %d", hdr->hdr_protocmd.minor);
		else
			printf ("ProtocolCmd from card %d chan %d", hdr->hdr_protocmd.card, hdr->hdr_protocmd.channel);
		break;
	case HDR_XDATA:
		printf ("XData from %d", hdr->hdr_xdata.minor);
		break;
	case HDR_DATA:
		printf ("Idata from %d/%02x", hdr->hdr_data.card, hdr->hdr_data.SAPI);
		break;
	case HDR_UIDATA:
		printf ("UIdata from %d/%02x", hdr->hdr_uidata.card, hdr->hdr_uidata.SAPI);
		break;
	case HDR_RAWDATA:
		printf ("RawData from %d, dchan %d, flags 0%o", hdr->hdr_rawdata.card,hdr->hdr_rawdata.dchan,hdr->hdr_rawdata.flags);
		break;
	case HDR_LOAD:
		printf("Boot from %d, stage %d, offset %d len %d",hdr->hdr_load.card,hdr->hdr_load.seqnum,hdr->hdr_load.foffset,hdr->hdr_load.len);
		break;
	case HDR_TEI:
		printf ("TEI card %d, TEI %d", hdr->hdr_tei.card, hdr->hdr_tei.TEI);
		break;
	case HDR_OPEN:
		printf ("Open port %d, flags %o", hdr->hdr_open.minor, hdr->hdr_open.flags);
		break;
	case HDR_CLOSE:
		printf ("Close port %d, errno %d", hdr->hdr_close.minor, hdr->hdr_close.error);
		break;
	case HDR_ATTACH:
		printf ("Attach id %ld chan %d/%d to port %d, %s%s",
				hdr->hdr_attach.connref, hdr->hdr_attach.card, hdr->hdr_attach.chan, hdr->hdr_attach.minor,
				(hdr->hdr_attach.listen & 2) ? "setup" : ((hdr->hdr_attach.listen & 1) ? "listen" : "talk"),
				(hdr->hdr_attach.listen & 4) ? " force" : "");
		break;
	case HDR_DETACH:
		printf ("Detach id %ld from port %d, errno %d, force %d",
				hdr->hdr_detach.connref, hdr->hdr_detach.minor, 
				hdr->hdr_detach.error,hdr->hdr_detach.perm);
		break;
	case HDR_CARD:
		printf ("Card %d online (%d D channels, %d B channels, flag 0%o)",
				hdr->hdr_card.card,
				hdr->hdr_card.dchans,hdr->hdr_card.bchans,
				hdr->hdr_card.flags);
		break;
	case HDR_NOCARD:
		printf ("Card %d offline.", hdr->hdr_nocard.card);
		break;
	case HDR_OPENPROT:
		printf ("OpenProtocol %d/%02x, Ind %s", hdr->hdr_openprot.card, hdr->hdr_openprot.SAPI, conv_ind(hdr->hdr_openprot.ind));
		break;
	case HDR_CLOSEPROT:
		printf ("CloseProtocol %d/%02x, Ind %s", hdr->hdr_closeprot.card, hdr->hdr_closeprot.SAPI, conv_ind(hdr->hdr_closeprot.ind));
		break;
	case HDR_NOTIFY:
		printf ("State Change %d/%02x, Ind %s:%x", hdr->hdr_notify.card, hdr->hdr_notify.SAPI, conv_ind(hdr->hdr_notify.ind), hdr->hdr_notify.add);
		break;
	case HDR_INVAL:
#ifdef KERNEL
		if(hdr->hdr_inval.error != 0)
			printf ("Error %d", hdr->hdr_inval.error);
		else
			printf ("Command OK");
#else
		if(hdr->hdr_inval.error != 0)
			printf ("Error %s", strerror(hdr->hdr_inval.error));
		else
			printf ("Command OK");
#endif
		break;
	}
}

void
dump_hdr (isdn23_hdr hdr, const char *what, uchar_t * data)
{
	if(what != NULL)
		printf (" %s: ", what);

	dump_one_hdr(hdr);
	switch (hdr->key & ~HDR_FLAGS) {
	default:
		dumpaschex((uchar_t *)hdr,sizeof(*hdr));
		printf("\n");
#ifndef KERNEL
		abort();
#endif
		break;
	case HDR_ATCMD:
	case HDR_PROTOCMD:
	case HDR_XDATA:
		if(data != NULL) {
			printf (": ");
			dumpascii (data, hdr->hdr_data.len);
		}
		printf ("\n");
		break;
	case HDR_DATA:
	case HDR_UIDATA:
	case HDR_RAWDATA:
		if(data != NULL) {
			printf (": ");
			dumphex (data, hdr->hdr_data.len);
		}
		printf("\n");
		break;
	case HDR_LOAD:
	case HDR_TEI:
	case HDR_OPEN:
	case HDR_CLOSE:
	case HDR_ATTACH:
	case HDR_DETACH:
	case HDR_CARD:
	case HDR_NOCARD:
	case HDR_OPENPROT:
	case HDR_CLOSEPROT:
	case HDR_NOTIFY:
		printf("\n");
		break;
	case HDR_INVAL:
		printf(" <");
		if(data != NULL)
			dump_one_hdr ((isdn23_hdr) data);
		else
			dump_one_hdr(hdr+1);
		printf (">\n");
		break;
	}
}

#ifdef MODULE
static int do_init_module(void)
{
	return 0;
}

static int do_exit_module(void)
{
	return 0;
}
#endif
