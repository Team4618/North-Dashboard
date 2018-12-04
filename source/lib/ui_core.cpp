struct texture {
   v2 size;
   GLuint handle;
};

struct glyphInfo {
   v2 textureLocation;
   v2 size;
   v2 offset;
   f32 xadvance;
};

//TODO: load fonts from a ttf using stb_truetype 
struct sdfFont {
   texture sdfTexture;
   f32 native_line_height;
   f32 max_char_width;
   glyphInfo glyphs[128];
};

//---------------------------
struct glyph_texture {
   u32 codepoint;
   texture tex;
   v2 size_over_line_height;
};

struct loaded_font {
   MemoryArena arena;
   glyph_texture *glyphs[128];
   stbtt_fontinfo fontinfo;

   f32 ascent;
   f32 descent;
   f32 line_gap;
};
//---------------------------

enum RenderCommandType {
   RenderCommand_SDF,
   RenderCommand_Texture,
   RenderCommand_Rectangle,
   RenderCommand_Line
};

struct RenderCommand {
   RenderCommandType type;
   RenderCommand *next;
   
   union {
      struct {
         rect2 bounds;
         rect2 uvBounds; //NOTE: specified in texture space, not 0 to 1
         texture sdf;
         v4 colour;
      } drawSDF;
      
      struct { 
         rect2 bounds;
         rect2 uvBounds; //NOTE: specified in texture space, not 0 to 1
         texture tex; 
      } drawTexture;
      
      struct {
         rect2 bounds;
         v4 colour;
      } drawRectangle;
      
      struct {
         v4 colour;
         v2 a;
         v2 b;
         f32 thickness;
      } drawLine;
   };
};

texture loadTexture(char *path, bool in_exe_directory = false);
texture createTexture(u32 *texels, u32 width, u32 height);
void deleteTexture(texture tex);

struct ui_id {
   char *loc;
   u64 a;
   u64 b;
};

ui_id UIID(char *loc, u64 a, u64 b) {
   ui_id result = {loc, a, b};
   return result;
}

bool operator== (ui_id a, ui_id b) {
   return (a.a == b.a) && (a.b == b.b);
}

bool operator!= (ui_id a, ui_id b) {
   return (a.a != b.a) || (a.b != b.b);
}

//TODO: im not a fan of this, profile to see how spread out GEN_UI_ID id's are
ui_id operator+ (ui_id a, ui_id b) {
   return UIID(a.loc, a.a + b.a, a.b + b.b);
}

#define __STRINGIFY(x) #x
#define __TOSTRING(x) __STRINGIFY(x)

#define GEN_UI_ID UIID(__FILE__ __TOSTRING(__LINE__), (u64) (__FILE__ __TOSTRING(__LINE__)), 0)
#define POINTER_UI_ID(pointer) UIID(__FILE__ __TOSTRING(__LINE__), (u64) (__FILE__ __TOSTRING(__LINE__)), (u64) pointer)
#define NULL_UI_ID UIID((char *)0, (u64) 0, (u64) 0)

struct InputState {
   //NOTE: Mouse pos, scroll & buttons
   v2 last_pos;
   v2 pos;
   f32 vscroll;
   f32 hscroll;
   
   bool left_down;
   bool left_up;

   bool right_down;
   bool right_up;
   
   //NOTE: keyboard text entry stuff
   char key_char;
   bool key_backspace;
   bool key_enter;
   bool key_up_arrow;
   bool key_down_arrow;
   bool key_left_arrow;
   bool key_right_arrow;
};

struct persistent_hash_link {
   persistent_hash_link *next;
   ui_id id;
   u8 *data;
};

//TODO: debugging mode stuff
struct element;
struct UIContext {
   sdfFont *font;
   MemoryArena frame_arena;
   
   MemoryArena persistent_arena;
   persistent_hash_link *persistent_hash[128];
  
   ui_id hot_e;
   ui_id new_hot_e;

   ui_id active_e;
   ui_id new_active_e;   
   
   ui_id clicked_e;
   ui_id new_clicked_e;
   
   ui_id selected_e;
   
   ui_id dragged_e;
   v2 drag;
   ui_id new_dragged_e;
   v2 new_drag;
   
   ui_id vscroll_e;
   f32 vscroll;
   ui_id new_vscroll_e;
   f32 new_vscroll;

