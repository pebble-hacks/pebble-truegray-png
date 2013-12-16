pebble-truegray-png
===================

Pebble smartwatch app using Pulse-Width-Modulation and Framebuffer tricks to render grayscale PNGs to Pebble E-Paper Black and White display.

### Pebble True Gray PNG Viewer
The Pebble Smartwatch display is a low-power E-Paper display, like E-Ink, that can only display black or white for each pixel.  By turning pixels on and off quickly a third color, gray, can be created.  

### PNG support
Includes Grayscale support for 1, 2, 4 and 8 bit
Currently we only can support 1 and 2 bit for size
And must be created without compression (not enough ram to decompress).

### True Gray using Phasing and Pulse-Width-Modulation
By turning pixels on and off very fast, the apparent average
makes the pixel look gray.  Unfortunately the effect can be seen
on a large scale as screen flicker.  By alternating consecutive pixels
to opposite states ( x=0 off, x=1 on ) the effect can be minimized.
Pulse width modulation is handled by switching state each pass,
but anything more than 50% cycle ( on->off->on->off) is noticeable,
so only 1 shade of gray works.  

### Converting images using imagemagick (graphicsmagick)
convert image_8bit.bmp -type Grayscale -colorspace Gray -depth 2 -define png:compression-level=0 image_2bit_nocompress.png

convert image_2bit.png -type Grayscale -colorspace Gray -depth 2 -define png:compression-level=0 image_2bit_nocompress.png

### Polishing images using the Gimp
Under Image->Mode->Grayscale
Under Colors->Posterize Choose 3 levels
Export to PNG with compression=0
Still need to convert using imagemagic for 2-bit depth

To improve image before posterize, use Colors->Brightness_&_Contrast
and slide contrast until image is closer to 3 colors, using brightness slider
to get more favorable results.
