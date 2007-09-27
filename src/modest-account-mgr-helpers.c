/* Copyright (c) 2006, Nokia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of the Nokia Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <modest-account-mgr-helpers.h>
#include <modest-account-mgr-priv.h>
#include <tny-simple-list.h>
#include <modest-runtime.h>
#include <string.h>

gboolean
modest_account_mgr_set_enabled (ModestAccountMgr *self, const gchar* name,
					gboolean enabled)
{
	return modest_account_mgr_set_bool (self, name, MODEST_ACCOUNT_ENABLED, enabled,FALSE);
}


gboolean
modest_account_mgr_get_enabled (ModestAccountMgr *self, const gchar* name)
{
	return modest_account_mgr_get_bool (self, name, MODEST_ACCOUNT_ENABLED, FALSE);
}

gboolean modest_account_mgr_set_signature (ModestAccountMgr *self, const gchar* name, 
	const gchar* signature, gboolean use_signature)
{
	gboolean result = modest_account_mgr_set_bool (self, name, MODEST_ACCOUNT_USE_SIGNATURE, 
		use_signature, FALSE);
	result = result && modest_account_mgr_set_string (self, name, MODEST_ACCOUNT_SIGNATURE, 
		signature, FALSE);
	return result;
}

gchar* modest_account_mgr_get_signature (ModestAccountMgr *self, const gchar* name, 
	gboolean* use_signature)
{
	if (use_signature) {
		*use_signature = 
			modest_account_mgr_get_bool (self, name, MODEST_ACCOUNT_USE_SIGNATURE, FALSE);
	}
	
	return modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_SIGNATURE, FALSE);
}


ModestTransportStoreProtocol modest_account_mgr_get_store_protocol (ModestAccountMgr *self, const gchar* name)
{
	ModestTransportStoreProtocol result = MODEST_PROTOCOL_STORE_POP; /* Arbitrary default */
	
	gchar *server_account_name = modest_account_mgr_get_string (self, name,
							MODEST_ACCOUNT_STORE_ACCOUNT,
							FALSE);
	if (server_account_name) {
		ModestServerAccountData* server_data = 
			modest_account_mgr_get_server_account_data (self, server_account_name);
		result = server_data->proto;
			
		modest_account_mgr_free_server_account_data (self, server_data);
		
		g_free (server_account_name);
	}
	
	return result;
}

gboolean modest_account_mgr_set_connection_specific_smtp (ModestAccountMgr *self, 
	const gchar* connection_name, const gchar* server_account_name)
{
	modest_account_mgr_remove_connection_specific_smtp (self, connection_name);
	
	ModestConf *conf = MODEST_ACCOUNT_MGR_GET_PRIVATE (self)->modest_conf;

	gboolean result = TRUE;
	GError *err = NULL;
	GSList *list = modest_conf_get_list (conf, MODEST_CONF_CONNECTION_SPECIFIC_SMTP_LIST,
						    MODEST_CONF_VALUE_STRING, &err);
	if (err) {
		g_printerr ("modest: %s: error getting list: %s.\n", __FUNCTION__, err->message);
		g_error_free (err);
		err = NULL;
		result = FALSE;
	} else {	
		/* The server account is in the item after the connection name: */
		GSList *list_connection = g_slist_append (list, (gpointer)connection_name);
		list_connection = g_slist_append (list_connection, (gpointer)server_account_name);
	
		/* Reset the changed list: */
		modest_conf_set_list (conf, MODEST_CONF_CONNECTION_SPECIFIC_SMTP_LIST, list_connection,
						    MODEST_CONF_VALUE_STRING, &err);
		if (err) {
			g_printerr ("modest: %s: error setting list: %s.\n", __FUNCTION__, err->message);
			g_error_free (err);
			result = FALSE;
		}
	}
				
	/* TODO: Should we free the items too, or just the list? */
	g_slist_free (list);
	
	return result;
}

/**
 * modest_account_mgr_remove_connection_specific_smtp
 * @self: a ModestAccountMgr instance
 * @name: the account name
 * @connection_name: A libconic IAP connection name
 * 
 * Disassacoiate a server account to use with the specific connection for this account.
 *
 * Returns: TRUE if it worked, FALSE otherwise
 */				 