   ui_id hscroll_e;
   f32 hscroll;
   ui_id new_hscroll_e;
   f32 new_hscroll;

   ui_id filedrop_e;
   ui_id new_filedrop_e;
   MemoryArena filedrop_arena;
   u32 filedrop_count;
   string *filedrop_names;

   f64 curr_time;
   f64 dt;
   f32 fps;

   ui_id scope_id;

   //below will probably get rewritten   
   InputState input_state;
};

//----------------------------------------------------
//TODO: test this, remove most uses of POINTER_UI_ID
#define UI_SCOPE(context, ptr) _ui_scope _UI_SCOPE_##__LINE__ ((context), POINTER_UI_ID(ptr)); 
struct _ui_scope {
   UIContext *context;
   ui_id prev_scope_id;

   _ui_scope(UIContext *context_in, ui_id scope_id) {
      context = context_in;
      prev_scope_id = context->scope_id;
      context->scope_id = scope_id;
   }

   ~_ui_scope() {
      context->scope_id = prev_scope_id;
   }
};
//----------------------------------------------------
typedef rect2 (*layout_callback)(element *e, u8 *layout_data, v2 element_size, v2 padding_size, v2 margin_size);

struct element {
   UIContext *context;
   element *parent;
   element *next;
   
   element *curr_child;
   element *first_child;
   
   ui_id id;
   
   rect2 bounds;
   rect2 cliprect;
   RenderCommand *first_command;
   RenderCommand *curr_command;
   
   u32 captures;

   u8 *layout_data;
   layout_callback layout_func;
};

v2 Size(element *e) {
   return Size(e->bounds);
}

v2 Center(element *e) {
   return Center(e->bounds);
}

element *beginFrame(v2 window_size, UIContext *context, f32 dt) {
   context->hot_e = context->new_hot_e;
   context->new_hot_e = NULL_UI_ID;

   context->active_e = context->new_active_e;
   context->new_active_e = NULL_UI_ID;
   
   context->clicked_e = context->new_clicked_e;
   context->new_clicked_e = NULL_UI_ID;
   
   context->dragged_e = context->new_dragged_e;
   context->drag = context->new_drag;
   context->new_clicked_e = NULL_UI_ID;
   context->new_drag = V2(0, 0);

   context->vscroll_e = context->new_vscroll_e;
   context->vscroll = context->new_vscroll;
   context->new_vscroll_e = NULL_UI_ID;
   context->new_vscroll = 0;

   context->hscroll_e = context->new_hscroll_e;
   context->hscroll = context->new_hscroll;
   context->new_hscroll_e = NULL_UI_ID;
   context->new_hscroll = 0;

   context->filedrop_e = context->new_filedrop_e;
   context->new_filedrop_e = NULL_UI_ID;
   
   context->curr_time += dt;
   context->dt = dt;
   context->fps = 1.0 / dt;

   Reset(&context->frame_arena);
   element *root = PushStruct(&context->frame_arena, element);
   root->context = context;
   root->bounds = RectMinSize(V2(0, 0), window_size);
   root->cliprect = RectMinSize(V2(0, 0), window_size);
   return root;
}

void addCommand(element *e, RenderCommand *command) {
   if(e->first_command == NULL) {
      e->first_command = command;
   } else {
      e->curr_command->next = command;
   }
   
   e->curr_command = command;
}

//Render command helper functions---------------------------------
RenderCommand *Rectangle(element *e, rect2 bounds, v4 colour) {
   UIContext *context = e->context;
   RenderCommand *result = PushStruct(&context->frame_arena, RenderCommand);
   result->type = RenderCommand_Rectangle;
   result->next = NULL;
   result->drawRectangle.bounds = bounds;
   result->drawRectangle.colour = colour;
   
   addCommand(e, result);
   return result;
}

RenderCommand *Background(element *e, v4 colour) {
   return Rectangle(e, e->bounds, colour);
}

RenderCommand *Line(element *e, v2 a, v2 b, v4 colour, f32 thickness = 2) {
   UIContext *context = e->context;
   RenderCommand *result = PushStruct(&context->frame_arena, RenderCommand);
   result->type = RenderCommand_Line;
   result->next = NULL;
   result->drawLine.a = a;
   result->drawLine.b = b;
   result->drawLine.colour = colour;
   result->drawLine.thickness = thickness;
   
   addCommand(e, result);
   return result;
}

