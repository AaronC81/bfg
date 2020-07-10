#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MD_AD9833.h>
#include <Encoder.h>

#define ROTARY_ENCODER_DT 2
#define ROTARY_ENCODER_CLK 3
#define ROTARY_ENCODER_SW 6
Encoder encoder(ROTARY_ENCODER_CLK, ROTARY_ENCODER_DT);

#define DDS_FSYNC 9
#define DDS_SCLK 13
#define DDS_SDATA 11
MD_AD9833 dds(DDS_FSYNC);

#define POT_CS 8

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

#define ICON_SIZE 14

Adafruit_SSD1306 display = Adafruit_SSD1306(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire);

struct func_menu_item {
  char *title;
  uint8_t *icon;
  MD_AD9833::mode_t dds_mode;
};

uint8_t sine_wave_icon[] = {
  0xef, 0xfc, 0xd7, 0xfc, 0xbb, 0xfc, 0xbb, 0xfc, 0x7d, 0xfc, 0x7d, 0xfc, 0x7d, 0xfc, 0xfe, 0xf8, 
  0xfe, 0xf8, 0xfe, 0xf8, 0xff, 0x74, 0xff, 0x74, 0xff, 0xac, 0xff, 0xdc
};

uint8_t square_wave_icon[] = {
  0x00, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 
  0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0xfc, 0xfe, 0x00
};

uint8_t triangle_wave_icon[] = {
  0xef, 0xfc, 0xd7, 0xfc, 0xd7, 0xfc, 0xbb, 0xfc, 0xbb, 0xfc, 0x7d, 0xfc, 0x7d, 0xfc, 0xfe, 0xf8, 
  0xfe, 0xf8, 0xff, 0x74, 0xff, 0x74, 0xff, 0xac, 0xff, 0xac, 0xff, 0xdc
};

uint8_t on_icon[] = {
  0xff, 0xfc, 0xff, 0xfc, 0xc6, 0xf4, 0xba, 0x74, 0xba, 0x74, 0xba, 0xb4, 0xba, 0xb4, 0xba, 0xd4, 
  0xba, 0xd4, 0xba, 0xe4, 0xba, 0xe4, 0xc6, 0xf4, 0xff, 0xfc, 0xff, 0xfc
};

uint8_t off_icon[] = {
  0xff, 0xfc, 0xff, 0xfc, 0xcc, 0x44, 0xb5, 0xdc, 0xb5, 0xdc, 0xb5, 0xdc, 0xb4, 0x44, 0xb5, 0xdc, 
  0xb5, 0xdc, 0xb5, 0xdc, 0xb5, 0xdc, 0xcd, 0xdc, 0xff, 0xfc, 0xff, 0xfc
};

#define INVERT_BITMAP(x) for (int i = 0; i < sizeof(x); i++) x[i] = ~x[i];

// http://javl.github.io/image2cpp/ outputs with the wrong colours, and it won't change for some reason.
// This inverts the colour. 
void prepare_bitmaps() {
  INVERT_BITMAP(sine_wave_icon);
  INVERT_BITMAP(square_wave_icon);
  INVERT_BITMAP(triangle_wave_icon);
  INVERT_BITMAP(on_icon);
  INVERT_BITMAP(off_icon);
}

// The array of available functions.
struct func_menu_item funcs[] = {
  { .title = "Sine", .icon = sine_wave_icon, .dds_mode = MD_AD9833::mode_t::MODE_SINE },
  { .title = "Square", .icon = square_wave_icon, .dds_mode = MD_AD9833::mode_t::MODE_SQUARE1 },
  { .title = "Triangle", .icon = triangle_wave_icon, .dds_mode = MD_AD9833::mode_t::MODE_TRIANGLE },
};

// Whether the function generator is currently enabled.
bool enabled = true;

// The number of functions.
uint16_t funcs_len = 3;

// The current function index.
uint16_t func = 0;

// The current frequency.
uint32_t freq = 10000;

// The whole part of the current amplitude.
uint8_t amplitude_whole = 5;

// The decimal part of the current amplitude.
uint8_t amplitude_decimal = 0;

