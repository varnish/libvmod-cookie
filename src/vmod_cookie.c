/*
Cookie VMOD for Varnish.

Simplifies handling of the Cookie request header.

Author: Lasse Karstensen <lasse@varnish-software.com>, July 2012.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "vrt.h"
#include "vqueue.h"
#include "cache/cache.h"

#include "vcc_if.h"

#ifndef VRT_CTX
#define VRT_CTX const struct vrt_ctx *ctx
#endif

struct cookie {
	unsigned magic;
#define VMOD_COOKIE_ENTRY_MAGIC 0x3BB41543
	char *name;
	char *value;
	VTAILQ_ENTRY(cookie) list;
};

struct whitelist {
	char *name;
	VTAILQ_ENTRY(whitelist) list;
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
cobj_get(VRT_CTX) {
	struct vmod_cookie *vcp = pthread_getspecific(key);

	if (!vcp) {
		vcp = malloc(sizeof *vcp);
		AN(vcp);
		cobj_clear(vcp);
		vcp->xid = ctx->vsl->wid;
		AZ(pthread_setspecific(key, vcp));
	}

	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (vcp->xid != ctx->vsl->wid) {
		// Reuse previously allocated storage
		cobj_clear(vcp);
		vcp->xid = ctx->vsl->wid;
	}

	return (vcp);
}

VCL_VOID
vmod_parse(VRT_CTX, VCL_STRING cookieheader) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	char *name, *value;
	const char *p, *sep;
	int i = 0;

	if (!cookieheader || strlen(cookieheader) == 0) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: nothing to parse");
		return;
	}

	// VSLb(ctx->vsl, SLT_Debug, "cookie: cookie string is %lu bytes: %s",
	//    strlen(cookieheader), cookieheader);

	if (!VTAILQ_EMPTY(&vcp->cookielist)) {
		/* If called twice during the same request, clean out old state */
		vmod_clean(ctx);
	}

	p = cookieheader;
	while (*p != '\0') {
		while (isspace(*p)) p++;
		sep = strchr(p, '=');
		if (sep == NULL)
			break;
		name = strndup(p, pdiff(p, sep));
		p = sep + 1;
		sep = strchr(p, ';');
		if (sep == NULL)
			sep = strchr(p, '\0');

		if (sep == NULL) {
			free(name);
			break;
		}
		value = strndup(p, pdiff(p, sep));
		if (sep != '\0')
			p = sep + 1;

		// VSLb(ctx->vsl, SLT_Debug, "name(%lu)=%s value(%lu)=\"%s\" ",
		//   strlen(name), name, strlen(value), value);
		vmod_set(ctx, name, value);
		free(name);
		free(value);
		i++;
	}
	VSLb(ctx->vsl, SLT_VCL_Log, "cookie: parsed %i cookies.", i);
}


VCL_VOID
vmod_set(VRT_CTX, VCL_STRING name, VCL_STRING value) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	/* Empty cookies should be ignored. */
	if (name == NULL || strlen(name) == 0)
		return;
	if (value == NULL || strlen(value) == 0)
		return;

	char *p;
	struct cookie *cookie;
	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(cookie, VMOD_COOKIE_ENTRY_MAGIC);
		if (strcmp(cookie->name, name) == 0) {
			p = WS_Printf(ctx->ws, "%s", value);
			if (p == NULL) {
				WS_MarkOverflow(ctx->ws); // Remove when WS_Printf() does it.
				VSLb(ctx->vsl, SLT_VCL_Log,
				    "cookie: Workspace overflow in set()");
			} else
				cookie->value = p;

			return;
		}
	}

	struct cookie *newcookie = (struct cookie *)WS_Alloc(ctx->ws,
	    sizeof(struct cookie));
	if (newcookie == NULL) {
		VSLb(ctx->vsl, SLT_VCL_Log, "cookie: unable to get storage for cookie");
		return;
	}
	newcookie->magic = VMOD_COOKIE_ENTRY_MAGIC;
	newcookie->name = WS_Printf(ctx->ws, "%s", name);
	newcookie->value = WS_Printf(ctx->ws, "%s", value);
	if (newcookie->name == NULL || newcookie->value == NULL) {
		WS_MarkOverflow(ctx->ws);
		WS_Release(ctx->ws, sizeof(struct cookie));
		return;
	}
	VTAILQ_INSERT_TAIL(&vcp->cookielist, newcookie, list);
}

