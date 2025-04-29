// SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2025 Thorsten Kukuk
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

#include <assert.h>
#include <limits.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libintl.h>
#include <syslog.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "mkdir_p.h"
#include "sysextmgr.h"
#include "osrelease.h"
#include "download.h"
#include "images-list.h"
#include "extension-util.h"
#include "tmpfile-util.h"
#include "strv.h"
#include "log_msg.h"

#include "varlink-org.openSUSE.sysextmgr.h"

static int socket_activation = false;

static int
vl_method_ping(sd_varlink *link, sd_json_variant *parameters,
	       sd_varlink_method_flags_t _unused_(flags),
	       void _unused_(*userdata))
{
  int r;

  log_msg(LOG_INFO, "Varlink method \"Ping\" called...");

  r = sd_varlink_dispatch(link, parameters, NULL, NULL);
  if (r != 0)
    return r;

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Alive", true));
}

static int
vl_method_set_log_level(sd_varlink *link, sd_json_variant *parameters,
			sd_varlink_method_flags_t _unused_(flags),
			void _unused_(*userdata))
{
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Level", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, 0, SD_JSON_MANDATORY },
    {}
  };
  uid_t peer_uid;
  int r, level;

  log_msg(LOG_INFO, "Varlink method \"SetLogLevel\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &level);
  if (r != 0)
    return r;

  log_msg(LOG_DEBUG, "Log level %i requested", level);

  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "SetLogLevel: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  set_max_log_level(level);

  log_msg(LOG_INFO, "New log setting: level=%i", level);

  return sd_varlink_reply(link, NULL);
}

static int
vl_method_get_environment(sd_varlink *link, sd_json_variant *parameters,
			  sd_varlink_method_flags_t _unused_(flags),
			  void _unused_(*userdata))
{
  int r;

  log_msg(LOG_INFO, "Varlink method \"GetEnvironment\" called...");

  r = sd_varlink_dispatch(link, parameters, NULL, NULL);
  if (r != 0)
    return r;

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "GetEnvironment: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

#if 0 /* XXX */
  for (char **e = environ; *e != 0; e++)
    {
      if (!env_assignment_is_valid(*e))
	goto invalid;
      if (!utf8_is_valid(*e))
	goto invalid;
    }
#endif

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_STRV("Environment", environ));

#if 0
 invalid:
  return sd_varlink_error(link, "io.systemd.service.InconsistentEnvironment", parameters);
#endif
}

