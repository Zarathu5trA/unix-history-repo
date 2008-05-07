/*
 * Copyright (c) 1997-2007 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "kdc_locl.h"

RCSID("$Id: krb5tgs.c 22071 2007-11-14 20:04:50Z lha $");

/*
 * return the realm of a krbtgt-ticket or NULL
 */

static Realm 
get_krbtgt_realm(const PrincipalName *p)
{
    if(p->name_string.len == 2
       && strcmp(p->name_string.val[0], KRB5_TGS_NAME) == 0)
	return p->name_string.val[1];
    else
	return NULL;
}

/*
 * The KDC might add a signed path to the ticket authorization data
 * field. This is to avoid server impersonating clients and the
 * request constrained delegation.
 *
 * This is done by storing a KRB5_AUTHDATA_IF_RELEVANT with a single
 * entry of type KRB5SignedPath.
 */

static krb5_error_code
find_KRB5SignedPath(krb5_context context,
		    const AuthorizationData *ad,
		    krb5_data *data)
{
    AuthorizationData child;
    krb5_error_code ret;
    int pos;
	
    if (ad == NULL || ad->len == 0)
	return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;

    pos = ad->len - 1;

    if (ad->val[pos].ad_type != KRB5_AUTHDATA_IF_RELEVANT)
	return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;

    ret = decode_AuthorizationData(ad->val[pos].ad_data.data,
				   ad->val[pos].ad_data.length,
				   &child,
				   NULL);
    if (ret) {
	krb5_set_error_string(context, "Failed to decode "
			      "IF_RELEVANT with %d", ret);
	return ret;
    }

    if (child.len != 1) {
	free_AuthorizationData(&child);
	return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
    }

    if (child.val[0].ad_type != KRB5_AUTHDATA_SIGNTICKET) {
	free_AuthorizationData(&child);
	return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
    }

    if (data)
	ret = der_copy_octet_string(&child.val[0].ad_data, data);
    free_AuthorizationData(&child);
    return ret;
}

krb5_error_code
_kdc_add_KRB5SignedPath(krb5_context context,
			krb5_kdc_configuration *config,
			hdb_entry_ex *krbtgt,
			krb5_enctype enctype,
			krb5_const_principal server,
			KRB5SignedPathPrincipals *principals,
			EncTicketPart *tkt)
{
    krb5_error_code ret;
    KRB5SignedPath sp;
    krb5_data data;
    krb5_crypto crypto = NULL;
    size_t size;

    if (server && principals) {
	ret = add_KRB5SignedPathPrincipals(principals, server);
	if (ret)
	    return ret;
    }

    {
	KRB5SignedPathData spd;
	
	spd.encticket = *tkt;
	spd.delegated = principals;
	
	ASN1_MALLOC_ENCODE(KRB5SignedPathData, data.data, data.length,
			   &spd, &size, ret);
	if (ret)
	    return ret;
	if (data.length != size)
	    krb5_abortx(context, "internal asn.1 encoder error");
    }

    {
	Key *key;
	ret = hdb_enctype2key(context, &krbtgt->entry, enctype, &key);
	if (ret == 0)
	    ret = krb5_crypto_init(context, &key->key, 0, &crypto);
	if (ret) {
	    free(data.data);
	    return ret;
	}
    }

    /*
     * Fill in KRB5SignedPath
     */

    sp.etype = enctype;
    sp.delegated = principals;

    ret = krb5_create_checksum(context, crypto, KRB5_KU_KRB5SIGNEDPATH, 0,
			       data.data, data.length, &sp.cksum);
    krb5_crypto_destroy(context, crypto);
    free(data.data);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(KRB5SignedPath, data.data, data.length, &sp, &size, ret);
    free_Checksum(&sp.cksum);
    if (ret)
	return ret;
    if (data.length != size)
	krb5_abortx(context, "internal asn.1 encoder error");

    
    /*
     * Add IF-RELEVANT(KRB5SignedPath) to the last slot in
     * authorization data field.
     */

    ret = _kdc_tkt_add_if_relevant_ad(context, tkt,
				      KRB5_AUTHDATA_SIGNTICKET, &data);
    krb5_data_free(&data);

    return ret;
}

static krb5_error_code
check_KRB5SignedPath(krb5_context context,
		     krb5_kdc_configuration *config,
		     hdb_entry_ex *krbtgt,
		     EncTicketPart *tkt,
		     KRB5SignedPathPrincipals **delegated,
		     int require_signedpath)
{
    krb5_error_code ret;
    krb5_data data;
    krb5_crypto crypto = NULL;

    *delegated = NULL;

    ret = find_KRB5SignedPath(context, tkt->authorization_data, &data);
    if (ret == 0) {
	KRB5SignedPathData spd;
	KRB5SignedPath sp;
	AuthorizationData *ad;
	size_t size;

	ret = decode_KRB5SignedPath(data.data, data.length, &sp, NULL);
	krb5_data_free(&data);
	if (ret)
	    return ret;

	spd.encticket = *tkt;
	/* the KRB5SignedPath is the last entry */
	ad = spd.encticket.authorization_data;
	if (--ad->len == 0)
	    spd.encticket.authorization_data = NULL;
	spd.delegated = sp.delegated;

	ASN1_MALLOC_ENCODE(KRB5SignedPathData, data.data, data.length,
			   &spd, &size, ret);
	ad->len++;
	spd.encticket.authorization_data = ad;
	if (ret) {
	    free_KRB5SignedPath(&sp);
	    return ret;
	}
	if (data.length != size)
	    krb5_abortx(context, "internal asn.1 encoder error");

	{
	    Key *key;
	    ret = hdb_enctype2key(context, &krbtgt->entry, sp.etype, &key);
	    if (ret == 0)
		ret = krb5_crypto_init(context, &key->key, 0, &crypto);
	    if (ret) {
		free(data.data);
		free_KRB5SignedPath(&sp);
		return ret;
	    }
	}
	ret = krb5_verify_checksum(context, crypto, KRB5_KU_KRB5SIGNEDPATH, 
				   data.data, data.length, 
				   &sp.cksum);
	krb5_crypto_destroy(context, crypto);
	free(data.data);
	if (ret) {
	    free_KRB5SignedPath(&sp);
	    return ret;
	}

	if (sp.delegated) {

	    *delegated = malloc(sizeof(*sp.delegated));
	    if (*delegated == NULL) {
		free_KRB5SignedPath(&sp);
		return ENOMEM;
	    }

	    ret = copy_KRB5SignedPathPrincipals(*delegated, sp.delegated);
	    if (ret) {
		free_KRB5SignedPath(&sp);
		free(*delegated);
		*delegated = NULL;
		return ret;
	    }
	}
	free_KRB5SignedPath(&sp);
	
    } else {
	if (require_signedpath)
	    return KRB5KDC_ERR_BADOPTION;
    }

    return 0;
}

/*
 *
 */

