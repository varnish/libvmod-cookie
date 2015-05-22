/*
Cookie VMOD for Varnish.

Simplifies handling of the Cookie request header.

Author: Lasse Karstensen <lasse@varnish-software.com>, July 2012.
*/

#include <stdlib.h>
#include <stdio.h>

#include "vrt.h"
#include "vqueue.h"
#include "cache/cache.h"

#include "vcc_if.h"

#define MAX_COOKIE_NAME 1024   /* name maxsize */
#define MAX_COOKIE_STRING 4096 /* cookie string maxlength */

struct cookie {
	unsigned magic;
#define VMOD_COOKIE_ENTRY_MAGIC 0x3BB41543
	char *name;
	char *value;
	VTAILQ_ENTRY(cookie) list;
};

struct list {
	char name[MAX_COOKIE_NAME];
	VTAILQ_ENTRY(list) list;
};

struct vmod_cookie {
	unsigned magic;
#define VMOD_COOKIE_MAGIC 0x4EE5FB2E
	unsigned xid;
	VTAILQ_HEAD(, cookie) cookielist;
};

static pthread_key_t key;
static pthread_once_t key_is_initialized = PTHREAD_ONCE_INIT;

static void
mkkey(void) {
	AZ(pthread_key_create(&key, free));
}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
	pthread_once(&key_is_initialized, mkkey);
	return (0);
}

static void
cobj_clear(struct vmod_cookie *c) {
	c->magic = VMOD_COOKIE_MAGIC;
	VTAILQ_INIT(&c->cookielist);
	c->xid = 0;
}

static struct vmod_cookie *
cobj_get(const struct vrt_ctx *ctx) {
	struct vmod_cookie *vcp = pthread_getspecific(key);

	if (!vcp) {
		vcp = malloc(sizeof *vcp);
		AN(vcp);
		cobj_clear(vcp);
		vcp->xid = ctx->req->sp->vxid;
		AZ(pthread_setspecific(key, vcp));
	}

	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (vcp->xid != ctx->req->sp->vxid) {
		// Reuse previously allocated storage
		cobj_clear(vcp);
		vcp->xid = ctx->req->sp->vxid;
	}

	return (vcp);
}

VCL_VOID
vmod_parse(const struct vrt_ctx *ctx, VCL_STRING cookieheader) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	char tokendata[MAX_COOKIE_STRING];
	char *token, *tokstate, *value, *sepindex, *dataptr;

	int i = 0;

	/* If called twice during the same request, clean out old state */
	vmod_clean(ctx);

	if (!cookieheader || strlen(cookieheader) == 0) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: nothing to parse");
		return;
	}

	VSLb(ctx->vsl, SLT_Debug, "cookie: cookie string is %lu bytes.", strlen(cookieheader));

	if (strlen(cookieheader) >= MAX_COOKIE_STRING) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: cookie string overflowed, abort");
		return;
	}

	/* strtok modifies source, fewer surprises. */
	strncpy(tokendata, cookieheader, sizeof(tokendata));
	dataptr = tokendata;

	while (1) {
		token = strtok_r(dataptr, ";", &tokstate);
		dataptr = NULL; /* strtok() wants NULL on subsequent calls. */

		if (token == NULL)
		    break;

		while (token[0] == ' ')
		    token++;

		sepindex = strchr(token, '=');
		if (sepindex == NULL) {
			/* No delimiter, this cookie is invalid. Next! */
			continue;
		}
		value = sepindex + 1;
		*sepindex = '\0';

		VSLb(ctx->vsl, SLT_Debug, "value length is %lu.", strlen(value));
		vmod_set(ctx, token, value);
		i++;
	}
	VSLb(ctx->vsl, SLT_VCL_Log, "cookie: parsed %i cookies.", i);
}


VCL_VOID
vmod_set(const struct vrt_ctx *ctx, VCL_STRING name, VCL_STRING value) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	AN(name);
	AN(value);

	/* Empty cookies should be ignored. */
	if (strlen(name) == 0 || strlen(value) == 0) {
		return;
	}

	if (strlen(name) + 1 >= MAX_COOKIE_NAME) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: cookie string overflowed");
		return;
	}

	struct cookie *cookie;
	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		if (strcmp(cookie->name, name) == 0) {
			cookie->value = WS_Printf(ctx->ws, "%s", value);
			return;
		}
	}

	struct cookie *newcookie = (struct cookie *)WS_Alloc(ctx->ws, sizeof(struct cookie));
	if (newcookie == NULL) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: unable to get storage for cookie");
		return;
	}
	newcookie->magic = VMOD_COOKIE_ENTRY_MAGIC;
	newcookie->name = WS_Printf(ctx->ws, "%s", name);
	newcookie->value = WS_Printf(ctx->ws, "%s", value);

	VTAILQ_INSERT_TAIL(&vcp->cookielist, newcookie, list);
}

