#include "esphome.h"
using namespace esphome;

// returns the new bytepos
uint16_t get_next_char(const char *text, display::Font *font, uint16_t byte_pos,
                       char *buf, uint16_t buf_len) {
  assert(buf_len > 0);

  if (text[byte_pos] == '\0') // finished, start over!
    byte_pos = 0;

  int glyph_length;
  int glyph_n = font->match_next_glyph(text + byte_pos, &glyph_length);

  int i = 0;
  if (glyph_n >= 0) { // valid glyph, save it to buf
    while ((i < glyph_length) && (i < (buf_len - 1))) { // copy
      buf[i] = text[byte_pos + i];
      i++;
    }
    byte_pos += glyph_length;
  } else
    byte_pos += 1; // invalid glyph, advance one byte

  buf[i++] = 0; // terminate string

  return byte_pos;
}

class MatrixLedDisplay : public display::DisplayBuffer {
public:
  void set_addr_light(light::AddressableLight *lights) {
    this->lights = lights;
  }
  void set_fg_color(const light::ESPColor &fg_color) {
    this->fg_color = fg_color;
  }
  void set_bg_color(const light::ESPColor &bg_color) {
    this->bg_color = bg_color;
  }
  void set_width(const uint8_t width) { this->width = width; }
  void set_height(const uint8_t height) { this->height = height; }


  void dump_buffer() {
    ESP_LOGD("MatrixLedDisplay", "----- %4i x %4i -------",
             this->get_width_internal(), get_height_internal());

    for (int row = 0; row < this->get_height_internal(); row++) {
      char text[this->get_width_internal() * 9 + 1];
      for (int col = 0; col < this->get_width_internal(); col++) {
        int pos = col + row * this->get_width_internal();

        if ((pos < 0) || (pos > (this->width * this->height))) {
          ESP_LOGD("MatrixLedDisplay", "Error at pos=%i", pos);
          continue;
        }
        sprintf(&text[col*9], "%8x ", (*(this->lights))[pos].get().raw_32);
      }
      ESP_LOGD("MatrixLedDisplay", "%4i |%s|", row, text);
    };
  };

protected:
  void draw_absolute_pixel_internal(int x, int y, int color) override {
    if (x >= this->get_width_internal() || x < 0 ||
        y >= this->get_height_internal() || y < 0)
      return;

    uint16_t pos = x + y * this->get_width_internal();
    if (pos >= (*(this->lights)).size()) {
      ESP_LOGD("MatrixLedDisplay",
               "Size mismatch of AddressableLight and Matrix, could not get "
               "light %i",
               pos);
      return;
    }

    if (color) {
      (*(this->lights))[pos] = this->fg_color;
    } else {
      (*(this->lights))[pos] = this->bg_color;
    }
  }

  int get_height_internal() override { return this->height; }

  int get_width_internal() override { return this->width; }

  uint8_t width{8};
  uint8_t height{8};
  light::AddressableLight *lights;
  light::ESPColor fg_color = light::ESPColor(0xff, 0xff, 0xff);
  light::ESPColor bg_color = light::ESPColor(0x00, 0x00, 0x00);
};
