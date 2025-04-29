//SPDX-License-Identifier: GPL-2.0-or-later

#include "varlink-org.openSUSE.sysextmgr.h"

static SD_VARLINK_DEFINE_STRUCT_TYPE(ImageData,
				     SD_VARLINK_FIELD_COMMENT("Name without version/arch/suffix of the image"),
				     SD_VARLINK_DEFINE_FIELD(NAME,              SD_VARLINK_STRING, 0),
				     SD_VARLINK_FIELD_COMMENT("Full image name including version/arch/suffix"),
				     SD_VARLINK_DEFINE_FIELD(IMAGE_NAME,        SD_VARLINK_STRING, 0),
				     SD_VARLINK_FIELD_COMMENT("Sysext image version"),
				     SD_VARLINK_DEFINE_FIELD(SYSEXT_VERSION_ID, SD_VARLINK_STRING, 0),
				     SD_VARLINK_FIELD_COMMENT("Sysext image scope"),
				     SD_VARLINK_DEFINE_FIELD(SYSEXT_SCOPE,      SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Expected ID of OS (os-release)"),
				     SD_VARLINK_DEFINE_FIELD(ID,                SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Sysext level to which the sysext image is compatible"),
				     SD_VARLINK_DEFINE_FIELD(SYSEXT_LEVEL,      SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Expected ID of OS (os-release)"),
				     SD_VARLINK_DEFINE_FIELD(VERSION_ID,        SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Architecture of machine"),
				     SD_VARLINK_DEFINE_FIELD(ARCHITECTURE,      SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Image is local available"),
				     SD_VARLINK_DEFINE_FIELD(LOCAL,             SD_VARLINK_BOOL,   SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Image is remote available at URL"),
				     SD_VARLINK_DEFINE_FIELD(REMOTE,            SD_VARLINK_BOOL,   SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Image is installed (linked into /etc/extensions)"),
				     SD_VARLINK_DEFINE_FIELD(INSTALLED,         SD_VARLINK_BOOL,   SD_VARLINK_NULLABLE),
				     SD_VARLINK_FIELD_COMMENT("Image is compatible to installed OS and HW architecture"),
				     SD_VARLINK_DEFINE_FIELD(COMPATIBLE,        SD_VARLINK_BOOL,   SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_STRUCT_TYPE(UpdatedImage,
				     SD_VARLINK_FIELD_COMMENT("Old Image Name"),
				     SD_VARLINK_DEFINE_FIELD(OldImage, SD_VARLINK_STRING, 0),
				     SD_VARLINK_FIELD_COMMENT("New Image Name"),
				     SD_VARLINK_DEFINE_FIELD(NewImage, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                Install,
		SD_VARLINK_FIELD_COMMENT("Name of sysext images"),
                SD_VARLINK_DEFINE_INPUT(Install, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("URL of remote sysext images"),
                SD_VARLINK_DEFINE_INPUT(URL, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Verbose logging to journald"),
                SD_VARLINK_DEFINE_INPUT(Verbose, SD_VARLINK_BOOL, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("If call succeeded"),
                SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0),
                SD_VARLINK_FIELD_COMMENT("Data of sysext images"),
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(Images, ImageData, SD_VARLINK_ARRAY | SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Error Message"),
                SD_VARLINK_DEFINE_OUTPUT(ErrorMsg, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                ListImages,
                SD_VARLINK_FIELD_COMMENT("URL of remote sysext images, requires root rights"),
                SD_VARLINK_DEFINE_INPUT(URL, SD_VARLINK_STRING,  SD_VARLINK_NULLABLE),
		SD_VARLINK_FIELD_COMMENT("Verbose logging to journald"),
		SD_VARLINK_DEFINE_INPUT(Verbose, SD_VARLINK_BOOL, SD_VARLINK_NULLABLE),
		SD_VARLINK_FIELD_COMMENT("If call succeeded"),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0),
                SD_VARLINK_FIELD_COMMENT("Data of sysext images"),
		SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(Images, ImageData, SD_VARLINK_ARRAY | SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Error Message"),
                SD_VARLINK_DEFINE_OUTPUT(ErrorMsg, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                Update,
                SD_VARLINK_FIELD_COMMENT("URL of remote sysext images, requires root rights"),
                SD_VARLINK_DEFINE_INPUT(URL, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
		SD_VARLINK_FIELD_COMMENT("Verbose logging to journald"),
		SD_VARLINK_DEFINE_INPUT(Verbose, SD_VARLINK_BOOL, SD_VARLINK_NULLABLE),
		SD_VARLINK_FIELD_COMMENT("If call succeeded"),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0),
                SD_VARLINK_FIELD_COMMENT("List of updated images"),
		SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(Images, UpdatedImage, SD_VARLINK_ARRAY | SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Error Message"),
                SD_VARLINK_DEFINE_OUTPUT(ErrorMsg, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
		Quit,
		SD_VARLINK_FIELD_COMMENT("Optional error code for exit function"),
		SD_VARLINK_DEFINE_INPUT(ExitCode, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
		SD_VARLINK_FIELD_COMMENT("If operation succeeded"),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
		Ping,
		SD_VARLINK_FIELD_COMMENT("If service is alive"),
		SD_VARLINK_DEFINE_OUTPUT(Alive, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
                SetLogLevel,
                SD_VARLINK_FIELD_COMMENT("The maximum log level, using BSD syslog log level integers."),
                SD_VARLINK_DEFINE_INPUT(Level, SD_VARLINK_INT, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                GetEnvironment,
                SD_VARLINK_FIELD_COMMENT("Returns the current environment block, i.e. the contents of environ[]."),
                SD_VARLINK_DEFINE_OUTPUT(Environment, SD_VARLINK_STRING, SD_VARLINK_NULLABLE|SD_VARLINK_ARRAY));

static SD_VARLINK_DEFINE_ERROR(NoEntryFound);
static SD_VARLINK_DEFINE_ERROR(InternalError);
static SD_VARLINK_DEFINE_ERROR(DownloadError);

SD_VARLINK_DEFINE_INTERFACE(
                org_openSUSE_sysextmgr,
                "org.openSUSE.sysextmgr",
		SD_VARLINK_INTERFACE_COMMENT("SysextMgr control APIs"),
		SD_VARLINK_SYMBOL_COMMENT("Install newest compatible image with this name"),
                &vl_method_Install,
		SD_VARLINK_SYMBOL_COMMENT("List all images including dependencies"),
                &vl_method_ListImages,
		SD_VARLINK_SYMBOL_COMMENT("Update installed images"),
                &vl_method_Update,
 		SD_VARLINK_SYMBOL_COMMENT("Stop the daemon"),
                &vl_method_Quit,
		SD_VARLINK_SYMBOL_COMMENT("Checks if the service is running."),
                &vl_method_Ping,
                SD_VARLINK_SYMBOL_COMMENT("Sets the maximum log level."),
                &vl_method_SetLogLevel,
                SD_VARLINK_SYMBOL_COMMENT("Get current environment block."),
                &vl_method_GetEnvironment,
		SD_VARLINK_SYMBOL_COMMENT("No entry found"),
                &vl_error_NoEntryFound,
		SD_VARLINK_SYMBOL_COMMENT("Internal Error"),
		&vl_error_InternalError,
		SD_VARLINK_SYMBOL_COMMENT("Download Error"),
		&vl_error_DownloadError);
