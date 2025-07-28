//SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <string.h>
#include <syslog.h>
#include <libeconf.h>

#include "basics.h"
#include "sysextmgr.h"
#include "log_msg.h"

struct config config = {
  .verbose = false,
  .verify_signature = true,
  .url = NULL,
  .sysext_store_dir = SYSEXT_STORE_DIR,
  .extensions_dir = EXTENSIONS_DIR
};

static econf_err
open_config_file(econf_file **key_file)
{
  return econf_readConfig(key_file,
			  PACKAGE,
			  DATADIR,
			  "sysextmgr",
			  "conf", "=", "#");
}

static int
getBoolValueDef(econf_file *key_file, const char *group, const char *key, bool *val, bool def)
{
  econf_err error;

  /* first try, special (client, daemon) group */
  error = econf_getBoolValue(key_file, group, key, val);
  if (!error)
    return 0;

  /* second try, use "default" group */
  if (error && error == ECONF_NOKEY)
    error = econf_getBoolValueDef(key_file, "default", key, val, def);

  if (error && error != ECONF_NOKEY)
    {
      log_msg(LOG_ERR, "ERROR (econf): cannot get key '%s': %s",
	      key, econf_errString(error));
      return -1;
    }

  return 0;
}

static int
getStringValueDef(econf_file *key_file, const char *group, const char *key, char **val, char *def)
{
  econf_err error;

  /* first try, special (client, daemon) group */
  error = econf_getStringValue(key_file, group, key, val);
  if (!error)
    return 0;

  /* second try, use "default" group */
  if (error && error == ECONF_NOKEY)
    error = econf_getStringValueDef(key_file, "default", key, val, def);

  if (error && error != ECONF_NOKEY)
    {
      log_msg(LOG_ERR, "ERROR (econf): cannot get key '%s': %s",
	      key, econf_errString(error));
      return -1;
    }

  return 0;
}

int
load_config(const char *defgroup)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;

  error = open_config_file(&key_file);
  if (error)
    {
      /* ignore if there is no configuration file at all */
      if (error == ECONF_NOFILE)
	return 0;

      log_msg(LOG_ERR, "econf_readConfig: %s\n",
	      econf_errString(error));
      return -1;
    }

  if (key_file == NULL) /* can this happen? */
      log_msg(LOG_ERR, "Cannot load 'sysextmgr.conf'");
  else
    {
      int r;

      r = getBoolValueDef(key_file, defgroup, "verbose", &config.verbose, config.verbose);
      if (r < 0)
	return r;
      r = getBoolValueDef(key_file, defgroup, "verify_signature", &config.verify_signature, config.verify_signature);
      if (r < 0)
	return r;
      r = getStringValueDef(key_file, defgroup, "url", &config.url, config.url);
      if (r < 0)
	return r;
      r = getStringValueDef(key_file, defgroup, "sysext_store_dir", &config.sysext_store_dir, config.sysext_store_dir);
      if (r < 0)
	return r;
      r = getStringValueDef(key_file, defgroup, "extensions_dir", &config.extensions_dir, config.extensions_dir);
      if (r < 0)
	return r;
    }

  return 0;
}
