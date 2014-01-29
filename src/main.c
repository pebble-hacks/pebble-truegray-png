#include <pebble.h>

// PNG support
// Includes Grayscale support for 1 bit (B&W)
#include "upng.h"

#define MAX_IMAGES 14

#define MAX(A,B) ((A>B) ? A : B)
#define MIN(A,B) ((A<B) ? A : B)

//Register stack pointer to print it (needs -ffixed-sp in CFLAGS also)
register uint32_t sp __asm("sp");
static uint32_t bsp = 0x2001a26c; //stack grows downward from this

void flip_byte(uint8_t* byteval) {
  uint8_t v = *byteval;
  uint8_t r = v; // r will be reversed bits of v; first get LSB of v
  int s = 7; // extra shift needed at end

  for (v >>= 1; v; v >>= 1) {   
    r <<= 1;
    r |= v & 1;
    s--;
  }
  r <<= s; // shift when v's highest bits are zero
  *byteval = r;
}

static struct main_ui {
  Window* window;
  BitmapLayer* bitmap_layer;
  BitmapLayer* bitmap_layer_old;
  PropertyAnimation* prop_animation;

  GBitmap bitmap;
  GBitmap bitmap_old;
  upng_t* upng;
  uint8_t image_index;
} ui = {
  .bitmap.addr = NULL,
  .bitmap.bounds = {{0},{0}},
  .image_index = 0,
  .prop_animation = NULL,
  .upng = NULL
};

static bool gbitmap_from_bitmap(
    GBitmap* gbitmap, const uint8_t* bitmap_buffer, int width, int height) {

  //copy current bitmap ptr into old bitmap
  if (gbitmap->addr) {
    if (ui.bitmap_old.addr) {
      free(ui.bitmap_old.addr);
      ui.bitmap_old.addr = gbitmap->addr;
      gbitmap->addr = NULL;
    }
  }

  if (gbitmap->addr) {
    free(gbitmap->addr);
  }

  // Limit PNG to screen size
  width = MIN(width,144);
  height = MIN(height,168);

  // Copy width and height to GBitmap
  gbitmap->bounds.size.w = width;
  gbitmap->bounds.size.h = height;
  // GBitmap needs to be word aligned per line (bytes)
  gbitmap->row_size_bytes = ((width + 31) / 32 ) * 4;
  //Allocate new gbitmap array
  gbitmap->addr = malloc(height * gbitmap->row_size_bytes); 

  APP_LOG(APP_LOG_LEVEL_DEBUG, "copy rows from:%p to:%p",
    bitmap_buffer,gbitmap->addr);
  for(int y = 0; y < height; y++) {
    memcpy(
      &(((uint8_t*)gbitmap->addr)[y * gbitmap->row_size_bytes]), 
      &(bitmap_buffer[y * ((width + 7) / 8)]), 
      (width + 7) / 8);
  }

  // GBitmap pixels are most-significant bit, so need to flip each byte.
  for(int i = 0; i < gbitmap->row_size_bytes * height; i++){
    flip_byte(&((uint8_t*)gbitmap->addr)[i]);
  }

  return true;
}

static void init_animation(void) {
  GRect right_of_screen = {.origin={.x=143,.y=0},.size={.w=1,.h=168}};
    
  //layer_get_frame(bitmap_layer_get_layer(ui.bitmap_layer)).size};//.w=144,.h=168}};

  //layer_set_clips(bitmap_layer_get_layer(ui.bitmap_layer),true);
  GRect on_screen = layer_get_frame(bitmap_layer_get_layer(ui.bitmap_layer));

  ui.prop_animation = property_animation_create_layer_frame( 
    bitmap_layer_get_layer(ui.bitmap_layer),
    &right_of_screen, NULL);// &on_screen);//&bounds);

  animation_set_duration((Animation*)ui.prop_animation, 1000);
  animation_set_curve((Animation*)ui.prop_animation, AnimationCurveEaseInOut);
  //animation_set_delay((Animation*)ui.prop_animation, 100);
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
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stack Used:%ld SP:%p", bsp - sp, sp);
  upng_decode(ui.upng);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Decode:%d", upng_get_error(ui.upng));



  gbitmap_from_bitmap(&ui.bitmap, upng_get_buffer(ui.upng),
    upng_get_width(ui.upng), upng_get_height(ui.upng));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "converted to gbitmap");

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
  APP_LOG(APP_LOG_LEVEL_DEBUG, "down_click_handler");
  if(ui.prop_animation && !animation_is_scheduled(ui.prop_animation)) {
    // Increment the index (wrap around if necessary)
    ui.image_index = (ui.image_index + 1) % MAX_IMAGES;
    load_png_resource(ui.image_index);
    //bitmap_layer_set_bitmap(ui.bitmap_layer, &ui.bitmap);
    //GRect right_of_screen = {.origin={.x=143,.y=0},.size={.w=144,.h=168}};
    //((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer))->bounds = right_of_screen;
    //((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer))->clips = false;
    //layer_set_frame(bitmap_layer_get_layer(ui.bitmap_layer), right_of_screen);
  
    animation_schedule((Animation*)ui.prop_animation);
  }
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
  load_png_resource(ui.image_index);
  bitmap_layer_set_bitmap(ui.bitmap_layer, &ui.bitmap);
  //layer_mark_dirty(bitmap_layer_get_layer(ui.bitmap_layer));

  // Animation
  if (ui.prop_animation == NULL) {
    init_animation();
  }
 
  
}

static void window_unload(Window *window) {
  if (ui.prop_animation != NULL) {
    property_animation_destroy(ui.prop_animation);
  }
}

static void init(void) {
  light_enable(true);

  ui.window = window_create();
  window_set_fullscreen(ui.window, true);
  window_set_background_color(ui.window, GColorClear);
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
