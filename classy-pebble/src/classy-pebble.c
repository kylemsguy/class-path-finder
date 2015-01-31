#include <pebble.h>
#include "classy-pebble.h"
#define EVENT_STR_LIMIT 16

static Window *window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_event_layer1; // current event name
static TextLayer *s_event_layer2; // extra data for said event
static GFont s_time_font;
static GFont s_date_font;
static GFont s_event_font;
static InverterLayer *s_invert_screen;
static bool s_enable_invert_screen = true;

static Event current_event;

// AppMessage Receive
enum {
  /*"status": 0,
    "evTitle": 1,
    "evStart": 2,
    "evEnd": 3*/
  RESULT_STATUS = 0x0,  // TUPLE_INT
  RESULT_TITLE = 0x1,  // TUPLE_CSTRING
  RESULT_START = 0x2, // TUPLE_time_t
  RESULT_END = 0x3, // TUPLE_time_t
};

static void update_failed();
static void update_event();

/* Communication with phone/internet */
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received! Processing...");
  Tuple *status = dict_find(iterator, RESULT_STATUS);
  if(status){
    if(status->value->uint8 == 0){
      // success get info
      Tuple *event_title = dict_find(iterator, RESULT_TITLE);
      Tuple *event_start = dict_find(iterator, RESULT_START);
      Tuple *event_end = dict_find(iterator, RESULT_END);

      strncpy(current_event.name, event_title->value->cstring, EVENT_STR_LIMIT+1);
      current_event.start = ((time_t) event_start->value->uint32);
      current_event.end = ((time_t) event_end->value->uint32);
      update_event();
    }
    else
      update_failed();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void request_update(){
  APP_LOG(APP_LOG_LEVEL_INFO, "Request update!");
  Tuplet value = TupletInteger(0, (uint8_t) 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if(iter == NULL)
    return;

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();

}

static void update_failed(){
  text_layer_set_text(s_event_layer1, "Update failed.");
  text_layer_set_text(s_event_layer2, "Chk options/data");
}

/* event component */
static void update_event(){
  text_layer_set_text(s_event_layer1, "Please wait.");
  text_layer_set_text(s_event_layer2, "Loading data...");
  // update event data
  if(strlen(current_event.name) == 0){
    // fetch data from phone
    request_update();
  }
  else{
    time_t curr_time = time(NULL);
    struct tm local_time;
    struct tm event_start_time;

    memcpy(&local_time, localtime(&curr_time), sizeof(struct tm));
    memcpy(&event_start_time, localtime(&current_event.start), sizeof(struct tm));

    if(local_time.tm_mday != event_start_time.tm_mday){
      text_layer_set_text(s_event_layer1, "No more classes.");
      text_layer_set_text(s_event_layer2, "See you tomorrow!");
    }
    else if(curr_time >= current_event.end){
      // fetch data from phone
      request_update();
    }
    else if (curr_time >= current_event.start){
      // calculate diff in time
      int diff = curr_time - current_event.start;
      int hrs = diff / 3600;
      int mins = (diff % 3600) / 60;

      char event_str1[EVENT_STR_LIMIT+1];
      char event_str2[EVENT_STR_LIMIT+1];

      snprintf(event_str1, sizeof(event_str1), "%s", current_event.name);
      if(hrs > 0)
        snprintf(event_str2, sizeof(event_str2), "ends in %dhr %dmin", hrs, mins);
      else
        snprintf(event_str2, sizeof(event_str2), "ends in %dmin", mins);

      text_layer_set_text(s_event_layer1, event_str1);
      text_layer_set_text(s_event_layer2, event_str2);
    }
    else{
      // calculate diff in time
      int diff = curr_time - current_event.start;
      int hrs = diff / 3600;
      int mins = (diff % 3600) / 60;

      char event_str1[EVENT_STR_LIMIT+1];
      char event_str2[EVENT_STR_LIMIT+1];

      snprintf(event_str1, sizeof(event_str1), "%s", current_event.name);

      if(hrs > 0)
        snprintf(event_str2, sizeof(event_str2), "starts in %dhr %dmin", hrs, mins);
      else
        snprintf(event_str2, sizeof(event_str2), "starts in %dmin", mins);

      text_layer_set_text(s_event_layer1, event_str1);
      text_layer_set_text(s_event_layer2, event_str2);
    }
  }
}

/* watch/time component */

static void update_time() {
  static char s_time[8] = "--:--";
  static char s_date[16] = "Sat Jan 17 2015";

  // get a tm struct
  time_t tmp = time(NULL);
  struct tm *tick_time = localtime(&tmp);

  if(clock_is_24h_style()) {
    // use 24 hour format
    strftime(s_time, sizeof(s_time), "%H:%M", tick_time);
  } else {
    // use 12 hour format with AM/PM
    strftime(s_time, sizeof(s_time), "%I:%M", tick_time);
  }

  strftime(s_date, sizeof(s_date), "%a %b %d", tick_time);

  // Display this time on the TextLayers
  text_layer_set_text(s_time_layer, s_time);
  text_layer_set_text(s_date_layer, s_date);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  update_event();
}

/* Window stuff/core Pebble stuff */

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // initialize time/date display
  s_time_layer = text_layer_create(GRect(0, 5, 144, 50));
  s_date_layer = text_layer_create(GRect(0, 60, 144, 50));

  text_layer_set_background_color(s_time_layer, GColorClear);
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_WHAT_TIME_IS_IT_47));
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  text_layer_set_background_color(s_date_layer, GColorClear);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_WHAT_TIME_IS_IT_24));
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  update_time();

  // set up event layers
  s_event_layer1 = text_layer_create(GRect(0, 100, 144, 50));
  s_event_layer2 = text_layer_create(GRect(0, 125, 144, 50));
  
  text_layer_set_text_alignment(s_event_layer1, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_event_layer2, GTextAlignmentCenter);

  s_event_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  text_layer_set_font(s_event_layer1, s_event_font);
  text_layer_set_font(s_event_layer2, s_event_font);

  text_layer_set_text(s_event_layer1, "Please wait.");
  text_layer_set_text(s_event_layer2, "Loading data...");

  // add the layers to the window
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_event_layer1));
  layer_add_child(window_layer, text_layer_get_layer(s_event_layer2));

  if(s_enable_invert_screen){
    s_invert_screen = inverter_layer_create(GRect(0, 0,  bounds.size.w,  bounds.size.h));
    layer_add_child(window_layer, inverter_layer_get_layer(s_invert_screen));
  }
}

static void window_unload(Window *window) {
  fonts_unload_custom_font(s_time_font);
  text_layer_destroy(s_time_layer);
}

static void init(void) {
  window = window_create();
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(0x100, 0x100);
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