static int
vl_method_quit(sd_varlink *link, sd_json_variant *parameters,
	       sd_varlink_method_flags_t _unused_(flags),
	       void *userdata)
{
  struct p {
    int code;
  } p = {
    .code = 0
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "ExitCode", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, offsetof(struct p, code), 0 },
    {}
  };
  sd_event *loop = userdata;
  int r;

  log_msg(LOG_INFO, "Varlink method \"Quit\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Quit request: varlink dispatch failed: %s", strerror(-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Quit: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  r = sd_event_exit(loop, p.code);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Quit request: disabling event loop failed: %s",
	      strerror(-r));
      return sd_varlink_errorbo(link, "org.openSUSE.wtmpdb.InternalError",
                                SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

/* XXX */
void
oom(void)
{
  log_msg(LOG_CRIT, "Out of memory");
  exit (1);
}

/* compare the image names, but move NULL to the end
   of the list */
static int
image_cmp(const void *a, const void *b)
{
  const struct image_entry *const *i_a = a;
  const struct image_entry *const *i_b = b;

  if (a == NULL && b == NULL)
    return 0;

  if (a == NULL)
    return 1;

  if (b == NULL)
    return -1;

  return strverscmp((*i_a)->deps->image_name, (*i_b)->deps->image_name);
}

struct parameters {
  char *url;
  bool verbose;
  char *install;
};

static void
parameters_free(struct parameters *var)
{
  var->url = mfree(var->url);
  var->install = mfree(var->install);
}

static int
vl_method_list_images(sd_varlink *link, sd_json_variant *parameters,
		      sd_varlink_method_flags_t _unused_(flags),
		      void _unused_(*userdata))
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *array = NULL;
  _cleanup_(parameters_free) struct parameters p = {
    .url = NULL,
    .verbose = config.verbose,
    .install = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "URL",     SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct parameters, url), 0},
    { "Verbose", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct parameters, verbose), 0},
    {}
  };
  _cleanup_(freep) char *osrelease_id = NULL;
  _cleanup_(freep) char *osrelease_sysext_level = NULL;
  _cleanup_(freep) char *osrelease_version_id = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_remote = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_local = NULL;
  size_t n_remote = 0, n_local = 0, n_etc = 0;
  const char *url = NULL;
  int r;

  log_msg(LOG_INFO, "Varlink method \"ListImages\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
  if (r < 0)
    {
      log_msg(LOG_ERR, "List image request: varlink dispatch failed: %s", strerror(-r));
      return r;
    }

  /* Only allow URL or verbose argument if called by root */
  if (p.url || p.verbose != config.verbose)
    {
      uid_t peer_uid;
      r = sd_varlink_get_peer_uid(link, &peer_uid);
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
	  return r;
	}
      if (peer_uid != 0)
	{
	  log_msg(LOG_WARNING, "ListImages: peer UID %i denied with additional options", peer_uid);
	  return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
	}
    }

  r = load_os_release(&osrelease_id, &osrelease_version_id, &osrelease_sysext_level);
  if (r < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Couldn't read os-release file: %s", strerror(-r)) < 0)
	error = NULL;
      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
                                SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }

  /* use URL from config if none got provided via parameter */
  if (p.url)
    url = p.url;
  else
    url = config.url;

  if (url)
    {
      r = image_remote_metadata(url, &images_remote, &n_remote, NULL, config.verify_signature);
      if (r < 0)
        {
	  _cleanup_free_ char *error = NULL;
          if (asprintf(&error, "Fetching image data from '%s' failed: %s",
		       url, strerror(-r)) < 0)
	    error = NULL;
	  log_msg(LOG_ERR, "%s", error);
	  return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				    SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				    SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
        }

      if (n_remote > 0)
        {
          for (size_t i = 0; i < n_remote; i++)
            if (images_remote[i]->deps)
              images_remote[i]->compatible = extension_release_validate(images_remote[i]->deps->image_name,
                                                                        osrelease_id, osrelease_version_id,
                                                                        osrelease_sysext_level, "system",
                                                                        images_remote[i]->deps, p.verbose);
        }
    }

  /* local available images */
  r = image_local_metadata(SYSEXT_STORE_DIR, &images_local, &n_local, NULL);
  if (r < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Searching for images in '%s' failed: %s",
		   SYSEXT_STORE_DIR, strerror(-r)) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
                                SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }

  if ((n_local + n_remote) == 0)
    {
      log_msg(LOG_INFO, "No images found");
      return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true),
				SD_JSON_BUILD_PAIR_VARIANT("Images", array)); /* XXX */
    }

  /* list of "installed" images visible to systemd-sysext */
  _cleanup_strv_free_ char **list_etc = NULL;
  r = discover_images(EXTENSIONS_DIR, &list_etc);
  if (r < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Searching for images in '%s' failed: %s",
		   EXTENSIONS_DIR, strerror(-r)) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
                                SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }
  n_etc = strv_length(list_etc);

  /* merge remote and local images */
  images = calloc((n_remote + n_local + 1), sizeof(struct image_entry *));
  if (images == NULL)
    oom(); /* XXX */

  size_t n = 0;

  for (size_t i = 0; i < n_remote; i++)
    {
      images[n] = TAKE_PTR(images_remote[i]);

      if (images[n]->deps)
        images[n]->compatible = extension_release_validate(images[n]->deps->image_name,
                                                           osrelease_id, osrelease_version_id,
                                                           osrelease_sysext_level, "system",
                                                           images[n]->deps, p.verbose);
      n++;
    }
  for (size_t i = 0; i < n_local; i++)
    {
      bool found = false;

      for (size_t j = 0; j < n_etc; j++)
        {
          if (streq(list_etc[j], images_local[i]->deps->image_name))
            images_local[i]->installed = true;
        }

      /* check if we know already the image */
      for (size_t j = 0; j < n_remote; j++)
        {
          if (streq(images_local[i]->deps->image_name, images[j]->deps->image_name))
            {
              images[j]->local = true;
              images[j]->installed = images_local[i]->installed;
              found = true;
              break;
            }
        }

      if (!found)
        {
          images[n] = TAKE_PTR(images_local[i]);

          if (images[n]->deps)
            images[n]->compatible = extension_release_validate(images[n]->deps->image_name,
                                                               osrelease_id, osrelease_version_id,
                                                               osrelease_sysext_level, "system",
                                                               images[n]->deps, p.verbose);
          n++;
        }
      else /* Free unused images_local[i] */
        free_image_entryp(&images_local[i]);
    }

  /* sort list */
  qsort(images, n, sizeof(struct image_entry *), image_cmp);

  for (size_t i = 0; images[i] != NULL; i++)
    {
      r = sd_json_variant_append_arraybo(&array,
					 SD_JSON_BUILD_PAIR_STRING("NAME", images[i]->name),
					 SD_JSON_BUILD_PAIR_STRING("IMAGE_NAME", images[i]->deps->image_name),
					 SD_JSON_BUILD_PAIR_STRING("SYSEXT_VERSION_ID", images[i]->deps->sysext_version_id),
					 SD_JSON_BUILD_PAIR_STRING("SYSEXT_SCOPE", images[i]->deps->sysext_scope),
					 SD_JSON_BUILD_PAIR_STRING("ID", images[i]->deps->id),
					 SD_JSON_BUILD_PAIR_STRING("SYSEXT_LEVEL", images[i]->deps->sysext_level),
					 SD_JSON_BUILD_PAIR_STRING("VERSION_ID", images[i]->deps->version_id),
					 SD_JSON_BUILD_PAIR_STRING("ARCHITECTURE", images[i]->deps->architecture),
					 SD_JSON_BUILD_PAIR_BOOLEAN("LOCAL", images[i]->local),
					 SD_JSON_BUILD_PAIR_BOOLEAN("REMOTE", images[i]->remote),
					 SD_JSON_BUILD_PAIR_BOOLEAN("INSTALLED", images[i]->installed),
					 SD_JSON_BUILD_PAIR_BOOLEAN("COMPATIBLE", images[i]->compatible));
      if(r < 0)
	{
	  log_msg(LOG_ERR, "Appending array failed: %s", strerror(-r));
	  /* XXX */
	}
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true),
			    SD_JSON_BUILD_PAIR_VARIANT("Images", array));
}

