struct ui_field_topdown {
   v2 size_in_ft;
   rect2 bounds; 
   element *e;
};

#define FieldTopdown(...) _FieldTopdown(GEN_UI_ID, __VA_ARGS__)
ui_field_topdown _FieldTopdown(ui_id id, element *parent, texture background_image, 
                   v2 size_in_ft, f32 width) {
   width = Clamp(0, 700, width);
   v2 size = V2(width, size_in_ft.y  * (width / size_in_ft.x));
   f32 x_padding = (Size(parent->bounds).x - size.x) / 2;
   element *e = _Panel(id, parent, size, Padding(x_padding, 0));
   Texture(e, background_image, e->bounds);

   ui_field_topdown result = {};
   result.size_in_ft = size_in_ft;
   result.bounds = e->bounds;
   result.e = e;
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

v2 FeetToPixels(ui_field_topdown *field, v2 in_ft) {
   return V2(FeetToPixels(field, in_ft.x), FeetToPixels(field, in_ft.y));
}

v2 GetPoint(ui_field_topdown *field, v2 p_in_ft) {
   return Center(field->bounds) + FeetToPixels(field, p_in_ft);
}

v2 CubicHermiteSpline(v2 a_pos, v2 a_tan, v2 b_pos, v2 b_tan, f32 t) {
   return (2*t*t*t - 3*t*t + 1)*a_pos + (t*t*t - 2*t*t + t)*a_tan + 
          (-2*t*t*t + 3*t*t)*b_pos + (t*t*t - t*t)*b_tan;
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