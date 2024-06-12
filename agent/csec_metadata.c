#include "csec_metadata.h"
#include "util_strings.h"
#include "php_hash.h"
#include "php_api_internal.h"

static void nr_csec_php_add_assoc_string_const(zval* arr,
                                          const char* key,
                                          const char* value) {
  char* val = NULL;

  if (NULL == arr || NULL == key || NULL == value) {
    return;
  }

  val = nr_strdup(value);
  nr_php_add_assoc_string(arr, key, val);
  nr_free(val);
}

#ifdef TAGS
void zif_newrelic_get_security_metadata(void); /* ctags landing pad only */
void newrelic_get_security_metadata(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_security_metadata) {

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_EXECUTE_DATA;

  array_init(return_value);

  nr_csec_php_add_assoc_string_const(return_value, KEY_ENTITY_NAME, nr_app_get_entity_name(NRPRG(app)));
  nr_csec_php_add_assoc_string_const(return_value, KEY_ENTITY_TYPE, nr_app_get_entity_type(NRPRG(app)));
  nr_csec_php_add_assoc_string_const(return_value, KEY_ENTITY_GUID, nr_app_get_entity_guid(NRPRG(app)));
  nr_csec_php_add_assoc_string_const(return_value, KEY_HOSTNAME, nr_app_get_host_name(NRPRG(app)));
  nr_csec_php_add_assoc_string_const(return_value, KEY_LICENSE, NRPRG(license).value);

  if (NRPRG(app)) {
    nr_csec_php_add_assoc_string_const(return_value, KEY_AGENT_RUN_ID, NRPRG(app)->agent_run_id);
    nr_csec_php_add_assoc_string_const(return_value, KEY_ACCOUNT_ID, NRPRG(app)->account_id);
    nr_csec_php_add_assoc_string_const(return_value, KEY_PLICENSE, NRPRG(app)->plicense);
    int high_security = NRPRG(app)->info.high_security;
    add_assoc_long(return_value, KEY_HIGH_SECURITY, (long)high_security);
  }

}
