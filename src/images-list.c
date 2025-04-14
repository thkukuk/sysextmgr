/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>

#include <systemd/sd-json.h>

#include "basics.h"
#include "extension-util.h"
#include "sysext-cli.h"
#include "osrelease.h"
#include "extrelease.h"
#include "download.h"
#include "extract.h"
#include "tmpfile-util.h"
#include "strv.h"

static bool arg_verbose = false;

static int
image_filter(const struct dirent *de)
{
  if (endswith(de->d_name, ".raw") || endswith(de->d_name, ".img"))
    return 1;
  return 0;
}

static int
discover_images(const char *path, char ***result)
{
  struct dirent **de = NULL;

  assert(result);

  int num_dirs = scandir(path, &de, image_filter, alphasort);
  if (num_dirs < 0)
    return -errno;

  if (num_dirs > 0)
    {
      *result = malloc((num_dirs+1) * sizeof(char *));
      if (*result == NULL)
	oom();
      (*result)[num_dirs] = NULL;

      for (int i = 0; i < num_dirs; i++)
      {
	(*result)[i] = strdup(de[i]->d_name);
	if ((*result)[i] == NULL)
	  oom();
	free(de[i]);
      }
      free(de);
    }

  return 0;
}

static int
image_read_metadata(const char *name, struct image_deps **res)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-extrelease.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  int r;

  assert(name);
  assert(res);

  fd = mkostemp_safe(tmpfn);

  r = extract(SYSEXT_STORE_DIR, name, fd);
  if (r < 0)
    {
      fprintf(stderr, "Failed to extract extension-release from '%s': %s\n",
	      name, strerror(-r));
      return r;
    }
  else if (r > 0)
    {
      fprintf(stderr, "Failed to extract extension-release from '%s': systemd-dissect failed (%i)\n",
	      name, r);
      return -EINVAL;
    }

  r = load_ext_release(name, tmpfn, res);
  if (r < 0)
    return r;

  return 0;
}

static int
image_json_from_url(const char *url, const char *name, struct image_deps **res)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-json.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_free_ char *jsonfn = NULL;
  int r;

  assert(url);
  assert(res);

  fd = mkostemp_safe(tmpfn);

  jsonfn = malloc(strlen(name) + strlen(".json") + 1);
  char *p = stpcpy(jsonfn, name);
  strcpy(p, ".json");

  r = download(url, jsonfn, tmpfn, false /*XXX*/);
  if (r < 0)
    {
      fprintf(stderr, "Failed to download '%s' from '%s': %s",
	      jsonfn, url, strerror(-r));
      return r;
    }

  _cleanup_(free_image_deps_list) struct image_deps **images = NULL;
  r = load_image_json(fd, tmpfn, &images);
  if (r < 0)
    return r;

  if (images == NULL || images[0] == NULL)
    {
      fprintf(stderr, "No entry with dependencies found (%s)!\n", jsonfn);
      return -ENOENT;
    }

  if (images[1] == NULL)
      *res = TAKE_PTR(images[0]);
  else
    {
      /* XXX go through the list and search the corret image */
      /* XXX we cannot use TAKE_PTR else the rest of the list will not be free'd */
      fprintf(stderr, "More than one entry found, not implemented yet!\n");
      exit(EXIT_FAILURE);
    }

  return 0;
}

static int
image_list_from_url(const char *url, char ***result)
{
  _cleanup_(unlink_tempfilep) char name[] = "/tmp/sysext-SHA256SUMS.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_fclose_ FILE *fp = NULL;
  int r;

  assert(url);
  assert(result);

  fd = mkostemp_safe(name);

  r = download(url, "SHA256SUMS", name, false /*XXX*/);
  if (r < 0)
    {
      fprintf(stderr, "Failed to download 'SHA256SUMS' from '%s': %s",
	      url, strerror(-r));
      return r;
    }

  fp = fdopen(fd, "r");
  if (!fp)
    return -errno;

  size_t cur_entry = 0, max_entry = 10;
  *result = malloc((max_entry + 1) * sizeof(char *));
  if (*result == NULL)
    oom();
  (*result)[0] = NULL;

  _cleanup_(freep) char *line = NULL;
  size_t size = 0;
  ssize_t nread;

  while ((nread = getline(&line, &size, fp)) != -1)
    {
      /* Remove trailing newline character */
      if (nread && line[nread-1] == '\n')
	line[nread-1] = '\0';

      if (endswith(line, ".raw") || endswith(line, ".img"))
	{
	  /* get image name, skip SHA256SUM hash and spaces */
	  char *p = strchr(line, ' ');
	  while (*p == ' ')
	    ++p;

	  if (cur_entry == max_entry)
	    {
	      max_entry = max_entry * 2;
	      *result = realloc(*result, (max_entry + 1) * sizeof(char *));
	      if (*result == NULL)
		oom();
	    }
	  (*result)[cur_entry] = strdup(p);
	  if ((*result)[cur_entry] == NULL)
	    oom();
	  cur_entry++;
	  (*result)[cur_entry] = NULL;
	}
    }

  return 0;
}

