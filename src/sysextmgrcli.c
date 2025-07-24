/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>

#include <libeconf.h>
#include <systemd/sd-json.h>

#include "basics.h"
#include "sysextmgr.h"

void
oom(void)
{
  fputs("ERROR: running out of memory!\n", stderr);
  exit(EXIT_FAILURE);
}

void
usage(int retval)
{
  FILE *output = (retval != EXIT_SUCCESS) ? stderr : stdout;

  fputs("Usage: sysextmgrcli [command] [options]\n", output);
  fputs("Commands: create-json, check, cleanup, dump-json, install, list, merge-json, update\n\n", output);

  fputs("create-json - create json file from release file\n", output);
  fputs("Options for create-json:\n", output);
  fputs("  -n, --name          Name of the image\n", output);
  fputs("  -i, --input         Input file in KEY=VALUE format\n", output);
  fputs("  -o, --output        Output file in json format\n", output);
  fputs("\n", output);

  fputs("check - Check if updates are available and all installed images are compatible\n", output);
  fputs("Options for check:\n", output);
  fputs("  -p, --prefix          Prefix to different root directory\n", output);
  fputs("  -q, --quiet           Don't print list of images but use return values\n", output);

  fputs("cleanup - Remove images no longer referenced\n", output);
  fputs("Options for check:\n", output);
  fputs("  -q, --quiet           Return 0 if images got removed, else ENODATA\n", output);

  fputs("dump-json - dump content of json file\n", output);
  fputs("Options for dump-json:\n", output);
  fputs("  <file 1> <file 2>...  Input files in json format\n", output);
  fputs("\n", output);

  fputs("install - Install newest compatible sysext image\n", output);
  fputs("Options for install:\n", output);
  fputs("  -u, --url URL         Remote directory with sysext images\n", output);
  fputs("  <name 1> <name 2>...  Names of the images to be installed\n", output);
  fputs("\n", output);

  fputs("list - list all images and if they are compatible\n", output);
  fputs("Options for merge-json:\n", output);
  fputs("  -u, --url URL         Remote directory with sysext images\n", output);
  fputs("  -v, --verbose         Verbose output\n", output);
  fputs("\n", output);

  fputs("merge-json - merge serveral json files into one json array\n", output);
  fputs("Options for merge-json:\n", output);
  fputs("  -o, --output FILE     Output file in json format\n", output);
  fputs("  <file 1> <file 2>...  Input files in json format\n", output);
  fputs("\n", output);

  fputs("update - Check if there are newer images available and update them\n", output);
  fputs("Options for update:\n", output);
  fputs("  -p, --prefix          Prefix to different root directory\n", output);
  fputs("  -q, --quiet           Return 0 if updates exist, else ENODATA\n", output);
  fputs("  -u, --url URL         Remote directory with sysext images\n", output);
  fputs("\n", output);

  fputs("Generic options:\n", output);
  fputs("  -h, --help          Display this help message and exit\n", output);
  fputs("  -v, --version       Print version number and exit\n", output);
  fputs("\n", output);
  exit(retval);
}

static int
main_create_json(int argc, char **argv)
{
  struct option const longopts[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"name", required_argument, NULL, 'n'},
    {NULL, 0, NULL, '\0'}
  };
  char *input = NULL, *output = NULL, *name = NULL;
  econf_file *key_file = NULL;
  char **keys;
  size_t key_number;
  econf_err error;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *array = NULL;
  _cleanup_fclose_ FILE *of = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "i:o:n:", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'i':
          input = optarg;
          break;
	case 'n':
	  name = optarg;
	  break;
	case 'o':
	  output = optarg;
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

  if (input == NULL)
    {
      fprintf(stderr, "No input file specified!\n");
      usage(EXIT_FAILURE);
    }

  if ((error = econf_readFile(&key_file, input, "=", "#")))
    {
      fprintf(stderr, "ERROR: couldn't read input file %s: %s\n",
	      input, econf_errString(error));
      return 1;
    }

  error = econf_getKeys(key_file, NULL, &key_number, &keys);
  if (error)
    {
      fprintf(stderr, "Error getting all keys: %s\n", econf_errString(error));
      return 1;
    }
  if (key_number == 0)
    {
      fprintf(stderr, "%s: No entries found?\n", input);
      return 1;
    }

  if (name)
    {
      r = sd_json_buildo(&json,
			 SD_JSON_BUILD_PAIR("image_name", SD_JSON_BUILD_STRING(name)));
      if (r < 0)
	{
	  fprintf(stderr, "Create json struct failed: %s\n", strerror(-r));
	  return -r;
	}
    }

  for (size_t i = 0; i < key_number; i++)
    {
      char *val = NULL;

      if ((error = econf_getStringValue(key_file, NULL, keys[i], &val)))
	{
	  fprintf(stderr, "Error reading %s: %s\n",
		  keys[i], econf_errString(error));
	  return 1;
	}
      r = sd_json_variant_merge_objectbo(&array,
					 SD_JSON_BUILD_PAIR_STRING(keys[i], val));
      if (r < 0)
	{
	  fprintf(stderr, "Appending key/value to json struct failed: %s\n", strerror(-r));
	  return -r;
	}

      r = sd_json_variant_merge_objectbo(&array,
					 SD_JSON_BUILD_PAIR_STRING(keys[i], val));
      if (r < 0)
	{
	  fprintf(stderr, "Appending key/value to json struct failed: %s\n", strerror(-r));
	  return -r;
	}

      free(val);
    }

  econf_free(keys);
  econf_free(key_file);

  r = sd_json_variant_merge_objectbo(&json, SD_JSON_BUILD_PAIR_VARIANT("sysext", array));
  if (r < 0)
    {
      fprintf(stderr, "Combining meta fields and sysext data failed: %s\n", strerror(-r));
      return -r;
    }

  if (output)
    {
      of = fopen(output, "w");
      if (of == NULL)
	{
	  fprintf(stderr, "Failed to create %s: %m", output);
	  return EXIT_FAILURE;
	}
    }
  r = sd_json_variant_dump(json, SD_JSON_FORMAT_NEWLINE, of, NULL);
  if (r < 0)
    {
      fprintf(stderr, "Failed to write json data: %s\n", strerror(-r));
      return -r;
    }

  return EXIT_SUCCESS;
}

