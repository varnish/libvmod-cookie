#include <stdlib.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

const char *
vmod_parse(struct sess *sp, const char *cookieheader)
{
	char *p;
	unsigned u, v;
	// TODO: run through cookieheader string, split on ; and 
	// make an assosiative array out of it.
	return (p);
}

const char *
vmod_get_string(struct sess *sp, const char *cookiename)
{
	char *p;
	unsigned u, v;

/*	u = WS_Reserve(sp->wrk->ws, 0); /* Reserve some work space */
	//p = sp->wrk->ws->f;		/* Front of workspace area */

/* #	v = sprintf(name, "Hello foo"); // snprintf(p, u, "Hello, %s", name);
	v++;
	if (v > u) {
		WS_Release(sp->wrk->ws, 0);
		return (NULL);
	}
	WS_Release(sp->wrk->ws, v);
	*/
	return (p);
}

#ifdef STANDALONE
void main(int argc, char* argv[]) {
    printf("ohai\n");
}
#endif 
