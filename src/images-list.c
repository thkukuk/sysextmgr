/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <systemd/sd-json.h>

#include "basics.h"
#include "extension-util.h"
#include "sysextmgr.h"
#include "osrelease.h"
#include "extrelease.h"
#include "download.h"
#include "extract.h"
#include "tmpfile-util.h"
#include "strv.h"
#include "images-list.h"
#include "log_msg.h"

static int
readlink_malloc(const char *path, const char *name, char **ret)
{
  _cleanup_free_ char *fn = NULL;
  _cleanup_free_ char *buf = NULL;
  ssize_t nbytes, bufsiz;
  struct stat sb;
  int r;

  r = join_path(path, name, &fn);
  if (r < 0)
    return r;

  if (lstat(fn, &sb) == -1)
    {
      perror("lstat");
      return -errno;
    }

  /* Add one to the link size, so that we can determine whether
     the buffer returnd by readlink() was truncated. */
  bufsiz = sb.st_size + 1;

  /* Some magic symlinks under (for example) /proc and /sys
     report 'st_size' as zero. In that case, take PATH_MAX as
     a "good enough" estimate. */
  if (sb.st_size == 0)
    bufsiz = PATH_MAX;

  buf = malloc(bufsiz);
  if (buf == NULL)
    return -ENOMEM;

  nbytes = readlink(fn, buf, bufsiz);
  if (nbytes == -1)
    return -errno;

  if (nbytes == bufsiz)
    {
      log_msg(LOG_CRIT, "Returned buffer may have been truncated!");
      exit(EXIT_FAILURE);
    }

  /* It doesn't contain a terminating null byte ('\0'). */
  buf[nbytes] = '\0';

  *ret = TAKE_PTR(buf);

  return 0;
}

static int
image_filter(const struct dirent *de)
{
  if (endswith(de->d_name, ".raw") || endswith(de->d_name, ".img"))
    return 1;
  return 0;
}

int
discover_images(const char *path, char ***result)
{
  struct dirent **de = NULL;
  int r;

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
	if (de[i]->d_type == DT_LNK)
	  {
	    _cleanup_free_ char *fn = NULL;
	    char *p;

	    r = readlink_malloc(path, de[i]->d_name, &fn);
	    if (r < 0)
	      return r;

	    p = strrchr(fn, '/');
	    if (p)
	      (*result)[i] = strdup(++p);
	    else
	      (*result)[i] = strdup(fn);
	  }
	else
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
directory_filter(const struct dirent *de)
{
  /* only directories, but not "." or ".." */
  if (de->d_type == DT_DIR && !streq(de->d_name, ".") && !streq(de->d_name, ".."))
    return 1;
  return 0;
}

static int
snapshot_list(const char *snapshot, struct image_entry **list, size_t n)
{
  struct dirent **de = NULL;
  int r;

  assert(list);

  _cleanup_free_ char *path = NULL;
  if (asprintf(&path, "/.snapshots/%s/snapshot/etc/extensions", snapshot) < 0)
    return -ENOMEM;

  int num_dirs = scandir(path, &de, image_filter, NULL /* alphasort */);
  if (num_dirs < 0)
    return -errno;

  for (int i = 0; i < num_dirs; i++)
    {
      _cleanup_free_ char *p = NULL;
      char *fn = NULL;

      if (de[i]->d_type == DT_LNK)
	{

	  r = readlink_malloc(path, de[i]->d_name, &p);
	  if (r < 0)
	    return r;

	  fn = strrchr(p, '/');
	  if (fn)
	    ++fn;
	  else
	    fn = p;
	}
      else
	fn = de[i]->d_name;

      for (size_t j = 0; j < n; j++)
	{
	  if (streq(fn, list[j]->image_name))
	    list[j]->refcount++;
	}

      free(de[i]);
    }

  free(de);

  return 0;
}

int
calc_refcount(struct image_entry **list, size_t n)
{
  struct dirent **de = NULL;
  int r = 0;

  if (n == 0)
    return 0;

  assert(list);

  int num_dirs = scandir("/.snapshots", &de, directory_filter, NULL /* alphasort */);
  if (num_dirs < 0)
    return -errno;

  for (int i = 0; i < num_dirs; i++)
    {
      r = snapshot_list(de[i]->d_name, list, n);
      free(de[i]);
      if (r < 0)
	break;
    }
  free(de);

  return r;
}

static int
image_read_metadata(const char *image_name, struct image_deps **res)
{
  _cleanup_(free_image_depsp) struct image_deps *image = NULL;
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-extrelease.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  int r;

  assert(image_name);
  assert(res);

  fd = mkostemp_safe(tmpfn);

  r = extract(SYSEXT_STORE_DIR, image_name, fd);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to extract extension-release from '%s': %s",
	      image_name, strerror(-r));
      return r;
    }
  else if (r > 0)
    {
      log_msg(LOG_ERR, "Failed to extract extension-release from '%s': systemd-dissect failed (%i)",
	      image_name, r);
      return -EINVAL;
    }

  r = load_ext_release(tmpfn, &image);
  if (r < 0)
    return r;

  if (image)
    *res = TAKE_PTR(image);

  return 0;
}

