#include <pebble.h>

// True Gray using Phasing and Pulse-Width-Modulation
// By turning pixels on and off very fast, the apparent average
// makes the pixel look gray.  Unfortunately the effect can be seen
// on a large scale as screen flicker.  By alternating consecutive pixels
// to opposite states ( x=0 off, x=1 on ) the effect can be minimized.
// Pulse width modulation is handled by switching state each pass,
// but anything more than 50% cycle ( on->off->on->off) is noticeable,
// so only 1 shade of gray works.  

// PNG support
// Includes Grayscale support for 1, 2, 4 and 8 bit
// Currently we only can support 1 and 2 bit for size
// And must be created without compression (not enough ram to decompress).
#include "upng.h"

static Window *gray_window;
static Layer *render_layer;

#define MAX_IMAGES 8
static int image_index = 0;

static upng_t* upng = NULL;

#pragma pack(push, 1)
  struct gray_2bit_image {
    uint8_t bpp;     // supports 1 or 2 bits (bw or gray)
    uint8_t width;   // Max width 144
    uint8_t height;  // Max height 168
    uint8_t* pixels; // Pixel array
  } image;
#pragma pack(pop)


// Framebuffer width is word aligned, so 160 for 144 wide screen
#define DRAW_PIXEL( framebuffer, x, y, white ) \
  framebuffer[((y*160 + x) / 8)] = \
    ( framebuffer[((y*160 + x) / 8)] \
    & (0xFF ^ (0x01 << (x%8))) )\
    | (white << (x%8))

static bool load_png_resource(int index) {
  ResHandle rHdl = resource_get_handle(RESOURCE_ID_IMAGE_1 + image_index);
  int png_raw_size = resource_size(rHdl);
  
  if (upng) {
    upng_free(upng);
    upng = NULL;
  }

  psleep(1); // Avoid watchdog kill
  
  uint8_t* png_raw_buffer = malloc(png_raw_size);
  resource_load(rHdl, png_raw_buffer, png_raw_size);
  upng = upng_new_from_bytes(png_raw_buffer, png_raw_size);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Loaded:%d", upng_get_error(upng));
  upng_decode(upng);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Decode:%d", upng_get_error(upng));

  image.pixels = upng_get_buffer(upng);
  image.width = upng_get_width(upng);
  image.height = upng_get_height(upng);
  image.bpp = upng_get_bpp(upng);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "PNG info width:%d height:%d bpp:%d", 
    image.width, image.height, image.bpp);

  psleep(1); // Avoid watchdog kill  
}

// This draws the gray image buffer struct to the screen framebuffer
// and is triggered by layer_dirty.  A timer is set to force
// layer_dirty so that the Pulse-Width-Modulation occurs "Fast Enough".
// Because of the timing of layer_dirty callback, other layers such
// as text_layer can be used to draw ontop of the updated framebuffer.
static void draw_gray(Layer* layer, GContext *ctx) {
  GBitmap* bitmap = (GBitmap*)ctx;
  uint8_t* framebuffer = (uint8_t*)bitmap->addr;
  static int pass = 0; // 2 passes for 1 shade of gray with alternate phase

  for (int y=0; y < image.height; y++) {
    for (int x=0; x < image.width; x++) {
      // PNG bit order is LSBit, so need to invert it in each byte
      // to match framebuffers MSBit for each byte (3 - x%4)
      uint8_t color = 
        image.pixels[(y*image.width + x) / 4] >> ((3 - x%4) * 2) & 0x03;
      if (color == 0x00) { //Black 2 bit
        DRAW_PIXEL(framebuffer, x, y, 0);
      } else if (color == 0x03) { //White 2 bit
        DRAW_PIXEL(framebuffer, x, y, 1);
      } else { // 1 shade of Gray for both 0x01 and 0x02
        // using (x%2 + y%2 + pass)%2 allows for 
        // pixels next to each other in both x and y to alternate on state
        // entire thing to alternate state each pass (pulse-width-modulation)
        DRAW_PIXEL(framebuffer, x, y, (x%2 + y%2 + pass)%2);
      }
    }
  }
  pass = (pass+1)%2;
}


// Forces window updates by marking the screen dirty
// which causes a layer redraw callback
static void register_timer(void* data) {
  // 20ms == 50fps, anything above shows flicker
  // Always has a little flicker in direct sunlight
  // Causes watchdog timer reset if < 15ms
  app_timer_register(15, register_timer, data);
  layer_mark_dirty(render_layer);
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Decrement the index (wrap around if negative)
  image_index = ((image_index - 1) < 0)? (MAX_IMAGES - 1) : (image_index - 1);
  load_png_resource(image_index);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Increment the index (wrap around if necessary)
  image_index = (image_index + 1) % MAX_IMAGES;
  load_png_resource(image_index);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  render_layer = layer_create(bounds);

  // Hook our gray rendering call to the screen refresh
  layer_set_update_proc(render_layer, draw_gray);
  layer_add_child(window_layer, render_layer);
  register_timer(NULL);
}

static void window_unload(Window *window) {
}

static void init(void) {
  //Allocate 4-bit grayscale buffer
  APP_LOG(APP_LOG_LEVEL_DEBUG, "About to load initial resource.");
  image_index = 0;
  load_png_resource(image_index);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded initial resource.");

  gray_window = window_create();
  window_set_fullscreen(gray_window, true);
  window_set_click_config_provider(gray_window, click_config_provider);
  window_set_window_handlers(gray_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = false;
  window_stack_push(gray_window, animated);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "window push.");
}

static void deinit(void) {
  window_destroy(gray_window);
  if (upng) upng_free(upng);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
