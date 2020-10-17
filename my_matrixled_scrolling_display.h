#include "esphome.h"
using namespace esphome;

struct coord {
  int x;
  int y;
};

struct floatcoord {
  float x;
  float y;
};

enum DisplayOrient {
  DISPLAY_ROTATION_0_DEGREES = 0x00,
  DISPLAY_ROTATION_90_DEGREES = 0x01,
  DISPLAY_ROTATION_180_DEGREES = 0x02,
  DISPLAY_ROTATION_270_DEGREES = 0x03,
  DISPLAY_FLIP_HORIZONTAL = 0x10,
  DISPLAY_FLIP_VERTICAL = 0x20,
  DISPLAY_ZIGZAG = 0x40,
};



// wrap coord to the interval wrap_start (including) and wrap_stop (excluding)
int wrap(const int val, const int wrap_start, const int wrap_stop) {
  int ret;
  int span = (wrap_stop - wrap_start);

  if (span <= 0)
    return wrap_start;

  ret = (val - wrap_start) % span;
  if (ret < 0) {
    ret += span;
  }

  ret += wrap_start;

  if ((ret < wrap_start) || (ret >= wrap_stop)) {
    // ESP_LOGD("MatrixLedDisplay", "F**K: %i [%i-%i]", ret, wrap_start,
    // wrap_stop);
    ret = wrap_start;
  }

  return ret;
}

// struct coord active_idx, int dx, int dy
using action_cb = std::function<void(struct coord *, int, int)>;
using draw_cb =
    std::function<void(struct coord *, bool)>; // struct coord cursor

class MatrixLedScrollingDisplay;

template <int N_COLS, int N_ROWS> class Menu {
public:
  Menu(MatrixLedScrollingDisplay *sdisp, int ROW_HEIGHT)
      : sdisp(sdisp), row_height(ROW_HEIGHT){};

  void set_draw_fn(const coord menu_idx, const draw_cb draw_fn) {
    this->draw_fn[menu_idx.x, menu_idx.y] = draw_fn;
  };

  void set_action_fn(const coord menu_idx, const action_cb action_fn) {
    this->action_fn[menu_idx.x, menu_idx.y] = action_fn;
  };

  void tick(float dt) {
    // update time and pos
    this->t += dt;
    this->pos.x += this->vel.x * dt;
    this->pos.y += this->vel.y * dt;

    // go to target
    if (this->is_scrolling) { // not scrolling
      float dx = this->pos_targ.x - this->pos.x;
      float dy = this->pos_targ.y - this->pos.y;
      float f = sqrt(dx * dx + dy * dy);

      if ((f > 0.5) && (f > this->scroll_speed)) {
        dx *= this->traverse_speed / f;
        dy *= this->traverse_speed / f;
      } else // target reached
      {
        this->idx = this->idx_targ; // TODO SETTER
        this->is_scrolling =
            this->action_fn[this->idx.x][this->idx.y](); // change to scrolling
      }
    } else // scroll mode
    {
    }
  };

  void refresh() {
    struct coord cursor = {.x = 0, .y = 0};
    bool force_redraw = false;

    for (int col = 0; col < N_COLS; col++)
      for (int row = 0; row < N_ROWS; row++) {
        draw_cb draw_fn[N_COLS][N_ROWS](&cursor, force_redraw);
        if (this->menu_pos_x[col][row] != (cursor.x - 1)) {
          force_redraw = true;
          this->menu_pos_x[col][row] = (cursor.x - 1);
        }
        // ESP_LOGD("main", "text '%s' at x=%i y=%i",text, cursor->x,
        // cursor->y); if (x >= TOTAL_WIDTH)
        //  continue;
      }
  };

  void draw_text(struct coord *cursor, const char *text, Font *font) {
    //+Y_OFFSET
    int x_start, y_start;
    int width, height;
    this->sdisp->get_text_bounds(cursor->x, cursor->y, text, font,
                                 TextAlign::TOP_LEFT, &x_start, &y_start,
                                 &width, &height);
    cursor->x += width;
    cursor->y += height;
  };

protected:
  struct coord idx = {.x = 0, .y = 0};
  struct coord idx_targ = {.x = 0, .y = 0};
  struct floatcoord pos = {.x = 0., .y = 0.};
  struct floatcoord vel = {.x = 0., .y = 0.};
  struct floatcoord pos_targ;
  float targ_dist = 0;

  int menu_pos_x[N_COLS][N_ROWS] = {0};
  draw_cb draw_fn[N_COLS][N_ROWS] = {0};

  action_cb action_fn[N_COLS][N_ROWS] = {0};

  int row_height;

  float t;
  float scroll_speed = 1.0f;
  float traverse_speed = 1.0f;
  bool is_scrolling = false;

  MatrixLedScrollingDisplay *sdisp;
};

class MatrixLedScrollingDisplay : public display::DisplayBuffer {
public:
  void set_size(const int width, const int height) { // external coords
    if (this->buffer_ != nullptr)
      delete this->buffer_;
    this->init_internal_(width * height * sizeof(Color));
    this->buffer_color_ = (Color*) this->buffer_;

    this->width = width;
    this->height = height;

    this->set_wrapped_area();
  };

