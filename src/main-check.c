/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>
#include <stdbool.h>

#include "basics.h"
#include "sysextmgr.h"
#include "varlink-client.h"

static bool arg_verbose = false;
static bool arg_quiet = false;

struct update {
  bool success;
  char *error;
  sd_json_variant *contents_update;
  sd_json_variant *contents_broken;
};

static void
update_free(struct update *var)
{
  var->error = mfree(var->error);
  var->contents_update = sd_json_variant_unref(var->contents_update);
  var->contents_broken = sd_json_variant_unref(var->contents_broken);
}

struct image_data {
  char *old_name;
  char *new_name;
};

static void
image_data_free(struct image_data *var)
{
  var->old_name = mfree(var->old_name);
  var->new_name = mfree(var->new_name);
}

int
varlink_check(const char *url, const char *prefix)
{
  _cleanup_(update_free) struct update p = {
    .success = false,
    .error = NULL,
    .contents_update = NULL,
    .contents_broken = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success",      SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct update, success), 0 },
    { "ErrorMsg",     SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct update, error), SD_JSON_NULLABLE },
    { "Images",       SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct update, contents_update), SD_JSON_NULLABLE },
    { "BrokenImages", SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct update, contents_broken), SD_JSON_NULLABLE },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  const char *error_id = NULL;
  bool update_available = false;
  bool broken_images = false;
  int r;

  r = connect_to_sysextmgrd(&link, _VARLINK_SYSEXTMGR_SOCKET);
  if (r < 0)
    return r;

  if (url)
    {
      r = sd_json_buildo(&params,
                         SD_JSON_BUILD_PAIR("URL", SD_JSON_BUILD_STRING(url)));
      if (r < 0)
        {
          fprintf(stderr, "Failed to build param list: %s\n", strerror(-r));
        }
    }
  if (prefix)
    {
      r = sd_json_buildo(&params,
                         SD_JSON_BUILD_PAIR("Prefix", SD_JSON_BUILD_STRING(prefix)));
      if (r < 0)
        {
          fprintf(stderr, "Failed to build param list: %s\n", strerror(-r));
        }
    }

  if (arg_verbose)
    {
      r = sd_json_variant_merge_objectbo(&params,
                                         SD_JSON_BUILD_PAIR("Verbose", SD_JSON_BUILD_BOOLEAN(arg_verbose)));
      if (r < 0)
        {
          fprintf(stderr, "Failed to add verbose to parameter list: %s\n", strerror(-r));
          return r;
        }
    }

  r = sd_varlink_call(link, "org.openSUSE.sysextmgr.Check", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call Check method: %s\n", strerror(-r));
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

      fprintf(stderr, "Failed to call Check method: %s\n", error);
      return -EIO;
    }

  if ((p.contents_update == NULL || sd_json_variant_is_null(p.contents_update)) &&
       (p.contents_broken == NULL || sd_json_variant_is_null(p.contents_broken)))
    {
      printf("No updates found\n");
      return 0;
    }

  if (p.contents_update != NULL && !sd_json_variant_is_null(p.contents_update) && !sd_json_variant_is_array(p.contents_update))
    {
      fprintf(stderr, "JSON image update data is no array!\n");
      return -EINVAL;
    }

  if (p.contents_broken != NULL && !sd_json_variant_is_null(p.contents_broken) && !sd_json_variant_is_array(p.contents_broken))
    {
      fprintf(stderr, "JSON broken image data is no array!\n");
      return -EINVAL;
    }

  /* XXX Use table_print_with_pager */
  if (!arg_quiet && sd_json_variant_elements(p.contents_update) > 0)
    printf ("Old image -> New Image\n");

  for (size_t i = 0; i < sd_json_variant_elements(p.contents_update); i++)
    {
      _cleanup_(image_data_free) struct image_data e =
        {
          .old_name = NULL,
          .new_name = NULL
        };
      static const sd_json_dispatch_field dispatch_entry_table[] = {
        { "OldName", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct image_data, old_name), SD_JSON_MANDATORY },
        { "NewName", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct image_data, new_name), SD_JSON_NULLABLE},
        {}
      };

      sd_json_variant *entry = sd_json_variant_by_index(p.contents_update, i);
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

      if (e.new_name)
	update_available = true;

      if (!arg_quiet)
	{
	  if (e.new_name)
	    printf ("%s -> %s\n", e.old_name, e.new_name);
	  else
	    if (arg_verbose)
	      printf ("%s -> No compatible newer version found\n", e.old_name);
	}
    }

  /* XXX Use table_print_with_pager */
  if (!arg_quiet && sd_json_variant_elements(p.contents_broken) > 0)
    printf ("Incompatible installed images without update:\n");

  for (size_t i = 0; i < sd_json_variant_elements(p.contents_broken); i++)
    {
      static const sd_json_dispatch_field dispatch_entry_table[] = {
        { "IMAGE_NAME", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, 0, SD_JSON_MANDATORY },
        {}
      };
      _cleanup_free_ char *image_name = NULL;

      sd_json_variant *entry = sd_json_variant_by_index(p.contents_broken, i);
      if (!sd_json_variant_is_object(entry))
        {
          fprintf(stderr, "entry is no object!\n");
          return -EINVAL;
        }

      r = sd_json_dispatch(entry, dispatch_entry_table, SD_JSON_ALLOW_EXTENSIONS, &image_name);
      if (r < 0)
        {
          fprintf(stderr, "Failed to parse JSON sysext image entry: %s\n", strerror(-r));
          return r;
        }

      if (image_name)
	broken_images = true;

      if (!arg_quiet && image_name)
	printf ("%s\n", image_name);
    }

  /* no images for the installed version available */
  if (broken_images)
    return ENOMEDIUM;

  if (!update_available)
    return ENODATA;
  else
    return 0;
}



int
main_check(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"prefix", required_argument, NULL, 'p'},
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, '\0'}
  };
  char *url = NULL, *prefix = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "p:qu:v", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'p':
	  prefix = optarg;
          break;
	case 'v':
	  arg_verbose = true;
	  break;
	case 'q':
	  arg_quiet = true;
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

  r = varlink_check(url, prefix);
  if (r < 0)
    {
      if (VARLINK_IS_NOT_RUNNING(r))
        fprintf(stderr, "sysextmgrd not running!\n");
      return -r;
    }

  /* Return ENOMEDIUM if current image is incompatible and there is no update */
  if (r == ENOMEDIUM && arg_quiet)
    return ENOMEDIUM;

  /* Return ENODATA if there is no update and we should not print anything */
  if (r == ENODATA && arg_quiet)
    return ENODATA;

  return EXIT_SUCCESS;
}
