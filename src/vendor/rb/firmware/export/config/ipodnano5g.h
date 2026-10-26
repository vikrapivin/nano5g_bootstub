/*
 * This config file is for iPod Nano 5th Generation (S5L8730, N33)
 */

#define IPOD_ARCH 1

/* For Rolo and boot loader */
#define MODEL_NUMBER 134

#define MODEL_NAME   "Apple iPod Nano 5G"

/* define this if you have recording possibility */
#define HAVE_RECORDING
//#define HAVE_AGC
//#define HAVE_HISTOGRAM

/* Define bitmask of input sources - recordable bitmask can be defined
   explicitly if different */
#define INPUT_SRC_CAPS (SRC_CAP_MIC | SRC_CAP_LINEIN)

/* define the bitmask of hardware sample rates */
#define HW_SAMPR_CAPS   (SAMPR_CAP_44 | SAMPR_CAP_22 | SAMPR_CAP_11 \
                       | SAMPR_CAP_48 | SAMPR_CAP_24 | SAMPR_CAP_12 \
                       | SAMPR_CAP_32 | SAMPR_CAP_16 | SAMPR_CAP_8)

/* define the bitmask of recording sample rates */
#define REC_SAMPR_CAPS  (SAMPR_CAP_44 | SAMPR_CAP_22 | SAMPR_CAP_11 \
                       | SAMPR_CAP_48 | SAMPR_CAP_24 | SAMPR_CAP_12 \
                       | SAMPR_CAP_32 | SAMPR_CAP_16 | SAMPR_CAP_8)

/* define this if you have a bitmap LCD display */
#define HAVE_LCD_BITMAP

/* define this if you can flip your LCD */
//#define HAVE_LCD_FLIP

/* define this if you have a colour LCD */
#define HAVE_LCD_COLOR

/* define this if you want album art for this target */
#define HAVE_ALBUMART

/* define this to enable bitmap scaling */
#define HAVE_BMP_SCALING

/* define this to enable JPEG decoding */
#define HAVE_JPEG

/* define this if you can invert the colours on your LCD */
//#define HAVE_LCD_INVERT

/* Define this if your LCD can set contrast */
//#define HAVE_LCD_CONTRAST

/* LCD stays visible without backlight - simulator hint */
#define HAVE_TRANSFLECTIVE_LCD

/* define this if you have access to the quickscreen */
#define HAVE_QUICKSCREEN

/* define this if you would like tagcache to build on this target */
#define HAVE_TAGCACHE

/* define this if the unit uses a scrollwheel for navigation */
#define HAVE_SCROLLWHEEL
#define HAVE_WHEEL_ACCELERATION
#define WHEEL_ACCEL_START 270
#define WHEEL_ACCELERATION 3

/* Define this if you can read an absolute wheel position */
#define HAVE_WHEEL_POSITION

#define CONFIG_KEYPAD IPOD_4G_PAD

/* Define this if you can detect headphones */
#define HAVE_HEADPHONE_DETECTION

/* define this if you have a flash memory storage */
#define HAVE_FLASH_STORAGE

#define CONFIG_STORAGE STORAGE_NAND

/* The flash controller DMAs into the caller's buffer; buffers must be
 * cache-line aligned (the driver bounces anything else, the FAT code
 * bounces unaligned transfers before that) */
#define STORAGE_WANTS_ALIGN
#define STORAGE_NEEDS_BOUNCE_BUFFER

#define CONFIG_NAND 0

/* define this if at least one storage driver
   needs to do cleanup on shutdown */
#define HAVE_STORAGE_FLUSH

/* Expose write protection: bootloader read-only, main firmware writable. */
#define HAVE_STORAGE_READONLY

/* One sector is one page of Apple's FTL, 4096 bytes; the partition table
 * on the NAND counts in these units too */
#define SECTOR_SIZE 4096