// The currently selected digit. 0 indexed, and there are 8 digits.
// A value of > 7 indicates a special function.
uint8_t ui_current_item = 0;
#define UI_FIRST_SPECIAL_ITEM 8
#define UI_CURRENT_ITEM_ENABLED 8
#define UI_CURRENT_ITEM_FUNCTION 9
#define UI_CURRENT_ITEM_AMPLITUDE 10
#define UI_LAST_ITEM 10

// Whether the selected digit is being actively edited.
bool ui_editing = false;

void freq_draw() {
  display.clearDisplay();

  // Write digits
  // TODO: would be neat to group digits
  const int character_width = 12;
  const int total_text_width = 8 * character_width;
  const int text_pad = 0;
  const int icon_pad = 1;
  display.setTextSize(2);
  display.setTextColor(1);
  display.setCursor(text_pad, 8);
  char frequency[9];
  sprintf(frequency, "%08lu", freq);
  display.print(frequency);

  // Draw separator line after frequency
  display.drawLine(total_text_width + text_pad * 2, 0, total_text_width + text_pad * 2, DISPLAY_HEIGHT, 1);

  // Draw enabled icon
  int enabled_icon_colour = 1;
  if (ui_current_item == UI_CURRENT_ITEM_ENABLED) {
    display.fillRect(total_text_width + text_pad * 2, 0, icon_pad * 2 + ICON_SIZE, DISPLAY_HEIGHT / 2, 1);
    enabled_icon_colour = 0;
  }
  display.drawBitmap(total_text_width + text_pad * 2 + 2, 1, enabled ? on_icon : off_icon, ICON_SIZE, ICON_SIZE, enabled_icon_colour);

  // Draw signal icon
  int signal_icon_colour = 1;
  if (ui_current_item == UI_CURRENT_ITEM_FUNCTION) {
    display.fillRect(total_text_width + text_pad * 2, DISPLAY_HEIGHT / 2, icon_pad * 2 + ICON_SIZE, DISPLAY_HEIGHT / 2, 1);
    signal_icon_colour = 0;
  }
  display.drawBitmap(total_text_width + text_pad * 2 + 2, (DISPLAY_HEIGHT / 2) + 1, funcs[func].icon, ICON_SIZE, ICON_SIZE, signal_icon_colour);

  const int amplitude_start = total_text_width + text_pad * 2 + icon_pad * 2 + ICON_SIZE + 1;

  // Draw separator line after enabled/signal
  display.drawLine(amplitude_start, 0, amplitude_start, DISPLAY_HEIGHT, 1);

  // Draw amplitude
  display.setTextSize(1);
  display.setCursor(amplitude_start + 4, 4);
  display.write('0' + amplitude_whole);
  display.setCursor(amplitude_start + 4, 8);
  display.print(".");
  display.setCursor(amplitude_start + 4, 17);
  display.write('0' + amplitude_decimal);
  if (ui_current_item == UI_CURRENT_ITEM_AMPLITUDE) {
    if (ui_editing) {
      display.fillRect(amplitude_start + 2, 26, 10, 2, 1);
    } else {
      display.fillRect(amplitude_start + 4 + 1, 26, 4, 2, 1);
    }
  }

  // Draw current digit underline/selection
  if (ui_current_item < UI_FIRST_SPECIAL_ITEM) {
    int selection_x = ui_current_item * character_width + text_pad;
    if (ui_editing) {
      display.fillRect(selection_x, 26, character_width, 2, 1);
    } else {
      display.fillRect(selection_x + character_width / 3, 26, character_width / 3, 2, 1);
    }
  }

  display.display();
}

void amplitude_inc() {
  // Hard limit of 5V

  if (amplitude_whole == 5) return;

  if (amplitude_decimal == 9) {
    amplitude_decimal = 0;
    amplitude_whole++;
  } else {
    amplitude_decimal++;
  }
}

void amplitude_dec() {
  if (amplitude_whole == 0 && amplitude_decimal == 0) return;

  if (amplitude_decimal == 0) {
    amplitude_decimal = 9;
    amplitude_whole--;
  } else {
    amplitude_decimal--;
  }
}