gboolean modest_account_mgr_remove_connection_specific_smtp (ModestAccountMgr *self, 
	const gchar* connection_name)
{
	ModestAccountMgrPrivate *priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	
	gboolean result = TRUE;
	GError *err = NULL;
	GSList *list = modest_conf_get_list (priv->modest_conf, 
							MODEST_CONF_CONNECTION_SPECIFIC_SMTP_LIST,
						    MODEST_CONF_VALUE_STRING, &err);
	if (err) {
		g_printerr ("modest: %s: error getting list: %s.\n", __FUNCTION__, err->message);
		g_error_free (err);
		err = NULL;
		result = FALSE;
	}

	if (!list)
		return FALSE;
		
	/* The server account is in the item after the connection name: */
	GSList *list_connection = g_slist_find_custom (list, connection_name, (GCompareFunc)strcmp);
	if (list_connection) {
		/* remove both items: */
		GSList *temp = g_slist_delete_link(list_connection, list_connection);
		temp = g_slist_delete_link(temp, g_slist_next(temp));
	}
	
	/* Reset the changed list: */
	modest_conf_set_list (priv->modest_conf, MODEST_CONF_CONNECTION_SPECIFIC_SMTP_LIST, list,
						    MODEST_CONF_VALUE_STRING, &err);
	if (err) {
		g_printerr ("modest: %s: error setting list: %s.\n", __FUNCTION__, err->message);
		g_error_free (err);
		result = FALSE;
	}
				
	/* TODO: Should we free the items too, or just the list? */
	g_slist_free (list);
	
	return result;
}


gboolean modest_account_mgr_get_use_connection_specific_smtp (ModestAccountMgr *self, const gchar* account_name)
{
	return modest_account_mgr_get_bool (self, account_name, 
		MODEST_ACCOUNT_USE_CONNECTION_SPECIFIC_SMTP, FALSE);
}

gboolean modest_account_mgr_set_use_connection_specific_smtp (ModestAccountMgr *self, const gchar* account_name, 
	gboolean new_value)
{
	return modest_account_mgr_set_bool (self, account_name, MODEST_ACCOUNT_USE_CONNECTION_SPECIFIC_SMTP, 
		new_value, FALSE);
}

/**
 * modest_account_mgr_get_connection_specific_smtp
 * @self: a ModestAccountMgr instance
 * @connection_name: A libconic IAP connection name
 * 
 * Retrieve a server account to use with this specific connection for this account.
 *
 * Returns: a server account name to use for this connection, or NULL if none is specified.
 */			 
gchar* modest_account_mgr_get_connection_specific_smtp (ModestAccountMgr *self,  const gchar* connection_name)
{
	gchar *result = NULL;
	
	ModestAccountMgrPrivate *priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	
	GError *err = NULL;
	GSList *list = modest_conf_get_list (priv->modest_conf, MODEST_CONF_CONNECTION_SPECIFIC_SMTP_LIST,
						    MODEST_CONF_VALUE_STRING, &err);
	if (err) {
		g_printerr ("modest: %s: error getting list: %s.\n", __FUNCTION__, err->message);
		g_error_free (err);
		err = NULL;
	}

	if (!list)
		return NULL;

	/* The server account is in the item after the connection name: */
	GSList *iter = list;
	while (iter) {
		const gchar* this_connection_name = (const gchar*)(iter->data);
		if (strcmp (this_connection_name, connection_name) == 0) {
			iter = g_slist_next (iter);
			
			if (iter) {
				const gchar* account_name = (const gchar*)(iter->data);
				if (account_name) {
					result = g_strdup (account_name);
					break;
				}
			}
		}
		
		/* Skip 2 to go to the next connection in the list: */
		iter = g_slist_next (iter);
		if (iter)
			iter = g_slist_next (iter);
	}
		
	/*
	if (!result) {
		printf ("  debug: no server found for connection_name=%s.\n", connection_name);	
	}
	*/
				
	/* TODO: Should we free the items too, or just the list? */
	g_slist_free (list);
	
	return result;
}
					 