struct image_entry {
  char *name;
  struct image_deps *deps;
  bool remote;
  bool installed;
  bool compatible;
};

static void
free_image_entry_list(struct image_entry ***list)
{
  if (!list)
    return;

  for (size_t i = 0; *list && (*list)[i] != NULL; i++)
    {
      free((*list)[i]->name);
      free_image_depsp(&((*list)[i]->deps));
      free((*list)[i]);
    }
  free(*list);
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

  return strcmp((*i_a)->name, (*i_b)->name);
}

int
main_list(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_fclose_ FILE *of = NULL;
  _cleanup_(freep) char *osrelease_id = NULL;
  _cleanup_(freep) char *osrelease_sysext_level = NULL;
  _cleanup_(freep) char *osrelease_version_id = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images = NULL;
  char *url = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "u:v", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'v':
	  arg_verbose = true;
	  break;
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (argc > optind)
    {
      fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
      usage(EXIT_FAILURE);
    }

  r = load_os_release(&osrelease_id, &osrelease_version_id, &osrelease_sysext_level);
  if (r < 0)
    return EXIT_FAILURE;

  _cleanup_strv_free_ char **list = NULL;

  if (url)
    {
      r = image_list_from_url(url, &list);
      if (r < 0)
	{
	  fprintf(stderr, "Fetching image list from '%s' failed: %s\n",
		  url, strerror(-r));
	  return -r;
	}
    }

  size_t n_remote = strv_length(list);
  if (n_remote > 0)
    {
      images = malloc((n_remote+1) * sizeof(struct image_entry *));
      if (images == NULL)
	oom();
      images[n_remote] = NULL;

      for (size_t i = 0; i < n_remote; i++)
	{
	  images[i] = calloc(1, sizeof(struct image_entry));
	  if (images[i] == NULL)
	    oom();
	  images[i]->name = strdup(list[i]);
	  if (images[i]->name == NULL)
	    oom();
	  images[i]->remote = true;

	  r = image_json_from_url(url, list[i], &(images[i]->deps));
	  if (r < 0)
	    return -r;

	  if (images[i]->deps)
	    images[i]->compatible = extension_release_validate(images[i]->name,
							       osrelease_id, osrelease_version_id,
							       osrelease_sysext_level, "system",
							       images[i]->deps, arg_verbose);
	}
    }

  list = strv_free(list);
  r = discover_images("/var/lib/sysext-store", &list);
  if (r < 0)
    {
      fprintf(stderr, "Scan local images failed: %s\n",
	      strerror(-r));
      return -r;
    }

  if (n_remote == 0 && list == NULL)
    {
      printf("No images found\n");
      return EXIT_SUCCESS;
    }

  size_t n_local = strv_length(list);
  if (n_local > 0)
    {
      size_t n;

      images = realloc(images, (n_remote + n_local + 1) * sizeof(struct image_entry *));
      if (images == NULL)
	oom();
      for (n = n_remote; n < (n_remote + n_local + 1); n++)
	images[n] = NULL;

      n = n_remote; /* append new images at the end of the list */
      STRV_FOREACH(name, list)
	{
	  bool found = false;

	  /* check if we know already the image */
	  for (size_t i = 0; i < n_remote; i++)
	    if (streq(*name, images[i]->name))
	      {
		images[i]->installed = true;
		found = true;
		break;
	      }

	  if (!found)
	    {
	      images[n] = calloc(1, sizeof(struct image_entry));
	      if (images[n] == NULL)
		oom();
	      images[n]->name = strdup(*name);
	      if (images[n]->name == NULL)
		oom();
	      images[n]->installed = true;
	      r = image_read_metadata(*name, &(images[n]->deps));
	      if (r < 0)
		return r;

	      if (images[n]->deps)
		images[n]->compatible = extension_release_validate(images[n]->name,
								   osrelease_id, osrelease_version_id,
								   osrelease_sysext_level, "system",
								   images[n]->deps, arg_verbose);
	      n++;
	    }
	}
    }

  /* sort list */
  qsort(images, n_local+n_remote, sizeof(struct image_entry *), image_cmp);

  /* XXX Use table_print_with_pager */
  printf (" A I C Name\n");
  for (size_t i = 0; images[i] != NULL; i++)
    {
      if (images[i]->remote)
	printf(" x");
      else
	printf("  ");
      if (images[i]->installed)
	printf(" x");
      else
	printf("  ");
      if (images[i]->compatible)
	printf(" x");
      else
	printf("  ");
      printf(" %s\n", images[i]->name);
    }
  printf("A = available, I = installed, C = commpatible\n");

  return EXIT_SUCCESS;
}