/* LCD dimensions */
#define LCD_WIDTH  240
#define LCD_HEIGHT 376
/* sqrt(376^2 + 240^2) / 2.2 = 202.7 */
#define LCD_DPI 203
#define LCD_DEPTH  16   /* pseudo 262.144 colors */
#define LCD_PIXELFORMAT RGB565 /* rgb565 */

/* Define this if the LCD can shut down */
#define HAVE_LCD_SHUTDOWN

/* Define this if your LCD can be enabled/disabled */
//#define HAVE_LCD_ENABLE

/* Define this if your LCD can be put to sleep. HAVE_LCD_ENABLE
   should be defined as well. */
#ifndef BOOTLOADER
#define HAVE_LCD_SLEEP
#define HAVE_LCD_SLEEP_SETTING
#else /* BOOTLOADER */
#define DISABLE_ALPHA_BITMAP
#define DISABLE_BACKTRACE
#endif
#define HAVE_LCD_SLEEP

/* Define this to have CPU boosted while scrolling in the UI */
#define HAVE_GUI_BOOST

#define AB_REPEAT_ENABLE
#define ACTION_WPSAB_SINGLE ACTION_WPS_BROWSE

/* Define this to enable morse code input */
#define HAVE_MORSE_INPUT

/* Define this if you do software codec */
#define CONFIG_CODEC SWCODEC

// TODO
/* define this if you have a real-time clock */
//#define CONFIG_RTC RTC_NANO3G
#define CONFIG_RTC  0

/* Define if the device can wake from an RTC alarm */
//#define HAVE_RTC_ALARM

#define CONFIG_LCD LCD_IPOD6GNANO3G4G

/* Define the type of audio codec. TODO: the nano 5G codec is a different
 * part from the nano 4G's CS42L55 and is not documented yet; HAVE_CS42L55
 * only selects the generic codec driver so that the audio code compiles. */
#define HAVE_CS42L55
/* The audio hardware path (I2S, MCLK, codec I2C) is not available: with the
 * macro below the PCM, I2S and codec code compile but never touch the
 * hardware. The macro keeps the name it has in the nano 4G configuration. */
#define IPOD_NANO4G_AUDIO_HW_PENDING

#define HAVE_PCM_DMA_ADDRESS

/* Define this for LCD backlight available */
#define HAVE_BACKLIGHT
#define HAVE_BACKLIGHT_BRIGHTNESS

/* Define this if you have a software controlled poweroff */
#define HAVE_SW_POWEROFF

/* The number of bytes reserved for loadable codecs */
#define CODEC_SIZE 0x100000

/* The number of bytes reserved for loadable plugins */
#define PLUGIN_BUFFER_SIZE 0x80000

// TODO: actually these are the nano2g defines
#define BATTERY_CAPACITY_DEFAULT 400 /* default battery capacity */
#define BATTERY_CAPACITY_MIN     300 /* min. capacity selectable */
#define BATTERY_CAPACITY_MAX     500 /* max. capacity selectable */
#define BATTERY_CAPACITY_INC      10 /* capacity increment */

#define CONFIG_BATTERY_MEASURE VOLTAGE_MEASURE

/* Hardware controlled charging with monitoring */
#define CONFIG_CHARGING CHARGING_MONITOR

/* define current usage levels */
#define CURRENT_NORMAL     17  /* playback @48MHz clock, backlight off */
#define CURRENT_BACKLIGHT  23  /* maximum brightness */

/* define this if the unit can be powered or charged via USB */
#define HAVE_USB_POWER

#define HAVE_USB_CHARGING_ENABLE
/* A computer must grant the higher input-current budget: 500 mA only
 * after SET_CONFIGURATION (raising the PMU limit earlier is not safe) */
#define USB_CHARGING_REQUIRES_ENUMERATION

/* define this if the hardware can be powered off while charging */
#define HAVE_POWEROFF_WHILE_CHARGING

/* Define Apple remote tuner */
//#define CONFIG_TUNER IPOD_REMOTE_TUNER
//#define HAVE_RDS_CAP
//#define CONFIG_RDS RDS_CFG_PUSH

