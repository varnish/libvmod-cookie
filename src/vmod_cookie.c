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
#include <stdio.h>

#ifdef STANDALONE
#include <error.h>
#include <assert.h>
#endif 

#define MAX_COOKIEPART 1024 // name or value maxsize
#define MAX_COOKIESTRING 8196 // cookie string maxlength

struct cookie {
	char name[MAX_COOKIEPART];
	char value[MAX_COOKIEPART];
        UT_hash_handle hh;
};


// global hash. http://uthash.sourceforge.net/userguide.html
struct cookie *cookies = NULL;

void hash_set(struct cookie *s) {
    struct cookie *check = NULL;

    // check if the key already exists
    HASH_FIND_STR(cookies, s->name, check);
    if (check != NULL) {
#if STANDALONE
        printf("Changing cookie \"%s\" from \"%s\" to \"%s\".\n", s->name, check->value, s->value);
#endif
        HASH_DEL(cookies, check);
        free(check);
    }

    HASH_ADD_STR(cookies, name, s);
}

struct cookie *hash_get_by_name(char cookiename[MAX_COOKIESTRING]) {
    struct cookie *s;
    HASH_FIND_STR( cookies, cookiename, s);
    return s;
}

void hash_del_by_name(char cookiename[MAX_COOKIESTRING]) {
    struct cookie *curr = NULL;
    HASH_FIND_STR(cookies, cookiename, curr);
    if (curr != NULL) {
#if STANDALONE
        printf("deleting %s\n", curr->name);
#endif
        HASH_DEL(cookies, curr);
        // XXX: why does free()ing this pointer give me sigabrt?
        //free(curr);
    }

}

int hash_cleanup() {
    /* Delete all parsed cookies in the data structure. */
    struct cookie *current, *tmp;
    HASH_ITER(hh, cookies, current, tmp) {
#ifdef STANDALONE
        printf("deleting cookie named %s with value %s\n", current->name, current->value);
#endif
        HASH_DEL(cookies,current);
        free(current);
    }
    return(0);
}

int parse(char *cookieheader) { 
    char *token, *tokendata, *subtokendata, *key, *value, *sepindex;
    char *tokstate;
    char cookies[4096];
    char tokenitem[4096];

    struct cookie *new;

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

        new = malloc(sizeof(struct cookie));
        if (new == NULL) return(255);
        strcpy(new->name, key);
        strcpy(new->value, value);
        hash_set(new);
#if STANDALONE
	printf("parsed token; key:%s value:%s\n", key, value);
#endif
    }
    return(0);
}

int get_string(char *cookiestring) {
    struct cookie *s;
    char *stringpointer = cookiestring;
    char cookieitem[MAX_COOKIESTRING];
    int curlen = 0;
    for (s=cookies; s != NULL; s=s->hh.next) {
        sprintf(cookieitem, "%s:%s; ", s->name, s->value);
        // XXX: verify error handling
        curlen += (int)strlen(cookieitem);
        if (curlen >= MAX_COOKIESTRING) {
            cookiestring = '\0'; // zero out so that we're sure to notice the error ;)
            return(255);
        }
        strcat(stringpointer, cookieitem);
        memset(cookieitem, '\0', sizeof(cookieitem));
    }
    return(0);
}

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
    return (0);
}

void vmod_parse(struct sess *sp, const char *cookieheader) {
    parse((char *)cookieheader);
}

void vmod_set(struct sess *sp, const char *name, const char *value) {
    struct cookie *new = NULL;
    new = malloc(sizeof(struct cookie));
    if (new != NULL) {
        strcpy(new->name, name);
        strcpy(new->value, value);
        hash_set(new);
    }
}

void vmod_delete(struct sess *sp, const char *name) {
    hash_del_by_name((char *)name);
}

void vmod_clean(struct sess *sp) {
    hash_cleanup();
}

const char * vmod_get_string(struct sess *sp) {
    char *p;
    unsigned u, v;

    char cookiestring[MAX_COOKIESTRING];
//    cookiestring = malloc(MAX_COOKIESTRING);
    get_string((char *)&cookiestring);

    u = WS_Reserve(sp->wrk->ws, 0);
    p = sp->wrk->ws->f;	

    v = snprintf(p, u, "%s", cookiestring);
 //   free(cookiestring);
    v++;
    if (v > u) {
            WS_Release(sp->wrk->ws, 0);
            return (NULL);
    }
    WS_Release(sp->wrk->ws, v);
    return (p);
}

#ifdef STANDALONE
void test_parse() {
//    char* input = "cookie1=parsed1;cookie2=parsed2;cookie3=parsed3;";
    char *input = "__utma=21840418.36054577.1327330061.1333353182.1333370469.20; __utmb=21840418.490.10.1333370469; __utmc=21840418; __utmz=21840418.1327330061.1.1.utmcsr=google|utmccn=(organic)|utmcmd=organic|utmctr=foo";
    char empty[] = "";
    parse(NULL);
    parse(empty);
    parse(input);
    char *cookiestring;
    cookiestring = malloc(MAX_COOKIESTRING);
    get_string(cookiestring);
    printf("serialised cookie string is: %s\n", cookiestring);
}

void test_simple() {

    struct cookie cookie1 = { .name = "cookie1", .value = "setvalue1" };
    struct cookie cookie2 = { .name = "cookie2", .value = "setvalue2" };
    struct cookie *s = NULL;
    hash_set(&cookie1);
    hash_set(&cookie2);

    char *foo = malloc(MAX_COOKIESTRING); 
    memset(foo, '\0', sizeof(foo)); get_string(foo); printf("%s\n", foo);

    s = hash_get_by_name("cookie1");
    if (s == NULL) { printf("BAD: Cookie not found\n"); } 
    else { printf("OK: Found cookie %s with value %s\n", s->name, s->value); }
    s = NULL;

    memset(foo, '\0', sizeof(foo)); get_string(foo); printf("%s\n", foo);

    hash_del_by_name("does-not-exist");

    memset(foo, '\0', sizeof(foo)); get_string(foo); printf("%s\n", foo);

    hash_del_by_name("cookie1");
    memset(foo, '\0', sizeof(foo)); get_string(foo); printf("%s\n", foo);

    s = hash_get_by_name("cookie1");
    if (s == NULL) { printf("OK: Cookie not found as expected\n"); } 
    else { printf("BAD: Found cookie %s with value %s\n", s->name, s->value); }
}

int main(int argc, char* argv[]) {
    test_parse();
    test_simple();
    return(0);
}

#endif 
