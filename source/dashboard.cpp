enum GameMode {
   GameMode_Disabled = 0,
   GameMode_Autonomous = 1,
   GameMode_Teleop = 2,
   GameMode_Test = 3,
};

string ToString(GameMode mode) {
   switch(mode) {
      case GameMode_Disabled:
         return Literal("Disabled");
      case GameMode_Autonomous:
         return Literal("Autonomous");
      case GameMode_Teleop:
         return Literal("Teleop");
      case GameMode_Test:
         return Literal("Test");
   }
   return EMPTY_STRING;
}

/*
struct SubsystemCommand {
   string name;

   string *param_names;
   u32 param_count;
};
*/

struct Subsystem {
   string name;
   //SubsystemCommand *commands;
   u32 command_count;

   MultiLineGraphData diagnostics_graph;
};

enum DashboardPage {
   DashboardPage_Home,
   DashboardPage_Subsystem,
   DashboardPage_AutoRuns,
   DashboardPage_Robots,
   DashboardPage_Settings
};

#include "north_file_definitions.h"

struct DashboardState {
   MemoryArena state_arena;
   
   DashboardPage page;
   Subsystem *selected_subsystem;
   //selected_auto_run
   //selected_robot_profile
   
   GameMode prev_mode;
   GameMode mode;

   bool recording_auto_diagnostics;

   //TODO: we can either be connected or load robot data from a file
   bool connected;

   struct {
      bool loaded;
      string name;
      v2 size;

      Subsystem *subsystems;
      u32 subsystem_count;
   } robot;

   MemoryArena settings_arena; //TODO: this is just being used for the field_name, maybe we can get rid of it
   u32 team_number;
   string field_name;
   struct {
      bool loaded;
      v2 size;
      u32 starting_position_count;
      Field_StartingPosition starting_positions[16];
      texture image;
   } field;

   struct {
      bool open;
      f32 width;
      f32 height;
      
      char name_buffer[10];
      TextBoxData name_box;

      char image_file_buffer[10];
      TextBoxData image_file_box;
   } new_field;
   
   struct {
      bool loaded;

   } auto_run;

   MemoryArena file_lists_arena;
   FileListLink *ncff_files;
   FileListLink *ncar_files;

   f32 curr_time;
   f32 last_recieve_time;
   f32 last_send_time;
};

#include "theme.cpp"
#include "file_io.cpp"

void initDashboard(DashboardState *state) {
   state->state_arena = VirtualAllocArena(Megabyte(24));
   state->settings_arena = VirtualAllocArena(Megabyte(1));
   state->file_lists_arena = VirtualAllocArena(Megabyte(10));
   state->page = DashboardPage_AutoRuns;

   state->new_field.name_box.text = state->new_field.name_buffer;
   state->new_field.name_box.size = ArraySize(state->new_field.name_buffer);
   
   state->new_field.image_file_box.text = state->new_field.image_file_buffer;
   state->new_field.image_file_box.size = ArraySize(state->new_field.image_file_buffer);

   //TODO: do everytime we get a directory change notification
   Reset(&state->file_lists_arena);
   state->ncff_files = ListFilesWithExtension("*.ncff", &state->file_lists_arena);
   state->ncar_files = ListFilesWithExtension("*.ncar", &state->file_lists_arena);

   ReadSettingsFile(state);
}

//TODO: move into a ui_utils file?
#define SettingsRow(...) _SettingsRow(GEN_UI_ID, __VA_ARGS__)
ui_numberbox _SettingsRow(ui_id id, element *page, char *label, u32 *num, char *suffix = "") {
   element *row = Panel(page, RowLayout, V2(Size(page->bounds).x, 20), V2(0, 0), V2(0, 5));
   Label(row, label, 20);
   ui_numberbox result = _TextBox(id, row, num, 20);
   Label(row, suffix, 20);
   return result;
}

ui_numberbox _SettingsRow(ui_id id, element *page, char *label, f32 *num, char *suffix = "") {
   element *row = Panel(page, RowLayout, V2(Size(page->bounds).x, 20), V2(0, 0), V2(0, 5));
   Label(row, label, 20);
   ui_numberbox result = _TextBox(id, row, num, 20);
   Label(row, suffix, 20);
   return result;
}