static void
unlink_and_free_tempfilep(char **p)
{
  if (p == NULL || *p == NULL)
    return;

  /* If the file is created with mkstemp(), it will (almost always) change the suffix.
   * Treat this as a sign that the file was successfully created. We ignore both the rare case
   * where the original suffix is used and unlink failures. */
  if (!endswith(*p, ".XXXXXX"))
    (void) unlink(*p);

  *p = mfree(*p);
}

static int
vl_method_update(sd_varlink *link, sd_json_variant *parameters,
		 sd_varlink_method_flags_t _unused_(flags),
		 void _unused_(*userdata))
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *array = NULL;
  _cleanup_(parameters_free) struct parameters p = {
    .url = NULL,
    .verbose = config.verbose,
    .install = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "URL",     SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct parameters, url), 0},
    { "Verbose", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct parameters, verbose), 0},
    { "Install", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct parameters, install), 0},
    {}
  };
  _cleanup_(free_image_entry_list) struct image_entry **images_etc = NULL;
  size_t n_etc = 0;
  const char *url = NULL;
  int r;

  log_msg(LOG_INFO, "Varlink method \"Update\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Update request: varlink dispatch failed: %s", strerror(-r));
      return r;
    }

  /* only root is allowed to update images */
  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Update: peer UID %i denied to update images",
	      peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  /* use URL from config if none got provided via parameter */
  if (p.url)
    url = p.url;
  else
    url = config.url;

  /* list of "installed" images visible to systemd-sysext */
  r = image_local_metadata(EXTENSIONS_DIR, &images_etc, &n_etc, NULL);
  if (r < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Searching for images in '%s' failed: %s",
		   EXTENSIONS_DIR, strerror(-r)) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
                                SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }
  if (n_etc == 0)
    {
      log_msg(LOG_NOTICE, "No installed images found.");
      /* XXX provide error message to client */
      return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true),
				SD_JSON_BUILD_PAIR_VARIANT("Updated", array));
    }

  for (size_t n = 0; n < n_etc; n++)
    {
      _cleanup_(free_image_entryp) struct image_entry *update = NULL;

      r = get_latest_version(images_etc[n], &update, url, config.verify_signature);
      if (update)
        {
          _cleanup_free_ char *fn = NULL;
          _cleanup_free_ char *linkfn = NULL;

	  log_msg(LOG_NOTICE, "Updating %s -> %s", images_etc[n]->deps->image_name, update->deps->image_name);

          r = join_path(SYSEXT_STORE_DIR, update->deps->image_name, &fn);
          if (r < 0) /* XXX return error msg */
            return r;

          if (asprintf(&linkfn, "%s/%s.raw", EXTENSIONS_DIR, update->name) < 0)
            return -ENOMEM;

          if (!update->local && update->remote)
            {
              _cleanup_(unlink_and_free_tempfilep) char *tmpfn = NULL;
              _cleanup_close_ int fd = -EBADF;

              assert(url);

              if (asprintf(&tmpfn, "%s/.%s.XXXXXX", SYSEXT_STORE_DIR, update->deps->image_name) < 0)
                return -ENOMEM;

              fd = mkostemp_safe(tmpfn);

              r = download(url, update->deps->image_name, tmpfn, config.verify_signature);
              if (r < 0)
                {
		  _cleanup_free_ char *error = NULL;
		  if (asprintf(&error, "Failed to download '%s' from '%s': %s",
			       update->deps->image_name, url, strerror(-r)) < 0)
		    error = NULL;

		  log_msg(LOG_ERR, "%s", error);
		  return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.DownloadError",
					    SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
					    SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
                }

              if (rename(tmpfn, fn) < 0)
                {
		  _cleanup_free_ char *error = NULL;
		  if (asprintf(&error, "Error to rename '%s' to '%s': %m", tmpfn, fn) < 0)
		    error = NULL;

		  log_msg(LOG_ERR, "%s", error);
		  return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
					    SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
					    SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
                }
            }

          if (unlink(linkfn) < 0)
            {
	      _cleanup_free_ char *error = NULL;
	      if (asprintf(&error, "Error to delete '%s': %m", linkfn) < 0)
		error = NULL;

	      log_msg(LOG_ERR, "%s", error);
	      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
					SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
					SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
            }

          if (symlink(fn, linkfn) < 0)
            {
	      _cleanup_free_ char *error = NULL;
	      if (asprintf(&error, "Error to symlink '%s' to '%s': %m", fn, linkfn) < 0)
		error = NULL;

	      log_msg(LOG_ERR, "%s", error);
	      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
					SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
					SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
            }
	  r = sd_json_variant_append_arraybo(&array,
					     SD_JSON_BUILD_PAIR_STRING("OldName", images_etc[n]->deps->image_name),
					     SD_JSON_BUILD_PAIR_STRING("NewName", update->deps->image_name));
        }
      else /* No update found */
	r = sd_json_variant_append_arraybo(&array,
					   SD_JSON_BUILD_PAIR_STRING("OldName", images_etc[n]->deps->image_name),
					   SD_JSON_BUILD_PAIR_STRING("NewName", NULL));
      if(r < 0)
	{
	  log_msg(LOG_ERR, "Appending array failed: %s", strerror(-r));
	  /* XXX */
	}
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true),
			    SD_JSON_BUILD_PAIR_VARIANT("Updated", array));
}