static int
image_json_from_url(const char *url, const char *image_name,
		    struct image_deps **res, bool verify_signature)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-json.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_free_ char *jsonfn = NULL;
  int r;

  assert(url);
  assert(res);

  fd = mkostemp_safe(tmpfn);

  jsonfn = malloc(strlen(image_name) + strlen(".json") + 1);
  char *p = stpcpy(jsonfn, image_name);
  strcpy(p, ".json");

  r = download(url, jsonfn, tmpfn, verify_signature);
  if (r != 0)
    {
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to download '%s' from '%s': %s",
		  jsonfn, url, strerror(-r));
	  return r;
	}
      else
	{
	  log_msg(LOG_ERR, "Failed to download '%s' from '%s': %s", jsonfn, url, wstatus2str(r));
	  if (WIFEXITED(r))
	    return -ENOENT;

	  return -EIO;
	}
    }

  _cleanup_(free_image_deps_list) struct image_deps **images = NULL;
  r = load_image_json(fd, tmpfn, &images);
  if (r < 0)
    return r;

  if (images == NULL || images[0] == NULL)
    {
      log_msg(LOG_NOTICE, "No entry with dependencies found (%s)!", jsonfn);
      return -ENOENT;
    }

  if (images[1] == NULL)
      *res = TAKE_PTR(images[0]);
  else
    {
      /* XXX go through the list and search the corret image */
      /* XXX we cannot use TAKE_PTR else the rest of the list will not be free'd */
      log_msg(LOG_ERR, "More than one entry found, not implemented yet!");
      exit(EXIT_FAILURE);
    }

  return 0;
}

static int
image_manifest_from_url(const char *url, const char *image_name,
			struct image_deps **res, bool verify_signature)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-image-manifest.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_free_ char *jsonfn = NULL;
  int r;

  assert(url);
  assert(res);

  fd = mkostemp_safe(tmpfn);

  jsonfn = malloc(strlen(image_name) + strlen(".manifest.gz") + 1);
  char *p = stpcpy(jsonfn, image_name);
  p = endswith(jsonfn, ".raw");
  if (!p)
    {
      log_msg(LOG_ERR, "The image '%s' has no supported suffix", jsonfn);
      return -EINVAL;
    }
  strcpy(p, ".manifest.gz");

  r = download(url, jsonfn, tmpfn, verify_signature);
  if (r != 0)
    {
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to download '%s' from '%s': %s",
		  jsonfn, url, strerror(-r));
	  return r;
	}
      else
	{
	  log_msg(LOG_ERR, "Failed to download '%s' from '%s': %s", jsonfn, url, wstatus2str(r));
	  if (WIFEXITED(r))
	    return -ENOENT;

	  return -EIO;
	}
    }

  _cleanup_(free_image_deps_list) struct image_deps **images = NULL;
  r = load_image_json(fd, tmpfn, &images);
  if (r < 0)
    return r;

  if (images == NULL || images[0] == NULL)
    {
      log_msg(LOG_NOTICE, "No entry with dependencies found (%s)!", jsonfn);
      return -ENOENT;
    }

  if (images[1] == NULL)
      *res = TAKE_PTR(images[0]);
  else
    {
      /* XXX go through the list and search the corret image */
      /* XXX we cannot use TAKE_PTR else the rest of the list will not be free'd */
      log_msg(LOG_ERR, "More than one entry found, not implemented yet!");
      exit(EXIT_FAILURE);
    }

  return 0;
}

static int
image_list_from_url(const char *url, char ***result, bool verify_signature)
{
  _cleanup_(unlink_tempfilep) char tmpfn[] = "/tmp/sysext-SHA256SUMS.XXXXXX";
  _cleanup_close_ int fd = -EBADF;
  _cleanup_fclose_ FILE *fp = NULL;
  int r;

  assert(url);
  assert(result);

  fd = mkostemp_safe(tmpfn);

  r = download(url, "SHA256SUMS", tmpfn, verify_signature);
  if (r != 0)
    {
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to download 'SHA256SUMS' from '%s': %s",
		  url, strerror(-r));
	  return r;
	}
      else
	{
	  log_msg(LOG_ERR, "Failed to download 'SHA256SUMS' from '%s': %s", url, wstatus2str(r));
	  if (WIFEXITED(r))
	    return -ENOENT;

	  return -EIO;
	}
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

