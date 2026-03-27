#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <stdio.h>
#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif
#include "libretro.h"

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240
#define VIDEO_PIXELS VIDEO_WIDTH * VIDEO_HEIGHT

static uint8_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static bool use_audio_cb;
static float last_aspect;
static float last_sample_rate;
char retro_base_directory[4096];
char retro_game_path[4096];

static inline void put_pixel(int x, int y, uint16_t color);

// =================================================== FOR SCREEN DEBUG ===================================================
static const uint8_t font8x8[128][8] = {
    ['A'] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00},
    ['B'] = {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},
    ['C'] = {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00},
    ['D'] = {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00},
    ['E'] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00},
    ['F'] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00},
    ['G'] = {0x3C,0x42,0x40,0x40,0x4E,0x42,0x3C,0x00},
    ['H'] = {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},
    ['I'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['L'] = {0x40,0x40,0x40,0x40,0x40,0x42,0x7E,0x00},
    ['O'] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['N'] = {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00},
    ['P'] = {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00},
    ['R'] = {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00},
    ['S'] = {0x3C,0x40,0x40,0x3C,0x02,0x02,0x3C,0x00},
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['U'] = {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['W'] = {0x42,0x42,0x42,0x5A,0x5A,0x66,0x42,0x00},
    ['Y'] = {0x42,0x24,0x18,0x18,0x18,0x18,0x18,0x00},
    ['X'] = {0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x00},

    ['0'] = {0x3C,0x46,0x4A,0x52,0x62,0x42,0x3C,0x00},
    ['1'] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['2'] = {0x3C,0x42,0x02,0x1C,0x20,0x40,0x7E,0x00},
    ['3'] = {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00},
    ['4'] = {0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x00},
    ['5'] = {0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00},
    ['6'] = {0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00},
    ['7'] = {0x7E,0x02,0x04,0x08,0x10,0x20,0x20,0x00},
    ['8'] = {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00},
    ['9'] = {0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00},

    [' '] = {0,0,0,0,0,0,0,0},
    ['-'] = {0,0,0,0x7E,0,0,0,0},
    [':'] = {0,0,0x18,0x18,0,0x18,0x18,0},
};


static void draw_char(int x, int y, char c, uint16_t color)
{
    const uint8_t *glyph = font8x8[(int)c];

    for (int row = 0; row < 8; row++)
    {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++)
        {
            if (bits & (1 << (7 - col)))
                put_pixel(x + col, y + row, color);
        }
    }
}

static void draw_text(int x, int y, const char *str, uint16_t color)
{
    while (*str)
    {
        draw_char(x, y, *str, color);
        x += 8;
        str++;
    }
}
// =================================================== FOR SCREEN DEBUG ===================================================

static inline void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT)
        return;

    ((uint16_t*)frame_buf)[y * VIDEO_WIDTH + x] = color;
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}


static retro_environment_t environ_cb;

void retro_init(void)
{
   frame_buf = (uint8_t*)malloc(VIDEO_PIXELS * sizeof(uint16_t));

   uint16_t *fb = (uint16_t*)frame_buf;
   for (int i = 0; i < VIDEO_PIXELS; i++)
      fb[i] = 0x07E0;

   const char *dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
   {
      snprintf(retro_base_directory, sizeof(retro_base_directory), "%s", dir);
   }
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "skeleton";
   info->library_version  = "0.1";
   info->need_fullpath    = false;
   info->valid_extensions = "";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect                = 4.0f / 3.0f;
   float sampling_rate         = 22050.0f;


   info->geometry.base_width   = VIDEO_WIDTH;
   info->geometry.base_height  = VIDEO_HEIGHT;
   info->geometry.max_width    = VIDEO_WIDTH;
   info->geometry.max_height   = VIDEO_HEIGHT;
   info->geometry.aspect_ratio = aspect;

   last_aspect                 = aspect;
   last_sample_rate            = sampling_rate;

   info->timing.sample_rate    = sampling_rate;
   info->timing.fps            = 60.0;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   static const struct retro_controller_description controllers[] = {
      { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
   };

   static const struct retro_controller_info ports[] = {
      { controllers, 1 },
      { NULL, 0 },
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static unsigned phase;

void retro_reset(void)
{

}

static void update_input(void)
{
   if (!input_poll_cb || !input_state_cb)
      return;

    input_poll_cb();

    int y = 10;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
        draw_text(10, y, "A", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
        draw_text(10, y, "B", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
        draw_text(10, y, "X", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
        draw_text(10, y, "Y", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
        draw_text(10, y, "L", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
        draw_text(10, y, "R", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
        draw_text(10, y, "START", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
        draw_text(10, y, "SELECT", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
        draw_text(10, y, "UP", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
        draw_text(10, y, "DOWN", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
        draw_text(10, y, "LEFT", 0xFFFF), y += 12;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
        draw_text(10, y, "RIGHT", 0xFFFF), y += 12;
}

static void check_variables(void)
{

}

static void audio_callback(void)
{
   if (!audio_cb)
      return;

   for (unsigned i = 0; i < 30000 / 60; i++, phase++)
   {
      int16_t val = 0x800 * sinf(2.0f * M_PI * phase * 300.0f / 30000.0f);
      audio_cb(val, val);
   }

   phase %= 100;
}

static void audio_set_state(bool enable)
{
   (void)enable;
}

void retro_run(void)
{
   if (!frame_buf || !video_cb)
      return;

   //clear screen to black
   memset(frame_buf, 0x00, VIDEO_WIDTH * VIDEO_HEIGHT * 2);

   update_input();

   video_cb(frame_buf, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * 2);

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
       log_cb(RETRO_LOG_INFO, "RGB565 format not supported.\n");
       return false;
   }

   if (info && info->path)
      snprintf(retro_game_path, sizeof(retro_game_path), "%s", info->path);
   else
      retro_game_path[0] = '\0';

   struct retro_audio_callback audio_cb = { audio_callback, audio_set_state };
   use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);

   check_variables();

   (void)info;
   return true;
}

void retro_unload_game(void) {}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}