ui_textbox _SettingsRow(ui_id id, element *page, char *label, TextBoxData *text, char *suffix = "") {
   element *row = Panel(page, RowLayout, V2(Size(page->bounds).x, 20), V2(0, 0), V2(0, 5));
   Label(row, label, 20);
   ui_textbox result = _TextBox(id, row, text, 20);
   if(IsSelected(result.e))
      Outline(result.e, GREEN);
   Label(row, suffix, 20);
   return result;
}

struct ui_slide_animation {
   bool init;
   bool open;
   f32 min;
   f32 max;
   f32 value;
};

#define SlideAnimation(...) _SlideAnimation(GEN_UI_ID, __VA_ARGS__)
ui_slide_animation *_SlideAnimation(ui_id id, UIContext *context, f32 min, f32 max, f32 time) {
   ui_slide_animation *result = (ui_slide_animation *) _GetOrAllocate(id, context, sizeof(ui_slide_animation));
   
   if(!result->init) {
      result->init = true;
      result->min = min;
      result->value = min;
      result->max = max;
   } else {
      //TODO: this
      if(min != result->min){

      }

      if(max != result->max) {

      }
   }
   
   //TODO: time stuff, proper animations
   result->value = Clamp(min, max, result->value + (result->open ? 5 : -5));

   return result;
}

void DrawHome(element *page, DashboardState *state) {
   StackLayout(page);

   if(state->field.loaded) {
      element *base = Panel(page, ColumnLayout, Size(page->bounds));
      ui_field_topdown field = FieldTopdown(base, state->field.image, state->field.size,
                                            Size(page->bounds).x);

      v2 points[] = { V2(-10, -10), V2(0, 0), V2(10, 0), V2(10, 10) };
      DrawBezierCurve(&field, points, ArraySize(points));
      
      for(s32 i = 0; i < ArraySize(points); i++) {
         v2 p = GetPoint(&field, points[i]);
         Rectangle(field.e, RectCenterSize(p, V2(5, 5)), RED);
      }

      v2 robot_size_px = state->robot.loaded ? FeetToPixels(&field, state->robot.size) : V2(20, 20);
      for(u32 i = 0; i < state->field.starting_position_count; i++) {
         Field_StartingPosition *starting_pos = state->field.starting_positions + i;
         element *field_starting_pos = _Panel(POINTER_UI_ID(starting_pos), field.e, NULL, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px));
         ui_click starting_pos_click = DefaultClickInteraction(field_starting_pos);

         v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                                 -sinf(starting_pos->angle * (PI32 / 180)));
         Background(field_starting_pos, RED);
         Line(field_starting_pos, Center(field_starting_pos->bounds),
                                  Center(field_starting_pos->bounds) + 10 * direction_arrow,
                                  BLACK);           

         if(starting_pos_click.clicked) {

         }   
      }

      /*
      for(u32 i = 0; i < path_count; i++) {
         DrawPath(field_topdown, path);
      }
      */
   }

   ui_slide_animation *auto_selector_anim = SlideAnimation(page->context, 50, 300, 3);
   v2 selector_pos = V2(page->bounds.max.x - auto_selector_anim->value, page->bounds.min.y + 5);
   element *auto_selector = VerticalList(page, RectMinSize(selector_pos, V2(300, Size(page->bounds).y - 10)));
   auto_selector_anim->open = ContainsCursor(auto_selector);
   Background(auto_selector, V4(0.5, 0.5, 0.5, 0.5));

   Label(auto_selector, "Autos", 20);
   /*
   for(auto file) {
      if(IsHot()) {
         if(state->field.loaded) {
            DrawPaths(auto);
         }
      }
   }
   */
}

