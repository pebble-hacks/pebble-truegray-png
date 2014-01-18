#include <pebble.h>

// PNG support
// Includes Grayscale support for 1 bit (B&W)
#include "upng.h"

#define MAX_IMAGES 1

#define MAX(A,B) ((A>B) ? A : B)
#define MIN(A,B) ((A<B) ? A : B)

static struct main_ui {
  Window* window;
  BitmapLayer* bitmap_layer;

  GBitmap bitmap;
  upng_t* upng;
  uint8_t image_index;
} ui = {
  .bitmap.addr = NULL,
  .bitmap.bounds = {{0},{0}},
  .image_index = 0,
  .upng = NULL
};

static bool gbitmap_from_bitmap(
    GBitmap* gbitmap, const uint8_t* bitmap_buffer, int width, int height) {
  //Allocate new gbitmap array
  if (gbitmap->addr) {
    free(gbitmap->addr);
  }

  // Limit PNG to screen size
  width = MIN(width,144);
  height = MIN(height,168);

  // Copy width and height to GBitmap
  gbitmap->bounds.size.w = width;
  gbitmap->bounds.size.h = height;
  // GBitmap needs to be word aligned per line
  gbitmap->row_size_bytes = (width + 31) >> 5;
  // Allocate bytes, not bits
  gbitmap->addr = malloc(height * gbitmap->row_size_bytes); 

  for(int y = 0; y < height; y++) {
    memcpy(
      &(((uint8_t*)gbitmap->addr)[y * gbitmap->row_size_bytes]), 
      &(bitmap_buffer[y * (width + 7) / 8]), 
      (width + 7) / 8);
  }
  return true;
}

static bool load_png_resource(int index) {
  ResHandle rHdl = resource_get_handle(RESOURCE_ID_IMAGE_1 + ui.image_index);
  int png_raw_size = resource_size(rHdl);
  
  if (ui.upng) {
    upng_free(ui.upng);
    ui.upng = NULL;
  }

  psleep(1); // Avoid watchdog kill
  
  uint8_t* png_raw_buffer = malloc(png_raw_size);
  resource_load(rHdl, png_raw_buffer, png_raw_size);
  ui.upng = upng_new_from_bytes(png_raw_buffer, png_raw_size);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Loaded:%d", upng_get_error(ui.upng));
  upng_decode(ui.upng);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Decode:%d", upng_get_error(ui.upng));


  gbitmap_from_bitmap(&ui.bitmap, upng_get_buffer(ui.upng),
    upng_get_width(ui.upng), upng_get_height(ui.upng));

  // Free the png, no longer needed
  upng_free(ui.upng);
  ui.upng = NULL;

  psleep(1); // Avoid watchdog kill  
  return true;
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Decrement the index (wrap around if negative)
  ui.image_index = ((ui.image_index - 1) < 0)? (MAX_IMAGES - 1) : (ui.image_index - 1);
  load_png_resource(ui.image_index);
  bitmap_layer_set_bitmap(ui.bitmap_layer, &ui.bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(ui.bitmap_layer));
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Increment the index (wrap around if necessary)
  ui.image_index = (ui.image_index + 1) % MAX_IMAGES;
  load_png_resource(ui.image_index);
  bitmap_layer_set_bitmap(ui.bitmap_layer, &ui.bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(ui.bitmap_layer));
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  ui.bitmap_layer = bitmap_layer_create(bounds);

  layer_add_child(window_layer, bitmap_layer_get_layer(ui.bitmap_layer));
}

static void window_unload(Window *window) {
}

static void init(void) {
  load_png_resource(ui.image_index);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded initial resource.");

  ui.window = window_create();
  window_set_fullscreen(ui.window, true);
  window_set_click_config_provider(ui.window, click_config_provider);
  window_set_window_handlers(ui.window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = false;
  window_stack_push(ui.window, animated);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "window push.");
}

static void deinit(void) {
  window_destroy(ui.window);
  if (ui.upng) upng_free(ui.upng);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