static krb5_error_code
check_PAC(krb5_context context,
	  krb5_kdc_configuration *config,
	  const krb5_principal client_principal,
	  hdb_entry_ex *client,
	  hdb_entry_ex *server,
	  const EncryptionKey *server_key,
	  const EncryptionKey *krbtgt_key,
	  EncTicketPart *tkt,
	  krb5_data *rspac,
	  int *require_signedpath)
{
    AuthorizationData *ad = tkt->authorization_data;
    unsigned i, j;
    krb5_error_code ret;

    if (ad == NULL || ad->len == 0)
	return 0;

    for (i = 0; i < ad->len; i++) {
	AuthorizationData child;

	if (ad->val[i].ad_type != KRB5_AUTHDATA_IF_RELEVANT)
	    continue;

	ret = decode_AuthorizationData(ad->val[i].ad_data.data,
				       ad->val[i].ad_data.length,
				       &child,
				       NULL);
	if (ret) {
	    krb5_set_error_string(context, "Failed to decode "
				  "IF_RELEVANT with %d", ret);
	    return ret;
	}
	for (j = 0; j < child.len; j++) {

	    if (child.val[j].ad_type == KRB5_AUTHDATA_WIN2K_PAC) {
		krb5_pac pac;

		/* Found PAC */
		ret = krb5_pac_parse(context,
				     child.val[j].ad_data.data,
				     child.val[j].ad_data.length,
				     &pac);
		free_AuthorizationData(&child);
		if (ret)
		    return ret;

		ret = krb5_pac_verify(context, pac, tkt->authtime, 
				      client_principal,
				      krbtgt_key, NULL);
		if (ret) {
		    krb5_pac_free(context, pac);
		    return ret;
		}

		ret = _kdc_pac_verify(context, client_principal, 
				      client, server, &pac);
		if (ret) {
		    krb5_pac_free(context, pac);
		    return ret;
		}
		*require_signedpath = 0;

		ret = _krb5_pac_sign(context, pac, tkt->authtime,
				     client_principal,
				     server_key, krbtgt_key, rspac);

		krb5_pac_free(context, pac);

		return ret;
	    }
	}
	free_AuthorizationData(&child);
    }
    return 0;
}

/*
 *
 */

