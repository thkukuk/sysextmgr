/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>
#include <stdbool.h>

#include "basics.h"
#include "sysext-cli.h"
#include "varlink-client.h"

static bool arg_verbose = false;

struct list_images {
  bool success;
  char *error;
  sd_json_variant *contents_json;
};

static void
list_images_free (struct list_images *var)
{
  var->error = mfree(var->error);
  var->contents_json = sd_json_variant_unref(var->contents_json);
}

struct image_data {
  char *name;              /* name of the image, e.g. "gcc" */
  char *image_name;         /* full image name, e.g. "gcc-30.3.x86-64.raw" */
  char *sysext_version_id;
  char *sysext_scope;
  char *id;
  char *sysext_level;
  char *version_id;
  char *architecture;
  bool remote;
  bool local;
  bool installed;
  bool compatible;

};

static void
image_data_free (struct image_data *var)
{
  var->name = mfree(var->name);
  var->image_name = mfree(var->image_name);
  var->sysext_version_id = mfree(var->sysext_version_id);
  var->sysext_scope = mfree(var->sysext_scope);
  var->id = mfree(var->id);
  var->sysext_level = mfree(var->sysext_level);
  var->version_id = mfree(var->version_id);
  var->architecture = mfree(var->architecture);
}

int
varlink_list_images (const char *url)
{
  _cleanup_(list_images_free) struct list_images p = {
    .success = false,
    .error = NULL,
    .contents_json = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success",    SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct list_images, success), 0 },
    { "ErrorMsg",   SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct list_images, error), 0 },
    { "Images",     SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct list_images, contents_json), 0 },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  const char *error_id = NULL;
  int r;

  r = connect_to_sysextmgrd(&link, _VARLINK_SYSEXTMGR_SOCKET);
  if (r < 0)
    return r;

  /* XXX add Verbose */
  if (url)
    {
      r = sd_json_buildo(&params,
			 SD_JSON_BUILD_PAIR("URL", SD_JSON_BUILD_STRING(url)));
      if (r < 0)
	{
	  fprintf(stderr, "Failed to build param list: %s\n", strerror(-r));
	}
    }
  r = sd_varlink_call(link, "org.openSUSE.sysextmgr.ListImages", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call ListImages method: %s\n", strerror(-r));
      return r;
    }
  /* dispatch before checking error_id, we may need the result for the error
     message */
  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse JSON answer: %s\n", strerror(-r));
      return r;
    }

  if (error_id && strlen(error_id) > 0)
    {
      const char *error = NULL;

      if (p.error)
	error = p.error;
      else
	error = error_id;

      fprintf(stderr, "Failed to call ListImages method: %s\n", error);
      return -EIO;
    }

  if (p.contents_json == NULL)
    {
      printf("No images found\n");
      return 0;
    }
  if (!sd_json_variant_is_array(p.contents_json))
    {
      fprintf(stderr, "JSON 'Data' is no array!\n");
      return -EINVAL;
    }

  /* XXX Use table_print_with_pager */
  printf (" R L I C Name\n");

  for (size_t i = 0; i < sd_json_variant_elements(p.contents_json); i++)
    {
      _cleanup_(image_data_free) struct image_data e =
	{
	  .name = NULL,
	  .image_name = NULL,
	  .sysext_version_id = NULL,
	  .sysext_scope = NULL,
	  .id = NULL,
	  .sysext_level = NULL,
	  .version_id = NULL,
	  .architecture = NULL,
	  .remote = false,
	  .local = false,
	  .installed = false,
	  .compatible = false,
	};
      static const sd_json_dispatch_field dispatch_entry_table[] = {
        { "NAME",              SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, name), SD_JSON_MANDATORY },
        { "IMAGE_NAME",        SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, image_name), SD_JSON_MANDATORY },
        { "SYSEXT_VERSION_ID", SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, sysext_version_id), SD_JSON_MANDATORY },
	{ "SYSEXT_SCOPE",      SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, sysext_scope), SD_JSON_NULLABLE},
	{ "ID",                SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, id), SD_JSON_NULLABLE},
	{ "SYSEXT_LEVEL",      SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, sysext_level), SD_JSON_NULLABLE},
	{ "VERSION_ID",        SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, version_id), SD_JSON_NULLABLE},
	{ "ARCHITECTURE",      SD_JSON_VARIANT_STRING, sd_json_dispatch_string,   offsetof(struct image_data, architecture), SD_JSON_NULLABLE},
	{ "LOCAL",             SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct image_data, local), 0},
	{ "REMOTE",            SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct image_data, remote), 0},
	{ "INSTALLED",         SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct image_data, installed), 0},
	{ "COMPATIBLE",        SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct image_data, compatible), 0},
        {}
      };

      sd_json_variant *entry = sd_json_variant_by_index(p.contents_json, i);
      if (!sd_json_variant_is_object(entry))
        {
          fprintf(stderr, "entry is no object!\n");
          return -EINVAL;
        }

      r = sd_json_dispatch(entry, dispatch_entry_table, SD_JSON_ALLOW_EXTENSIONS, &e);
      if (r < 0)
        {
	  fprintf(stderr, "Failed to parse JSON sysext image entry: %s\n", strerror(-r));
          return r;
        }

      if (e.remote)
	printf(" x");
      else
	printf("  ");
      if (e.local)
	printf(" x");
      else
	printf("  ");
      if (e.installed)
	printf(" x");
      else
	printf("  ");
      if (e.compatible)
	printf(" x");
      else
	printf("  ");
      printf(" %s\n", e.image_name);
    }

  printf("R = remote, L = local, I = installed, C = commpatible\n");

  return 0;
}

int
main_list(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, '\0'}
  };
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

  r = varlink_list_images(url);
  if (r < 0)
    {
      if (VARLINK_IS_NOT_RUNNING(r))
	fprintf(stderr, "sysextmgrd not running!\n");
      return r;
    }

  return EXIT_SUCCESS;
}
