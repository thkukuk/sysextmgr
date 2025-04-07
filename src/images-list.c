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
#include "download.h"
#include "tmpfile-util.h"
#include "strv.h"

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
image_json_from_url(const char *url, const char *name, struct image_deps **res)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-json.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_(freep) char *jsonfn;
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

int
main_list(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
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

  /* XXX We need to mount the image and read the extension release file! */

  while ((c = getopt_long(argc, argv, "u:", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
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

  if (url == NULL)
    {
      fprintf(stderr, "No url specified!\n");
      usage(EXIT_FAILURE);
    }

  r = load_os_release(&osrelease_id, &osrelease_version_id, &osrelease_sysext_level);
  if (r < 0)
    return EXIT_FAILURE;

  char **list = NULL;
  r = image_list_from_url(url, &list);
  if (r < 0)
    {
      fprintf(stderr, "Fetching image list from '%s' failed: %s\n",
	      url, strerror(-r));
      return -r;
    }

  size_t n = strv_length(list);
  if (n > 0)
    {
      images = malloc((n+1) * sizeof(struct image_entry *));
      if (images == NULL)
	oom();
      images[n] = NULL;

      for (size_t i = 0; i < n; i++)
	{
	  images[i] = calloc(1, sizeof(struct image_entry));
	  if (images[i] == NULL)
	    oom();
	  images[i]->name = list[i];
	  images[i]->remote = true;

	  r = image_json_from_url(url, list[i], &(images[i]->deps));
	  if (r < 0)
	    return -r;

	  if (images[i]->deps)
	    images[i]->compatible = extension_release_validate(images[i]->name,
							       osrelease_id, osrelease_version_id,
							       osrelease_sysext_level, "system",
							       images[i]->deps, true /* XXX */);
	}
      free(list);
    }

  _cleanup_strv_free_  char **result = NULL;
  r = discover_images("/var/lib/sysext-store", &result);
  if (r < 0)
    {
      fprintf(stderr, "Scan local images failed: %s\n",
	      strerror(-r));
      return -r;
    }

  if (result == NULL)
    {
      printf("No images found\n");
      return EXIT_SUCCESS;
    }

  printf (" A I C Name\n");
  for (size_t i = 0; images[i] != NULL; i++)
    {
      /* XXX Use table_print_with_pager */
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