static int
main_merge_json(int argc, char **argv)
{
  struct option const longopts[] = {
    {"output", required_argument, NULL, 'o'},
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_fclose_ FILE *of = NULL;
  char *output = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "o:", longopts, NULL)) != -1)
    {
      switch (c)
        {
	case 'o':
	  output = optarg;
	  break;
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (optind >= argc)
    {
      fprintf(stderr, "No input files specified!\n\n");
      usage(EXIT_FAILURE);
    }

  for (int i = optind; i < argc; i++)
    {
      _cleanup_(sd_json_variant_unrefp) sd_json_variant *single = NULL;
      unsigned line = 0, column = 0;

      r = sd_json_parse_file(NULL, argv[i], 0, &single, &line, &column);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to parse json file (%s) %u:%u: %s",
		  argv[i], line, column, strerror(-r));
	  return -r;
	}

      r = sd_json_variant_append_array(&json, single);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to merge json struct: %s\n", strerror(-r));
	  return -r;
	}
    }

  if (output)
    {
      of = fopen(output, "w");
      if (of == NULL)
	{
	  fprintf(stderr, "Failed to create %s: %m", output);
	  return EXIT_FAILURE;
	}
    }
  r = sd_json_variant_dump(json, SD_JSON_FORMAT_NEWLINE /* SD_JSON_FORMAT_PRETTY_AUTO */, of, NULL);
  if (r < 0)
    {
      fprintf(stderr, "Failed to write json data: %s\n", strerror(-r));
      return -r;
    }

  return EXIT_SUCCESS;
}

static int
main_dump_json(int argc, char **argv)
{
  struct option const longopts[] = {
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_fclose_ FILE *of = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "", longopts, NULL)) != -1)
    {
      switch (c)
        {
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (optind >= argc)
    {
      fprintf(stderr, "No input files specified!\n\n");
      usage(EXIT_FAILURE);
    }

  for (int i = optind; i < argc; i++)
    {
      _cleanup_(free_image_deps_list) struct image_deps **images = NULL;

      r = load_image_json(AT_FDCWD, argv[i], &images);
      if (r < 0)
	return EXIT_FAILURE;

      for (size_t j = 0; images && images[j] != NULL; j++)
	dump_image_deps(images[j]);
    }

  return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
  struct option const longopts[] = {
    {"help",     no_argument,       NULL, 'h'},
    {"version",  no_argument,       NULL, 'v'},
    {NULL, 0, NULL, '\0'}
  };
  int c;

  const char *progname = basename(argv[0]);

  if (strlen(progname) > 4)
    {
      /* check if this is XX-sysext-update
	 or sysext-update */
      if (streq(progname, "sysext-update") ||
	  streq(&progname[2], "-sysext-update"))
	return main_tukit_plugin(--argc, ++argv);
    }

  if (argc == 1)
    usage(EXIT_FAILURE);
  else if (strcmp(argv[1], "create-json") == 0)
    return main_create_json(--argc, ++argv);
  else if (strcmp(argv[1], "check") == 0)
    return main_check(--argc, ++argv);
  else if (strcmp(argv[1], "cleanup") == 0)
    return main_cleanup(--argc, ++argv);
  else if (strcmp(argv[1], "dump-json") == 0)
    return main_dump_json(--argc, ++argv);
  else if (strcmp(argv[1], "install") == 0)
    return main_install(--argc, ++argv);
  else if (strcmp(argv[1], "list") == 0)
    return main_list(--argc, ++argv);
  else if (strcmp(argv[1], "merge-json") == 0)
    return main_merge_json(--argc, ++argv);
  else if (strcmp(argv[1], "update") == 0)
    return main_update(--argc, ++argv);

  while ((c = getopt_long(argc, argv, "hv", longopts, NULL)) != -1)
    {
      switch (c)
	{
	case 'h':
	  usage(EXIT_SUCCESS);
	  break;
	case 'v':
	  printf("sysextmgrcli %s\n", VERSION);
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

  exit(EXIT_SUCCESS);
}
