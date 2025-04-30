# sysextmgr

`sysextmgr` is a set of command line utility and daemon to manage [systemd-sysext](https://manpages.opensuse.org/systemd-sysext) images on [openSUSE MicroOS](https://microos.opensuse.org).

`sysxtmgrcli` is the command line utility which communiates with `sysextmgrd` in the background. This allows even unprivileged users to list existing sysext images without the need to be setuid root.
`sysmgrd` is a server normally activated via a systemd socket, but which can also ran standalone as daemon.

## Directory Structure

Directories:
* `/var/lib/sysext-store` to store the sysext images
* `/etc/extensions` contains a symlink to the image

The reason for this is:

* Storing the images in `/var` means we can share them between snapshots
* Using symlinks in `/etc/extensions` means every snapshot:
  * will only see images fitting with this snapshot
  * rollback will also rollback the sysext images

## Workflows

For all downloads `systemd-pull` will be used, which is also able to verify the downloaded files.

### Import image

`sysextmgrcli` will differentiate two cases:

A full image name (including version and architecture) is specified:
* Download specific image to `/var/lib/sysext-store`.
* Create the symlink in `/etc/extensions`

Only the image name without version and architecture information are specified:
* Download the SHA256SUMs file from the remote repository.
* Check if there are matching image entries.
* Download the json files for this entries.
* Check if they are compatible with the OS.
* Download the image.
* Create symlink to `/etc/extionsions` in the running snapshot.

### Update image

`sysextmgrcli` will:
* Scan the local directory `/var/lib/sysext-store` for the existing images.
* Download the SHA256SUMs file from the remote repository.
* Check if there are newer versions for the installed images. If yes:
  * Download the `<image>.json` file.
  * Verify it machtes the OS version of the new snapshot.
  * Download the `<image>`.
  * Create symlink to `/etc/extionsions` inside the new snapshot

### Cleanup images

`sysextmgrcli` will:
* Check all snapshots for list of used images and remove the no longer needed ones.

## Dependency handling

The dependencies of sysext images are stored in a file inside of the image. To get the dependencies of an image you need to download and loopback mount it, which can end in a huge amount of data to download.

The solution is to provide a `<image>.json` file with the following structure:´, which contains all data:

```json
{
  "image_name": "strace-29.1.x86-64.raw",
  "sysext": {
    "ID": "opensuse-microos",
    "SYSEXT_LEVEL": "glibc-2.41",
    "VERSION_ID": "20250329",
    "SYSEXT_VERSION_ID": "29.1",
    "SYSEXT_SCOPE": "system",
    "ARCHITECTURE": "x86-64"
  }
}
```

## Configuration

The sysextmgr tools read an INI style configuration file following the [Configuration Files Specification](https://uapi-group.org/specifications/specs/configuration_files_specification/) of the [The Linux Userspace API (UAPI) Group](https://uapi-group.org/).

The search order for configuration files:
```
/etc/sysextmgr/sysextmgr.conf
/run/sysextmgr/sysextmgr.conf
/usr/share/sysextmgr/sysextmgr.conf
/etc/sysextmgr/sysextmgr.conf.d/*.conf
/run/sysextmgr/sysextmgr.conf.d/*.conf
/usr/share/sysextmgr/sysextmgr.conf.d/*.conf
```

Supported groups:
* *sysextmgrd* - the key/value pairs of this group are read by `sysextmgrd`.
* *default* - this key/value pairs of this group are read by `sysextmgrd` and `sysextmgrcli` if they are not specific in a tool specific section. But they are not necessarly supported by both.

Example configuration file:
```
[default]
url=https://download.opensuse.org/repositories/home:/kukuk:/sysext/mkosi/

[sysextmgrd]
verbose=true
```