static krb5_error_code
check_tgs_flags(krb5_context context,        
		krb5_kdc_configuration *config,
		KDC_REQ_BODY *b, const EncTicketPart *tgt, EncTicketPart *et)
{
    KDCOptions f = b->kdc_options;
	
    if(f.validate){
	if(!tgt->flags.invalid || tgt->starttime == NULL){
	    kdc_log(context, config, 0,
		    "Bad request to validate ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	if(*tgt->starttime > kdc_time){
	    kdc_log(context, config, 0,
		    "Early request to validate ticket");
	    return KRB5KRB_AP_ERR_TKT_NYV;
	}
	/* XXX  tkt = tgt */
	et->flags.invalid = 0;
    }else if(tgt->flags.invalid){
	kdc_log(context, config, 0, 
		"Ticket-granting ticket has INVALID flag set");
	return KRB5KRB_AP_ERR_TKT_INVALID;
    }

    if(f.forwardable){
	if(!tgt->flags.forwardable){
	    kdc_log(context, config, 0,
		    "Bad request for forwardable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.forwardable = 1;
    }
    if(f.forwarded){
	if(!tgt->flags.forwardable){
	    kdc_log(context, config, 0,
		    "Request to forward non-forwardable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.forwarded = 1;
	et->caddr = b->addresses;
    }
    if(tgt->flags.forwarded)
	et->flags.forwarded = 1;
	
    if(f.proxiable){
	if(!tgt->flags.proxiable){
	    kdc_log(context, config, 0,
		    "Bad request for proxiable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.proxiable = 1;
    }
    if(f.proxy){
	if(!tgt->flags.proxiable){
	    kdc_log(context, config, 0,
		    "Request to proxy non-proxiable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.proxy = 1;
	et->caddr = b->addresses;
    }
    if(tgt->flags.proxy)
	et->flags.proxy = 1;

    if(f.allow_postdate){
	if(!tgt->flags.may_postdate){
	    kdc_log(context, config, 0,
		    "Bad request for post-datable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.may_postdate = 1;
    }
    if(f.postdated){
	if(!tgt->flags.may_postdate){
	    kdc_log(context, config, 0,
		    "Bad request for postdated ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	if(b->from)
	    *et->starttime = *b->from;
	et->flags.postdated = 1;
	et->flags.invalid = 1;
    }else if(b->from && *b->from > kdc_time + context->max_skew){
	kdc_log(context, config, 0, "Ticket cannot be postdated");
	return KRB5KDC_ERR_CANNOT_POSTDATE;
    }

    if(f.renewable){
	if(!tgt->flags.renewable){
	    kdc_log(context, config, 0,
		    "Bad request for renewable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.renewable = 1;
	ALLOC(et->renew_till);
	_kdc_fix_time(&b->rtime);
	*et->renew_till = *b->rtime;
    }
    if(f.renew){
	time_t old_life;
	if(!tgt->flags.renewable || tgt->renew_till == NULL){
	    kdc_log(context, config, 0,
		    "Request to renew non-renewable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	old_life = tgt->endtime;
	if(tgt->starttime)
	    old_life -= *tgt->starttime;
	else
	    old_life -= tgt->authtime;
	et->endtime = *et->starttime + old_life;
	if (et->renew_till != NULL)
	    et->endtime = min(*et->renew_till, et->endtime);
    }	    
    
#if 0
    /* checks for excess flags */
    if(f.request_anonymous && !config->allow_anonymous){
	kdc_log(context, config, 0,
		"Request for anonymous ticket");
	return KRB5KDC_ERR_BADOPTION;
    }
#endif
    return 0;
}

/*
 *
 */

static krb5_error_code
check_constrained_delegation(krb5_context context, 
			     krb5_kdc_configuration *config,
			     hdb_entry_ex *client,
			     krb5_const_principal server)
{
    const HDB_Ext_Constrained_delegation_acl *acl;
    krb5_error_code ret;
    int i;

    ret = hdb_entry_get_ConstrainedDelegACL(&client->entry, &acl);
    if (ret) {
	krb5_clear_error_string(context);
	return ret;
    }

    if (acl) {
	for (i = 0; i < acl->len; i++) {
	    if (krb5_principal_compare(context, server, &acl->val[i]) == TRUE)
		return 0;
	}
    }
    kdc_log(context, config, 0,
	    "Bad request for constrained delegation");
    return KRB5KDC_ERR_BADOPTION;
}

/*
 *
 */

static krb5_error_code
verify_flags (krb5_context context, 
	      krb5_kdc_configuration *config,
	      const EncTicketPart *et,
	      const char *pstr)
{
    if(et->endtime < kdc_time){
	kdc_log(context, config, 0, "Ticket expired (%s)", pstr);
	return KRB5KRB_AP_ERR_TKT_EXPIRED;
    }
    if(et->flags.invalid){
	kdc_log(context, config, 0, "Ticket not valid (%s)", pstr);
	return KRB5KRB_AP_ERR_TKT_NYV;
    }
    return 0;
}

/*
 *
 */

static krb5_error_code
fix_transited_encoding(krb5_context context, 
		       krb5_kdc_configuration *config,
		       krb5_boolean check_policy,
		       const TransitedEncoding *tr, 
		       EncTicketPart *et, 
		       const char *client_realm, 
		       const char *server_realm, 
		       const char *tgt_realm)
{
    krb5_error_code ret = 0;
    char **realms, **tmp;
    int num_realms;
    int i;

    switch (tr->tr_type) {
    case DOMAIN_X500_COMPRESS:
	break;
    case 0:
	/*
	 * Allow empty content of type 0 because that is was Microsoft
	 * generates in their TGT.
	 */
	if (tr->contents.length == 0)
	    break;
	kdc_log(context, config, 0,
		"Transited type 0 with non empty content");
	return KRB5KDC_ERR_TRTYPE_NOSUPP;
    default:
	kdc_log(context, config, 0,
		"Unknown transited type: %u", tr->tr_type);
	return KRB5KDC_ERR_TRTYPE_NOSUPP;
    }

    ret = krb5_domain_x500_decode(context, 
				  tr->contents,
				  &realms, 
				  &num_realms,
				  client_realm,
				  server_realm);
    if(ret){
	krb5_warn(context, ret,
		  "Decoding transited encoding");
	return ret;
    }
    if(strcmp(client_realm, tgt_realm) && strcmp(server_realm, tgt_realm)) {
	/* not us, so add the previous realm to transited set */
	if (num_realms < 0 || num_realms + 1 > UINT_MAX/sizeof(*realms)) {
	    ret = ERANGE;
	    goto free_realms;
	}
	tmp = realloc(realms, (num_realms + 1) * sizeof(*realms));
	if(tmp == NULL){
	    ret = ENOMEM;
	    goto free_realms;
	}
	realms = tmp;
	realms[num_realms] = strdup(tgt_realm);
	if(realms[num_realms] == NULL){
	    ret = ENOMEM;
	    goto free_realms;
	}
	num_realms++;
    }
    if(num_realms == 0) {
	if(strcmp(client_realm, server_realm)) 
	    kdc_log(context, config, 0,
		    "cross-realm %s -> %s", client_realm, server_realm);
    } else {
	size_t l = 0;
	char *rs;
	for(i = 0; i < num_realms; i++)
	    l += strlen(realms[i]) + 2;
	rs = malloc(l);
	if(rs != NULL) {
	    *rs = '\0';
	    for(i = 0; i < num_realms; i++) {
		if(i > 0)
		    strlcat(rs, ", ", l);
		strlcat(rs, realms[i], l);
	    }
	    kdc_log(context, config, 0,
		    "cross-realm %s -> %s via [%s]",
		    client_realm, server_realm, rs);
	    free(rs);
	}
    }
    if(check_policy) {
	ret = krb5_check_transited(context, client_realm, 
				   server_realm, 
				   realms, num_realms, NULL);
	if(ret) {
	    krb5_warn(context, ret, "cross-realm %s -> %s", 
		      client_realm, server_realm);
	    goto free_realms;
	}
	et->flags.transited_policy_checked = 1;
    }
    et->transited.tr_type = DOMAIN_X500_COMPRESS;
    ret = krb5_domain_x500_encode(realms, num_realms, &et->transited.contents);
    if(ret)
	krb5_warn(context, ret, "Encoding transited encoding");
  free_realms:
    for(i = 0; i < num_realms; i++)
	free(realms[i]);
    free(realms);
    return ret;
}


static krb5_error_code
tgs_make_reply(krb5_context context, 
	       krb5_kdc_configuration *config,
	       KDC_REQ_BODY *b, 
	       krb5_const_principal tgt_name,
	       const EncTicketPart *tgt, 
	       const EncryptionKey *serverkey,
	       const krb5_keyblock *sessionkey,
	       krb5_kvno kvno,
	       AuthorizationData *auth_data,
	       hdb_entry_ex *server, 
	       const char *server_name, 
	       hdb_entry_ex *client, 
	       krb5_principal client_principal, 
	       hdb_entry_ex *krbtgt,
	       krb5_enctype krbtgt_etype,
	       KRB5SignedPathPrincipals *spp,
	       const krb5_data *rspac,
	       const char **e_text,
	       krb5_data *reply)
{
    KDC_REP rep;
    EncKDCRepPart ek;
    EncTicketPart et;
    KDCOptions f = b->kdc_options;
    krb5_error_code ret;
    
    memset(&rep, 0, sizeof(rep));
    memset(&et, 0, sizeof(et));
    memset(&ek, 0, sizeof(ek));
    
    rep.pvno = 5;
    rep.msg_type = krb_tgs_rep;

    et.authtime = tgt->authtime;
    _kdc_fix_time(&b->till);
    et.endtime = min(tgt->endtime, *b->till);
    ALLOC(et.starttime);
    *et.starttime = kdc_time;
    
    ret = check_tgs_flags(context, config, b, tgt, &et);
    if(ret)
	goto out;

    /* We should check the transited encoding if:
       1) the request doesn't ask not to be checked
       2) globally enforcing a check
       3) principal requires checking
       4) we allow non-check per-principal, but principal isn't marked as allowing this
       5) we don't globally allow this
    */

#define GLOBAL_FORCE_TRANSITED_CHECK		\
    (config->trpolicy == TRPOLICY_ALWAYS_CHECK)
#define GLOBAL_ALLOW_PER_PRINCIPAL			\
    (config->trpolicy == TRPOLICY_ALLOW_PER_PRINCIPAL)
#define GLOBAL_ALLOW_DISABLE_TRANSITED_CHECK			\
    (config->trpolicy == TRPOLICY_ALWAYS_HONOUR_REQUEST)

/* these will consult the database in future release */
#define PRINCIPAL_FORCE_TRANSITED_CHECK(P)		0
#define PRINCIPAL_ALLOW_DISABLE_TRANSITED_CHECK(P)	0

    ret = fix_transited_encoding(context, config, 
				 !f.disable_transited_check ||
				 GLOBAL_FORCE_TRANSITED_CHECK ||
				 PRINCIPAL_FORCE_TRANSITED_CHECK(server) ||
				 !((GLOBAL_ALLOW_PER_PRINCIPAL && 
				    PRINCIPAL_ALLOW_DISABLE_TRANSITED_CHECK(server)) ||
				   GLOBAL_ALLOW_DISABLE_TRANSITED_CHECK),
				 &tgt->transited, &et,
				 *krb5_princ_realm(context, client_principal),
				 *krb5_princ_realm(context, server->entry.principal),
				 *krb5_princ_realm(context, krbtgt->entry.principal));
    if(ret)
	goto out;

    copy_Realm(krb5_princ_realm(context, server->entry.principal), 
	       &rep.ticket.realm);
    _krb5_principal2principalname(&rep.ticket.sname, server->entry.principal);
    copy_Realm(&tgt_name->realm, &rep.crealm);
/*
    if (f.request_anonymous)
	_kdc_make_anonymous_principalname (&rep.cname);
    else */

    copy_PrincipalName(&tgt_name->name, &rep.cname);
    rep.ticket.tkt_vno = 5;

    ek.caddr = et.caddr;
    if(et.caddr == NULL)
	et.caddr = tgt->caddr;

    {
	time_t life;
	life = et.endtime - *et.starttime;
	if(client && client->entry.max_life)
	    life = min(life, *client->entry.max_life);
	if(server->entry.max_life)
	    life = min(life, *server->entry.max_life);
	et.endtime = *et.starttime + life;
    }
    if(f.renewable_ok && tgt->flags.renewable && 
       et.renew_till == NULL && et.endtime < *b->till){
	et.flags.renewable = 1;
	ALLOC(et.renew_till);
	*et.renew_till = *b->till;
    }
    if(et.renew_till){
	time_t renew;
	renew = *et.renew_till - et.authtime;
	if(client && client->entry.max_renew)
	    renew = min(renew, *client->entry.max_renew);
	if(server->entry.max_renew)
	    renew = min(renew, *server->entry.max_renew);
	*et.renew_till = et.authtime + renew;
    }
	    
    if(et.renew_till){
	*et.renew_till = min(*et.renew_till, *tgt->renew_till);
	*et.starttime = min(*et.starttime, *et.renew_till);
	et.endtime = min(et.endtime, *et.renew_till);
    }
    
    *et.starttime = min(*et.starttime, et.endtime);

    if(*et.starttime == et.endtime){
	ret = KRB5KDC_ERR_NEVER_VALID;
	goto out;
    }
    if(et.renew_till && et.endtime == *et.renew_till){
	free(et.renew_till);
	et.renew_till = NULL;
	et.flags.renewable = 0;
    }
    
    et.flags.pre_authent = tgt->flags.pre_authent;
    et.flags.hw_authent  = tgt->flags.hw_authent;
    et.flags.anonymous   = tgt->flags.anonymous;
    et.flags.ok_as_delegate = server->entry.flags.ok_as_delegate;
	    
    if (auth_data) {
	/* XXX Check enc-authorization-data */
	et.authorization_data = calloc(1, sizeof(*et.authorization_data));
	if (et.authorization_data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	ret = copy_AuthorizationData(auth_data, et.authorization_data);
	if (ret)
	    goto out;

	/* Filter out type KRB5SignedPath */
	ret = find_KRB5SignedPath(context, et.authorization_data, NULL);
	if (ret == 0) {
	    if (et.authorization_data->len == 1) {
		free_AuthorizationData(et.authorization_data);
		free(et.authorization_data);
		et.authorization_data = NULL;
	    } else {
		AuthorizationData *ad = et.authorization_data;
		free_AuthorizationDataElement(&ad->val[ad->len - 1]);
		ad->len--;
	    }
	}
    }

    if(rspac->length) {
	/*
	 * No not need to filter out the any PAC from the
	 * auth_data since it's signed by the KDC.
	 */
	ret = _kdc_tkt_add_if_relevant_ad(context, &et,
					  KRB5_AUTHDATA_WIN2K_PAC,
					  rspac);
	if (ret)
	    goto out;
    }

    ret = krb5_copy_keyblock_contents(context, sessionkey, &et.key);
    if (ret)
	goto out;
    et.crealm = tgt->crealm;
    et.cname = tgt_name->name;
	    
    ek.key = et.key;
    /* MIT must have at least one last_req */
    ek.last_req.len = 1;
    ek.last_req.val = calloc(1, sizeof(*ek.last_req.val));
    if (ek.last_req.val == NULL) {
	ret = ENOMEM;
	goto out;
    }
    ek.nonce = b->nonce;
    ek.flags = et.flags;
    ek.authtime = et.authtime;
    ek.starttime = et.starttime;
    ek.endtime = et.endtime;
    ek.renew_till = et.renew_till;
    ek.srealm = rep.ticket.realm;
    ek.sname = rep.ticket.sname;
    
    _kdc_log_timestamp(context, config, "TGS-REQ", et.authtime, et.starttime, 
		       et.endtime, et.renew_till);

    /* Don't sign cross realm tickets, they can't be checked anyway */
    {
	char *r = get_krbtgt_realm(&ek.sname);

	if (r == NULL || strcmp(r, ek.srealm) == 0) {
	    ret = _kdc_add_KRB5SignedPath(context,
					  config,
					  krbtgt,
					  krbtgt_etype,
					  NULL,
					  spp,
					  &et);
	    if (ret)
		goto out;
	}
    }

    /* It is somewhat unclear where the etype in the following
       encryption should come from. What we have is a session
       key in the passed tgt, and a list of preferred etypes
       *for the new ticket*. Should we pick the best possible
       etype, given the keytype in the tgt, or should we look
       at the etype list here as well?  What if the tgt
       session key is DES3 and we want a ticket with a (say)
       CAST session key. Should the DES3 etype be added to the
       etype list, even if we don't want a session key with
       DES3? */
    ret = _kdc_encode_reply(context, config, 
			    &rep, &et, &ek, et.key.keytype,
			    kvno, 
			    serverkey, 0, &tgt->key, e_text, reply);
out:
    free_TGS_REP(&rep);
    free_TransitedEncoding(&et.transited);
    if(et.starttime)
	free(et.starttime);
    if(et.renew_till)
	free(et.renew_till);
    if(et.authorization_data) {
	free_AuthorizationData(et.authorization_data);
	free(et.authorization_data);
    }
    free_LastReq(&ek.last_req);
    memset(et.key.keyvalue.data, 0, et.key.keyvalue.length);
    free_EncryptionKey(&et.key);
    return ret;
}

static krb5_error_code
tgs_check_authenticator(krb5_context context, 
			krb5_kdc_configuration *config,
	                krb5_auth_context ac,
			KDC_REQ_BODY *b, 
			const char **e_text,
			krb5_keyblock *key)
{
    krb5_authenticator auth;
    size_t len;
    unsigned char *buf;
    size_t buf_size;
    krb5_error_code ret;
    krb5_crypto crypto;
    
    krb5_auth_con_getauthenticator(context, ac, &auth);
    if(auth->cksum == NULL){
	kdc_log(context, config, 0, "No authenticator in request");
	ret = KRB5KRB_AP_ERR_INAPP_CKSUM;
	goto out;
    }
    /*
     * according to RFC1510 it doesn't need to be keyed,
     * but according to the latest draft it needs to.
     */
    if (
#if 0
!krb5_checksum_is_keyed(context, auth->cksum->cksumtype)
	||
#endif
 !krb5_checksum_is_collision_proof(context, auth->cksum->cksumtype)) {
	kdc_log(context, config, 0, "Bad checksum type in authenticator: %d", 
		auth->cksum->cksumtype);
	ret =  KRB5KRB_AP_ERR_INAPP_CKSUM;
	goto out;
    }
		
    /* XXX should not re-encode this */
    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, buf_size, b, &len, ret);
    if(ret){
	kdc_log(context, config, 0, "Failed to encode KDC-REQ-BODY: %s", 
		krb5_get_err_text(context, ret));
	goto out;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(context, config, 0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	ret = KRB5KRB_ERR_GENERIC;
	goto out;
    }
    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret) {
	free(buf);
	kdc_log(context, config, 0, "krb5_crypto_init failed: %s",
		krb5_get_err_text(context, ret));
	goto out;
    }
    ret = krb5_verify_checksum(context,
			       crypto,
			       KRB5_KU_TGS_REQ_AUTH_CKSUM,
			       buf, 
			       len,
			       auth->cksum);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if(ret){
	kdc_log(context, config, 0,
		"Failed to verify authenticator checksum: %s", 
		krb5_get_err_text(context, ret));
    }
out:
    free_Authenticator(auth);
    free(auth);
    return ret;
}

/*
 *
 */

static const char *
find_rpath(krb5_context context, Realm crealm, Realm srealm)
{
    const char *new_realm = krb5_config_get_string(context,
						   NULL,
						   "capaths", 
						   crealm,
						   srealm,
						   NULL);
    return new_realm;
}
	    

static krb5_boolean
need_referral(krb5_context context, krb5_principal server, krb5_realm **realms)
{
    if(server->name.name_type != KRB5_NT_SRV_INST ||
       server->name.name_string.len != 2)
	return FALSE;
 
    return _krb5_get_host_realm_int(context, server->name.name_string.val[1],
				    FALSE, realms) == 0;
}

static krb5_error_code
tgs_parse_request(krb5_context context, 
		  krb5_kdc_configuration *config,
		  KDC_REQ_BODY *b,
		  const PA_DATA *tgs_req,
		  hdb_entry_ex **krbtgt,
		  krb5_enctype *krbtgt_etype,
		  krb5_ticket **ticket,
		  const char **e_text,
		  const char *from,
		  const struct sockaddr *from_addr,
		  time_t **csec,
		  int **cusec,
		  AuthorizationData **auth_data)
{
    krb5_ap_req ap_req;
    krb5_error_code ret;
    krb5_principal princ;
    krb5_auth_context ac = NULL;
    krb5_flags ap_req_options;
    krb5_flags verify_ap_req_flags;
    krb5_crypto crypto;
    Key *tkey;

    *auth_data = NULL;
    *csec  = NULL;
    *cusec = NULL;

    memset(&ap_req, 0, sizeof(ap_req));
    ret = krb5_decode_ap_req(context, &tgs_req->padata_value, &ap_req);
    if(ret){
	kdc_log(context, config, 0, "Failed to decode AP-REQ: %s", 
		krb5_get_err_text(context, ret));
	goto out;
    }

    if(!get_krbtgt_realm(&ap_req.ticket.sname)){
	/* XXX check for ticket.sname == req.sname */
	kdc_log(context, config, 0, "PA-DATA is not a ticket-granting ticket");
	ret = KRB5KDC_ERR_POLICY; /* ? */
	goto out;
    }
    
    _krb5_principalname2krb5_principal(context,
				       &princ,
				       ap_req.ticket.sname,
				       ap_req.ticket.realm);
    
    ret = _kdc_db_fetch(context, config, princ, HDB_F_GET_KRBTGT, NULL, krbtgt);

    if(ret) {
	char *p;
	ret = krb5_unparse_name(context, princ, &p);
	if (ret != 0)
	    p = "<unparse_name failed>";
	krb5_free_principal(context, princ);
	kdc_log(context, config, 0,
		"Ticket-granting ticket not found in database: %s: %s",
		p, krb5_get_err_text(context, ret));
	if (ret == 0)
	    free(p);
	ret = KRB5KRB_AP_ERR_NOT_US;
	goto out;
    }
    
    if(ap_req.ticket.enc_part.kvno && 
       *ap_req.ticket.enc_part.kvno != (*krbtgt)->entry.kvno){
	char *p;

	ret = krb5_unparse_name (context, princ, &p);
	krb5_free_principal(context, princ);
	if (ret != 0)
	    p = "<unparse_name failed>";
	kdc_log(context, config, 0,
		"Ticket kvno = %d, DB kvno = %d (%s)", 
		*ap_req.ticket.enc_part.kvno,
		(*krbtgt)->entry.kvno,
		p);
	if (ret == 0)
	    free (p);
	ret = KRB5KRB_AP_ERR_BADKEYVER;
	goto out;
    }

    *krbtgt_etype = ap_req.ticket.enc_part.etype;

    ret = hdb_enctype2key(context, &(*krbtgt)->entry, 
			  ap_req.ticket.enc_part.etype, &tkey);
    if(ret){
	char *str = NULL, *p = NULL;

	krb5_enctype_to_string(context, ap_req.ticket.enc_part.etype, &str);
	krb5_unparse_name(context, princ, &p);
 	kdc_log(context, config, 0,
		"No server key with enctype %s found for %s",
		str ? str : "<unknown enctype>",
		p ? p : "<unparse_name failed>");
	free(str);
	free(p);
	ret = KRB5KRB_AP_ERR_BADKEYVER;
	goto out;
    }
    
    if (b->kdc_options.validate)
	verify_ap_req_flags = KRB5_VERIFY_AP_REQ_IGNORE_INVALID;
    else
	verify_ap_req_flags = 0;

    ret = krb5_verify_ap_req2(context,
			      &ac,
			      &ap_req,
			      princ,
			      &tkey->key,
			      verify_ap_req_flags,
			      &ap_req_options,
			      ticket,
			      KRB5_KU_TGS_REQ_AUTH);
			     
    krb5_free_principal(context, princ);
    if(ret) {
	kdc_log(context, config, 0, "Failed to verify AP-REQ: %s", 
		krb5_get_err_text(context, ret));
	goto out;
    }

    {
	krb5_authenticator auth;

	ret = krb5_auth_con_getauthenticator(context, ac, &auth);
	if (ret == 0) {
	    *csec   = malloc(sizeof(**csec));
	    if (*csec == NULL) {
		krb5_free_authenticator(context, &auth);
		kdc_log(context, config, 0, "malloc failed");
		goto out;
	    }
	    **csec  = auth->ctime;
	    *cusec  = malloc(sizeof(**cusec));
	    if (*cusec == NULL) {
		krb5_free_authenticator(context, &auth);
		kdc_log(context, config, 0, "malloc failed");
		goto out;
	    }
	    **cusec  = auth->cusec;
	    krb5_free_authenticator(context, &auth);
	}
    }

    ret = tgs_check_authenticator(context, config, 
				  ac, b, e_text, &(*ticket)->ticket.key);
    if (ret) {
	krb5_auth_con_free(context, ac);
	goto out;
    }

    if (b->enc_authorization_data) {
	unsigned usage = KRB5_KU_TGS_REQ_AUTH_DAT_SUBKEY;
	krb5_keyblock *subkey;
	krb5_data ad;

	ret = krb5_auth_con_getremotesubkey(context,
					    ac,
					    &subkey);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    kdc_log(context, config, 0, "Failed to get remote subkey: %s", 
		    krb5_get_err_text(context, ret));
	    goto out;
	}
	if(subkey == NULL){
	    usage = KRB5_KU_TGS_REQ_AUTH_DAT_SESSION;
	    ret = krb5_auth_con_getkey(context, ac, &subkey);
	    if(ret) {
		krb5_auth_con_free(context, ac);
		kdc_log(context, config, 0, "Failed to get session key: %s", 
			krb5_get_err_text(context, ret));
		goto out;
	    }
	}
	if(subkey == NULL){
	    krb5_auth_con_free(context, ac);
	    kdc_log(context, config, 0,
		    "Failed to get key for enc-authorization-data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out;
	}
	ret = krb5_crypto_init(context, subkey, 0, &crypto);
	if (ret) {
	    krb5_auth_con_free(context, ac);
	    kdc_log(context, config, 0, "krb5_crypto_init failed: %s",
		    krb5_get_err_text(context, ret));
	    goto out;
	}
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  usage,
					  b->enc_authorization_data,
					  &ad);
	krb5_crypto_destroy(context, crypto);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    kdc_log(context, config, 0, 
		    "Failed to decrypt enc-authorization-data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out;
	}
	krb5_free_keyblock(context, subkey);
	ALLOC(*auth_data);
	if (*auth_data == NULL) {
	    krb5_auth_con_free(context, ac);
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out;
	}
	ret = decode_AuthorizationData(ad.data, ad.length, *auth_data, NULL);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    free(*auth_data);
	    *auth_data = NULL;
	    kdc_log(context, config, 0, "Failed to decode authorization data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out;
	}
    }

    krb5_auth_con_free(context, ac);
    
out:
    free_AP_REQ(&ap_req);
    
    return ret;
}

static krb5_error_code
tgs_build_reply(krb5_context context, 
		krb5_kdc_configuration *config,
		KDC_REQ *req, 
		KDC_REQ_BODY *b,
		hdb_entry_ex *krbtgt,
		krb5_enctype krbtgt_etype,
		krb5_ticket *ticket,
		krb5_data *reply,
		const char *from,
		const char **e_text,
		AuthorizationData *auth_data,
		const struct sockaddr *from_addr,
		int datagram_reply)
{
    krb5_error_code ret;
    krb5_principal cp = NULL, sp = NULL;
    krb5_principal client_principal = NULL;
    char *spn = NULL, *cpn = NULL;
    hdb_entry_ex *server = NULL, *client = NULL;
    EncTicketPart *tgt = &ticket->ticket;
    KRB5SignedPathPrincipals *spp = NULL;
    const EncryptionKey *ekey;
    krb5_keyblock sessionkey;
    krb5_kvno kvno;
    krb5_data rspac;
    int cross_realm = 0;

    PrincipalName *s;
    Realm r;
    int nloop = 0;
    EncTicketPart adtkt;
    char opt_str[128];
    int require_signedpath = 0;

    memset(&sessionkey, 0, sizeof(sessionkey));
    memset(&adtkt, 0, sizeof(adtkt));
    krb5_data_zero(&rspac);

    s = b->sname;
    r = b->realm;

    if(b->kdc_options.enc_tkt_in_skey){
	Ticket *t;
	hdb_entry_ex *uu;
	krb5_principal p;
	Key *uukey;
	    
	if(b->additional_tickets == NULL || 
	   b->additional_tickets->len == 0){
	    ret = KRB5KDC_ERR_BADOPTION; /* ? */
	    kdc_log(context, config, 0,
		    "No second ticket present in request");
	    goto out;
	}
	t = &b->additional_tickets->val[0];
	if(!get_krbtgt_realm(&t->sname)){
	    kdc_log(context, config, 0,
		    "Additional ticket is not a ticket-granting ticket");
	    ret = KRB5KDC_ERR_POLICY;
	    goto out;
	}
	_krb5_principalname2krb5_principal(context, &p, t->sname, t->realm);
	ret = _kdc_db_fetch(context, config, p, 
			    HDB_F_GET_CLIENT|HDB_F_GET_SERVER, 
			    NULL, &uu);
	krb5_free_principal(context, p);
	if(ret){
	    if (ret == HDB_ERR_NOENTRY)
		ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	    goto out;
	}
	ret = hdb_enctype2key(context, &uu->entry, 
			      t->enc_part.etype, &uukey);
	if(ret){
	    _kdc_free_ent(context, uu);
	    ret = KRB5KDC_ERR_ETYPE_NOSUPP; /* XXX */
	    goto out;
	}
	ret = krb5_decrypt_ticket(context, t, &uukey->key, &adtkt, 0);
	_kdc_free_ent(context, uu);
	if(ret)
	    goto out;

	ret = verify_flags(context, config, &adtkt, spn);
	if (ret)
	    goto out;

	s = &adtkt.cname;
	r = adtkt.crealm;
    }

    _krb5_principalname2krb5_principal(context, &sp, *s, r);
    ret = krb5_unparse_name(context, sp, &spn);	
    if (ret)
	goto out;
    _krb5_principalname2krb5_principal(context, &cp, tgt->cname, tgt->crealm);
    ret = krb5_unparse_name(context, cp, &cpn);
    if (ret)
	goto out;
    unparse_flags (KDCOptions2int(b->kdc_options),
		   asn1_KDCOptions_units(),
		   opt_str, sizeof(opt_str));
    if(*opt_str)
	kdc_log(context, config, 0,
		"TGS-REQ %s from %s for %s [%s]", 
		cpn, from, spn, opt_str);
    else
	kdc_log(context, config, 0,
		"TGS-REQ %s from %s for %s", cpn, from, spn);

    /*
     * Fetch server
     */

server_lookup:
    ret = _kdc_db_fetch(context, config, sp, HDB_F_GET_SERVER, NULL, &server);

    if(ret){
	const char *new_rlm;
	Realm req_rlm;
	krb5_realm *realms;

	if ((req_rlm = get_krbtgt_realm(&sp->name)) != NULL) {
	    if(nloop++ < 2) {
		new_rlm = find_rpath(context, tgt->crealm, req_rlm);
		if(new_rlm) {
		    kdc_log(context, config, 5, "krbtgt for realm %s "
			    "not found, trying %s", 
			    req_rlm, new_rlm);
		    krb5_free_principal(context, sp);
		    free(spn);
		    krb5_make_principal(context, &sp, r, 
					KRB5_TGS_NAME, new_rlm, NULL);
		    ret = krb5_unparse_name(context, sp, &spn);	
		    if (ret)
			goto out;
		    auth_data = NULL; /* ms don't handle AD in referals */
		    goto server_lookup;
		}
	    }
	} else if(need_referral(context, sp, &realms)) {
	    if (strcmp(realms[0], sp->realm) != 0) {
		kdc_log(context, config, 5,
			"Returning a referral to realm %s for "
			"server %s that was not found",
			realms[0], spn);
		krb5_free_principal(context, sp);
		free(spn);
		krb5_make_principal(context, &sp, r, KRB5_TGS_NAME,
				    realms[0], NULL);
		ret = krb5_unparse_name(context, sp, &spn);
		if (ret)
		    goto out;
		krb5_free_host_realm(context, realms);
		auth_data = NULL; /* ms don't handle AD in referals */
		goto server_lookup;
	    }
	    krb5_free_host_realm(context, realms);
	}
	kdc_log(context, config, 0,
		"Server not found in database: %s: %s", spn,
		krb5_get_err_text(context, ret));
	if (ret == HDB_ERR_NOENTRY)
	    ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto out;
    }

    ret = _kdc_db_fetch(context, config, cp, HDB_F_GET_CLIENT, NULL, &client);
    if(ret) {
	const char *krbtgt_realm; 

	/*
	 * If the client belongs to the same realm as our krbtgt, it
	 * should exist in the local database.
	 *
	 */

	krbtgt_realm = 
	    krb5_principal_get_comp_string(context, 
					   krbtgt->entry.principal, 1);

	if(strcmp(krb5_principal_get_realm(context, cp), krbtgt_realm) == 0) {
	    if (ret == HDB_ERR_NOENTRY)
		ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	    kdc_log(context, config, 1, "Client no longer in database: %s",
		    cpn);
	    goto out;
	}
	
	kdc_log(context, config, 1, "Client not found in database: %s: %s",
		cpn, krb5_get_err_text(context, ret));

	cross_realm = 1;
    }
    
    /*
     * Check that service is in the same realm as the krbtgt. If it's
     * not the same, it's someone that is using a uni-directional trust
     * backward.
     */
    
    if (strcmp(krb5_principal_get_realm(context, sp),
	       krb5_principal_get_comp_string(context, 
					      krbtgt->entry.principal, 
					      1)) != 0) {
	char *tpn;
	ret = krb5_unparse_name(context, krbtgt->entry.principal, &tpn);
	kdc_log(context, config, 0,
		"Request with wrong krbtgt: %s",
		(ret == 0) ? tpn : "<unknown>");
	if(ret == 0)
	    free(tpn);
	ret = KRB5KRB_AP_ERR_NOT_US;
	goto out;
    }

    /*
     *
     */

    client_principal = cp;

    if (client) {
	const PA_DATA *sdata;
	int i = 0;

	sdata = _kdc_find_padata(req, &i, KRB5_PADATA_S4U2SELF);
	if (sdata) {
	    krb5_crypto crypto;
	    krb5_data datack;
	    PA_S4U2Self self;
	    char *selfcpn = NULL;
	    const char *str;

	    ret = decode_PA_S4U2Self(sdata->padata_value.data, 
				     sdata->padata_value.length,
				     &self, NULL);
	    if (ret) {
		kdc_log(context, config, 0, "Failed to decode PA-S4U2Self");
		goto out;
	    }

	    ret = _krb5_s4u2self_to_checksumdata(context, &self, &datack);
	    if (ret)
		goto out;

	    ret = krb5_crypto_init(context, &tgt->key, 0, &crypto);
	    if (ret) {
		free_PA_S4U2Self(&self);
		krb5_data_free(&datack);
		kdc_log(context, config, 0, "krb5_crypto_init failed: %s",
			krb5_get_err_text(context, ret));
		goto out;
	    }

	    ret = krb5_verify_checksum(context,
				       crypto,
				       KRB5_KU_OTHER_CKSUM,
				       datack.data, 
				       datack.length, 
				       &self.cksum);
	    krb5_data_free(&datack);
	    krb5_crypto_destroy(context, crypto);
	    if (ret) {
		free_PA_S4U2Self(&self);
		kdc_log(context, config, 0, 
			"krb5_verify_checksum failed for S4U2Self: %s",
			krb5_get_err_text(context, ret));
		goto out;
	    }

	    ret = _krb5_principalname2krb5_principal(context,
						     &client_principal,
						     self.name,
						     self.realm);
	    free_PA_S4U2Self(&self);
	    if (ret)
		goto out;

	    ret = krb5_unparse_name(context, client_principal, &selfcpn);	
	    if (ret)
		goto out;

	    /*
	     * Check that service doing the impersonating is
	     * requesting a ticket to it-self.
	     */
	    if (krb5_principal_compare(context, cp, sp) != TRUE) {
		kdc_log(context, config, 0, "S4U2Self: %s is not allowed "
			"to impersonate some other user "
			"(tried for user %s to service %s)",
			cpn, selfcpn, spn);
		free(selfcpn);
		ret = KRB5KDC_ERR_BADOPTION; /* ? */
		goto out;
	    }

	    /*
	     * If the service isn't trusted for authentication to
	     * delegation, remove the forward flag.
	     */

	    if (client->entry.flags.trusted_for_delegation) {
		str = "[forwardable]";
	    } else {
		b->kdc_options.forwardable = 0;
		str = "";
	    }
	    kdc_log(context, config, 0, "s4u2self %s impersonating %s to "
		    "service %s %s", cpn, selfcpn, spn, str);
	    free(selfcpn);
	}
    }

    /*
     * Constrained delegation
     */

    if (client != NULL
	&& b->additional_tickets != NULL
	&& b->additional_tickets->len != 0
	&& b->kdc_options.enc_tkt_in_skey == 0)
    {
	Key *clientkey;
	Ticket *t;
	char *str;

	t = &b->additional_tickets->val[0];

	ret = hdb_enctype2key(context, &client->entry, 
			      t->enc_part.etype, &clientkey);
	if(ret){
	    ret = KRB5KDC_ERR_ETYPE_NOSUPP; /* XXX */
	    goto out;
	}

	ret = krb5_decrypt_ticket(context, t, &clientkey->key, &adtkt, 0);
	if (ret) {
	    kdc_log(context, config, 0,
		    "failed to decrypt ticket for "
		    "constrained delegation from %s to %s ", spn, cpn);
	    goto out;
	}

	/* check that ticket is valid */

	if (adtkt.flags.forwardable == 0) {
	    kdc_log(context, config, 0,
		    "Missing forwardable flag on ticket for "
		    "constrained delegation from %s to %s ", spn, cpn);
	    ret = KRB5KDC_ERR_ETYPE_NOSUPP; /* XXX */
	    goto out;
	}

	ret = check_constrained_delegation(context, config, client, sp);
	if (ret) {
	    kdc_log(context, config, 0,
		    "constrained delegation from %s to %s not allowed", 
		    spn, cpn);
	    goto out;
	}

	ret = _krb5_principalname2krb5_principal(context,
						 &client_principal,
						 adtkt.cname,
						 adtkt.crealm);
	if (ret)
	    goto out;

	ret = krb5_unparse_name(context, client_principal, &str);
	if (ret)
	    goto out;

	ret = verify_flags(context, config, &adtkt, str);
	if (ret) {
	    free(str);
	    goto out;
	}

	/*
	 * Check KRB5SignedPath in authorization data and add new entry to
	 * make sure servers can't fake a ticket to us.
	 */

	ret = check_KRB5SignedPath(context,
				   config,
				   krbtgt,
				   &adtkt,
				   &spp,
				   1);
	if (ret) {
	    kdc_log(context, config, 0,
		    "KRB5SignedPath check from service %s failed "
		    "for delegation to %s for client %s "
		    "from %s failed with %s",
		    spn, str, cpn, from, krb5_get_err_text(context, ret));
	    free(str);
	    goto out;
	}

	kdc_log(context, config, 0, "constrained delegation for %s "
		"from %s to %s", str, cpn, spn);
	free(str);

	/* 
	 * Also require that the KDC have issue the service's krbtgt
	 * used to do the request. 
	 */
	require_signedpath = 1;
    }

    /*
     * Check flags
     */

    ret = _kdc_check_flags(context, config, 
			   client, cpn,
			   server, spn,
			   FALSE);
    if(ret)
	goto out;

    if((b->kdc_options.validate || b->kdc_options.renew) && 
       !krb5_principal_compare(context, 
			       krbtgt->entry.principal,
			       server->entry.principal)){
	kdc_log(context, config, 0, "Inconsistent request.");
	ret = KRB5KDC_ERR_SERVER_NOMATCH;
	goto out;
    }

    /* check for valid set of addresses */
    if(!_kdc_check_addresses(context, config, tgt->caddr, from_addr)) {
	ret = KRB5KRB_AP_ERR_BADADDR;
	kdc_log(context, config, 0, "Request from wrong address");
	goto out;
    }
	
    /*
     * Select enctype, return key and kvno.
     */

    {
	krb5_enctype etype;

	if(b->kdc_options.enc_tkt_in_skey) {
	    int i;
	    ekey = &adtkt.key;
	    for(i = 0; i < b->etype.len; i++)
		if (b->etype.val[i] == adtkt.key.keytype)
		    break;
	    if(i == b->etype.len) {
		krb5_clear_error_string(context);
		return KRB5KDC_ERR_ETYPE_NOSUPP;
	    }
	    etype = b->etype.val[i];
	    kvno = 0;
	} else {
	    Key *skey;
	    
	    ret = _kdc_find_etype(context, server, b->etype.val, b->etype.len,
				  &skey, &etype);
	    if(ret) {
		kdc_log(context, config, 0, 
			"Server (%s) has no support for etypes", spp);
		return ret;
	    }
	    ekey = &skey->key;
	    kvno = server->entry.kvno;
	}
	
	ret = krb5_generate_random_keyblock(context, etype, &sessionkey);
	if (ret)
	    goto out;
    }

    /* check PAC if not cross realm and if there is one */
    if (!cross_realm) {
	Key *tkey;

	ret = hdb_enctype2key(context, &krbtgt->entry, 
			      krbtgt_etype, &tkey);
	if(ret) {
	    kdc_log(context, config, 0,
		    "Failed to find key for krbtgt PAC check");
	    goto out;
	}

	ret = check_PAC(context, config, client_principal, 
			client, server, ekey, &tkey->key,
			tgt, &rspac, &require_signedpath);
	if (ret) {
	    kdc_log(context, config, 0,
		    "Verify PAC failed for %s (%s) from %s with %s",
		    spn, cpn, from, krb5_get_err_text(context, ret));
	    goto out;
	}
    }

    /* also check the krbtgt for signature */
    ret = check_KRB5SignedPath(context,
			       config,
			       krbtgt,
			       tgt,
			       &spp,
			       require_signedpath);
    if (ret) {
	kdc_log(context, config, 0,
		"KRB5SignedPath check failed for %s (%s) from %s with %s",
		spn, cpn, from, krb5_get_err_text(context, ret));
	goto out;
    }

    /*
     *
     */

    ret = tgs_make_reply(context,
			 config, 
			 b, 
			 client_principal,
			 tgt, 
			 ekey,
			 &sessionkey,
			 kvno,
			 auth_data,
			 server, 
			 spn,
			 client, 
			 cp, 
			 krbtgt, 
			 krbtgt_etype,
			 spp,
			 &rspac,
			 e_text,
			 reply);
	
out:
    free(spn);
    free(cpn);
	    
    krb5_data_free(&rspac);
    krb5_free_keyblock_contents(context, &sessionkey);
    if(server)
	_kdc_free_ent(context, server);
    if(client)
	_kdc_free_ent(context, client);

    if (client_principal && client_principal != cp)
	krb5_free_principal(context, client_principal);
    if (cp)
	krb5_free_principal(context, cp);
    if (sp)
	krb5_free_principal(context, sp);

    free_EncTicketPart(&adtkt);

    return ret;
}

/*
 *
 */

krb5_error_code
_kdc_tgs_rep(krb5_context context, 
	     krb5_kdc_configuration *config,
	     KDC_REQ *req, 
	     krb5_data *data,
	     const char *from,
	     struct sockaddr *from_addr,
	     int datagram_reply)
{
    AuthorizationData *auth_data = NULL;
    krb5_error_code ret;
    int i = 0;
    const PA_DATA *tgs_req;

    hdb_entry_ex *krbtgt = NULL;
    krb5_ticket *ticket = NULL;
    const char *e_text = NULL;
    krb5_enctype krbtgt_etype = ETYPE_NULL;

    time_t *csec = NULL;
    int *cusec = NULL;

    if(req->padata == NULL){
	ret = KRB5KDC_ERR_PREAUTH_REQUIRED; /* XXX ??? */
	kdc_log(context, config, 0,
		"TGS-REQ from %s without PA-DATA", from);
	goto out;
    }
    
    tgs_req = _kdc_find_padata(req, &i, KRB5_PADATA_TGS_REQ);

    if(tgs_req == NULL){
	ret = KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
	
	kdc_log(context, config, 0, 
		"TGS-REQ from %s without PA-TGS-REQ", from);
	goto out;
    }
    ret = tgs_parse_request(context, config, 
			    &req->req_body, tgs_req,
			    &krbtgt,
			    &krbtgt_etype,
			    &ticket,
			    &e_text,
			    from, from_addr,
			    &csec, &cusec,
			    &auth_data);
    if (ret) {
	kdc_log(context, config, 0, 
		"Failed parsing TGS-REQ from %s", from);
	goto out;
    }

    ret = tgs_build_reply(context,
			  config,
			  req,
			  &req->req_body,
			  krbtgt,
			  krbtgt_etype,
			  ticket,
			  data,
			  from,
			  &e_text,
			  auth_data,
			  from_addr,
			  datagram_reply);
    if (ret) {
	kdc_log(context, config, 0, 
		"Failed building TGS-REP to %s", from);
	goto out;
    }

    /* */
    if (datagram_reply && data->length > config->max_datagram_reply_length) {
	krb5_data_free(data);
	ret = KRB5KRB_ERR_RESPONSE_TOO_BIG;
	e_text = "Reply packet too large";
    }

out:
    if(ret && data->data == NULL){
	krb5_mk_error(context,
		      ret,
		      NULL,
		      NULL,
		      NULL,
		      NULL,
		      csec,
		      cusec,
		      data);
    }
    free(csec);
    free(cusec);
    if (ticket)
	krb5_free_ticket(context, ticket);
    if(krbtgt)
	_kdc_free_ent(context, krbtgt);

    if (auth_data) {
	free_AuthorizationData(auth_data);
	free(auth_data);
    }

    return 0;
}