void Outline(element *e, rect2 b, v4 colour, f32 thickness = 2) {
   //TODO: move the outline in by the thickness
   Line(e, V2(b.min.x, b.min.y), V2(b.max.x, b.min.y), colour, thickness);
   Line(e, V2(b.max.x, b.min.y), V2(b.max.x, b.max.y), colour, thickness);
   Line(e, V2(b.max.x, b.max.y), V2(b.min.x, b.max.y), colour, thickness);
   Line(e, V2(b.min.x, b.max.y), V2(b.min.x, b.min.y), colour, thickness);
}

void Outline(element *e, v4 colour, f32 thickness = 2) {
   Outline(e, e->bounds, colour, thickness);
}

RenderCommand *Texture(element *e, texture tex, rect2 bounds) {
   UIContext *context = e->context;
   RenderCommand *result = PushStruct(&context->frame_arena, RenderCommand);
   result->type = RenderCommand_Texture;
   result->next = NULL;
   result->drawTexture.bounds = bounds;
   result->drawTexture.uvBounds = RectMinSize(V2(0, 0), tex.size);
   result->drawTexture.tex = tex;
   
   addCommand(e, result);
   return result;
}

f32 TextWidth(sdfFont *font, string text, f32 line_height) {   
   f32 width = 0;
   f32 scale = line_height / font->native_line_height;

   for(u32 i = 0; i < text.length; i++) {
      u32 id = (u32) text.text[i];
      
      if(id < ArraySize(font->glyphs)) {
         glyphInfo glyph = font->glyphs[id];
               
         width += scale * glyph.xadvance;
      }
   }

   return width;
}

f32 GetMaxCharWidth(sdfFont *font, f32 line_height) {
   f32 scale = line_height / font->native_line_height;
   return font->max_char_width * scale;      
}

rect2 GetCharBounds(sdfFont *font, string text, u32 char_i, v2 pos, f32 line_height) {
   f32 scale = line_height / font->native_line_height;
   
   for(u32 i = 0; i < text.length; i++) {
      u32 id = (u32) text.text[i];
      
      if(id < ArraySize(font->glyphs)) {
         glyphInfo glyph = font->glyphs[id];
         f32 width = scale * glyph.size.x;
         f32 height = scale * glyph.size.y; 

         if(i == char_i) {
            return RectMinSize((scale * glyph.offset) + pos, V2(width, height));
         }

         pos = pos + V2(scale * glyph.xadvance, 0);
      }
   }

   return RectMinSize(V2(0, 0), V2(0, 0));
}

//TODO: kerning
// void Text(element *e, string text, v2 pos, f32 line_height, v4 colour = V4(0, 0, 0, 1)) {   
//    UIContext *context = e->context;
//    sdfFont *font = context->font;
//    f32 scale = line_height / font->native_line_height;
   
//    for(u32 i = 0; i < text.length; i++) {
//       u32 id = (u32) text.text[i];
      
//       if(id < ArraySize(font->glyphs)) {
//          glyphInfo glyph = font->glyphs[id];
//          f32 width = scale * glyph.size.x;
//          f32 height = scale * glyph.size.y; 

//          RenderCommand *command = PushStruct(&context->frame_arena, RenderCommand);
//          command->type = RenderCommand_SDF;
//          command->next = NULL;
//          command->drawSDF.bounds = RectMinSize((scale * glyph.offset) + pos, 
//                                                V2(width, height));
//          command->drawSDF.uvBounds = RectMinSize(glyph.textureLocation, glyph.size);
//          command->drawSDF.sdf = font->sdfTexture;
//          command->drawSDF.colour = colour;
         
//          addCommand(e, command);
//          pos = pos + V2(scale * glyph.xadvance, 0);
//       }
//    }
// }

extern loaded_font test_font;
glyph_texture *getOrLoadGlyph(loaded_font *font, u32 codepoint);

void Text(element *e, string text, v2 pos, f32 line_height, v4 colour) {   
   UIContext *context = e->context;
   
   for(u32 i = 0; i < text.length; i++) {
      glyph_texture *glyph = getOrLoadGlyph(&test_font, text.text[i]);
      v2 size = line_height * glyph->size_over_line_height;
      
      //TODO: actually respect the text colour
      Texture(e, glyph->tex, RectMinSize(pos, size));
      Outline(e, RectMinSize(pos, size), V4(0, 0, 0, 1));
      pos = pos + V2(size.x, 0);
   }
}