void DrawSubsystem(element *page, Subsystem *subsystem) {
   Label(page, subsystem->name, 50);
   MultiLineGraph(page, &subsystem->diagnostics_graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
}

f32 slider_test = 0;
void DrawAutoRuns(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = Panel(full_page, ColumnLayout,
                         RectMinMax(full_page->bounds.min + V2(50, 0), 
                                    full_page->bounds.max - V2(50, 0)));
   Outline(page, BLACK);
   /*
   if(state->selected_auto_run == NULL)
      state->selected_auto_run = state->latest_auto_run;
   */

   if(state->field.loaded) {
      ui_field_topdown field = FieldTopdown(page, state->field.image, state->field.size, 
                                            Size(page->bounds).x);

      //TODO: draw robot samples
      /*
      for(s32 i = 0; i < ArraySize(points); i++) {
         v2 p = GetPoint(&field, points[i]);
         Rectangle(field.e, RectCenterSize(p, V2(5, 5)), RED);
      }
      */
   } else {
      Label(page, "No field loaded", 20);
   }

   //TODO: automatically center this somehow, maybe make a CenterColumnLayout?
   HorizontalSlider(page, &slider_test, -10, 10, V2(Size(page->bounds).x - 40, 40), V2(20, 20));
   
   //TODO: draw robot at current slider time

   //draw graphs
   /*
   for()
      MultiLineGraph(page, &subsystem->diagnostics_graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
   */

   ui_slide_animation *run_selector_anim = SlideAnimation(page->context, 50, 300, 3);
   v2 selector_pos = V2(full_page->bounds.max.x - run_selector_anim->value, full_page->bounds.min.y + 5);
   element *run_selector = VerticalList(full_page, RectMinSize(selector_pos, V2(300, Size(full_page->bounds).y - 10)));
   Background(run_selector, V4(0.5, 0.5, 0.5, 0.5));
   run_selector_anim->open = ContainsCursor(run_selector);

   for(FileListLink *file = state->ncar_files; file; file = file->next) {
      if(_Button(POINTER_UI_ID(file), run_selector, menu_button, file->name).clicked) {
         
      }
   }
}

void DrawRobots(element *page, DashboardState *state) {

}

//TODO: do we want to save settings & field data every time a change is made or have a "save" button?
void DrawSettings(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page, RectMinMax(full_page->bounds.min + V2(60, 0), 
                                                      full_page->bounds.max - V2(60, 0)));
   
   Label(page, "Settings", 50);
   
   bool settings_changed = false;
   settings_changed |= SettingsRow(page, "Team Number: ", &state->team_number).valid_enter;
   
   if(settings_changed) {
      WriteSettingsFile(state);
   }

   if(state->field.loaded) {
      bool field_data_changed = false;

      Label(page, state->field_name, 20);
      Label(page, "Field Dimensions", 20, V2(0, 0), V2(0, 5));
      field_data_changed |= SettingsRow(page, "Field Width: ", &state->field.size.x, "ft").valid_enter;
      field_data_changed |= SettingsRow(page, "Field Height: ", &state->field.size.y, "ft").valid_enter;
      
      ui_field_topdown field = FieldTopdown(page, state->field.image, state->field.size, Size(page->bounds).x);

      v2 robot_size_px = state->robot.loaded ? FeetToPixels(&field, state->robot.size) : V2(20, 20);
      for(u32 i = 0; i < state->field.starting_position_count; i++) {
         Field_StartingPosition *starting_pos = state->field.starting_positions + i;

         //TODO: we had to type POINTER_UI_ID(starting_pos) a lot here, need a better way to do things
         element *field_starting_pos = _Panel(POINTER_UI_ID(starting_pos), field.e, NULL, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px));
         v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                                 -sinf(starting_pos->angle * (PI32 / 180)));
         Background(field_starting_pos, RED);
         Line(field_starting_pos, Center(field_starting_pos->bounds),
                                  Center(field_starting_pos->bounds) + 10 * direction_arrow,
                                  BLACK);
         ui_drag drag_pos = DefaultDragInteraction(field_starting_pos);

         starting_pos->pos = ClampTo(starting_pos->pos + PixelsToFeet(&field, drag_pos.drag), 
                                     RectCenterSize(V2(0, 0), field.size_in_ft));
         
         element *starting_pos_panel = _Panel(POINTER_UI_ID(starting_pos), page, RowLayout, V2(Size(page->bounds).x, 40), V2(0, 0), V2(0, 5));
         Background(starting_pos_panel, BLUE); 

         Label(starting_pos_panel, "X: ", 20);
         field_data_changed |= _TextBox(POINTER_UI_ID(starting_pos), starting_pos_panel, &starting_pos->pos.x, 20).valid_changed;
         Label(starting_pos_panel, "Y: ", 20);
         field_data_changed |= _TextBox(POINTER_UI_ID(starting_pos), starting_pos_panel, &starting_pos->pos.y, 20).valid_changed;
         Label(starting_pos_panel, "Angle: ", 20);
         field_data_changed |= _TextBox(POINTER_UI_ID(starting_pos), starting_pos_panel, &starting_pos->angle, 20).valid_changed;

         if(_Button(POINTER_UI_ID(starting_pos), starting_pos_panel, menu_button, "Delete").clicked) {
            for(u32 j = i; j < (state->field.starting_position_count - 1); j++) {
               state->field.starting_positions[j] = state->field.starting_positions[j + 1];
            }
            state->field.starting_position_count--;
         }

         if(IsHot(field_starting_pos) || ContainsCursor(starting_pos_panel)) {
            Outline(field_starting_pos, BLACK);
            Outline(starting_pos_panel, BLACK);
         }

         field_data_changed |= (drag_pos.drag.x != 0) || (drag_pos.drag.y != 0);
      }

      if((state->field.starting_position_count + 1) < ArraySize(state->field.starting_positions)) {
         element *add_starting_pos = Panel(page, RowLayout, V2(Size(page->bounds).x, 40), V2(0, 0), V2(0, 5));
         Background(add_starting_pos, RED);
         if(DefaultClickInteraction(add_starting_pos).clicked) {
            state->field.starting_position_count++;
         }
      }

      if(field_data_changed) {
         WriteFieldFile(state);
      }
   } else {
      Label(page, Concat(state->field_name, Literal(".ncff not found")), 20);
   }

   ui_slide_animation *field_selector_anim = SlideAnimation(full_page->context, 50 /*min*/, 300 /*max*/, 3 /*sec*/);
   v2 selector_pos = V2(full_page->bounds.max.x - field_selector_anim->value, full_page->bounds.min.y + 5);
   element *field_selector = VerticalList(full_page, RectMinSize(selector_pos, V2(300, Size(full_page->bounds).y - 10)));
   Background(field_selector, V4(0.5, 0.5, 0.5, 0.5));
   field_selector_anim->open = ContainsCursor(field_selector);

   Label(field_selector, "Fields", 50); //TODO: center this
   
   for(FileListLink *file = state->ncff_files; file; file = file->next) {
      if(_Button(POINTER_UI_ID(file), field_selector, menu_button, file->name).clicked) {
         //TODO: this is safe because the ReadSettingsFile allocates field_name in settings_arena
         //       its pretty ugly tho so clean up
         state->field_name = file->name;
         WriteSettingsFile(state);
         ReadSettingsFile(state);
      }
   }

   if(state->new_field.open) {
      element *create_game_window = Panel(field_selector, ColumnLayout, V2(Size(field_selector->bounds).x - 20, 300), V2(10, 10));
      Background(create_game_window, menu_button.colour);

      Label(create_game_window, "Create New Field File", 20);
      ui_textbox name_box = SettingsRow(create_game_window, "Name: ", &state->new_field.name_box);
      ui_numberbox width_box = SettingsRow(create_game_window, "Width: ", &state->new_field.width, "ft");
      ui_numberbox height_box = SettingsRow(create_game_window, "Height: ", &state->new_field.height, "ft");
      ui_textbox image_file_box = SettingsRow(create_game_window, "Image: ", &state->new_field.image_file_box);

      bool valid = (GetText(state->new_field.name_box).length > 0) &&
                   (GetText(state->new_field.image_file_box).length > 0) &&
                   width_box.valid && (state->new_field.width > 0) &&
                   height_box.valid && (state->new_field.height > 0);
      
      if(Button(create_game_window, menu_button, "Create", valid).clicked) {
         WriteNewFieldFile(state);
         state->new_field.open = false;
      }

      if(Button(create_game_window, menu_button, "Exit").clicked) {
         state->new_field.width = 0;
         state->new_field.height = 0;
         state->new_field.name_box.used = 0;
         state->new_field.image_file_box.used = 0;
         state->new_field.open = false;
      }
   } else {
      if(Button(field_selector, menu_button, "Create Field").clicked) {
         state->new_field.open = true;
      }
   }
}