gchar*
modest_account_mgr_get_server_account_username (ModestAccountMgr *self, const gchar* account_name)
{
	return modest_account_mgr_get_string (self, account_name, MODEST_ACCOUNT_USERNAME, 
		TRUE /* server account */);
}

void
modest_account_mgr_set_server_account_username (ModestAccountMgr *self, const gchar* account_name, 
	const gchar* username)
{
	/* Note that this won't work properly as long as the gconf cache is broken 
	 * in Maemo Bora: */
	gchar *existing_username = modest_account_mgr_get_server_account_username(self, 
		account_name);
	
	modest_account_mgr_set_string (self, account_name, MODEST_ACCOUNT_USERNAME, 
		username, TRUE /* server account */);
		
	/* We don't know anything about new usernames: */
	if (strcmp (existing_username, username) != 0)
		modest_account_mgr_get_server_account_username_has_succeeded (self, account_name);
		
	g_free (existing_username);
}

gboolean
modest_account_mgr_get_server_account_username_has_succeeded (ModestAccountMgr *self, const gchar* account_name)
{
	return modest_account_mgr_get_bool (self, account_name, MODEST_ACCOUNT_USERNAME_HAS_SUCCEEDED, 
					    TRUE /* server account */);
}

void
modest_account_mgr_set_server_account_username_has_succeeded (ModestAccountMgr *self, 
						  const gchar* account_name, 
						  gboolean succeeded)
{
	modest_account_mgr_set_bool (self, account_name, MODEST_ACCOUNT_USERNAME_HAS_SUCCEEDED, 
				     succeeded, TRUE /* server account */);
}

void
modest_account_mgr_set_server_account_password (ModestAccountMgr *self, const gchar* account_name, 
				    const gchar* password)
{
	modest_account_mgr_set_string (self, account_name, MODEST_ACCOUNT_PASSWORD, 
				       password, TRUE /* server account */);
}

	
gchar*
modest_account_mgr_get_server_account_password (ModestAccountMgr *self, const gchar* account_name)
{
	return modest_account_mgr_get_string (self, account_name, MODEST_ACCOUNT_PASSWORD, 
		TRUE /* server account */);	
}

gboolean
modest_account_mgr_get_server_account_has_password (ModestAccountMgr *self, const gchar* account_name)
{
	gboolean result = FALSE;
	gchar *password = modest_account_mgr_get_string (self, account_name, MODEST_ACCOUNT_PASSWORD, 
		TRUE /* server account */);
	if (password && strlen (password)) {
		result = TRUE;
	}
	
	g_free (password);
	return result;
}
			 
	
gchar*
modest_account_mgr_get_server_account_hostname (ModestAccountMgr *self, 
						const gchar* account_name)
{
	return modest_account_mgr_get_string (self, 
					      account_name, 
					      MODEST_ACCOUNT_HOSTNAME, 
					      TRUE /* server account */);
}
 
void
modest_account_mgr_set_server_account_hostname (ModestAccountMgr *self, 
						const gchar *server_account_name,
						const gchar *hostname)
{
	modest_account_mgr_set_string (self, 
				       server_account_name,
				       MODEST_ACCOUNT_HOSTNAME, 
				       hostname, 
				       TRUE /* server account */);
}


static ModestAuthProtocol
get_secure_auth_for_conf_string(const gchar* value)
{
	ModestAuthProtocol result = MODEST_PROTOCOL_AUTH_NONE;
	if (value) {
		if (strcmp(value, MODEST_ACCOUNT_AUTH_MECH_VALUE_NONE) == 0)
			result = MODEST_PROTOCOL_AUTH_NONE;
		else if (strcmp(value, MODEST_ACCOUNT_AUTH_MECH_VALUE_PASSWORD) == 0)
			result = MODEST_PROTOCOL_AUTH_PASSWORD;
		else if (strcmp(value, MODEST_ACCOUNT_AUTH_MECH_VALUE_CRAMMD5) == 0)
			result = MODEST_PROTOCOL_AUTH_CRAMMD5;
	}
	
	return result;
}

ModestAuthProtocol
modest_account_mgr_get_server_account_secure_auth (ModestAccountMgr *self, 
	const gchar* account_name)
{
	ModestAuthProtocol result = MODEST_PROTOCOL_AUTH_NONE;
	gchar* value = modest_account_mgr_get_string (self, account_name, MODEST_ACCOUNT_AUTH_MECH, 
		TRUE /* server account */);
	if (value) {
		result = get_secure_auth_for_conf_string (value);
			
		g_free (value);
	}
	
	return result;
}