void Text(element *e, char *s, v2 pos, f32 height, v4 colour) {
   Text(e, Literal(s), pos, height, colour);
}

//TODO: pass font in as a param, dont store in context?
//or maybe we could do themeing in the context?  

#define RED V4(1, 0, 0, 1)
#define BLUE V4(0, 0, 1, 1)
#define GREEN V4(0, 1, 0, 1)
#define WHITE V4(1, 1, 1, 1)
#define BLACK V4(0, 0, 0, 1)
//---------------------------------------------------------------------

v2 Cursor(element *e) {
   UIContext *context = e->context;
   return context->input_state.pos;
}

bool ContainsCursor(element *e) {
   UIContext *context = e->context;
   return Contains(e->cliprect, context->input_state.pos);
}

//Interactions---------------------------------------------------------
enum ui_interaction_captures {
   INTERACTION_HOT = (1 << 0),
   _INTERACTION_ACTIVE = (1 << 1),
   INTERACTION_ACTIVE = INTERACTION_HOT | _INTERACTION_ACTIVE,
   _INTERACTION_CLICK = (1 << 2),
   INTERACTION_CLICK = INTERACTION_ACTIVE | _INTERACTION_CLICK,
   _INTERACTION_SELECT = (1 << 3),
   INTERACTION_SELECT = INTERACTION_CLICK | _INTERACTION_SELECT,
   _INTERACTION_DRAG = (1 << 4),
   INTERACTION_DRAG = INTERACTION_ACTIVE | _INTERACTION_DRAG,
   _INTERACTION_VERTICAL_SCROLL = (1 << 5),
   INTERACTION_VERTICAL_SCROLL = INTERACTION_HOT | _INTERACTION_VERTICAL_SCROLL,
   _INTERACTION_HORIZONTAL_SCROLL = (1 << 6),
   INTERACTION_HORIZONTAL_SCROLL = INTERACTION_HOT | _INTERACTION_HORIZONTAL_SCROLL,
   INTERACTION_FILEDROP = (1 << 7)
};

#define AssertHasFlags(var, flags) Assert(((var) & (flags)) == (flags))

bool IsHot(element *e) {
   AssertHasFlags(e->captures, INTERACTION_HOT);
   return e->id == e->context->hot_e;
}

bool IsActive(element *e) {
   AssertHasFlags(e->captures, INTERACTION_ACTIVE);
   return e->id == e->context->active_e;
}

bool WasClicked(element *e) {
   AssertHasFlags(e->captures, INTERACTION_CLICK);
   return e->id == e->context->clicked_e;
}

bool IsSelected(element *e) {
   AssertHasFlags(e->captures, INTERACTION_SELECT);
   return e->id == e->context->selected_e;
}

v2 GetDrag(element *e) {
   AssertHasFlags(e->captures, INTERACTION_DRAG);
   return (e->id == e->context->dragged_e) ? e->context->drag : V2(0, 0);
}

f32 GetVerticalScroll(element *e) {
   AssertHasFlags(e->captures, INTERACTION_VERTICAL_SCROLL);
   return (e->id == e->context->vscroll_e) ? e->context->vscroll : 0;
}

f32 GetHorizontalScroll(element *e) {
   AssertHasFlags(e->captures, INTERACTION_HORIZONTAL_SCROLL);
   return (e->id == e->context->hscroll_e) ? e->context->hscroll : 0;
}

struct ui_dropped_files {
   u32 count;
   string *names;
};

ui_dropped_files GetDroppedFiles(element *e) {
   ui_dropped_files result = {};
   if(e->id == e->context->filedrop_e) {
      result.count = e->context->filedrop_count;
      result.names = e->context->filedrop_names;
   }
   return result;
}

