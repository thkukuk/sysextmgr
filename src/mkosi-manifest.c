/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <netdb.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/utsname.h>

#include <systemd/sd-json.h>

#include "basics.h"
#include "sysextmgr.h"
#include "image-deps.h"

struct manifest {
  int manifest_version;
  sd_json_variant *config;
  char *config_name;
  char *config_architecture;
  char *config_version;
  sd_json_variant *extensions;
};

static void
manifest_free (struct manifest *var)
{
  var->config = sd_json_variant_unref(var->config);
  var->config_name = mfree(var->config_name);
  var->config_architecture = mfree(var->config_architecture);
  var->config_version = mfree(var->config_version);
  var->extensions = sd_json_variant_unref(var->extensions);
}

static int
parse_manifest_config(sd_json_variant *json, char **image_name)
{
  _cleanup_(manifest_free) struct manifest p = {
    .manifest_version = -1,
    .config = NULL,
    .config_name = NULL,
    .config_architecture = NULL,
    .config_version = NULL,
    .extensions = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "name",         SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct manifest, config_name),         0 },
    { "architecture", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct manifest, config_architecture), 0 },
    { "version",      SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct manifest, config_version),      0 },
    {}
  };
  int r;

  assert(image_name);

  r = sd_json_dispatch(json, dispatch_table, SD_JSON_LOG|SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse JSON config content: %s\n", strerror(-r));
      return r;
    }

  if (asprintf(image_name, "%s-%s.%s.raw", p.config_name, p.config_version, p.config_architecture) < 0)
    return -ENOMEM;

  return 0;
}


static int
parse_manifest(sd_json_variant *json, struct image_deps **res)
{
  _cleanup_(manifest_free) struct manifest p = {
    .manifest_version = -1,
    .config = NULL,
    .config_name = NULL,
    .config_architecture = NULL,
    .config_version = NULL,
    .extensions = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "manifest_version", SD_JSON_VARIANT_NUMBER,  sd_json_dispatch_int,     offsetof(struct manifest, manifest_version), 0 },
    { "config",           SD_JSON_VARIANT_OBJECT,  sd_json_dispatch_variant, offsetof(struct manifest, config), 0 },
    { "extensions",       SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct manifest, extensions), SD_JSON_NULLABLE },
    {}
  };
  int r;

  assert(res);

  r = sd_json_dispatch(json, dispatch_table, SD_JSON_LOG|SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse JSON manifest: %s\n", strerror(-r));
      return r;
    }

  if (!sd_json_variant_is_array(p.extensions) || sd_json_variant_elements(p.extensions) != 1)
    {
      fprintf(stderr, "Failed to parse JSON extensions: %s\n", strerror(-r));
      return -EINVAL;
    }

  sd_json_variant *e = sd_json_variant_by_index(p.extensions, 0);
  if (!sd_json_variant_is_array(e) || sd_json_variant_elements(e) != 2)
    {
      fprintf(stderr, "Failed to parse JSON extension: %s\n", strerror(-r));
      return -EINVAL;
    }

  /* Format is: "sysext", {...} */

  e = sd_json_variant_by_index(e, 1);

  r = parse_image_deps(e, res);
  if (r < 0)
    return r;

  r = parse_manifest_config(p.config, &((*res)->image_name_json));
  if (r < 0)
    return r;

  return 0;
}

#include <zio.h>

int
load_manifest(int dir_fd, const char *path, struct image_deps ***images)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  unsigned line = 0, column = 0;
  int r;

  FILE *fp = fzopen(path, "r");

  if (fp)
    {
      r = sd_json_parse_file_at(fp, dir_fd, NULL, 0, &json, &line, &column);
      fclose(fp);
    }
  else
    r = sd_json_parse_file_at(NULL, dir_fd, path, 0, &json, &line, &column);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse json file (%s) %u:%u: %s\n",
	      path, line, column, strerror(-r));
      return r;
    }

  *images = calloc(2, sizeof(struct image_deps *));
  if (*images == NULL)
    oom();
  (*images)[1] = NULL;

  r = parse_manifest(json, &(*images)[0]);
  if (r < 0)
    return r;

  return 0;
}