int
image_remote_metadata(const char *url, struct image_entry ***res, size_t *nr,
		      const char *filter, bool verify_signature,
		      const struct osrelease *osrelease, bool verbose)
{
  _cleanup_strv_free_ char **list = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images = NULL;
  size_t n = 0, pos = 0;
  int r;

  assert(url);
  assert(res);

  r = image_list_from_url(url, &list, verify_signature);
  if (r < 0)
    return r;

  n = strv_length(list);
  if (n > 0)
    {
      images = calloc((n+1), sizeof(struct image_entry *));
      if (images == NULL)
	return -ENOMEM;

      for (size_t i = 0; i < n; i++)
	{
	  _cleanup_free_ char *name = NULL;
	  char *p;

	  name = strdup(list[i]);
	  if (name == NULL)
	    return -ENOMEM;

	  /* create "debug-tools" from "debug-tools-23.7.x86-64.raw" */
	  p = strrchr(name, '.'); /* raw */
	  if (p)
	    *p = '\0';
	  p = strrchr(name, '.'); /* arch */
	  if (p)
	    *p = '\0';
	  p = strrchr(name, '-'); /* version */
	  if (p)
	    *p = '\0';

	  if (filter && !streq(name, filter))
	    continue;

	  images[pos] = calloc(1, sizeof(struct image_entry));
	  if (images[pos] == NULL)
	    return -ENOMEM;
	  images[pos]->image_name = strdup(list[i]);
	  if (images[pos]->image_name == NULL)
	    return -ENOMEM;
	  images[pos]->name = strdup(name);
	  if (images[pos]->name == NULL)
	    return -ENOMEM;
	  images[pos]->remote = true;

	  r = image_json_from_url(url, list[i], &(images[pos]->deps), verify_signature);
	  if (r < 0)
	    {
	      if (r == -ENOENT)
		r = image_manifest_from_url(url, list[i], &(images[pos]->deps), verify_signature);
	      if (r < 0)
		return r;
	    }
	  if (images[pos]->deps && osrelease)
	    images[pos]->compatible =
	      extension_release_validate(images[pos]->image_name,
					 osrelease, "system",
					 images[pos]->deps, verbose);

	  pos++;
	}
    }

  if (nr)
    *nr = pos;
  if (images)
    *res = TAKE_PTR(images);

  return 0;
}

int
image_local_metadata(const char *store, struct image_entry ***res, size_t *nr,
		     const char *filter, const struct osrelease *osrelease,
		     bool read_metadata, bool verbose)
{
  _cleanup_strv_free_ char **list = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images = NULL;
  size_t n = 0, pos = 0;
  int r;

  r = discover_images(store, &list);
  if (r < 0)
    {
      if (r == -ENOENT)
	return 0;

      log_msg(LOG_ERR, "Scan local images failed: %s", strerror(-r));
      return r;
    }

  n = strv_length(list);
  if (n > 0)
    {
      images = calloc((n+1), sizeof(struct image_entry *));
      if (images == NULL)
	return -ENOMEM;

      for (size_t i = 0; i < n; i++)
	{
	  _cleanup_free_ char *name = NULL;
	  char *p;

	  name = strdup(list[i]);
	  if (name == NULL)
	    return -ENOMEM;

	  /* create "debug-tools" from "debug-tools-23.7.x86-64.raw" */
	  p = strrchr(name, '.'); /* raw */
	  if (p)
	    *p = '\0';
	  p = strrchr(name, '.'); /* arch */
	  if (p)
	    *p = '\0';
	  p = strrchr(name, '-'); /* version */
	  if (p)
	    *p = '\0';

	  if (filter && !streq(name, filter))
	    continue;

	  images[pos] = calloc(1, sizeof(struct image_entry));
	  if (images[pos] == NULL)
	    return -ENOMEM;
	  images[pos]->name = strdup(name);
	  if (images[pos]->name == NULL)
	    return -ENOMEM;
	  images[pos]->image_name = strdup(list[i]);
	  if (images[pos]->image_name == NULL)
	    return -ENOMEM;

	  images[pos]->local = true;

	  if (read_metadata)
	    {
	      r = image_read_metadata(list[i], &(images[pos]->deps));
	      if (r < 0)
		return r;
	    }

	  if (images[pos]->deps && osrelease)
	    images[pos]->compatible =
	      extension_release_validate(images[pos]->image_name,
					 osrelease, "system",
					 images[pos]->deps, verbose);

	  pos++;
	}
    }

  if (nr)
    *nr = pos;
  if (images)
    *res = TAKE_PTR(images);

  return 0;
}