  void set_wrapped_area() {
    this->set_wrapped_area(0, 0, this->width, this->height);
  };

  void set_wrapped_area(const int x0, const int y0, const int x1,
                        const int y1) {

    this->x0 = clamp(x0, 0, this->width);
    this->x1 = clamp(x1, 0, this->width);
    this->y0 = clamp(y0, 0, this->height);
    this->y1 = clamp(y1, 0, this->height);
    ESP_LOGD("set_wrapped_area", "x0=%i y0=%i x1=%i y1=%i", this->x0, this->y0,
             this->x1, this->y1);
  };

  void dump_buffer() {
    ESP_LOGD("ScrollDispBuffer", "----- %4i x %4i -------",
             this->get_width_internal(), get_height_internal());

    for (int row = 0; row < this->get_height_internal(); row++) {
      char text[this->get_width_internal() + 1];
      for (int col = 0; col < this->get_width_internal(); col++) {
        int pos = col + row * this->get_width_internal();

        if ((pos < 0) || (pos > (this->width * this->height))) {
          ESP_LOGD("main", "!!!X!!!!! pos=%i", pos);
          continue;
        }
        Color c =this->buffer_color_[pos];
        if (c.is_on())
          {
          	text[col] = (c.w > 0x80 ? 'W' : (c.b > 0x80 ? 'B' : (c.g > 0x80 ? 'G' : 'R')));
          }
        else
          text[col] = ' ';
      }
      text[this->get_width_internal()] = 0;
      ESP_LOGD("ScrollDispBuffer", "%4i |%s|", row, text);
    };
  };

  struct coord to_addressable_lights(
      light::AddressableLight *lights, int rows, int columns, int x, int y,
      DisplayOrient rotation = DisplayOrient::DISPLAY_ROTATION_0_DEGREES)
  // x,y coordinate of the upper left corner
  {

    struct coord upperleftcoord;

    if (rows * columns > (*(lights)).size()) {
      ESP_LOGD("MatrixLedDisplay",
               "Size mismatch of AddressableLight and Matrix");
      return upperleftcoord;
    }

    int vp_pos = 0;

    for (int vp_x = 0; vp_x < columns; vp_x++)
      for (int vp_y = 0; vp_y < rows; vp_y++) {
        int vp_x_rot, vp_y_rot;

        // rotate
        switch (rotation & 0x0f) {
        default:
        case DisplayOrient::DISPLAY_ROTATION_0_DEGREES:
          vp_x_rot = vp_x;
          vp_y_rot = vp_y;
          break;
        case DisplayOrient::DISPLAY_ROTATION_90_DEGREES:
          vp_x_rot = rows - vp_y - 1;
          vp_y_rot = vp_x;
          break;
        case DisplayOrient::DISPLAY_ROTATION_180_DEGREES:
          vp_x_rot = columns - vp_x - 1;
          vp_y_rot = rows - vp_y - 1;
          break;
        case DisplayOrient::DISPLAY_ROTATION_270_DEGREES:
          vp_x_rot = vp_y;
          vp_y_rot = columns - vp_x - 1;
          break;
        }

        if (rotation & DISPLAY_FLIP_HORIZONTAL)
          vp_x_rot = columns - vp_x_rot - 1;

        if (rotation & DISPLAY_FLIP_VERTICAL)
          vp_y_rot = rows - vp_y_rot - 1;
         
        int thisx = wrap(vp_x_rot + x, x0, x1);
        int thisy = wrap(vp_y_rot + y, y0, y1);

        if ((rotation &  DISPLAY_ZIGZAG) &&  ((thisy & 0x01) == 0)) 
        	thisx = this->get_width_internal() - thisx - 1;  

        int pos = thisx + thisy * this->get_width_internal();

        if (vp_pos == 0) {
          // ESP_LOGD("main", "x=%i y=%i thisx=%i thisy=%i ", x, y, thisx,
          // thisy);
          upperleftcoord.x = thisx;
          upperleftcoord.y = thisy;
        }

        if ((pos < 0) || (pos > (this->get_width_internal() *
                                 this->get_height_internal()))) {
          ESP_LOGD("main", "!!!!!!!! pos=%i thisxint=%i thisyint=%i", pos,
                   thisx, thisy);
          return upperleftcoord;
        }

        Color c = this->buffer_color_[pos];
        (*(lights))[vp_pos] = light::ESPColor(c.r, c.g, c.b, c.w);

        vp_pos++;
      }
    return upperleftcoord;
  }

protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override {
    if (x >= this->get_width_internal() || x < 0 ||
        y >= this->get_height_internal() || y < 0)
      return;
    int pos = x + y * this->get_width_internal();
    this->buffer_color_[pos] = color;
  }

  Color *buffer_color_;

  int get_height_internal() override { return this->height; }
  int get_width_internal() override { return this->width; }

  int width{0};
  int height{0};

  int x0{0};
  int x1{0};
  int y0{0};
  int y1{0};
};
