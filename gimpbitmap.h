#ifndef _GIMPBITMAP_
#define _GIMPBITMAP_

typedef struct  {
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
  unsigned char	 pixel_data[64 * 64 * 3 + 1];
} gimp64x64bitmap;

#endif