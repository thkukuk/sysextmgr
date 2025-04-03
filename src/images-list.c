/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>

#include <libeconf.h>
#include <systemd/sd-json.h>

#include "basics.h"
#include "extension-util.h"
#include "sysext-cli.h"

static int
load_os_release(char **id, char **version_id, char **sysext_level)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;

  const char *osrelease = NULL;
  if (access("/etc/os-release", F_OK) == 0)
    osrelease = "/etc/os-release";
  else
    osrelease = "/usr/lib/os-release";

  if ((error = econf_readFile(&key_file, osrelease, "=", "#")))
    {
      fprintf(stderr, "ERROR: couldn't read %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ID", id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'ID' from %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "VERSION_ID", version_id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'VERSION_ID' from %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_LEVEL", sysext_level))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_LEVEL' from %s: %s\n",
	      osrelease, econf_errString(error));
      return -1;
    }

  return 0;
}

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
  if(num_dirs > 0)
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

int
main_list(int argc, char **argv)
{
  struct option const longopts[] = {
    {"json", required_argument, NULL, 'j'},
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_fclose_ FILE *of = NULL;
  _cleanup_(freep) char *osrelease_id = NULL;
  _cleanup_(freep) char *osrelease_sysext_level = NULL;
  _cleanup_(freep) char *osrelease_version_id = NULL;
  char *jf = NULL;
  int c, r;

  /* XXX We need to mount the image and read the extension release file! */

  while ((c = getopt_long(argc, argv, "j:", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'j':
          jf = optarg;
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

  if (jf == NULL)
    {
      fprintf(stderr, "No json input file specified!\n");
      usage(EXIT_FAILURE);
    }

  r = load_os_release(&osrelease_id, &osrelease_version_id, &osrelease_sysext_level);
  if (r < 0)
    return EXIT_FAILURE;

  char **result = NULL;
  r = discover_images("/var/lib/sysext-store", &result);
  if (r < 0)
    return EXIT_FAILURE;

  if (result == NULL)
    {
      printf("No images found\n");
      return EXIT_SUCCESS;
    }

  _cleanup_(free_images_list) struct image_entry **images = NULL;

  r = load_image_entries(jf, &images);
  if (r != 0)
    return EXIT_FAILURE;

  for (size_t i = 0; result[i] != NULL; i++)
    {
      int valid = 0;

      for (size_t j = 0; images && images[j] != NULL; j++)
	{
	  if (streq(result[i], images[j]->image_name))
	    {
	      valid = extension_release_validate(images[j]->image_name,
						 osrelease_id, osrelease_version_id,
						 osrelease_sysext_level, "system",
						 images[j], false /* XXX */);
	      break;
	    }
	}
      if (valid)
	printf("%s: compatible\n", result[i]);
      else
	printf("%s: incompatible\n", result[i]);
    }

  return EXIT_SUCCESS;
}
