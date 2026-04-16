/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>
#include <stdbool.h>
#include <libsmartcols/libsmartcols.h>

#include "basics.h"
#include "sysextmgr.h"
#include "varlink-client.h"
#include "pager.h"

static bool arg_verbose = false;
static bool arg_quiet = false;

struct update {
  bool success;
  char *error;
  sd_json_variant *contents_json;
};

static void
update_free (struct update *var)
{
  var->error = mfree(var->error);
  var->contents_json = sd_json_variant_unref(var->contents_json);
}

struct image_data {
  char *old_name;
  char *new_name;
};

static void
image_data_free (struct image_data *var)
{
  var->old_name = mfree(var->old_name);
  var->new_name = mfree(var->new_name);
}

int
varlink_update (const char *url, const char *prefix)
{
  _cleanup_(update_free) struct update p = {
    .success = false,
    .error = NULL,
    .contents_json = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success",    SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct update, success), 0 },
    { "ErrorMsg",   SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct update, error), SD_JSON_NULLABLE },
    { "Updated",    SD_JSON_VARIANT_ARRAY,   sd_json_dispatch_variant, offsetof(struct update, contents_json), SD_JSON_NULLABLE },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  const char *error_id = NULL;
  int r;
  struct libscols_table *table = NULL;
  struct libscols_line *line = NULL;

  r = connect_to_sysextmgrd(&link, _VARLINK_SYSEXTMGR_SOCKET);
  if (r < 0)
    return r;

  if (url)
    {
      r = sd_json_variant_merge_objectbo(&params,
					 SD_JSON_BUILD_PAIR("URL", SD_JSON_BUILD_STRING(url)));
      if (r < 0)
        {
          fprintf(stderr, "Failed to build param list: %s\n", strerror(-r));
          return r;
        }
    }
  if (prefix)
    {
      r = sd_json_variant_merge_objectbo(&params,
					 SD_JSON_BUILD_PAIR("Prefix", SD_JSON_BUILD_STRING(prefix)));
      if (r < 0)
        {
          fprintf(stderr, "Failed to build param list: %s\n", strerror(-r));
          return r;
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

  r = sd_varlink_call(link, "org.openSUSE.sysextmgr.Update", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call Update method: %s\n", strerror(-r));
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

      fprintf(stderr, "Failed to call Update method: %s\n", error);
      return -EIO;
    }

  if (p.contents_json == NULL || sd_json_variant_is_null(p.contents_json))
    {
      printf("No updates found\n");
      return 0;
    }

  if (!sd_json_variant_is_array(p.contents_json))
    {
      fprintf(stderr, "JSON 'Data' is no array!\n");
      return -EINVAL;
    }

  if (!arg_quiet && sd_json_variant_elements(p.contents_json) > 0)
    {
      /* Initialize the table */
      table = scols_new_table();
      if (!table)
        {
	  fprintf(stderr, "Failed to allocate table\n");
	  return -EIO;
        }

      // Define Column Headers
      scols_table_new_column(table, "Old image", 0, 0);
      scols_table_new_column(table, "New image", 0, 0);
      scols_table_set_column_separator(table, " -> ");
    }

  for (size_t i = 0; i < sd_json_variant_elements(p.contents_json); i++)
    {
      _cleanup_(image_data_free) struct image_data e =
        {
          .old_name = NULL,
          .new_name = NULL
        };
      static const sd_json_dispatch_field dispatch_entry_table[] = {
        { "OldName", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct image_data, old_name), SD_JSON_MANDATORY },
        { "NewName", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct image_data, new_name), SD_JSON_NULLABLE },
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

      if (!arg_quiet)
	{
          line = scols_table_new_line(table, NULL);
          scols_line_sprintf(line, 0, "%s", e.old_name);
          scols_line_sprintf(line, 1, "%s", strna(e.new_name));
	}
    }

  if (table)
    {
      /* Setup Pager and Print */
      pager(table,"");

      scols_unref_table(table);
    }

  return 0;
}

int
main_update(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"quiet", no_argument, NULL, 'q'},
    {"verbose", no_argument, NULL, 'v'},
    {"prefix", required_argument, NULL, 'p'},
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

  r = varlink_update(url, prefix);
  if (r < 0)
    {
      if (VARLINK_IS_NOT_RUNNING(r))
        fprintf(stderr, "sysextmgrd not running!\n");
      return -r;
    }

  return EXIT_SUCCESS;
}
