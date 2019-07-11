enum EditorView {
   EditorView_Blank,
   EditorView_OpenFile,
   EditorView_NewFile,
   EditorView_Editing
};

enum EditorSelectedType {
   NothingSelected,
   NodeSelected,
   PathSelected
};

enum PathEditMode {
   EditControlPoints,
   AddControlPoint,
   RemoveControlPoint,
};

struct EditorState {
   EditorView view;
   element *top_bar;

   //reset on directory change
   AutoProjectList auto_programs;
   
   //all pointers here are in project_arena
   MemoryArena *project_arena;
   AutoProjectLink *project;

   EditorSelectedType selected_type;
   union {
      AutoNode *selected_node;
      AutoPath *selected_path;
   };
   bool path_got_selected;
   PathEditMode path_edit;

   TextBoxData project_name_box;
   char _project_name_box[20];

   //Set every frame in DrawAutoEditor
   RobotProfile *profile;
   NorthSettings *settings;
};

//--------------------move-to-common---------------------------
#define ArrayInsert(arena, type, original, insert_index, new_elem, count) (type *) _ArrayInsert(arena, (u8 *) (original), insert_index, (u8 *) (new_elem), sizeof(type), count)
u8 *_ArrayInsert(MemoryArena *arena, u8 *original, u32 insert_index, 
                 u8 *new_elem, u32 element_size, u32 count)
{
   u8 *result = PushSize(arena, element_size * (count + 1));

   u32 before_count = insert_index;
   u32 after_count = count - insert_index;
   Copy(original, before_count * element_size, result);
   Copy(original + element_size * before_count, after_count * element_size, result + (before_count + 1) * element_size);
   Copy(new_elem, element_size, result + element_size * insert_index); 
   return result;
}

#define ArrayRemove(arena, type, original, remove_index, count) (type *) _ArrayRemove(arena, (u8 *) (original), remove_index, sizeof(type), count)
u8 *_ArrayRemove(MemoryArena *arena, u8 *original, 
                 u32 remove_index, u32 element_size, u32 count)
{
   u8 *result = PushSize(arena, element_size * (count - 1));

   u32 before_count = remove_index;
   Copy(original, before_count * element_size, result);
   Copy(original + element_size * (remove_index + 1), 
         (count - remove_index - 1) * element_size, result + before_count * element_size);
   return result;
}

#define ArrayMove(type, array, count, from, to) _ArrayMove(sizeof(type), (u8 *) (array), count, from, to)
void _ArrayMove(u32 size, u8 *array, u32 count, u32 from, u32 to) {
   TempArena temp_arena;
   u8 *temp_array = PushSize(&temp_arena.arena, count * size);

   if(to == from)
      return;

   if(to > from) {
      Copy(array, from * size, temp_array);
      Copy(array + (from + 1) * size, (to - from) * size, temp_array + from * size);
      Copy(array + from * size, size, temp_array + to * size);
      Copy(array + (to + 1) * size, (count - to) * size, temp_array + (to + 1) * size);
   } else if(to < from) {
      Copy(array, to * size, temp_array);
      Copy(array + from * size, size, temp_array + to * size);
      Copy(array + to * size, (from - to) * size, temp_array + (to + 1) * size);
      Copy(array + (from + 1) * size, (count - from) * size, temp_array + (from + 1) * size);
   }

   Copy(temp_array, count * size, array);
}
//---------------------------------------------------------------

//--------------------Kinda-specific-utils-----------------------
f32 MinDistFrom(ui_field_topdown *field, North_HermiteControlPoint *control_points, u32 control_point_count) {
   v2 p = Cursor(field->e);
   f32 result = F32_MAX;
   v2 curr_closest_point = V2(0, 0);

   for(u32 i = 1; i < control_point_count; i++) {
      North_HermiteControlPoint cpa = control_points[i - 1];
      North_HermiteControlPoint cpb = control_points[i];
      
      u32 point_count = 20;
      f32 step = (f32)1 / (f32)(point_count - 1);
      
      for(u32 i = 1; i < point_count; i++) {
         v2 a = GetPoint(field, CubicHermiteSpline(cpa, cpb, (i - 1) * step));
         v2 b = GetPoint(field, CubicHermiteSpline(cpa, cpb, i * step));
         
         v2 line_p0 = a;
         v2 line_d = b - a;
         f32 t = (Dot(p, line_d) - Dot(line_p0, line_d)) / Dot(line_d, line_d);

         if((0 <= t) && (t <= 1)) {
            v2 closest_point = line_p0 + t * line_d;
            f32 dist = Length(p - closest_point);
            
            if(dist < result) {
               result = dist;
               curr_closest_point = closest_point;
            }
         }
      }
   }

   // Rectangle(field->e, RectCenterSize(curr_closest_point, V2(5, 5)), GREEN);

   return result;
}
//---------------------------------------------------------------