VCL_BOOL
vmod_isset(VRT_CTX, const char *name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (name == NULL || strlen(name) == 0)
		return(0);

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
vmod_get(VRT_CTX, VCL_STRING name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (name == NULL || strlen(name) == 0)
		return(NULL);

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
vmod_delete(VRT_CTX, VCL_STRING name) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (name == NULL || strlen(name) == 0)
		return;

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
vmod_clean(VRT_CTX) {
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);
	AN(&vcp->cookielist);

	VTAILQ_INIT(&vcp->cookielist);
}

VCL_VOID
vmod_filter_except(VRT_CTX, VCL_STRING whitelist_s) {
	struct cookie *cookieptr, *safeptr;
	struct vmod_cookie *vcp = cobj_get(ctx);
	struct whitelist *whentry, *whsafe;
	char const *p = whitelist_s;
	char *q;
	int whitelisted = 0;

	VTAILQ_HEAD(, whitelist) whitelist_head;
	VTAILQ_INIT(&whitelist_head);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);
	AN(whitelist_s);

	/* Parse the supplied whitelist. */
	while (*p != '\0') {
		while (*p != '\0' && isspace(*p)) p++;
		if (p == '\0')
			break;

		q = strchr(p, ',');
		if (q == NULL) {
			q = strchr(p, '\0');
		}
		AN(q);

		if (q == p) {
			p++;
			continue;
		}

		assert(q > p);
		assert(q-p > 0);

		whentry = malloc(sizeof(struct whitelist));
		AN(whentry);
		whentry->name = strndup(p, q-p);
		AN(whentry->name);
		VSLb(ctx->vsl, SLT_Debug, "cookie: p is %s -- name: %s", p, whentry->name);

		VTAILQ_INSERT_TAIL(&whitelist_head, whentry, list);

		p = q;
		if (*p != '\0')
			p++;
	}

	/* Filter existing cookies that isn't in the whitelist. */
	VTAILQ_FOREACH_SAFE(cookieptr, &vcp->cookielist, list, safeptr) {
		CHECK_OBJ_NOTNULL(cookieptr, VMOD_COOKIE_ENTRY_MAGIC);
		whitelisted = 0;
		VTAILQ_FOREACH(whentry, &whitelist_head, list) {
			if (strlen(cookieptr->name) == strlen(whentry->name) &&
			    strcmp(cookieptr->name, whentry->name) == 0) {
				whitelisted = 1;
				break;
			}
		}
		if (!whitelisted) {
			VTAILQ_REMOVE(&vcp->cookielist, cookieptr, list);
		}
	}

	VTAILQ_FOREACH_SAFE(whentry, &whitelist_head, list, whsafe) {
		VTAILQ_REMOVE(&whitelist_head, whentry, list);
		free(whentry->name);
		free(whentry);
	}
}


VCL_STRING
vmod_get_string(VRT_CTX) {
	struct cookie *curr;
	struct vsb *output;
	void *u;
	struct vmod_cookie *vcp = cobj_get(ctx);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	output = VSB_new_auto();
	AN(output);

	VTAILQ_FOREACH(curr, &vcp->cookielist, list) {
		CHECK_OBJ_NOTNULL(curr, VMOD_COOKIE_ENTRY_MAGIC);
		AN(curr->name);
		AN(curr->value);
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
vmod_format_rfc1123(VRT_CTX, VCL_TIME ts, VCL_DURATION duration) {
        return VRT_TIME_string(ctx, ts + duration);
}

