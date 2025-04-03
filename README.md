# sysext-cli

`sysext-cli` is a command line utility to manage [systemd-sysext](https://manpages.opensuse.org/systemd-sysext) images on [openSUSE MicroOS](https://microos.opensuse.org).

## Directory Structure

Directories:
* `/var/lib/sysext-store` to store the sysext images
* `/etc/extensions` contains a symlink to the image

The reason for this is:

* Storing the images in `/var` means we can share them between snapshots
* Using symlinks in `/etc/extensions` means every snapshot:
  * will only see images fitting with this snapshot
  * rollback will also rollback the sysext images

## Workflow

`sysext-cli` will:
* Scan the local directory `/var/lib/sysext-store` for the existing images.
* Download the SHA256SUMs file from the remote repository.
* Check if there are newer versions for the installed images. If yes:
  * Download the `<image>.json` file.
  * Verify it machtes the OS version of the new snapshot.
  * Download the `<image>`.
  * Create symlink to `/etc/extionsions` inside the new snapshot
* Cleanup: check all snapshots for list of used images and remove the no longer needed ones.

For all downloads `systemd-pull` will be used, which is also able to verify the downloaded files.