void
modest_account_mgr_set_server_account_secure_auth (ModestAccountMgr *self, 
	const gchar* account_name, ModestAuthProtocol secure_auth)
{
	/* Get the conf string for the enum value: */
	const gchar* str_value = NULL;
	if (secure_auth == MODEST_PROTOCOL_AUTH_NONE)
		str_value = MODEST_ACCOUNT_AUTH_MECH_VALUE_NONE;
	else if (secure_auth == MODEST_PROTOCOL_AUTH_PASSWORD)
		str_value = MODEST_ACCOUNT_AUTH_MECH_VALUE_PASSWORD;
	else if (secure_auth == MODEST_PROTOCOL_AUTH_CRAMMD5)
		str_value = MODEST_ACCOUNT_AUTH_MECH_VALUE_CRAMMD5;
	
	/* Set it in the configuration: */
	modest_account_mgr_set_string (self, account_name, MODEST_ACCOUNT_AUTH_MECH, str_value, TRUE);
}

static ModestConnectionProtocol
get_security_for_conf_string(const gchar* value)
{
	ModestConnectionProtocol result = MODEST_PROTOCOL_CONNECTION_NORMAL;
	if (value) {
		if (strcmp(value, MODEST_ACCOUNT_SECURITY_VALUE_NONE) == 0)
			result = MODEST_PROTOCOL_CONNECTION_NORMAL;
		else if (strcmp(value, MODEST_ACCOUNT_SECURITY_VALUE_NORMAL) == 0) {
			/* The UI has "Normal (TLS)": */
			result = MODEST_PROTOCOL_CONNECTION_TLS;
		} else if (strcmp(value, MODEST_ACCOUNT_SECURITY_VALUE_SSL) == 0)
			result = MODEST_PROTOCOL_CONNECTION_SSL;
	}
	
	return result;
}

ModestConnectionProtocol
modest_account_mgr_get_server_account_security (ModestAccountMgr *self, 
	const gchar* account_name)
{
	ModestConnectionProtocol result = MODEST_PROTOCOL_CONNECTION_NORMAL;
	gchar* value = modest_account_mgr_get_string (self, account_name, MODEST_ACCOUNT_SECURITY, 
		TRUE /* server account */);
	if (value) {
		result = get_security_for_conf_string (value);
			
		g_free (value);
	}
	
	return result;
}

void
modest_account_mgr_set_server_account_security (ModestAccountMgr *self, 
	const gchar* account_name, ModestConnectionProtocol security)
{
	/* Get the conf string for the enum value: */
	const gchar* str_value = NULL;
	if (security == MODEST_PROTOCOL_CONNECTION_NORMAL)
		str_value = MODEST_ACCOUNT_SECURITY_VALUE_NONE;
	else if (security == MODEST_PROTOCOL_CONNECTION_TLS) {
		/* The UI has "Normal (TLS)": */
		str_value = MODEST_ACCOUNT_SECURITY_VALUE_NORMAL;
	} else if (security == MODEST_PROTOCOL_CONNECTION_SSL)
		str_value = MODEST_ACCOUNT_SECURITY_VALUE_SSL;
	
	/* Set it in the configuration: */
	modest_account_mgr_set_string (self, account_name, MODEST_ACCOUNT_SECURITY, str_value, TRUE);
}

