struct ui_field_topdown {
   v2 size_in_ft;
   rect2 bounds; 
   element *e;
   bool clicked;
};

v2 GetPoint(ui_field_topdown *field, v2 p_in_ft);

#define FieldTopdown(...) _FieldTopdown(GEN_UI_ID, __VA_ARGS__)
ui_field_topdown _FieldTopdown(ui_id id, element *parent, texture background_image, 
                   v2 size_in_ft, f32 width) {
   v2 size = V2(width, size_in_ft.y  * (width / size_in_ft.x));
   f32 x_padding = (Size(parent->bounds).x - size.x) / 2;
   element *e = _Panel(id, parent, Size(size).Padding(x_padding, 0).Captures(INTERACTION_CLICK));
   Texture(e, background_image, e->bounds);

   ui_field_topdown result = {};
   result.size_in_ft = size_in_ft;
   result.bounds = e->bounds;
   result.e = e;
   result.clicked = WasClicked(e);

   if(parent->context->debug_mode != UIDebugMode_Disabled) {
      Rectangle(e, RectCenterSize(GetPoint(&result, V2(0, 0)), V2(5, 5)), BLACK);
      Rectangle(e, RectCenterSize(GetPoint(&result, V2(0.25 * size_in_ft.x, 0)), V2(5, 5)), BLACK);
      Rectangle(e, RectCenterSize(GetPoint(&result, V2(0, 0.25 * size_in_ft.x)), V2(5, 5)), BLACK);
   }

   return result;
}

f32 PixelsToFeet(ui_field_topdown *field, f32 in_px) {
   f32 px_to_ft = field->size_in_ft.x / Size(field->bounds).x;
   f32 in_ft = in_px * px_to_ft;
   return in_ft;
}

v2 PixelsToFeet(ui_field_topdown *field, v2 in_px) {
   return V2(PixelsToFeet(field, in_px.x), PixelsToFeet(field, in_px.y));
}

f32 FeetToPixels(ui_field_topdown *field, f32 in_ft) {
   f32 ft_to_px = Size(field->bounds).x / field->size_in_ft.x;
   f32 in_px = in_ft * ft_to_px;
   return in_px;
}

//TODO: remove?
v2 FeetToPixels(ui_field_topdown *field, v2 in_ft) {
   return V2(FeetToPixels(field, in_ft.x), FeetToPixels(field, in_ft.y));
}
//-------------

v2 GetPoint(ui_field_topdown *field, v2 p_in_ft) {
   return Center(field->bounds) + V2(FeetToPixels(field, p_in_ft.x), -FeetToPixels(field, p_in_ft.y));
}

void CubicHermiteSpline(ui_field_topdown *field, 
                        v2 a_pos, v2 a_tan, v2 b_pos, v2 b_tan, 
                        v4 colour, f32 thickness = 2)
{
   u32 point_count = 20;
   f32 step = (f32)1 / (f32)(point_count - 1);
   v2 *points = PushTempArray(v2, point_count);

   for(u32 i = 0; i < point_count; i++) {
      points[i] = GetPoint(field, CubicHermiteSpline(a_pos, a_tan, b_pos, b_tan, i * step));
   }

   _Line(field->e, colour, thickness, points, point_count);
}

//TODO: FORMALIZE----------------------------------------------
v2 DirectionNormal(f32 angle) {
   return V2(cosf(ToRadians(angle)), 
             sinf(ToRadians(angle)));
}
//-------------------------------------------------------------

void DrawRobot(ui_field_topdown *field, v2 size, 
               v2 pos, f32 angle, v4 colour)
{
   v2 heading = DirectionNormal(ToDegrees(angle));
   v2 a = pos + heading * 0.5*size.x + Perp(heading) * 0.5*size.y;
   v2 b = pos + heading * 0.5*size.x - Perp(heading) * 0.5*size.y;
   v2 c = pos - heading * 0.5*size.x - Perp(heading) * 0.5*size.y;
   v2 d = pos - heading * 0.5*size.x + Perp(heading) * 0.5*size.y;
   Loop(field->e, colour, 3, GetPoint(field, a), GetPoint(field, b), GetPoint(field, c), GetPoint(field, d));
   Line(field->e, colour, 3, GetPoint(field, pos), GetPoint(field, pos + heading*0.5*size.y));
}

//TODO: 3d field view --------------------------------------
#if 1
struct ui_field_persistent_data {
   bool init;
   mat4 camera;
};

struct ui_field { 
   element *e;
   mat4 camera;

   //TODO: redo the clicking system so it makes sense in 3d
   // bool clicked;
};

ui_field _FieldTopdown(ui_id id, element *parent, v2 size, mat4 *camera) {
   UIContext *context = parent->context;
   InputState input = context->input_state;
   
   element *e = _Panel(id, parent, Size(size).Captures(INTERACTION_SELECT));

   //Camera controls
   if(IsSelected(e)) {
      v3 vel = V3(0, 0, 0);
      f32 speed = 1;

      if(input.key_up_arrow) {
         vel.y -= speed;
      }

      if(input.key_down_arrow) {
         vel.y += speed;
      }

      if(input.key_left_arrow) {
         vel.x -= speed;
      }

      if(input.key_right_arrow) {
         vel.x += speed;
      }

      f32 dist = 10; //TODO: control this with scroll?
      v3 pos = V3(0, 0, dist);
      pos = pos + vel * context->dt;
   }

   ui_field result = {};
   result.e = e;
   result.camera = *camera;
   return result;
}

ui_field _FieldTopdown(ui_id id, element *parent, v2 size, mat4 default_camera = Orthographic(10, -10, -10, 10, 0.1, 100)) {
   ui_field_persistent_data *persistent_data = NULL; //GetOrAllocate(id, ui_field_persistent_data); 
   if(!persistent_data->init) {
      persistent_data->camera = default_camera;
      persistent_data->init = true;
   }

   return _FieldTopdown(id, parent, size, &persistent_data->camera);
}

/*
void DrawTriangles(ui_field *field, v3 *vertices, v4 *colours, u32 triangle_count) {
   //transform vertices & add to render command buffer
}

//TODO: how do we implement the auto editor in 3d?

*/
#endif