#include "auto_editor_ui.cpp"

void initEditor(EditorState *state) {
   state->auto_programs.arena = PlatformAllocArena(Megabyte(10), "Auto Project");
   state->project_arena = PlatformAllocArena(Megabyte(10), "Auto Project");
   InitTextBoxData(&state->project_name_box, state->_project_name_box);
}

void DrawBlankView(element *page, EditorState *state) {
   Panel(state->top_bar, Size(40, Size(state->top_bar).y));
   if(Button(state->top_bar, "New", menu_button).clicked) {
      state->view = EditorView_NewFile;
   }

   if(Button(state->top_bar, "Open", menu_button).clicked) {
      state->view = EditorView_OpenFile;
   }
}

void DrawOpenFileView(element *page, EditorState *state) {
   Panel(state->top_bar, Size(40, Size(state->top_bar).y));
   if(Button(state->top_bar, "<- Back", menu_button).clicked) {
      state->view = EditorView_Blank;
   }

   for(AutoProjectLink *proj = state->auto_programs.first; proj; proj = proj->next) {
      UI_SCOPE(page->context, proj);
      bool valid = IsProjectCompatible(proj, state->profile);

      //TODO: draw previews, not just buttons
      //TODO: grey out buttons that are incompatible instead of just not working
      if(Button(page, proj->name, menu_button.IsEnabled(valid)).clicked) {
         Reset(state->project_arena);
         
         //TODO: currently we reload the project to copy it into its own arena, improve this
         state->project = ReadAutoProject(proj->name, state->project_arena);
         SetText(&state->project_name_box, state->project->name);
         state->view = EditorView_Editing;
         state->selected_type = NothingSelected;
      }
   }

   if(state->auto_programs.first == NULL) {
      Label(page, "No Auto Projects", V2(Size(page).x, 80), 50, BLACK);
   }
}

void DrawNewFileView(element *page, EditorState *state) {
   Panel(state->top_bar, Size(40, Size(state->top_bar).y));
   if(Button(state->top_bar, "<- Back", menu_button).clicked) {
      state->view = EditorView_Blank;
   }

   RobotProfile *profile = state->profile;
   ui_field_topdown field = FieldTopdown(page, state->settings->field.image, state->settings->field.size,
                                         Clamp(0, 700, Size(page->bounds).x));

   v2 robot_size_px =  FeetToPixels(&field, profile->size);
   for(u32 i = 0; i < state->settings->field.starting_position_count; i++) {
      Field_StartingPosition *starting_pos = state->settings->field.starting_positions + i;
      UI_SCOPE(page->context, starting_pos);
      
      element *field_starting_pos = Panel(field.e, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px),
                                          Captures(INTERACTION_CLICK));
      
      v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                              -sinf(starting_pos->angle * (PI32 / 180)));
      Background(field_starting_pos, IsHot(field_starting_pos) ? GREEN : RED);
      Line(field_starting_pos, BLACK, 2,
            Center(field_starting_pos->bounds),
            Center(field_starting_pos->bounds) + 10 * direction_arrow);           

      if(WasClicked(field_starting_pos)) {
         Reset(state->project_arena);
         state->project = PushStruct(state->project_arena, AutoProjectLink);
         state->project->starting_angle = starting_pos->angle;

         state->project->starting_node = PushStruct(state->project_arena, AutoNode);
         state->project->starting_node->pos = starting_pos->pos;
         
         state->view = EditorView_Editing;
      }   
   }
}

void DrawNodeGraphic(element *e, EditorState *state, AutoNode *node) {
   bool selected = (state->selected_type == NodeSelected) && (state->selected_node == node);
   Background(e, selected ? GREEN : RED);
   if(IsHot(e)) {
      Outline(e, BLACK);
   }
}

