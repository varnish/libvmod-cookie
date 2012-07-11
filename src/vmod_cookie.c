/*
Cookie VMOD for Varnish.

Simplifies handling of the Cookie request header.
Author: Lasse Karstensen <lasse@varnish-software.com>, July 2012.
*/

#include <stdlib.h>
#include <search.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#include "uthash.h"

#ifdef STANDALONE
#include <stdio.h>
#include <error.h>
#include <assert.h>
#endif 

// http://uthash.sourceforge.net/userguide.html

struct cookie {
	char name[255];
	char value[255]
        UT_hash_handle hh;
};


// global hash
struct cookie cookies = NULL;


int add(struct cookie *s) {
    HASH_ADD(cookies, name, s );
}

int del(struct cookie *s) {
    HASH_DEL(cookies, s );
    free(s);
}

int vcc_cleanup() {
    /* Delete all parsed cookies in the data structure. */
    struct cookie *current, *tmp;
    HASH_ITER(hh, cookies, current, tmp) {
        printf("deleting cookie named %s with value %s\n", current->name, current->value);
        HASH_DEL(cookies,current);
        free(current);
    }
}

int parse(char *cookieheader) { 
    char *token, *tokendata, *subtokendata, *key, *value, *sepindex;
    char *tokstate;
    char cookies[4096];
    char tokenitem[4096];

    if (cookieheader == NULL) return(255);
    else if (strlen(cookieheader) == 0) return(255);

//    printf("Input is: \"\"\"%s\"\"\"\n", cookieheader);

    tokendata = cookies;
    strcpy(tokendata, cookieheader); // strtok modifies source, fewer surprises.

    while (1) {
	token = strtok_r(tokendata, ";", &tokstate);
	tokendata = NULL; // strtok() wants NULL on subsequent calls.
	if (token == NULL) break;
	if (token[0] == ' ') token++;

	strcpy(tokenitem, token);
	sepindex = strchr(tokenitem, '='); 
	value = sepindex + 1;

	*sepindex = '\0';
	key = tokenitem;

	//printf("parsed token; key:%s value:%s\n", key, value);
    }
    return(0);
}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
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
#include <stdio.h>
void main(int argc, char* argv[]) {
    char* input = "cookie1=foo;cookie2=bar;cookie3=qux;";
    // char input[] = "__utma=21840418.36054577.1327330061.1333353182.1333370469.20; __utmb=21840418.490.10.1333370469; __utmc=21840418; __utmz=21840418.1327330061.1.1.utmcsr=google|utmccn=(organic)|utmcmd=organic|utmctr=foo";
    char empty[] = "";
    parse(NULL);
    parse(empty);
    parse(input); //, &output);
    char *foo = get_string("cookie1");
    printf("Response is: %s\n", foo);
    printf("Returned input is: %s\n", input);
//    printf("%s\n", res);
}
#endif 
