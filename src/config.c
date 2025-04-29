//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2024 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#include "config.h"

#include <string.h>
#include <syslog.h>
#include <libeconf.h>

#include "basics.h"
#include "sysextmgr.h"
#include "log_msg.h"

/* XXX free config */
struct config config;

static econf_err
open_config_file(econf_file **key_file)
{
  return econf_readConfig(key_file,
			  PACKAGE,
			  DATADIR,
			  "sysextmgr",
			  "conf", "=", "#");
}

/* XXX set config.url */
int
load_config(void)
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
      error = econf_getBoolValueDef(key_file, "default", "verbose", &config.verbose, false);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'verbose': %s",
		  econf_errString(error));
	  return -1;
	}
      error = econf_getBoolValueDef(key_file, "default", "verify_signature", &config.verify_signature, true);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'verify_signature': %s",
		  econf_errString(error));
	  return -1;
	}
      error = econf_getStringValueDef(key_file, "default", "sysext_store_dir", &config.sysext_store_dir, SYSEXT_STORE_DIR);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'sysext_store_dir': %s",
		  econf_errString(error));
	  return -1;
	}
      error = econf_getStringValueDef(key_file, "default", "extensions_dir", &config.extensions_dir, EXTENSIONS_DIR);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'extensions_dir': %s",
		  econf_errString(error));
	  return -1;
	}
    }
  return 0;
}
