/*
Cookie VMOD for Varnish.

Simplifies handling of the Cookie request header.

Author: Lasse Karstensen <lasse@varnish-software.com>, July 2012.
*/

#include <stdlib.h>
#include <stdio.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#define MAX_COOKIEPART 1024   /* name or value maxsize */
#define MAX_COOKIESTRING 8196 /* cookie string maxlength */
#define MAXCOOKIES 100

struct cookie {
	char *name;
	char *value;
	VTAILQ_ENTRY(cookie) list;
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
cobj_get(struct sess *sp) {
	struct vmod_cookie *vcp = pthread_getspecific(key);

	if (!vcp) {
		vcp = malloc(sizeof *vcp);
		AN(vcp);
		cobj_clear(vcp);
		vcp->xid = sp->xid;
		AZ(pthread_setspecific(key, vcp));
	}

	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	if (vcp->xid != sp->xid) {
		// Reuse previously allocated storage
		cobj_clear(vcp);
		vcp->xid = sp->xid;
	}

	return (vcp);
}

void
vmod_parse(struct sess *sp, const char *cookieheader) {
	char tokendata[MAX_COOKIESTRING];
	char *token, *tokstate, *key, *value, *sepindex;
	char *dataptr = tokendata;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	struct cookie *newcookie;

	int i = 0;

	// If called twice during the same request, clean out old state
	// before proceeding.
	while (!VTAILQ_EMPTY(&vcp->cookielist)) {
		newcookie = VTAILQ_FIRST(&vcp->cookielist);
		VTAILQ_REMOVE(&vcp->cookielist, newcookie, list);
	}

	if (cookieheader == NULL || strlen(cookieheader) == 0
	    || strlen(cookieheader) >= MAX_COOKIESTRING)
		return;

	/* strtok modifies source, fewer surprises. */
	strncpy(tokendata, cookieheader, sizeof(tokendata));
	dataptr = tokendata;

	while (1) {
		token = strtok_r(dataptr, ";", &tokstate);
		dataptr = NULL; /* strtok() wants NULL on subsequent calls. */

		if (token == NULL) break;
		while (token[0] == ' ') token++;
		//printf("token is: %s\n", token);

		sepindex = strchr(token, '=');
		if (sepindex == NULL) {
			// could not find delimiter, this cookie is invalid. skip to the
			// next (if any)
			continue;
		}
		value = sepindex + 1;
		*sepindex = '\0';

		vmod_set(sp, token, value);
		i++;
	}
	VSL(SLT_VCL_Log, 0, "libvmod-cookie: parsed %i cookies.", i);
}


void
vmod_set(struct sess *sp, const char *name, const char *value) {
	struct cookie *newcookie;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	// Empty cookies should be ignored.
	if (strlen(name) == 0 || strlen(value) == 0) {
		return;
	}
	struct cookie *cookie;

	VTAILQ_FOREACH(cookie, &vcp->cookielist, list) {
		if (strcmp(cookie->name, name) == 0) {
			cookie->value = WS_Dup(sp->ws, value);
			return;
		}
	}

	newcookie = (struct cookie *) WS_Alloc(sp->ws, sizeof(struct cookie));
	newcookie->name = WS_Dup(sp->ws, name);
	newcookie->value = WS_Dup(sp->ws, value);

	VTAILQ_INSERT_TAIL(&vcp->cookielist, newcookie, list);
}

const char *
vmod_get(struct sess *sp, const char *name) {
	struct cookie *cookie, *tmp;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	VTAILQ_FOREACH_SAFE(cookie, &vcp->cookielist, list, tmp) {
		if (strcmp(cookie->name, name) == 0) {
			return (cookie->value);
		}
	}
	return (NULL);
}


void
vmod_delete(struct sess *sp, const char *name) {
	struct cookie *cookie, *tmp;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	VTAILQ_FOREACH_SAFE(cookie, &vcp->cookielist, list, tmp) {
		if (strcmp(cookie->name, name) == 0) {
			VTAILQ_REMOVE(&vcp->cookielist, cookie, list);
			break;
		}
	}
}

void
vmod_clean(struct sess *sp) {
	struct cookie *cookie;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	while (!VTAILQ_EMPTY(&vcp->cookielist)) {
		cookie = VTAILQ_FIRST(&vcp->cookielist);
		VTAILQ_REMOVE(&vcp->cookielist, cookie, list);
	}
}

void
vmod_filter_except(struct sess *sp, const char *whitelist) {
	char cookienames[MAX_COOKIEPART][MAXCOOKIES];
	char tmpstr[MAX_COOKIESTRING];
	struct cookie *cookie, *tmp;
	char *tokptr, *saveptr;
	int i, found = 0, num_cookies = 0;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	strcpy(tmpstr, (char *)whitelist);
	tokptr = strtok_r(tmpstr, ",", &saveptr);
	if (!tokptr) return;

	// parse the whitelist.
	while (1) {
		strcpy(cookienames[num_cookies], tokptr);
		num_cookies++;

		tokptr = strtok_r(NULL, ",", &saveptr);
		if (!tokptr) break;
	}

	// filter existing cookies that isn't in the whitelist.
	VTAILQ_FOREACH_SAFE(cookie, &vcp->cookielist, list, tmp) {
		found = 0;
		for (i = 0; i < num_cookies; i++) {
			if (strlen(cookie->name) == strlen(cookienames[i]) &&
			    strcmp(cookienames[i], cookie->name) == 0) {
				found = 1;
				break;
			}
		}

		if (!found) {
			VTAILQ_REMOVE(&vcp->cookielist, cookie, list);
		}
	} // foreach
}


const char *
vmod_get_string(struct sess *sp) {
	struct cookie *curr;
	struct vsb *output;
	unsigned v, u;
	char *p;
	struct vmod_cookie *vcp = cobj_get(sp);
	CHECK_OBJ_NOTNULL(vcp, VMOD_COOKIE_MAGIC);

	u = WS_Reserve(sp->wrk->ws, 0);
	p = sp->wrk->ws->f;

	output = VSB_new_auto();
	AN(output);

	VTAILQ_FOREACH(curr, &vcp->cookielist, list) {
		VSB_printf(output, "%s=%s; ", curr->name, curr->value);
	}
	VSB_trim(output);
	VSB_finish(output);
	v = VSB_len(output);
	strcpy(p, VSB_data(output));

	VSB_delete(output);

	v++;
	if (v > u) {
		WS_Release(sp->wrk->ws, 0);
		VSL(SLT_Debug, 0, "cookie-vmod: Workspace overflowed, abort");
		return (NULL);
	}
	WS_Release(sp->wrk->ws, v);

	return (p);
}


const char *
vmod_format_rfc1123(struct sess *sp, double ts, double duration) {
        return VRT_time_string(sp, ts + duration);
}