void uiTick(element *e) {
   UIContext *context = e->context;
   InputState *input = &context->input_state; 
   
   if(e->captures & INTERACTION_HOT) {
      bool can_become_hot = (context->active_e == NULL_UI_ID) || (context->active_e == e->id);
      if(Contains(e->bounds, input->pos) && can_become_hot) {
         context->new_hot_e = e->id;
      }
   }

   if(e->captures & _INTERACTION_ACTIVE) {
      bool can_become_active = IsHot(e) && input->left_down;
      bool should_remain_active = IsActive(e) && input->left_down;
      if(can_become_active || should_remain_active) {
         context->new_active_e = e->id;
      }
   }

   if(e->captures & _INTERACTION_CLICK) {
      if(IsActive(e) && IsHot(e) && input->left_up) {
         context->new_clicked_e = e->id;

         if(e->captures & _INTERACTION_SELECT) {
            context->selected_e = IsSelected(e) ? NULL_UI_ID : e->id;
         }
      }
   }

   if(e->captures & _INTERACTION_DRAG) {
      if(IsActive(e)) {
         context->new_dragged_e = e->id;
         context->new_drag = input->pos - input->last_pos;
      }
   }

   if(e->captures & _INTERACTION_VERTICAL_SCROLL) {
      if(IsHot(e)) {
         context->new_vscroll_e = e->id;
         context->new_vscroll = input->vscroll;
      }
   }

   if(e->captures & _INTERACTION_HORIZONTAL_SCROLL) {
      if(IsHot(e)) {
         context->new_hscroll_e = e->id;
         context->new_hscroll = input->hscroll;
      }
   }

   if(e->captures & INTERACTION_FILEDROP) {
      if(IsHot(e)) {
         context->new_filedrop_e = e->id;
      }
   }
 }

//---------------------------------------------------------------------

typedef void (*layout_setup_callback)(element *e);

//NOTE: this is super stupid, but hey it works
struct panel_args {
   layout_setup_callback _layout_setup;
   v2 _padding;
   v2 _margin;
   u32 _captures;

   panel_args Layout(layout_setup_callback _layout_setup) {
      panel_args result = *this;
      result._layout_setup = _layout_setup;
      return result;
   }

   panel_args Padding(v2 _padding) {
      panel_args result = *this;
      result._padding = _padding;
      return result;
   }

   panel_args Padding(f32 x, f32 y) {
      panel_args result = *this;
      result._padding = V2(x, y);
      return result;
   }

   panel_args Margin(v2 _margin) {
      panel_args result = *this;
      result._margin = _margin;
      return result;
   }

   panel_args Margin(f32 x, f32 y) {
      panel_args result = *this;
      result._margin = V2(x, y);
      return result;
   }

   panel_args Captures(u32 _captures) {
      panel_args result = *this;
      result._captures |= _captures;
      return result;
   }
};

panel_args Layout(layout_setup_callback _layout_setup) {
   panel_args result = {};
   return result.Layout(_layout_setup);
}

panel_args Padding(v2 _padding) {
   panel_args result = {};
   return result.Padding(_padding);
}

panel_args Padding(f32 x, f32 y) {
   panel_args result = {};
   return result.Padding(V2(x, y));
}

panel_args Margin(v2 _margin) {
   panel_args result = {};
   return result.Margin(_margin);
}

panel_args Margin(f32 x, f32 y) {
   panel_args result = {};
   return result.Margin(V2(x, y));
}

panel_args Captures(u32 _captures) {
   panel_args result = {};
   return result.Captures(_captures);
}

#define Panel(...) _Panel(GEN_UI_ID, __VA_ARGS__)
element *_Panel(ui_id id, element *parent, rect2 bounds, panel_args args = {}) {
   UIContext *context = parent->context;
   element *e = PushStruct(&context->frame_arena, element);
   
   e->context = context;
   e->parent = parent;
   e->id = id + context->scope_id;
   e->bounds = bounds;
   e->cliprect = Overlap(parent->cliprect, bounds);
   e->captures = args._captures;
   if(args._layout_setup != NULL) {
         args._layout_setup(e);
   }
   
   if(parent->first_child == NULL) {
      parent->first_child = e;
   } else {
      parent->curr_child->next = e;
   }
   
   parent->curr_child = e;

   return e;
}

element *_Panel(ui_id id, element *parent, v2 size, panel_args args = {}) {
   Assert(parent->layout_func != NULL);
   rect2 bounds = parent->layout_func(parent, parent->layout_data, size, args._padding, args._margin);
   return _Panel(id, parent, bounds, args);
}

//Common layout types-----------------------------------
rect2 columnLayout(element *e, u8 *layout_data, v2 element_size, v2 padding_size, v2 margin_size) {
   v2 *at = (v2 *) layout_data;
   v2 pos = *at;
   at->y += element_size.y + padding_size.y + margin_size.y;
   return RectMinSize(e->bounds.min + pos + padding_size, element_size);
}