ModestServerAccountData*
modest_account_mgr_get_server_account_data (ModestAccountMgr *self, const gchar* name)
{
	ModestServerAccountData *data;
	gchar *proto;
	
	g_return_val_if_fail (modest_account_mgr_account_exists (self, name, TRUE), NULL);	
	data = g_slice_new0 (ModestServerAccountData);
	
	data->account_name = g_strdup (name);
	data->hostname     = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_HOSTNAME,TRUE);
	data->username     = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_USERNAME,TRUE);	
	proto              = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_PROTO, TRUE);
	data->proto        = modest_protocol_info_get_transport_store_protocol (proto);
	g_free (proto);

	data->port         = modest_account_mgr_get_int (self, name, MODEST_ACCOUNT_PORT, TRUE);
	
	gchar *secure_auth_str = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_AUTH_MECH, TRUE);
	data->secure_auth  = get_secure_auth_for_conf_string(secure_auth_str);
	g_free (secure_auth_str);
		
	gchar *security_str = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_SECURITY, TRUE);
	data->security     = get_security_for_conf_string(security_str);
	g_free (security_str);
	
	data->last_updated = modest_account_mgr_get_int    (self, name, MODEST_ACCOUNT_LAST_UPDATED,TRUE);
	
	data->password     = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_PASSWORD, TRUE);		   
	
	return data;
}


void
modest_account_mgr_free_server_account_data (ModestAccountMgr *self,
					     ModestServerAccountData* data)
{
	g_return_if_fail (self);

	if (!data)
		return; /* not an error */

	g_free (data->account_name);
	data->account_name = NULL;
	
	g_free (data->hostname);
	data->hostname = NULL;
	
	g_free (data->username);
	data->username = NULL;

	g_free (data->password);
	data->password = NULL;

	g_slice_free (ModestServerAccountData, data);
}

/** You must use modest_account_mgr_free_account_data() on the result.
 */
ModestAccountData*
modest_account_mgr_get_account_data     (ModestAccountMgr *self, const gchar* name)
{
	ModestAccountData *data;
	gchar *server_account;
	gchar *default_account;
	
	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (name, NULL);
	
	if (!modest_account_mgr_account_exists (self, name, FALSE)) {
		/* For instance, maybe you are mistakenly checking for a server account name? */
		g_warning ("%s: Account %s does not exist.", __FUNCTION__, name);
		return NULL;
	}
	
	data = g_slice_new0 (ModestAccountData);
	
	data->account_name = g_strdup (name);

	data->display_name = modest_account_mgr_get_string (self, name,
							    MODEST_ACCOUNT_DISPLAY_NAME,
							    FALSE);
 	data->fullname     = modest_account_mgr_get_string (self, name,
							      MODEST_ACCOUNT_FULLNAME,
							       FALSE);
	data->email        = modest_account_mgr_get_string (self, name,
							    MODEST_ACCOUNT_EMAIL,
							    FALSE);
	data->is_enabled   = modest_account_mgr_get_enabled (self, name);

	default_account    = modest_account_mgr_get_default_account (self);
	data->is_default   = (default_account && strcmp (default_account, name) == 0);
	g_free (default_account);

	/* store */
	server_account     = modest_account_mgr_get_string (self, name,
							    MODEST_ACCOUNT_STORE_ACCOUNT,
							    FALSE);
	if (server_account) {
		data->store_account =
			modest_account_mgr_get_server_account_data (self, server_account);
		g_free (server_account);
	}

	/* transport */
	server_account = modest_account_mgr_get_string (self, name,
							MODEST_ACCOUNT_TRANSPORT_ACCOUNT,
							FALSE);
	if (server_account) {
		data->transport_account =
			modest_account_mgr_get_server_account_data (self, server_account);
		g_free (server_account);
	}

	return data;
}


void
modest_account_mgr_free_account_data (ModestAccountMgr *self, ModestAccountData *data)
{
	g_return_if_fail (self);

	if (!data) /* not an error */ 
		return;

	g_free (data->account_name);
	g_free (data->display_name);
	g_free (data->fullname);
	g_free (data->email);

	modest_account_mgr_free_server_account_data (self, data->store_account);
	modest_account_mgr_free_server_account_data (self, data->transport_account);
	
	g_slice_free (ModestAccountData, data);
}

gint 
on_accounts_list_sort_by_title(gconstpointer a, gconstpointer b)
{
 	return g_utf8_collate((const gchar*)a, (const gchar*)b);
}