VCL_BOOL
vmod_isset(const struct vrt_ctx *ctx, const char *name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	struct cookie *cookie;
	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		if (strcmp(cookie->name, name) == 0) {
			return 1;
		}
	}
	return 0;
}

VCL_STRING
vmod_get(const struct vrt_ctx *ctx, VCL_STRING name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	struct cookie *cookie;
	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		if (strcmp(cookie->name, name) == 0) {
			return (cookie->value);
		}
	}
	return (NULL);
}


VCL_VOID
vmod_delete(const struct vrt_ctx *ctx, VCL_STRING name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	struct cookie *cookie;
	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		if (strcmp(cookie->name, name) == 0) {
			VTAILQ_REMOVE(&vcp->cookielist, cookie, list);
			/* No way to clean up storage, let ws reclaim do it. */
			break;
		}
	}
}

VCL_VOID
vmod_clean(const struct vrt_ctx *ctx) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	struct cookie *cookie, *c_safe;
	VTAILQ_FOREACH_SAFE(cookie, &vcp->cookielist, list, c_safe) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		VTAILQ_REMOVE(&vcp->cookielist, cookie, list);
	}
}

VCL_VOID
vmod_filter_function(const struct vrt_ctx *ctx, VCL_STRING list_s, VCL_INT white) {
	char buf[MAX_COOKIE_STRING];
	struct cookie *cookieptr;
	char *tokptr, *saveptr;
	int listed = 0;
	struct vmod_cookie *vcp = cobj_get(ctx);
	struct list *entry;

	VTAILQ_HEAD(, list) list_head;
	VTAILQ_INIT(&list_head);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	strcpy(buf, list_s);
	tokptr = strtok_r(buf, ",", &saveptr);
	if (!tokptr) return;

	/* Parse the supplied list. */
	while (1) {
		entry = malloc(sizeof(struct list));
		AN(entry);
		strcpy(entry->name, tokptr);
		VTAILQ_INSERT_TAIL(&list_head, entry, list);
		tokptr = strtok_r(NULL, ",", &saveptr);
		if (!tokptr) break;
	}

	/* Filter existing cookies that is or isn't (depends on the white flag) in the list. */
	VTAILQ_FOREACH(cookieptr, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookieptr, VMOD_COOKIE_ENTRY_MAGIC);
		listed = 0;
		VTAILQ_FOREACH(entry, &list_head, list) {
			if (strlen(cookieptr->name) == strlen(entry->name) &&
			    strcmp(cookieptr->name, entry->name) == 0) {
				listed = 1;
				break;
			}
		}
		if ((white && !listed) || (!white && listed)) {
			VTAILQ_REMOVE(&vcp->cookielist, cookieptr, list);
		}
	}

	VTAILQ_FOREACH(entry, &list_head, list) {
		VTAILQ_REMOVE(&list_head, entry, list);
		free(entry);
	}
}

VCL_VOID
vmod_filter_except(const struct vrt_ctx *ctx, VCL_STRING whitelist_s) {
	vmod_filter_function(ctx, whitelist_s, 1);
}

VCL_VOID
vmod_filter(const struct vrt_ctx *ctx, VCL_STRING blacklist_s) {
	vmod_filter_function(ctx, blacklist_s, 0);
}

VCL_STRING
vmod_get_string(const struct vrt_ctx *ctx) {
	struct cookie *curr;
	struct vsb *output;
	void *u;
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	output = VSB_new_auto();
	AN(output);

	VTAILQ_FOREACH(curr, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(curr, VMOD_COOKIE_ENTRY_MAGIC);
		VSB_printf(output, "%s=%s; ", curr->name, curr->value);
	}
	VSB_trim(output);
	VSB_finish(output);

	u = WS_Alloc(ctx->ws, VSB_len(output) + 1);
	if (!u) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: Workspace overflow");
		VSB_delete(output);
		return(NULL);
	}

	strcpy(u, VSB_data(output));
	VSB_delete(output);
	return (u);
}


VCL_STRING
vmod_format_rfc1123(const struct vrt_ctx *ctx, VCL_TIME ts, VCL_DURATION duration) {
        return VRT_TIME_string(ctx, ts + duration);
}

