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
	char name[MAX_COOKIEPART];
	char value[MAX_COOKIEPART];
	VTAILQ_ENTRY(cookie) list;
};
VTAILQ_HEAD(, cookie) cookielist = VTAILQ_HEAD_INITIALIZER(cookielist);

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
	return (0);
}

void vmod_parse(struct sess *sp, const char *cookieheader) {
	char tokendata[MAX_COOKIESTRING];
	char *token, *tokstate, *key, *value, *sepindex;
	char *dataptr = tokendata;

	struct cookie *newcookie;

	int i = 0;

	// If called twice during the same request, clean out old state
	// before proceeding.
	while (!VTAILQ_EMPTY(&cookielist)) {
		newcookie = VTAILQ_FIRST(&cookielist);
		VTAILQ_REMOVE(&cookielist, newcookie, list);
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


void vmod_set(struct sess *sp, const char *name, const char *value) {
	struct cookie *newcookie;

	// Empty cookies should be ignored.
	if (strlen(name) == 0 || strlen(value) == 0) {
		return;
	}
	struct cookie *cookie;

	VTAILQ_FOREACH(cookie, &cookielist, list) {
		if (strcmp(cookie->name, name) == 0) {
			strcpy(cookie->value, value);
			return;
		}
	}

	newcookie = (struct cookie*)WS_Alloc(sp->ws, sizeof(struct cookie));
	AN(newcookie);

	strcpy(newcookie->name, name);
	strcpy(newcookie->value, value);

	VTAILQ_INSERT_TAIL(&cookielist, newcookie, list);
}

const char * vmod_get(struct sess *sp, const char *name) {
	struct cookie *cookie, *tmp;
	VTAILQ_FOREACH_SAFE(cookie, &cookielist, list, tmp) {
		if (strcmp(cookie->name, name) == 0) {
			return cookie->value;
		}
	}
	return(NULL);
}


void vmod_delete(struct sess *sp, const char *name) {
	struct cookie *cookie, *tmp;
	VTAILQ_FOREACH_SAFE(cookie, &cookielist, list, tmp) {
		if (strcmp(cookie->name, name) == 0) {
			VTAILQ_REMOVE(&cookielist, cookie, list);
			break;
		}
	}
}

void vmod_clean(struct sess *sp) {
	struct cookie *cookie;

	while (!VTAILQ_EMPTY(&cookielist)) {
		cookie = VTAILQ_FIRST(&cookielist);
		VTAILQ_REMOVE(&cookielist, cookie, list);
	}
}

void vmod_filter_except(struct sess *sp, const char *whitelist) {
	char cookienames[MAX_COOKIEPART][MAXCOOKIES];
	char tmpstr[MAX_COOKIESTRING];
	struct cookie *cookie, *tmp;
	char *tokptr, *saveptr;
	int i, found = 0, num_cookies = 0;

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
	VTAILQ_FOREACH_SAFE(cookie, &cookielist, list, tmp) {
		found = 0;
		for (i=0; i<num_cookies; i++) {
			if (strlen(cookie->name) == strlen(cookienames[i]) &&
					strcmp(cookienames[i], cookie->name) == 0) {
				found = 1;
				break;
			}
		}

		if (!found) {
			VTAILQ_REMOVE(&cookielist, cookie, list);
		}
	} // foreach
}


const char * vmod_get_string(struct sess *sp) {
	struct cookie *curr;
	struct vsb *output;
	unsigned v, u;
	char *p;

	u = WS_Reserve(sp->wrk->ws, 0);
	p = sp->wrk->ws->f;

	output = VSB_new_auto();
	AN(output);

	VTAILQ_FOREACH(curr, &cookielist, list) {
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

