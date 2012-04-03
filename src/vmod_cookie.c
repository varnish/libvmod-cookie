#include <stdlib.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#ifdef STANDALONE
#include <stdio.h>
#include <error.h>
#include <assert.h>
#endif 

enum VAR_TYPE {
	STRING,
	INT
};

struct cookie {
	char *name;
	enum VAR_TYPE type;
	union {
		char *name;
		char *oter;
	} value;
    	VTAILQ_ENTRY(cookie) list;
};

struct cookie_head {
        unsigned magic;
#define VMOD_COOKIE_MAGIC 0x1A2E92DE
        unsigned xid;
        VTAILQ_HEAD(, cookie) cookies;
};




int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

// int init_list(add_cookie(COOKIE_ENTRY* start, char* name, char* value) {
int add_cookie(COOKIE_ENTRY* start, char* name, char* value) {
    COOKIE_ENTRY *curr = start;
    while (curr->next != NULL) curr = curr->next;
    printf("tip is: %i", curr->value);
    return(0);
}

int listtest(char* cookieheader) { 
    COOKIE_ENTRY *start = NULL;
    COOKIE_ENTRY *tip = NULL;
    int i = 0;

    start = malloc(sizeof(COOKIE_ENTRY));
    if (!start) perror("malloc");
    start->value = 10;
    start->next = NULL;
    tip = start;

    for (i=1; i<10; i++) {
        COOKIE_ENTRY *new;
        new = malloc(sizeof(COOKIE_ENTRY));
	if (!new) perror("malloc");

	// configure the new entry
	new->value = 10 + i;
	new->next = NULL;

	// hook the new intry into the end of the list.
	// assert(tip->next == NULL);
	printf("%d\n", tip->value);
	tip->next = new;
	tip = new;
    }
    
    COOKIE_ENTRY *curr = NULL;
    curr = start;
    while (curr) {
	printf("%d\n", curr->value);
	curr = curr->next;
    }
    return(1);
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
    // char* input = "cookie1=foo;cookie2=bar;cookie3=qux;";
    char input[] = "__utma=21840418.36054577.1327330061.1333353182.1333370469.20; __utmb=21840418.490.10.1333370469; __utmc=21840418; __utmz=21840418.1327330061.1.1.utmcsr=google|utmccn=(organic)|utmcmd=organic|utmctr=foo";
    char empty[] = "";
    parse(NULL);
    parse(empty);
    parse(input); //, &output);
    printf("Returned input is: %s\n", input);
//    printf("%s\n", res);
}
#endif 