void DrawPath(ui_field_topdown *field, EditorState *state, AutoPath *path);
void DrawNode(ui_field_topdown *field, EditorState *state, AutoNode *node, bool can_drag) {
   UI_SCOPE(field->e->context, node);
   element *e = Panel(field->e, RectCenterSize(GetPoint(field, node->pos), V2(10, 10)),
                      Captures(INTERACTION_DRAG | INTERACTION_SELECT));
   DrawNodeGraphic(e, state, node);
   
   if(WasClicked(e)) {
      state->selected_type = NodeSelected;
      state->selected_node = node;
   }
   
   if(can_drag && (state->view == EditorView_Editing)) {
      v2 drag_vector = GetDrag(e);
      drag_vector.y *= -1;
      node->pos = ClampTo(node->pos + PixelsToFeet(field, drag_vector),
                        RectCenterSize(V2(0, 0), field->size_in_ft));

      if(Length(drag_vector) > 0) {
         if(node->in_path != NULL) {
            RecalculateAutoPath(node->in_path);
         }

         for(u32 i = 0; i < node->path_count; i++) {
            RecalculateAutoPath(node->out_paths[i]);
         }
      }
   }

   for(u32 i = 0; i < node->path_count; i++) {
      DrawPath(field, state, node->out_paths[i]);
   }
}

void DrawPath(ui_field_topdown *field, EditorState *state, AutoPath *path) {
   UI_SCOPE(field->e->context, path);
   
   if(path->hidden)
      return;

   AutoPathSpline path_spline = GetAutoPathSpline(path);
   bool hot = IsHot(field->e) && (MinDistFrom(field, path_spline.points, path_spline.point_count) < 2);

   // for(u32 i = 1; i < path_spline.point_count; i++) {
   //    North_HermiteControlPoint a = path_spline.points[i - 1];
   //    North_HermiteControlPoint b = path_spline.points[i];
   //    CubicHermiteSpline(field, a.pos, a.tangent, b.pos, b.tangent, BLACK);
   // }

   //TODO: make this better, make a "coloured line" call
   {
      u32 point_count = (u32) (path->length * 4 * 4 /*added this second "* 4" because CIRC stuff is in meters not ft*/);
      f32 step = path->length / (f32)(point_count - 1);

      for(u32 i = 1; i < point_count; i++) {
         v2 point_a = GetAutoPathPoint(path, step * (i - 1));
         v2 point_b  = GetAutoPathPoint(path, step * i);
         f32 t_velocity = GetVelocityAt(path, step * i) / 20;
         Line(field->e, hot ? GREEN : lerp(BLUE, t_velocity, RED), 2, GetPoint(field, point_a), GetPoint(field, point_b));
      }

      // for(u32 i = 0; i < point_count; i++) {
      //    v2 point_b  = GetAutoPathPoint(path, step * i);
      //    f32 t_velocity = GetVelocityAt(path, step * i) / 20;
      //    Rectangle(field->e, RectCenterSize(GetPoint(field, point_b), V2(5, 5)), lerp(BLUE, t_velocity, RED));
      // }
   }

   // u32 sample_count = Power(2, sample_exp); 
   // for(u32 i = 0; i < sample_count; i++) {
   //    Rectangle(field->e, RectCenterSize(GetPoint(field, path->len_samples[i].pos), V2(5, 5)), BLACK);
   // }

   for(u32 i = 0; i < path->data.discrete_event_count; i++) {
      Rectangle(field->e, RectCenterSize(GetPoint(field, GetAutoPathPoint(path, path->data.discrete_events[i].distance)), V2(5, 5)), BLACK);
   }

   if(field->clicked && hot) {
      state->selected_type = PathSelected;
      state->selected_path = path;
      state->path_got_selected = true;
      ClearSelected(field->e);
   }

   DrawNode(field, state, path->out_node);
}

void DrawAutoEditor(element *root, EditorState *state, 
                    NorthSettings *settings, RobotProfile *profile)
{
   state->settings = settings;
   state->profile = profile;

   state->top_bar = RowPanel(root, Size(Size(root).x, page_tab_height));
   Background(state->top_bar, dark_grey);
   
   element *page = VerticalList(StackPanel(root, RectMinMax(root->bounds.min + V2(0, status_bar_height + page_tab_height), root->bounds.max)));
   if(settings->field.loaded && IsValid(profile)) {
      switch(state->view) {
         case EditorView_Blank: DrawBlankView(page, state); break;
         case EditorView_OpenFile: DrawOpenFileView(page, state); break;
         case EditorView_NewFile: DrawNewFileView(page, state); break;
         case EditorView_Editing: DrawEditingView(page, state); break;
      }
   } else {
      if(!settings->field.loaded)
         Label(page, "No Field", V2(Size(page).x, 80), 50, BLACK);
      
      if(!IsValid(profile))
         Label(page, "No Robot", V2(Size(page).x, 80), 50, BLACK);
   }
}