/* The exact type of CPU */
#define CONFIG_CPU S5L8730

/* Define this to the CPU frequency */
/* PLL0 = 24 MHz * 206 / 18 = 274.67 MHz */
#define CPU_FREQ        274000000

/* Define this if you have adjustable CPU frequency */
#define HAVE_ADJUSTABLE_CPU_FREQ

/* I2C interface */
#define CONFIG_I2C I2C_S5L8702

/* The size of the flash ROM */
#define FLASH_SIZE 0x100000

/* Offset ( in the firmware file's header ) to the file CRC */
#define FIRMWARE_OFFSET_FILE_CRC 0

/* Offset ( in the firmware file's header ) to the real data */
#define FIRMWARE_OFFSET_FILE_DATA 8

#define BOOTFILE_EXT "ipod"
#define BOOTFILE "rockbox." BOOTFILE_EXT
#define BOOTDIR "/.rockbox"

// TODO: no piezo driver for this model (the nano 4G has one)
//#define HAVE_HARDWARE_CLICK

/* Define this for FM radio input available */
#define HAVE_FMRADIO_IN

/* Still camera: OmniVision sensor behind the S5L8730 camera interface
 * (firmware/export/camera.h, plugin "photo") */
#define HAVE_CAMERA

/** Port-specific settings **/

#if 0
/* Main LCD contrast range and defaults */
#define MIN_CONTRAST_SETTING        1
#define MAX_CONTRAST_SETTING        30
#define DEFAULT_CONTRAST_SETTING    19 /* Match boot contrast */
#endif

/* Main LCD backlight brightness range and defaults */
#define MIN_BRIGHTNESS_SETTING      1
/* Qualified bring-up limit: backlight_hw_brightness scales by four,
 * so both the settings UI and the driver must keep LEDOUT <= 0x40. */
#define MAX_BRIGHTNESS_SETTING      0x10
#define DEFAULT_BRIGHTNESS_SETTING  0x10

/* USB */
#define CONFIG_USBOTG USBOTG_DESIGNWARE
#define USB_DW_CLOCK 0
#define USB_DW_TURNAROUND 5
/* logf() over USB serial (http://www.rockbox.org/wiki/PortalPlayerUsb) */
//#define USB_ENABLE_SERIAL
#define HAVE_USBSTACK
#define HAVE_USB_HID_MOUSE
/* The PMU interrupt reports USB power changes */
#define USB_STATUS_BY_EVENT
#define USB_VENDOR_ID 0x05AC
#define USB_PRODUCT_ID 0x1265
#define USB_DEVBSS_ATTR __attribute__((aligned(32)))
// TODO
//#define HAVE_BOOTLOADER_USB_MODE
#ifdef BOOTLOADER
#define USBPOWER_BTN_IGNORE (~0)
#endif

#define USB_READ_BUFFER_SIZE (1024*24)

/* Serial */
#ifdef BOOTLOADER
#if 0 /* Enable/disable LOGF_SERIAL for bootloader */
#define HAVE_SERIAL
#define ROCKBOX_HAS_LOGF
#define LOGF_SERIAL
#endif
#else /* !BOOTLOADER */
/* Bring-up: the UART / accessory-protocol path has not run on this
 * hardware yet; enable it once the main binary boots to the menu. */
#if 0
#define HAVE_SERIAL
/* Disable iAP when LOGF_SERIAL is enabled to avoid conflicts */
#ifndef LOGF_SERIAL
#define IPOD_ACCESSORY_PROTOCOL
#endif
#endif
#endif

/* Define this if you can switch on/off the accessory power supply */
#define HAVE_ACCESSORY_SUPPLY

/* Define this, if you can switch on/off the lineout */
#define HAVE_LINEOUT_POWEROFF

/* Define this if a programmable hotkey is mapped */
#define HAVE_HOTKEY
