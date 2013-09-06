/*-
 * Copyright (c) 2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <stdlib.h>

#include "cache.h"
#include "hash/hash_slinger.h"

struct objiter {
	unsigned			magic;
#define OBJITER_MAGIC			0x745fb151
	struct busyobj			*bo;
	struct object			*obj;
	struct storage			*st;
	struct worker			*wrk;
};

struct objiter *
ObjIterBegin(struct worker *wrk, struct object *obj)
{
	struct objiter *oi;

	CHECK_OBJ_NOTNULL(obj, OBJECT_MAGIC);
	ALLOC_OBJ(oi, OBJITER_MAGIC);
	if (oi == NULL)
		return (oi);
	oi->obj = obj;
	oi->wrk = wrk;
	oi->bo = HSH_RefBusy(obj->objcore);
	while (obj->objcore->busyobj != NULL)
		(void)usleep(10000);
	return (oi);
}

int
ObjIter(struct objiter *oi, void **p, ssize_t *l)
{

	CHECK_OBJ_NOTNULL(oi, OBJITER_MAGIC);
	AN(p);
	AN(l);

	if (oi->st == NULL)
		oi->st = VTAILQ_FIRST(&oi->obj->store);
	else
		oi->st = VTAILQ_NEXT(oi->st, list);
	if (oi->st != NULL) {
		*p = oi->st->ptr;
		*l = oi->st->len;
		return (1);
	}
	return (0);
}

void
ObjIterEnd(struct objiter **oi)
{

	AN(oi);
	CHECK_OBJ_NOTNULL((*oi), OBJITER_MAGIC);
	if ((*oi)->bo != NULL)
		VBO_DerefBusyObj((*oi)->wrk, &(*oi)->bo);
	FREE_OBJ((*oi));
	*oi = NULL;
}
