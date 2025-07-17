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
  sd_json_variant *contents_json;
};

static void
update_free(struct update *var)
{
  var->error = mfree(var->error);
  var->contents_json = sd_json_variant_unref(var->contents_json);
}

int
varlink_cleanup(void)
{
  _cleanup_(update_free) struct update p = {
    .success = false,
    .error = NULL,
    .contents_json = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success",    SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct update, success), 0 },
    { "ErrorMsg",   SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct update, error), SD_JSON_NULLABLE },
    { "Images",     SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct update, contents_json), SD_JSON_NULLABLE },
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

  r = sd_varlink_call(link, "org.openSUSE.sysextmgr.Cleanup", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call Cleanup method: %s\n", strerror(-r));
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

      fprintf(stderr, "Failed to call Cleanup method: %s\n", error);
      return -EIO;
    }

  if (sd_json_variant_is_null(p.contents_json))
    {
      printf ("No sysext images removed.\n");
      return -ENODATA;
    }

  if (!sd_json_variant_is_array(p.contents_json))
    {
      fprintf(stderr, "JSON data 'Images' is no array!\n");
      return -EINVAL;
    }

  /* XXX Use table_print_with_pager */
  if (!arg_quiet)
    printf ("Removed sysext images:\n");

  for (size_t i = 0; i < sd_json_variant_elements(p.contents_json); i++)
    {
      static const sd_json_dispatch_field dispatch_entry_table[] = {
        { "IMAGE_NAME", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, 0, SD_JSON_MANDATORY },
        {}
      };
      _cleanup_free_ char *image_name = NULL;

      sd_json_variant *entry = sd_json_variant_by_index(p.contents_json, i);
      if (!sd_json_variant_is_object(entry))
        {
          fprintf(stderr, "entry is no object!\n");
          return -EINVAL;
        }

      r = sd_json_dispatch(entry, dispatch_entry_table, SD_JSON_ALLOW_EXTENSIONS, &image_name);
      if (r < 0)
        {
          fprintf(stderr, "Failed to parse JSON (image_name): %s\n", strerror(-r));
          return r;
        }

      if (!arg_quiet)
	printf("%s\n", strempty(image_name));
    }

  return 0;
}

int
main_cleanup(int argc, char **argv)
{
  struct option const longopts[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, '\0'}
  };
  int c, r;

  while ((c = getopt_long(argc, argv, "qv", longopts, NULL)) != -1)
    {
      switch (c)
        {
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

  r = varlink_cleanup();
  if (r < 0)
    {
      if (VARLINK_IS_NOT_RUNNING(r))
        fprintf(stderr, "sysextmgrd not running!\n");
      return -r;
    }

  /* Return ENODATA if there was nothing to remove and we should not print anything */
  if (r == ENODATA && arg_quiet)
    return ENODATA;

  return EXIT_SUCCESS;
}