/** Get the first one, alphabetically, by title. */
gchar* 
modest_account_mgr_get_first_account_name (ModestAccountMgr *self)
{
	const gchar* account_name = NULL;
	GSList *account_names = modest_account_mgr_account_names (self, TRUE /* only enabled */);

	/* Return TRUE if there is no account */
	if (!account_names)
		return NULL;

	/* Get the first one, alphabetically, by title: */
	/* gchar *old_default = modest_account_mgr_get_default_account (self); */
	GSList* list_sorted = g_slist_sort (account_names, on_accounts_list_sort_by_title);

	GSList* iter = list_sorted;
	gboolean found = FALSE;
	while (iter && !found) {
		account_name = (const gchar*)list_sorted->data;

		if (account_name)
			found = TRUE;

		if (!found)
			iter = g_slist_next (iter);
	}

	gchar* result = NULL;
	if (account_name)
		result = g_strdup (account_name);
		
	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;

	return result;
}

gboolean
modest_account_mgr_set_first_account_as_default (ModestAccountMgr *self)
{
	gboolean result = FALSE;
	
	gchar* account_name = modest_account_mgr_get_first_account_name(self);
	if (account_name) {
		result = modest_account_mgr_set_default_account (self, account_name);
		g_free (account_name);
	}
	else
		result = TRUE; /* If there are no accounts then it's not a failure. */

	return result;
}

gchar*
modest_account_mgr_get_from_string (ModestAccountMgr *self, const gchar* name)
{
	gchar *fullname, *email, *from;
	
	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (name, NULL);

	fullname      = modest_account_mgr_get_string (self, name,MODEST_ACCOUNT_FULLNAME,
						       FALSE);
	email         = modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_EMAIL,
						       FALSE);
	from = g_strdup_printf ("%s <%s>",
				fullname ? fullname : "",
				email    ? email    : "");
	g_free (fullname);
	g_free (email);

	return from;
}

/* Add a number to the end of the text, or increment a number that is already there.
 */
static gchar*
util_increment_name (const gchar* text)
{
	/* Get the end character,
	 * also doing a UTF-8 validation which is required for using g_utf8_prev_char().
	 */
	const gchar* end = NULL;
	if (!g_utf8_validate (text, -1, &end))
		return NULL;
  
  	if (!end)
  		return NULL;
  		
  	--end; /* Go to before the null-termination. */
  		
  	/* Look at each UTF-8 characer, starting at the end: */
  	const gchar* p = end;
  	const gchar* alpha_end = NULL;
  	while (p)
  	{	
  		/* Stop when we reach the first character that is not a numeric digit: */
  		const gunichar ch = g_utf8_get_char (p);
  		if (!g_unichar_isdigit (ch)) {
  			alpha_end = p;
  			break;
  		}
  		
  		p = g_utf8_prev_char (p);	
  	}
  	
  	if(!alpha_end) {
  		/* The text must consist completely of numeric digits. */
  		alpha_end = text;
  	}
  	else
  		++alpha_end;
  	
  	/* Intepret and increment the number, if any: */
  	gint num = atol (alpha_end);
  	++num;
  	
	/* Get the name part: */
  	gint name_len = alpha_end - text;
  	gchar *name_without_number = g_malloc(name_len + 1);
  	memcpy (name_without_number, text, name_len);
  	name_without_number[name_len] = 0;\
  	
    /* Concatenate the text part and the new number: */	
  	gchar *result = g_strdup_printf("%s%d", name_without_number, num);
  	g_free (name_without_number);
  	
  	return result; 	
}

gchar*
modest_account_mgr_get_unused_account_name (ModestAccountMgr *self, const gchar* starting_name,
	gboolean server_account)
{
	gchar *account_name = g_strdup (starting_name);

	while (modest_account_mgr_account_exists (self, 
		account_name, server_account /*  server_account */)) {
			
		gchar * account_name2 = util_increment_name (account_name);
		g_free (account_name);
		account_name = account_name2;
	}
	
	return account_name;
}

gchar*
modest_account_mgr_get_unused_account_display_name (ModestAccountMgr *self, const gchar* starting_name)
{
	gchar *account_name = g_strdup (starting_name);

	while (modest_account_mgr_account_with_display_name_exists (self, account_name)) {
			
		gchar * account_name2 = util_increment_name (account_name);
		g_free (account_name);
		account_name = account_name2;
	}
	
	return account_name;
}

