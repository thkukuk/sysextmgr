/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>
#include <stdbool.h>

#include "basics.h"
#include "sysextmgr.h"
#include "varlink-client.h"

static bool arg_quiet = false;

struct install {
  bool success;
  char *error;
  char *installed;
};

static void
install_free (struct install *var)
{
  var->error = mfree(var->error);
  var->installed = mfree(var->installed);
}

int
varlink_install (const char *name, const char *url)
{
  _cleanup_(install_free) struct install p = {
    .success = false,
    .error = NULL,
    .installed = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success",   SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct install, success), 0 },
    { "ErrorMsg",  SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct install, error), 0 },
    { "Installed", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct install, installed), 0 },
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

  r = sd_json_buildo(&params,
		     SD_JSON_BUILD_PAIR("Install", SD_JSON_BUILD_STRING(name)));
  if (r < 0)
    {
      fprintf(stderr, "Failed to build parameter list: %s\n", strerror(-r));
      return r;
    }
  if (url)
    {
      r = sd_json_variant_merge_objectbo(&params,
					 SD_JSON_BUILD_PAIR("URL", SD_JSON_BUILD_STRING(url)));
      if (r < 0)
	{
	  fprintf(stderr, "Failed to add URL to parameter list: %s\n", strerror(-r));
	  return r;
	}
    }
  /* XXX add Verbose if set */

  r = sd_varlink_call(link, "org.openSUSE.sysextmgr.Install", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call Install method: %s\n", strerror(-r));
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

      fprintf(stderr, "Failed to call Install method: %s\n", error);
      return -EIO;
    }

  if (!arg_quiet)
    printf("%s\n", p.installed);

  return 0;
}

int
main_install(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, '\0'}
  };
  char *url = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "qu:", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'q':
	  arg_quiet = true;
	  break;
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (argc <= optind)
    {
      fprintf(stderr, "No images to install specified!\n");
      usage(EXIT_FAILURE);
    }

  printf("Installed:\n");
  for (int i = optind; i < argc; i++)
    {
      r = varlink_install(argv[i], url);
      if (r < 0)
	{
	  if (VARLINK_IS_NOT_RUNNING(r))
	    fprintf(stderr, "sysextmgrd not running!\n");
	  return -r;
	}
    }

  return EXIT_SUCCESS;
}