static int
vl_method_install(sd_varlink *link, sd_json_variant *parameters,
		  sd_varlink_method_flags_t _unused_(flags),
		  void _unused_(*userdata))
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *array = NULL;
  _cleanup_(parameters_free) struct parameters p = {
    .url = NULL,
    .verbose = config.verbose,
    .install = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "URL",     SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct parameters, url), 0},
    { "Verbose", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct parameters, verbose), 0},
    { "Install", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct parameters, install), SD_JSON_MANDATORY},
    {}
  };
  _cleanup_(free_image_entryp) struct image_entry *new = NULL;
  const char *url = NULL;
  int r;

  log_msg(LOG_INFO, "Varlink method \"Install\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Install request: varlink dispatch failed: %s", strerror(-r));
      return r;
    }

  /* only root is allowed to install images */
  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Install: peer UID %i denied to update images",
	      peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  /* use URL from config if none got provided via parameter */
  if (p.url)
    url = p.url;
  else
    url = config.url;

  struct image_deps wanted_deps = {
    .architecture = "x86-64" /* XXX */
  };
  struct image_entry wanted = {
    .name = p.install,
    .deps = &wanted_deps
  };

  r = get_latest_version(&wanted, &new, url, config.verify_signature);
  if (r < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Failed to get latest version for '%s' from '%s': %s",
		   p.install, url, strerror(-r)) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }
  if (!new)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Failed to find compatible version for '%s' from '%s': %s",
		   p.install, url, strerror(-r)) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.NoEntryFound",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }

  _cleanup_free_ char *fn = NULL;
  _cleanup_free_ char *linkfn = NULL;

  log_msg(LOG_NOTICE, "Installing %s", new->deps->image_name);

  r = join_path(SYSEXT_STORE_DIR, new->deps->image_name, &fn);
  if (r < 0) /* XXX return error msg */
    return r;

  if (asprintf(&linkfn, "%s/%s.raw", EXTENSIONS_DIR, new->name) < 0)
    return -ENOMEM;

  if (!new->local && new->remote)
    {
      _cleanup_(unlink_and_free_tempfilep) char *tmpfn = NULL;
      _cleanup_close_ int fd = -EBADF;

      assert(url);

      if (asprintf(&tmpfn, "%s/.%s.XXXXXX", SYSEXT_STORE_DIR, new->deps->image_name) < 0)
	return -ENOMEM;

      fd = mkostemp_safe(tmpfn);

      r = download(url, new->deps->image_name, tmpfn, config.verify_signature);
      if (r < 0)
	{
	  _cleanup_free_ char *error = NULL;
	  if (asprintf(&error, "Failed to download '%s' from '%s': %s",
		       new->deps->image_name, url, strerror(-r)) < 0)
	    error = NULL;

	  log_msg(LOG_ERR, "%s", error);
	  return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.DownloadError",
				    SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				    SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
	}

      if (rename(tmpfn, fn) < 0)
	{
	  _cleanup_free_ char *error = NULL;
	  if (asprintf(&error, "Error to rename '%s' to '%s': %m", tmpfn, fn) < 0)
	    error = NULL;

	  log_msg(LOG_ERR, "%s", error);
	  return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				    SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				    SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
	}
    }

  if (symlink(fn, linkfn) < 0)
    {
      _cleanup_free_ char *error = NULL;
      if (asprintf(&error, "Error to symlink '%s' to '%s': %m", fn, linkfn) < 0)
	error = NULL;

      log_msg(LOG_ERR, "%s", error);
      return sd_varlink_errorbo(link, "org.openSUSE.sysextmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false),
				SD_JSON_BUILD_PAIR_STRING("ErrorMsg", error?error:"Out of Memory"));
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true),
			    SD_JSON_BUILD_PAIR_STRING("Installed", new->deps->image_name));
}


