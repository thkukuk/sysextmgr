/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <systemd/sd-json.h>

#include "basics.h"
#include "image-deps.h"

void
free_image_deps(struct image_deps *e)
{
  if (!e)
    return;

  e->image_name_json = mfree(e->image_name_json);
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
    free_image_depsp(&(*images)[i]);

  free(*images);
}

void
dump_image_deps(struct image_deps *e)
{
  printf("image name: %s\n", e->image_name_json);
  printf("* sysext version_id: %s\n", e->sysext_version_id);
  printf("* sysext scope: %s\n", e->sysext_scope);
  printf("* id: %s\n", e->id);
  printf("* sysext_level: %s\n", e->sysext_level);
  printf("* version_id: %s\n", e->version_id);
  printf("* architecture: %s\n", e->architecture);
}

void
free_image_entry(struct image_entry *e)
{
  e->name = mfree(e->name);
  e->image_name = mfree(e->image_name);
  free_image_depsp(&(e->deps));
}

void
free_image_entryp(struct image_entry **e)
{
  if (!e || !*e)
    return;

  free_image_entry(*e);
  *e = mfree(*e);
}

void
free_image_entry_list(struct image_entry ***list)
{
  if (!list)
    return;

  for (size_t i = 0; *list && (*list)[i] != NULL; i++)
    free_image_entryp(&(*list)[i]);

  free(*list);
}