void ColumnLayout(element *e) {
   UIContext *context = e->context;
   e->layout_func = columnLayout;
   e->layout_data = (u8 *) PushStruct(&context->frame_arena, v2);
}

#define ColumnPanel(...) _ColumnPanel(GEN_UI_ID, __VA_ARGS__)
element *_ColumnPanel(ui_id id, element *parent, v2 size, panel_args args = {}) {
   return _Panel(id, parent, size, args.Layout(ColumnLayout));
}  

element *_ColumnPanel(ui_id id, element *parent, rect2 bounds, panel_args args = {}) {
   return _Panel(id, parent, bounds, args.Layout(ColumnLayout));
}

rect2 rowLayout(element *e, u8 *layout_data, v2 element_size, v2 padding_size, v2 margin_size) {
   v2 *at = (v2 *) layout_data;
   v2 pos = *at;
   at->x += element_size.x + padding_size.x + margin_size.x;
   return RectMinSize(e->bounds.min + pos + padding_size, element_size);
}

void RowLayout(element *e) {
   UIContext *context = e->context;
   e->layout_func = rowLayout;
   e->layout_data = (u8 *) PushStruct(&context->frame_arena, v2);
}

#define RowPanel(...) _RowPanel(GEN_UI_ID, __VA_ARGS__)
element *_RowPanel(ui_id id, element *parent, v2 size, panel_args args = {}) {
   return _Panel(id, parent, size, args.Layout(RowLayout));
}  

element *_RowPanel(ui_id id, element *parent, rect2 bounds, panel_args args = {}) {
   return _Panel(id, parent, bounds, args.Layout(RowLayout));
}

rect2 stackLayout(element *e, u8 *layout_data, v2 element_size, v2 padding_size, v2 margin_size) {
   return RectMinSize(e->bounds.min + padding_size + margin_size, element_size);
}

void StackLayout(element *e) {
   e->layout_func = stackLayout;
}

#define StackPanel(...) _StackPanel(GEN_UI_ID, __VA_ARGS__)
element *_StackPanel(ui_id id, element *parent, v2 size, panel_args args = {}) {
   return _Panel(id, parent, size, args.Layout(StackLayout));
}  

element *_StackPanel(ui_id id, element *parent, rect2 bounds, panel_args args = {}) {
   return _Panel(id, parent, bounds, args.Layout(StackLayout));
}
//--------------------------------------------------------

//TODO: text colour, maybe redo optional params like we did for other stuff
#define Label(...) _Label(GEN_UI_ID, __VA_ARGS__)
element *_Label(ui_id id, element *parent, string text, f32 line_height, v4 text_colour, 
                v2 p = V2(0, 0), v2 m = V2(0, 0)) 
{
   UIContext *context = parent->context;
   f32 width = TextWidth(context->font, text, line_height);

   element *result = _Panel(id, parent, V2(width, line_height), Padding(p).Margin(m));
   Text(result, text, result->bounds.min, line_height, text_colour);
   return result;
}

element *_Label(ui_id id, element *parent, char *text, f32 line_height, v4 text_colour,
                v2 p = V2(0, 0), v2 m = V2(0, 0)) 
{
   return _Label(id, parent, Literal(text), line_height, text_colour, p, m);
}

//Persistent-Data-------------------------------------------------
#define GetOrAllocate(e, type) (type *) _GetOrAllocate(e->id, e->context, sizeof(type))
u8 *_GetOrAllocate(ui_id in_id, UIContext *context, u32 size) {
   u8 *result = NULL;

   ui_id id = in_id + context->scope_id;
   u32 hash = (3 * id.a + 4 * id.b) % ArraySize(context->persistent_hash); //TODO: better hash function :)
   for(persistent_hash_link *link = context->persistent_hash[hash]; link; link = link->next) {
      if(link->id == id) {
         result = link->data;
         break;
      }
   }
   
   if(result == NULL) {
      result = PushSize(&context->persistent_arena, size);
      OutputDebugStringA("[ui_core] Allocating new persistant data\n");

      persistent_hash_link *new_link = PushStruct(&context->persistent_arena, persistent_hash_link);
      new_link->id = id;
      new_link->data = result;
      new_link->next = context->persistent_hash[hash];
      context->persistent_hash[hash] = new_link;
   }
   
   return result;
}

#define UIPersistentData(ctx, type) (type *) _GetOrAllocate(GEN_UI_ID, ctx, sizeof(type))