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

#include <libeconf.h>
#include <systemd/sd-json.h>

#include "basics.h"

#include "sysext-cli.h"

void
free_image_deps(struct image_deps *e)
{
  e->image_name = mfree(e->image_name);
  e->sysext_version_id = mfree(e->sysext_version_id);
  e->sysext_scope = mfree(e->sysext_scope);
  e->id = mfree(e->id);
  e->sysext_level = mfree(e->sysext_level);
  e->version_id = mfree(e->version_id);
  e->architecture = mfree(e->architecture);
  e->sysext = sd_json_variant_unref(e->sysext);
}

void
free_image_depsp(struct image_deps **e)
{
  if (!e || !*e)
    return;

  free_image_deps(*e);
  *e = mfree(*e);
}


void
free_image_deps_list(struct image_deps ***images)
{
  if (!images)
    return;

  for (size_t i = 0; *images && (*images)[i] != NULL; i++)
    {
      free_image_deps((*images)[i]);
      free((*images)[i]);
    }
  free(*images);
}

void
dump_image_deps(struct image_deps *e)
{
  printf("image name: %s\n", e->image_name);
  printf("* sysext version_id: %s\n", e->sysext_version_id);
  printf("* sysext scope: %s\n", e->sysext_scope);
  printf("* id: %s\n", e->id);
  printf("* sysext_level: %s\n", e->sysext_level);
  printf("* version_id: %s\n", e->version_id);
  printf("* architecture: %s\n", e->architecture);
}

int
parse_image_deps(sd_json_variant *json, struct image_deps **res)
{
  static const sd_json_dispatch_field dispatch_table[] = {
    { "image_name",        SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, image_name),       0 },
    { "SYSEXT_VERSION_ID", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, sysext_version_id), 0 },
    { "SYSEXT_SCOPE",      SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, sysext_scope),      0 },
    { "ID",                SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, id), 0 },
    { "SYSEXT_LEVEL",      SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, sysext_level), 0 },
    { "VERSION_ID",        SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, version_id), 0 },
    { "ARCHITECTURE",      SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct image_deps, architecture), 0 },
    { "sysext",            SD_JSON_VARIANT_OBJECT,  sd_json_dispatch_variant, offsetof(struct image_deps, sysext), 0 },
    {}
  };
  _cleanup_(free_image_depsp) struct image_deps *e = NULL; /* XXX add _cleanup_ */
  int r;

  assert(res);

  e = calloc(1, sizeof(struct image_deps));
  if (e == NULL)
    oom();

  r = sd_json_dispatch(json, dispatch_table, SD_JSON_LOG|SD_JSON_ALLOW_EXTENSIONS, e);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse JSON content: %s\n", strerror(-r));
      return r;
    }

  r = sd_json_dispatch(e->sysext, dispatch_table, SD_JSON_LOG|SD_JSON_ALLOW_EXTENSIONS, e);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse JSON entries object: %s\n", strerror(-r));
      return r;
    }
  e->sysext = sd_json_variant_unref(e->sysext);

  *res = TAKE_PTR(e);

  return 0;
}

int
load_image_json(int fd, const char *path, struct image_deps ***images)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  unsigned line = 0, column = 0;
  int r;

  r = sd_json_parse_file_at(NULL, fd, path, 0, &json, &line, &column);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse json file (%s) %u:%u: %s\n",
	      path, line, column, strerror(-r));
      return r;
    }

  if (sd_json_variant_is_array(json))
    {
      size_t nr = sd_json_variant_elements(json);

      *images = calloc(nr+1, sizeof (struct image_deps *));
      if (*images == NULL)
	oom();
      (*images)[nr] = NULL;

      for (size_t i = 0; i < nr; i++)
	{
	  sd_json_variant *e = sd_json_variant_by_index(json, i);
	  if (!sd_json_variant_is_object(e))
	    {
	      fprintf(stderr, "entry is no object!\n");
	      return -EINVAL;
	    }

	  r = parse_image_deps(e, &(*images)[i]);
	  if (r < 0)
	    return r;
        }

      return 0;
    }
  else
    {
      *images = calloc(2, sizeof(struct image_deps *));
      if (*images == NULL)
	oom();
      (*images)[1] = NULL;

      return parse_image_deps(json, &(*images)[0]);
    }
}
