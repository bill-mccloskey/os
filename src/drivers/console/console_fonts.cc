#include "drivers/console/console_fonts.h"

#include "drivers/console/Lat15-Fixed16.h"
#include "drivers/console/Lat15-VGA16.h"

unsigned char* GetFont_Fixed() {
  return drivers_console_Lat15_VGA16_psf;
}

unsigned char* GetFont_VGA() {
  return drivers_console_Lat15_Fixed16_psf;
}