void freq_inc_selected() {
  uint32_t base = 1;
  for (int i = 0; i < UI_FIRST_SPECIAL_ITEM - ui_current_item; i++)
    base *= 10;
  int digit = (freq / (base / 10)) % 10;
  if (digit == 9) {
    freq -= (base / 10) * 9;
  } else {
    freq += base / 10;
  }
}

void freq_dec_selected() {
  uint32_t base = 1;
  for (int i = 0; i < UI_FIRST_SPECIAL_ITEM - ui_current_item; i++)
    base *= 10;
  int digit = (freq / (base / 10)) % 10;
  if (digit == 0) {
    freq += (base / 10) * 9;
  } else {
    freq -= base / 10;
  }
}

enum input {
  INPUT_NONE = 0,
  INPUT_LEFT,
  INPUT_RIGHT,
  INPUT_SELECT,
};

long last_rotary_encoder_state = -999;

enum input input_get() {
  enum input result = INPUT_NONE;
  long curr_rotary_encoder_state = encoder.read() / 4;

  if (!digitalRead(ROTARY_ENCODER_SW)) {
    return INPUT_SELECT;
  }

  if (curr_rotary_encoder_state > last_rotary_encoder_state) {
    result = INPUT_LEFT;
  } else if (curr_rotary_encoder_state < last_rotary_encoder_state) {
    result = INPUT_RIGHT;
  }

  last_rotary_encoder_state = curr_rotary_encoder_state;

  return result;
}

void dds_update() {
  dds.setFrequency(MD_AD9833::CHAN_0, freq);
  dds.setMode(enabled ? funcs[func].dds_mode : MD_AD9833::mode_t::MODE_OFF);
}

void pot_set(int value) {
  digitalWrite(POT_CS, LOW);
  SPI.transfer(0);
  SPI.transfer(value);
  digitalWrite(POT_CS, HIGH);
}

void pot_update() {
  int pot_value = ((float)(amplitude_whole * 10 + amplitude_decimal) / 50) * 128;
  pot_set(pot_value);
}

void setup() {
  Serial.begin(9600);
  pinMode(ROTARY_ENCODER_DT, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_CLK, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_SW, INPUT_PULLUP);
  pinMode(DDS_FSYNC, OUTPUT);
  pinMode(DDS_SCLK, OUTPUT);
  pinMode(DDS_SDATA, OUTPUT);
  pinMode(POT_CS, OUTPUT);
  digitalWrite(DDS_FSYNC, HIGH);
  digitalWrite(POT_CS, HIGH);

  prepare_bitmaps();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display();

  SPI.begin();
  dds.begin();
  dds_update();
  pot_update();
  
  display.clearDisplay();
  freq_draw();
}

void loop() {
  switch (input_get()) {
  case INPUT_LEFT:
    if (ui_editing) {
      if (ui_current_item == UI_CURRENT_ITEM_AMPLITUDE) {
        amplitude_dec();
        pot_update();
      } else {
        freq_dec_selected();
        dds_update();
      }
    } else if (ui_current_item == 0) {
      ui_current_item = UI_LAST_ITEM;
    } else {
      ui_current_item--;
    }
    dds_update();
    freq_draw();
    break;
  case INPUT_RIGHT:
    if (ui_editing) {
      if (ui_current_item == UI_CURRENT_ITEM_AMPLITUDE) {
        amplitude_inc();
        pot_update();
      } else {
        freq_inc_selected();
        dds_update();
      }
    } else if (ui_current_item == UI_LAST_ITEM) {
      ui_current_item = 0;
    } else {
      ui_current_item++;
    }
    freq_draw();
    break;
  case INPUT_SELECT:
    if (ui_current_item == UI_CURRENT_ITEM_ENABLED) {
      enabled = !enabled;
      dds_update();
    } else if (ui_current_item == UI_CURRENT_ITEM_FUNCTION) {
      if (func == funcs_len - 1) {
        func = 0;
      } else {
        func++;
      }
      dds_update();
    } else {
      ui_editing = !ui_editing;
    }
    freq_draw();
    delay(200);
    break;
  default:
    break;
  }
  delay(1);
}