/* Send a messages to systemd daemon, that inicialization of daemon
   is finished and daemon is ready to accept connections. */
static void
announce_ready(void)
{
  int r = sd_notify(0, "READY=1\n"
		    "STATUS=Processing requests...");
  if (r < 0)
    log_msg(LOG_ERR, "sd_notify(READY) failed: %s", strerror(-r));
}

static void
announce_stopping(void)
{
  int r = sd_notify(0, "STOPPING=1\n"
		    "STATUS=Shutting down...");
  if (r < 0)
    log_msg(LOG_ERR, "sd_notify(STOPPING) failed: %s", strerror(-r));
}

/* event loop which quits after 30 seconds idle time */
#define USEC_PER_SEC  ((uint64_t) 1000000ULL)
#define DEFAULT_EXIT_USEC (30*USEC_PER_SEC)

static int
varlink_event_loop_with_idle(sd_event *e, sd_varlink_server *s)
{
  int r, code;

  for (;;)
    {
      r = sd_event_get_state(e);
      if (r < 0)
	return r;
      if (r == SD_EVENT_FINISHED)
	break;

      r = sd_event_run(e, DEFAULT_EXIT_USEC);
      if (r < 0)
	return r;

      if (r == 0 && (sd_varlink_server_current_connections(s) == 0))
	sd_event_exit(e, 0);
    }

  r = sd_event_get_exit_code(e, &code);
  if (r < 0)
    return r;

  return code;
}

