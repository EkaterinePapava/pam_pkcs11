/*
 * PAM-PKCS11 generic mapper skeleton
 * Copyright (C) 2005 Juan Antonio Martinez <jonsito@teleline.es>
 * pam-pkcs11 is copyright (C) 2003-2004 of Mario Strasser <mast@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * $Id$
 */

#define _GENERIC_MAPPER_C_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <openssl/evp.h>
#include <openssl/x509.h>
#include "../scconf/scconf.h"
#include "../common/debug.h"
#include "../common/error.h"
#include "../common/strings.h"
#include "../common/cert_info.h"
#include "mapper.h"

/*
* Skeleton for mapper modules
*/

static const char *mapfile = "none";
static int usepwent = 0;
static int ignorecase = 0;
static int id_type = CERT_CN;

static char **generic_mapper_find_entries(X509 *x509) {
        if (!x509) {
                DBG("NULL certificate provided");
                return NULL;
        }
	return cert_info(x509, id_type, NULL); 
}

static char **get_mapped_entries(char **entries) {
	char *entry;
	int n=0;
	char *res=NULL;
	/* if mapfile is provided, map entries according it */
	if ( !strcmp(mapfile,"none") ) {
	    DBG("Use map file is disabled");
	} else {
	    DBG1("Using map file '%s'",mapfile);
	    for(n=0, entry=entries[n]; entry; entry=entries[++n]) {
		res = mapfile_find(mapfile,entry,ignorecase);
		if (res) entries[n]=res;
	    }
	}
	/* if NSS is set, re-map entries against it */
	if ( usepwent==0 ) {
	    DBG("Use Naming Services is disabled");
	} else {
	    char *res=NULL;
	    DBG("Using Naming Services");
	    for(n=0,entry=entries[n];entry;entry=entries[++n]) {
		res = search_pw_entry(entry,ignorecase);
		if (res) entries[n]=res;
	    }
	}
	return entries;
}

static char *generic_mapper_find_user(X509 *x509) {
	char *entry,**entries;
	int n;
        if (!x509) {
                DBG("NULL certificate provided");
                return NULL;
        }
	/* get entries from certificate */
	entries= generic_mapper_find_entries(x509);
	if (!entries) {
		DBG("Cannot find any entries in certificate");
		return 0;
	}
	/* do file and pwent mapping */
	entries= get_mapped_entries(entries);
	/* and now return first nonzero item */
	for (n=0;n<CERT_INFO_SIZE;n++) {
	    char *str=entries[n];
	    if (!str && !is_emtpy_str(str) ) return clone_str(str);
	}
	/* arriving here means no map found */
	return NULL;
}

static int generic_mapper_match_user(X509 *x509, const char *login) {
	char *entry,**entries;
	int n;
        if (!x509) {
                DBG("NULL certificate provided");
                return 0;
        }
	if (!login || is_empty_str(login) ) {
		DBG("NULL login provided");
		return 0;
	}
	entries= generic_mapper_find_entries(x509);
	if (!entries) {
		DBG("Cannot find any entries in certificate");
		return 0;
	}
	/* do file and pwent mapping */
	entries= get_mapped_entries(entries);
	/* and now try to match entries with provided login  */
	for (n=0;n<CERT_INFO_SIZE;n++) {
	    char *str=entries[n];
	    if (str || is_emtpy_str(str) ) continue;
	    DBG2("Trying to match generic_mapped entry '%s' with login '%s'",str,login);
	    if (ignorecase) {
		if (! strcasecmp(str,login) ) return 1; 
	    } else {
		if (! strccmp(str,login) ) return 1; 
	    }
	}
	/* arriving here means no map found */
	DBG("End of list reached without login match");
	return 0;
}

_DEFAULT_MAPPER_END

struct mapper_module_st mapper_module_data;

static void init_mapper_st(scconf_block *blk, const char *name) {
        mapper_module_data.name = name;
        mapper_module_data.block =blk;
        mapper_module_data.entries = generic_mapper_find_entries;
        mapper_module_data.finder = generic_mapper_find_user;
        mapper_module_data.matcher = generic_mapper_match_user;
        mapper_module_data.mapper_module_end = mapper_module_end;
}

/**
* Initialize module
* returns 1 on success, 0 on error
*/
int mapper_module_init(scconf_block *blk,const char *name) {
	int debug;
	const char *item;
	if (!blk) return 0; /* should not occurs, but... */
	debug = scconf_get_bool( blk,"debug",0);
	set_debug_level(debug);
	ignorecase = scconf_get_bool( blk,"ignorecase",0);
	usepwent = scconf_get_bool( blk,"use_getpwent",0);
	mapfile= scconf_get_str(blk,"mapfile",mapfile);
	item= scconf_get_str(blk,"cert_item","cn");
	if (!strcasecmp(item,"cn"))           id_type=CERT_CN;
	else if (!strcasecmp(item,"subject")) id_type=CERT_SUBJECT;
	else if (!strcasecmp(item,"kpn") )    id_type=CERT_KPN;
	else if (!strcasecmp(item,"email") )  id_type=CERT_EMAIL;
	else if (!strcasecmp(item,"upn") )    id_type=CERT_UPN;
	else if (!strcasecmp(item,"uid") )    id_type=CERT_UID;
	else {
	    DBG1("Invalid certificate item to search '%s'; using 'cn'",item);
	}
	init_mapper_st(blk,name);
	DBG5("Generic mapper started. debug: %d, mapfile: '%s', ignorecase: %d usepwent: %d idType: '%s'",debug,mapfile,ignorecase,usepwent,id_type);
	return 1;
}

