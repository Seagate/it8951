# it8951

it8951 is a collection of tools for the IT8951 EPD (Electronic Paper Display)
controllers.

## it8951_cmd

### Description

it8951_cmd is a command line tool allowing to send commands to the IT8951
controller.

### Usage examples

* Show device configuration:

```
$ sudo it895_cmd /dev/sgX info
```

* Load and display a full-screen image:

```
$ sudo it8951_cmd /dev/sgX fwrite image-800x600.pgm display
```

* Load a full-screen image but only display a 100x100 square of it:

```
$ sudo it8951_cmd /dev/sgX fwrite image-800x600.pgm display 400x300x100x100
```

* Load a 200x100 image at coordinates 200x200 and display it:

```
$ sudo it8951_cmd /dev/sgX load image-200x100.pgm 200x200 display 200x200x200x100
```

* Load two 100x100 dark grey squares in two opposites corners and display them:

```
$ sudo it8951_cmd /dev/sgX load 100x100x50 0x0 load 100x100x50 700x500 display
```

## it8951_fw

### Description

it8951_fw is a command line tool allowing to handle the firmware images on IT8951
controllers. It supports the firmware versions 0.2 and 0.3 (USI releases).

### Usage examples

* Display firmware information:

```
$ sudo it8951_fw /dev/sgX info
Firmware version    : USI_v.0.3
Boot screen support : yes
Number of BS images : 5
BS image 0 address  : 0x00180000
BS image 1 address  : 0x00200000
BS image 2 address  : 0x00280000
BS image 3 address  : 0x00300000
BS image 4 address  : 0x00380000
Active BS image     : 4
```

* Update firmware image:

```
$ sudo it8951_fw /dev/sgX write_fw /lib/firmware/it8951/IT8951_DX_4M_800x600_6M14T_96MHZ_85HZ_USI_v.0.3.bin
```

* Write boot screen image into slot 4:

```
$ sudo it8951_fw /dev/sgX write_bs pictures/boot_screen_only_one_800x600.pgm 4
```

* Select boot screen image 4 to be displayed at startup:

```
$ sudo it8951_fw /dev/sgX enable_bs 4
```

## it8951_flash

### Description

it8951_flash is a command line tool allowing to perform raw operations (erase,
read, write) on the SPI flash. It intends to be used for debugging purpose.

### Usage examples

* Dump the whole flash content (4MB):

```
$ sudo it8951_flash /dev/sgX read 0 flash.img
```

* Read a boot screen image (800x600, raw format) stored at the flash address
  0x180000:

```
$ sudo it8951_flash /dev/sgX read 0x180000 boot_screen_image 480000
```

* Erase a single block at address 0x170000:

```
$ sudo it8951_flash /dev/sgX erase 0x170000 65536
```

## Pathfinder

### Display resolution

- On Pathfinder 1.0 the display resolution is 600x800.
- On Pathfinder 1.5 the display resolution is 758x1024.

### Flash device

On both Pathfinder 1.0 and 1.5 a Macronix MX25L3206EM2I-12G SPI flash device is
embedded. Its total size is 4MB and the erase block size is 64KB.

## Firmware versions and layouts

### Version 0.2

This 0.2 firmware version introduces support for an image library called
"imglib". This library allows to store a boot screen image in flash with the
firmware. This image is automatically displayed at power-up. An "imglib" header
can be found in the firmware image. It is tagged with the "IT8951_ImageLib"
string. Here is the "imglib" header format:
```
Bytes  0-31: magic string "IT8951_ImageLib"
Bytes 32-33: number of images
Bytes 34-47: unused
Bytes 48-49: image index
Bytes 50-51: bits per pixel
Bytes 52-55: image offset
Bytes 56-57: image width
Bytes 58-59: image height
```
Although "imglib" seems designed to store several images, a single image is
supported.

The known firmware versions are 0.2, 0.2T1 and 0.2T2.

Basically the boot screen image is stored at the header address plus the value
of the header's "image offset" field.

### Version 0.3

The 0.3 firmware version (USI release) introduces support to store multiple
boot screen images in flash. The selected/active boot screen is automatically
displayed at startup.

Here is the firmware layout in flash:

```
0x0     -0x100000: reserved for FW
0x100000-0x160000: reserved for waveform
0x160000-0x170000: 64kB for Vcom block
0x170000-0x180000: 64kB for switch block
0x180000-0x3FFFFF: available space
```

The available place left (2.5MB or 40x 64KB blocks) can be used to store the
boot screen images:

- Pathfinder 1.0: up to 5 images (600x800).
- Pathfinder 1.5: up to 3 images (758x1024).

Here are the rules to follow when writing a boot screen image in flash:

- The image address must be greater than (or equal to) 0x180000.
- The image address must be aligned with a flash erase block (64KB).
- The image must not overlap another one.

At startup the IT8951 firmware retrieves the boot screen image address from the
"switch block" (at 0x170000). For example, if the boot screen image address is
0x200000, then the switch block is:

```
00000000: 4c4f 474f 5f20 0000 ffff ffff ffff ffff  LOGO_ ..........
00000010: ffff ffff ffff ffff ffff ffff ffff ffff  ................
...
0000fff0: ffff ffff ffff ffff ffff ffff ffff ffff  ................
```