void 
modest_account_mgr_set_leave_on_server (ModestAccountMgr *self, 
					const gchar *account_name, 
					gboolean leave_on_server)
{
	modest_account_mgr_set_bool (self, 
				     account_name,
				     MODEST_ACCOUNT_LEAVE_ON_SERVER, 
				     leave_on_server, 
				     FALSE);
}

gboolean 
modest_account_mgr_get_leave_on_server (ModestAccountMgr *self, 
					const gchar* account_name)
{
	return modest_account_mgr_get_bool (self, 
					    account_name,
					    MODEST_ACCOUNT_LEAVE_ON_SERVER, 
					    FALSE);
}

gint 
modest_account_mgr_get_last_updated (ModestAccountMgr *self, 
				     const gchar* account_name)
{
	return modest_account_mgr_get_int (modest_runtime_get_account_mgr (), 
					   account_name, 
					   MODEST_ACCOUNT_LAST_UPDATED, 
					   TRUE);
}

void 
modest_account_mgr_set_last_updated (ModestAccountMgr *self, 
				     const gchar* account_name,
				     gint time)
{
	modest_account_mgr_set_int (self, 
				    account_name, 
				    MODEST_ACCOUNT_LAST_UPDATED, 
				    time, 
				    TRUE);

	/* TODO: notify about changes */
}

gint  
modest_account_mgr_get_retrieve_limit (ModestAccountMgr *self, 
				       const gchar* account_name)
{
	return modest_account_mgr_get_int (self, 
					   account_name,
					   MODEST_ACCOUNT_LIMIT_RETRIEVE, 
					   FALSE);
}

void  
modest_account_mgr_set_retrieve_limit (ModestAccountMgr *self, 
				       const gchar* account_name,
				       gint limit_retrieve)
{
	modest_account_mgr_set_int (self, 
				    account_name,
				    MODEST_ACCOUNT_LIMIT_RETRIEVE, 
				    limit_retrieve, 
				    FALSE /* not server account */);
}

gint  
modest_account_mgr_get_server_account_port (ModestAccountMgr *self, 
					    const gchar* account_name)
{
	return modest_account_mgr_get_int (self, 
					   account_name,
					   MODEST_ACCOUNT_PORT, 
					   TRUE);
}

void
modest_account_mgr_set_server_account_port (ModestAccountMgr *self, 
					    const gchar *account_name,
					    gint port_num)
{
	modest_account_mgr_set_int (self, 
				    account_name,
				    MODEST_ACCOUNT_PORT, 
				    port_num, TRUE /* server account */);
}

gchar* 
modest_account_mgr_get_server_account_name (ModestAccountMgr *self, 
					    const gchar *account_name,
					    TnyAccountType account_type)
{
	return modest_account_mgr_get_string (self, 
					      account_name,
					      (account_type == TNY_ACCOUNT_TYPE_STORE) ?
					      MODEST_ACCOUNT_STORE_ACCOUNT :
					      MODEST_ACCOUNT_TRANSPORT_ACCOUNT, 
					      FALSE);
}

gchar* 
modest_account_mgr_get_retrieve_type (ModestAccountMgr *self, 
				      const gchar *account_name)
{
	return modest_account_mgr_get_string (self, 
					      account_name,
					      MODEST_ACCOUNT_RETRIEVE, 
					      FALSE /* not server account */);
}

void 
modest_account_mgr_set_retrieve_type (ModestAccountMgr *self, 
				      const gchar *account_name,
				      const gchar *retrieve_type)
{
	modest_account_mgr_set_string (self, 
				       account_name,
				       MODEST_ACCOUNT_RETRIEVE, 
				       retrieve_type, 
				       FALSE /* not server account */);
}


void
modest_account_mgr_set_server_account_user_fullname (ModestAccountMgr *self, 
						     const gchar *account_name,
						     const gchar *fullname)
{
	modest_account_mgr_set_string (self, 
				       account_name,
				       MODEST_ACCOUNT_FULLNAME, 
				       fullname, 
				       FALSE /* not server account */);
}

void
modest_account_mgr_set_server_account_user_email (ModestAccountMgr *self, 
						  const gchar *account_name,
						  const gchar *email)
{
	modest_account_mgr_set_string (self, 
				       account_name,
				       MODEST_ACCOUNT_EMAIL, 
				       email, 
				       FALSE /* not server account */);
}
