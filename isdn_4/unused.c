/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"


cf
getcards(conngrab cg, cf list)
{
	if(cg->card == NULL)
		return NULL;
	if(cg->site == NULL)
		return NULL;
	if(cg->protocol == NULL)
		return NULL;
	if(cg->cclass == NULL)
		return NULL;
	while(list != NULL) {
		if(!wildmatch(list->site,cg->site))
			continue;
		if(!wildmatch(list->protocol,cg->protocol))
			continue;
		if(!wildmatch(list->card,cg->card))
			continue;
		if(!classmatch(list->cclass,cg->cclass))
			continue;
		if(!matchflag(cg->flags,list->type))
			continue;
		return list;
	}
	return NULL;
}

static mblk_t *
getprot (char *protocol, char *site, char *cclass, char *suffix)
{
	cf prot;
	mblk_t *mi;

	for (prot = cf_P; prot != NULL; prot = prot->next) {
		if (site != NULL && !wildmatch (site, prot->site))
			continue;
		else if (site == NULL)
			site = prot->site;
		if (protocol != NULL && !wildmatch (protocol, prot->protocol))
			continue;
		else if (*protocol == NULL)
			protocol = prot->protocol;
		if (cclass != NULL && !classmatch (cclass, prot->cclass))
			continue;
		else if (cclass == NULL)
			cclass = prot->cclass;
		break;
	}
	if (prot == NULL)
		return NULL;

	if ((mi = allocb (256, BPRI_MED)) == NULL)
		return NULL;
	for (; prot != NULL; prot = prot->next) {
		ushort_t id;
		char *newlim = NULL;
		mblk_t *mz = NULL;

		if (site != prot->site && (site = wildmatch (site, prot->site)) == NULL) 
			continue;
		if (protocol != prot->protocol && (protocol = wildmatch (protocol, prot->protocol)) == NULL) 
			continue;
		if (cclass != prot->cclass && (cclass = classmatch (protocol, prot->protocol))) 
			continue;
		mz = allocsb (strlen (prot->args), (streamchar *)prot->args);

		newlim = mz->b_wptr;
		while (m_getsx (mz, &id) == 0) {
			switch (id) {
			case ARG_NUMBER:
				if (*suffix != 0)
					m_getstr (mz, suffix, MAXNR);
				break;
			default:
				{
					ushort_t id2;
					char *news = NULL;
					char *olds = mi->b_rptr;

					while (m_getsx (mi, &id2) == 0) {
						if (id != id2)
							continue;
						mi->b_rptr = olds;
						goto skip;
					}
					mi->b_rptr = olds;
					m_putsx (mi, id);
					olds = mi->b_wptr;
					*olds++ = ' ';
					m_getskip (mz);
					news = mz->b_rptr;
					while (news < newlim && olds < mi->b_datap->db_lim
							&& *news != ':')
						*olds++ = *news++;
					mi->b_wptr = olds;
					mz->b_rptr = news;
				} break;
			}
		  skip:;
		}
		freeb (mz);
		if (strchr(prot->type,'X'))
			break;
	}
	return mi;
}