void DrawUI(element *root, DashboardState *state) {
   Texture(root, logoTexture, RectCenterSize(Center(root->bounds), logoTexture.size));
   ColumnLayout(root);
   
   element *top_bar = Panel(root, RowLayout, V2(Size(root->bounds).x, 20));
   DefaultClickInteraction(top_bar);
   HoverTooltip(top_bar, state->connected ? "Connected" : "Not Connected");
   
   Background(top_bar, V4(0.5, 0.5, 0.5, 1));
   if(state->connected) {
      Label(top_bar, state->robot.name, 20, V2(10, 0));
      Label(top_bar, Concat(Literal("Mode: "), ToString(state->mode)), 20, V2(10, 0));
   } else {
      Label(top_bar, "No Connection", 20, V2(5, 0));
   }
   Label(top_bar, Concat(Literal("Time: "), ToString((f32) root->context->curr_time)), 20, V2(10, 0));
   Label(top_bar, Concat(Literal("FPS: "), ToString((f32) root->context->fps)), 20, V2(10, 0));
   
   element *content = Panel(root, StackLayout, V2(Size(root->bounds).x, Size(root->bounds).y - 20));
   
   element *page = Panel(content, ColumnLayout, Size(content->bounds));
   switch(state->page) {
      case DashboardPage_Home: DrawHome(page, state); break;
      case DashboardPage_Subsystem: DrawSubsystem(page, state->selected_subsystem); break;
      case DashboardPage_AutoRuns: DrawAutoRuns(page, state); break;
      case DashboardPage_Robots: DrawRobots(page, state); break;
      case DashboardPage_Settings: DrawSettings(page, state); break;
   }

   ui_slide_animation *menu_anim = SlideAnimation(root->context, -250, 0, 3);
   rect2 menu_bounds = RectMinSize(V2(menu_anim->value, 5) + content->bounds.min, V2(300, Size(content->bounds).y - 10));
   element *menu_bar = VerticalList(content, menu_bounds);
   DefaultClickInteraction(menu_bar);
   Background(menu_bar, V4(0.5, 0.5, 0.5, 0.5));
   menu_anim->open = ContainsCursor(menu_bar);
   
   if(Button(menu_bar, menu_button, "Home").clicked) {
      state->page = DashboardPage_Home;
   }

   for(u32 i = 0; i < state->robot.subsystem_count; i++) {
      Subsystem *subsystem = state->robot.subsystems + i; 
      button_style style = menu_button;
      style.colour = V4(0.55, 0, 0, 0.5);
      if(_Button(POINTER_UI_ID(i), menu_bar, style, subsystem->name).clicked) {
         state->page = DashboardPage_Subsystem;
         state->selected_subsystem = subsystem;
      }
   }

   if(Button(menu_bar, menu_button, "Auto Runs").clicked) {
      state->page = DashboardPage_AutoRuns;
   }

   if(Button(menu_bar, menu_button, "Robots").clicked) {
      state->page = DashboardPage_Robots;
   }

   if(Button(menu_bar, menu_button, "Settings").clicked) {
      state->page = DashboardPage_Settings;
   }

   if((state->mode == GameMode_Autonomous) && 
      (state->prev_mode != GameMode_Autonomous)) {
      state->recording_auto_diagnostics = true;
   }

   if((state->mode != GameMode_Autonomous) && 
      (state->prev_mode == GameMode_Autonomous)) {
      state->recording_auto_diagnostics = false;
   }

   if(state->recording_auto_diagnostics) {
      //TODO: log auto run diagnostics if running auto
   }

   state->mode = state->prev_mode;
}