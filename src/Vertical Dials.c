#include <pebble.h>

static Window *window;
static Layer *middle_text_layer;
static Layer *left_layer;
static Layer *right_layer;
static Layer *middle_layer;
static Layer *battery_layer;

const char *DAYS_OF_WEEK[7] = {"Sunday", "Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
const char *MONTH_NAMES[12] = {"January","February","March","April","May","June","July","August","September","October","November","December"};

const char *NUMBERS[] = {"1","2","4","5","7","8","10","11"};
const char *BIGNUMBERS[] = {"5","10","20","25","35","40","50","55"};

static const GPathInfo HOUR_HAND_POINTS = {
  3,
  (GPoint []) {
    {0,-5},
    {17,0},
    {0,5}
  }
};

static GPath *hour_hand;
static GPath *minute_hand;

static bool wasConnected;
static bool setup = false;
static const int tickWidth = 2;
static const int tickLength = 15;
static int space;
static int margin;

static void createTicks(Layer *layer, GContext *ctx, int align, int numTicks) {
  graphics_context_set_fill_color(ctx,GColorWhite);
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect bounds = layer_get_bounds(layer);

  for(int i = 0; i < numTicks; i++) {
    if(i % ((numTicks - 1) / 4) == 0) {
      graphics_fill_rect(ctx, GRect(0 + align * (bounds.size.w - 2 * tickLength),bounds.size.h - (space + tickWidth) * i - margin - tickWidth,2 * tickLength,tickWidth),0,GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(0 + align * (bounds.size.w - tickLength),bounds.size.h - (space + tickWidth) * i - margin - tickWidth,tickLength,tickWidth),0,GCornerNone);
    }
  }

  for(int i = 2; i < numTicks - 3; i++) {
    if(align == 0) {
      graphics_draw_text(ctx,BIGNUMBERS[i - 2],fonts_get_system_font(FONT_KEY_GOTHIC_18),GRect(0 + (1 - align) * (bounds.size.w - tickWidth - 18) + align * 6,bounds.size.h - 18 * i + 4 - (1 - (i % 2)) * 4,15,15),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    } else {
      graphics_draw_text(ctx,NUMBERS[i - 2],fonts_get_system_font(FONT_KEY_GOTHIC_18),GRect(0 + (1 - align) * (bounds.size.w - tickWidth - 18) + align * 6,bounds.size.h - 18 * i + 4 - (1 - (i % 2)) * 4,15,15),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    }
  }
}

static void setup_outer_layer(Layer *layer, GContext *ctx) {
  if(layer == left_layer) {
    createTicks(layer,ctx,1,13);
  } else {
    createTicks(layer,ctx,0,13);
  }
}

static void update_hands(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_move_to(hour_hand,GPoint(-1,bounds.size.h - (space + tickWidth) * (((t->tm_hour + 11) % 12) + 1) - margin - tickWidth + 1));
  gpath_draw_filled(ctx,hour_hand);
  gpath_move_to(minute_hand,GPoint(bounds.size.w,(bounds.size.h - (bounds.size.h - 2* margin) * t->tm_min / 60) - margin - 1));
  gpath_draw_filled(ctx,minute_hand);
}

static void update_day(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char day_text[] = "00, 0000";
  day_text[0] = '0' + (t->tm_mday / 10);
  day_text[1] = '0' + (t->tm_mday % 10);
  day_text[4] = '0' + (t->tm_year / 1000 + 2);
  day_text[5] = '0' + ((t->tm_year / 100) % 10) - 1;
  day_text[6] = '0' + (t->tm_year / 10) % 10;
  day_text[7] = '0' + (t->tm_year % 10);
  GRect bounds = layer_get_bounds(layer);
  int top = (bounds.size.h - 60) / 2 - 5;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx,GColorWhite);
  graphics_fill_rect(ctx,bounds,0,GCornerNone);
  graphics_draw_text(ctx, DAYS_OF_WEEK[t->tm_wday], fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(0,top,bounds.size.w,10), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, MONTH_NAMES[t->tm_mon], fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(0,top + 20 ,bounds.size.w,10), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, day_text, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(0, top + 40 ,bounds.size.w,10), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
  layer_mark_dirty(middle_layer);

  if(units_changed & DAY_UNIT || !false) {
    layer_mark_dirty(middle_text_layer);
    setup = true;
  }
}

static void bluetooth_change(bool connected) {
  if(!connected && wasConnected) {
    wasConnected = false;
    vibes_long_pulse();
  } else if(connected && !wasConnected) {
    wasConnected = true;
    vibes_double_pulse();
    // Update the time in case it has changed (e.g. flight across time zones).
    psleep(10000);
    time_t    now           = time(NULL);
    struct tm *current_time = localtime(&now);
    handle_minute_tick(current_time, DAY_UNIT);
  }
}

static void update_battery(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  GRect bounds = layer_get_bounds(layer);
  graphics_fill_rect(ctx, GRect(0, 0, (bounds.size.w * battery_state_service_peek().charge_percent) / 100, bounds.size.h), 2, GCornerNone);
}

static void battery_change(BatteryChargeState charge_state) {
  layer_mark_dirty(battery_layer);
}

static void init(void) {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_stack_push(window, false);

  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);
  space = (bounds.size.h - 13 * tickWidth - 4 * tickWidth) / 13;
  margin = (bounds.size.h - 4 * tickWidth - (space + tickWidth) * 12 - tickWidth) / 2 + 2 * tickWidth;


  middle_text_layer = layer_create(GRect(bounds.size.w / 4,0,(2 * bounds.size.w) / 4,bounds.size.h));
  layer_set_update_proc(middle_text_layer, update_day);

  left_layer = layer_create(GRect(0,0,bounds.size.w / 4, bounds.size.h));
  layer_set_update_proc(left_layer,setup_outer_layer);

  right_layer = layer_create(GRect(3 * (bounds.size.w / 4),0,bounds.size.w / 4, bounds.size.h));
  layer_set_update_proc(right_layer,setup_outer_layer);

  hour_hand = gpath_create(&HOUR_HAND_POINTS);
  minute_hand = gpath_create(&HOUR_HAND_POINTS);
  gpath_rotate_to(minute_hand, TRIG_MAX_ANGLE / 2);

  middle_layer = layer_create(GRect(bounds.size.w / 4 - 15,0,2 * (bounds.size.w / 4) + 30,bounds.size.h));
  layer_set_update_proc(middle_layer,update_hands);

  battery_layer = layer_create(GRect(8, ((bounds.size.h - 60) / 2 - 5) + 65, (2 * bounds.size.w) / 4 - 16, 1));
  layer_set_update_proc(battery_layer, update_battery);

  wasConnected = bluetooth_connection_service_peek();
  tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
  bluetooth_connection_service_subscribe(&bluetooth_change);
  battery_state_service_subscribe(&battery_change);

  // Add layers to window.
  layer_add_child(root_layer,middle_text_layer);
  layer_add_child(root_layer, left_layer);
  layer_add_child(root_layer, right_layer);
  layer_add_child(root_layer, middle_layer);
  layer_add_child(middle_text_layer, battery_layer);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  gpath_destroy(hour_hand);
  gpath_destroy(minute_hand);
  layer_destroy(battery_layer);
  layer_destroy(middle_layer);
  layer_destroy(left_layer);
  layer_destroy(right_layer);
  layer_destroy(middle_text_layer);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