static int
run_varlink(void)
{
  int r;
  _cleanup_(sd_event_unrefp) sd_event *event = NULL;
  _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *varlink_server = NULL;

  r = mkdir_p(_VARLINK_SYSEXTMGR_SOCKET_DIR, 0755);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to create directory '"_VARLINK_SYSEXTMGR_SOCKET_DIR"' for Varlink socket: %s",
	      strerror(-r));
      return r;
    }

  r = sd_event_new(&event);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to create new event: %s",
	       strerror(-r));
      return r;
    }

  r = sd_varlink_server_new(&varlink_server, SD_VARLINK_SERVER_ACCOUNT_UID|SD_VARLINK_SERVER_INHERIT_USERDATA);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to allocate varlink server: %s",
	       strerror(-r));
      return r;
    }

  r = sd_varlink_server_set_description(varlink_server, "wtmpdbd");
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to set varlink server description: %s",
	       strerror(-r));
      return r;
    }

  r = sd_varlink_server_set_info(varlink_server, NULL, PACKAGE" (sysextmgrd)",
				  VERSION, "https://github.com/thkukuk/sysext-cli");
  if (r < 0)
    return r;

  r = sd_varlink_server_add_interface(varlink_server, &vl_interface_org_openSUSE_sysextmgr);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to add interface: %s", strerror(-r));
      return r;
    }

  r = sd_varlink_server_bind_method_many(varlink_server,
					 "org.openSUSE.sysextmgr.Install",        vl_method_install,
					 "org.openSUSE.sysextmgr.ListImages",     vl_method_list_images,
					 "org.openSUSE.sysextmgr.Update",         vl_method_update,
					 "org.openSUSE.sysextmgr.GetEnvironment", vl_method_get_environment,
					 "org.openSUSE.sysextmgr.Ping",           vl_method_ping,
					 "org.openSUSE.sysextmgr.Quit",           vl_method_quit,
					 "org.openSUSE.sysextmgr.SetLogLevel",    vl_method_set_log_level);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind Varlink methods: %s",
	      strerror(-r));
      return r;
    }

  sd_varlink_server_set_userdata(varlink_server, event);

  r = sd_varlink_server_attach_event(varlink_server, event, SD_EVENT_PRIORITY_NORMAL);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to attach to event: %s", strerror(-r));
      return r;
    }

  r = sd_varlink_server_listen_auto(varlink_server);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to listens: %s", strerror(-r));
      return r;
    }


  if (!socket_activation)
    {
      r = sd_varlink_server_listen_address(varlink_server, _VARLINK_SYSEXTMGR_SOCKET, 0666);
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to bind to Varlink socket: %s", strerror(-r));
	  return r;
	}
    }

  announce_ready();
  if (socket_activation)
    r = varlink_event_loop_with_idle(event, varlink_server);
  else
    r = sd_event_loop(event);
  announce_stopping();

  return r;
}

static void
print_help(void)
{
  printf("sysextmgrd - manage sysext images\n");

  printf("  -s, --socket   Activation through socket\n");
  printf("  -d, --debug    Debug mode\n");
  printf("  -v, --verbose  Verbose logging\n");
  printf("  -?, --help     Give this help list\n");
  printf("      --version  Print program version\n");
}

int
main(int argc, char **argv)
{
  int r;

  r = load_config();
  if (r < 0)
    {
      log_msg(LOG_ERR, "Couldn't load configuration file");
      exit(EXIT_FAILURE);
    }

  if (config.verbose)
    set_max_log_level(LOG_INFO);

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
        {
	  {"socket", no_argument, NULL, 's'},
          {"debug", no_argument, NULL, 'd'},
          {"verbose", no_argument, NULL, 'v'},
          {"version", no_argument, NULL, '\255'},
          {"usage", no_argument, NULL, '?'},
          {"help", no_argument, NULL, 'h'},
          {NULL, 0, NULL, '\0'}
        };

      c = getopt_long(argc, argv, "sdvh?", long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
	case 's':
	  socket_activation = true;
	  break;
        case 'd':
	  set_max_log_level(LOG_DEBUG);
          break;
        case '?':
        case 'h':
          print_help();
          return 0;
        case 'v':
	  set_max_log_level(LOG_INFO);
          break;
        case '\255':
          fprintf(stdout, "sysextmgrd (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_help();
          return 1;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      fprintf(stderr, "Try `sysextmgrd --help' for more information.\n");
      return 1;
    }

  log_msg(LOG_INFO, "Starting sysextmgrd (%s) %s...", PACKAGE, VERSION);

  r = run_varlink();
  if (r < 0)
    {
      log_msg(LOG_ERR, "ERROR: varlink loop failed: %s", strerror(-r));
      return -r;
    }

  log_msg(LOG_INFO, "sysextmgrd stopped.");

  return 0;
}
