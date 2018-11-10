#include "north/north_common_definitions.h"
#include "north/north_file_definitions.h"
#include "north/north_network_definitions.h"

string ToString(North_GameMode::type mode) {
   switch(mode) {
      case North_GameMode::Disabled:
         return Literal("Disabled");
      case North_GameMode::Autonomous:
         return Literal("Autonomous");
      case North_GameMode::Teleop:
         return Literal("Teleop");
      case North_GameMode::Test:
         return Literal("Test");
   }
   return EMPTY_STRING;
}

struct ConnectedSubsystem;
struct ConnectedParameter {
   ConnectedSubsystem *subsystem;

   string name;
   bool is_array;
   u32 length; //ignored if is_array is false
   union {
      f32 value; //is_array = false
      f32 *values; //is_array = true
   };
};

//TODO: something like this that sends the data over the network
void SetParamValue(ConnectedParameter *param, f32 value, u32 index) {
   buffer packet = PushTempBuffer(Megabyte(1));
/*
   ParameterOp_PacketHeader packet = {};
   packet.packet_type = PacketType::SetParameter;
   packet.subsystem_name_length = param->subsystem->name.length;
   packet.param_name_length = param->name.length;
   param->value =
   */ 
}

struct ConnectedSubsystem {
   string name;
   MultiLineGraphData diagnostics_graph;
   
   u32 param_count;
   ConnectedParameter *params;
};

enum DashboardPage {
   DashboardPage_Home,
   DashboardPage_Subsystem,
   DashboardPage_Recordings,
   DashboardPage_Robots,
   DashboardPage_Settings
};

void SetDiagnosticsGraphUnits(MultiLineGraphData *data) {
   SetUnit(data, 1 /*Feet*/, Literal("ft"));
   SetUnit(data, 2 /*FeetPerSecond*/, Literal("ft/s"));
   SetUnit(data, 3 /*Degrees*/, Literal("deg"));
   SetUnit(data, 4 /*DegreesPerSecond*/, Literal("deg/s"));
   SetUnit(data, 5 /*Seconds*/, Literal("s"));
   SetUnit(data, 6 /*Percent*/, Literal("%"));
   SetUnit(data, 7 /*Amp*/, Literal("amp"));
   SetUnit(data, 8 /*Volt*/, Literal("volt"));
}

#include "auto_project_utils.cpp"
#include "robot_recording.cpp"
#include "robot_profile_helper.cpp"

/**
TODO:
-split this up into connected_robot, dashboard & dashboard_ui
-get rid of file_io
**/

struct DashboardState {
   //TODO: change to connected_robot_arena & connected_robot
   MemoryArena state_arena;
   struct {
      bool connected;
      string name;
      v2 size;

      ConnectedSubsystem *subsystems;
      u32 subsystem_count;
   } robot;

   RobotProfileHelper robot_profile_helper;

   MemoryArena settings_arena; //TODO: this is just being used for the field_name, maybe we can get rid of it
   u32 team_number;
   string field_name;
   struct {
      bool loaded;
      v2 size;
      u32 flags;
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
   
   LoadedRobotRecording recording;
   RobotRecorder auto_recorder;
   RobotRecorder manual_recorder;

   struct {
      bool starting_pos_selected;
      v2 starting_pos;
   } home_field;

   MemoryArena auto_programs_arena;
   AutoProjectLink *first_auto_project;

   //--------------------------------------------------------
   DashboardPage page;
   ConnectedSubsystem *selected_subsystem; //NOTE: this doesnt survive a reconnect!!
   AutoProjectLink *selected_auto_project; //NOTE: this doesnt survive a directory change!!!
   AutoNode *selected_auto_node; //NOTE: this doesnt survive a directory change!!!
   //selected_robot_profile
   
   North_GameMode::type prev_mode;
   North_GameMode::type mode;
   //--------------------------------------------------------

   bool directory_changed;
   MemoryArena file_lists_arena;
   FileListLink *ncff_files; //Field 
   FileListLink *ncrr_files; //Recording
   FileListLink *ncrp_files; //Robot Profile
   
   f32 curr_time;
   f32 last_recieve_time;
   f32 last_send_time;
};

#include "theme.cpp"
#include "file_io.cpp"

void reloadFiles(DashboardState *state) {
   Reset(&state->file_lists_arena);
   state->ncff_files = ListFilesWithExtension("*.ncff", &state->file_lists_arena);
   state->ncrr_files = ListFilesWithExtension("*.ncrr", &state->file_lists_arena);
   state->ncrp_files = ListFilesWithExtension("*.ncrp", &state->file_lists_arena);
   
   Reset(&state->auto_programs_arena);
   state->first_auto_project = NULL;

   state->selected_auto_project = NULL;
   state->selected_auto_node = NULL;

   for(FileListLink *file = ListFilesWithExtension("*.ncap"); file; file = file->next) {
      AutoProjectLink *auto_proj = ReadAutoProject(file->name, &state->auto_programs_arena);
      if(auto_proj) {
         auto_proj->next = state->first_auto_project;
         state->first_auto_project = auto_proj;
      }
   }

   ReadSettingsFile(state);
}

void initDashboard(DashboardState *state) {
   state->state_arena = PlatformAllocArena(Megabyte(24));
   state->settings_arena = PlatformAllocArena(Megabyte(1));
   state->recording.arena = PlatformAllocArena(Megabyte(20));
   state->auto_programs_arena = PlatformAllocArena(Megabyte(20));
   state->file_lists_arena = PlatformAllocArena(Megabyte(10));
   state->page = DashboardPage_Home;

   InitRobotProfileHelper(&state->robot_profile_helper, Megabyte(10));

   state->new_field.name_box.text = state->new_field.name_buffer;
   state->new_field.name_box.size = ArraySize(state->new_field.name_buffer);
   
   state->new_field.image_file_box.text = state->new_field.image_file_buffer;
   state->new_field.image_file_box.size = ArraySize(state->new_field.image_file_buffer);

   //TODO: do everytime we get a directory change notification
   reloadFiles(state);
}

void DrawAutoPath(DashboardState *state, ui_field_topdown *field, AutoPath *path, bool preview);
void DrawAutoNode(DashboardState *state, ui_field_topdown *field, AutoNode *node, bool preview) {
   v2 p = GetPoint(field, node->pos);
   
   if((node->path_count > 1) && !preview) {
      UI_SCOPE(field->e->context, node);
      element *node_selector = Panel(field->e, NULL, RectCenterSize(p, V2(10, 10)));
      Background(node_selector, RED);
      if(DefaultClickInteraction(node_selector).clicked) {
         state->selected_auto_node = node;
      }
   } else {
      Rectangle(field->e, RectCenterSize(p, V2(5, 5)), RED);
   }

   for(u32 i = 0; i < node->path_count; i++) {
      DrawAutoPath(state, field, node->out_paths + i, preview);
   }
}

void DrawAutoPath(DashboardState *state, ui_field_topdown *field, AutoPath *path, bool preview) {
   if(path->control_point_count == 0) {
      Line(field->e, GetPoint(field, path->in_node->pos), GetPoint(field, path->out_node->pos), BLACK);
   } else {
      u32 point_count = path->control_point_count + 2;
      v2 *all_points = PushTempArray(v2, point_count);
      
      all_points[0] = path->in_node->pos;
      all_points[point_count - 1] = path->out_node->pos;
      for(u32 i = 0; i < path->control_point_count; i++)
         all_points[i + 1] = path->control_points[i];

      DrawBezierCurve(field, all_points, point_count);
   }

   DrawAutoNode(state, field, path->out_node, preview);
}

/**
TODO:

if(toggle_recording_button) {
   if(manual_recording->recording) {
      EndRecording("name")
   } else {
      BeginRecording()
   }
}
**/

void DrawHome(element *page, DashboardState *state) {
   StackLayout(page);

   if(state->field.loaded) {
      element *base = Panel(page, ColumnLayout, Size(page->bounds));
      ui_field_topdown field = FieldTopdown(base, state->field.image, state->field.size,
                                            Size(page->bounds).x);

      v2 robot_size_px = state->robot.connected ? FeetToPixels(&field, state->robot.size) : V2(20, 20);
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
            state->home_field.starting_pos_selected = true;
            state->home_field.starting_pos = starting_pos->pos;
            //TODO: send starting pos to robot
         }   
      }

      if(state->robot.connected) {
         //TODO: draw live field tracking samples
         //TODO: draw current auto path
      }

      if(state->home_field.starting_pos_selected) {
         element *auto_selector = VerticalList(SlidingSidePanel(page, 300, 5, 50, true));
         Background(auto_selector, V4(0.5, 0.5, 0.5, 0.5));

         bool drawing_auto_preview = false;
         Label(auto_selector, "Autos", 20);
         for(AutoProjectLink *auto_proj = state->first_auto_project; auto_proj; auto_proj = auto_proj->next) {
            UI_SCOPE(auto_selector->context, auto_proj);

            ui_button btn = Button(auto_selector, menu_button, auto_proj->name);

            if(btn.clicked) {
               state->selected_auto_project = auto_proj;
            }

            if(IsHot(btn.e)) {
               DrawAutoNode(state, &field, auto_proj->starting_node, true);
               drawing_auto_preview = true;
            }
         }

         if(state->selected_auto_node) {
            //TODO: reorder path priorities
         }

         if(state->selected_auto_project) {
            DrawAutoNode(state, &field, state->selected_auto_project->starting_node, false);
            if(Button(base, menu_button, "Upload Auto").clicked) {
               //TODO: upload auto
            }
         }
      }

   }
}

void DrawSubsystem(element *page, ConnectedSubsystem *subsystem) {
   Label(page, subsystem->name, 50);
   MultiLineGraph(page, &subsystem->diagnostics_graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
}

void DrawRecordings(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page, RectMinMax(full_page->bounds.min, 
                                                      full_page->bounds.max - V2(60, 0)));
   Outline(page, BLACK);

   if(state->recording.loaded) {
      if(state->field.loaded) {
         ui_field_topdown field = FieldTopdown(page, state->field.image, state->field.size, 
                                             Size(page->bounds).x);

         for(s32 i = 0; i < state->recording.robot_sample_count; i++) {
            RobotRecording_RobotStateSample *sample = state->recording.robot_samples + i;
            v2 p = GetPoint(&field, sample->pos);
            Rectangle(field.e, RectCenterSize(p, V2(5, 5)), RED);
         }
      } else {
         Label(page, "No field loaded", 20);
      }

      //TODO: automatically center this somehow, maybe make a CenterColumnLayout?
      HorizontalSlider(page, &state->recording.curr_time, state->recording.min_time, state->recording.max_time,
                       V2(Size(page->bounds).x - 40, 40), V2(20, 20));
      
      //TODO: highlight robot at current slider time
      //TODO: draw vertical bar in multiline graph at t=curr_time

      for(u32 i = 0; i < state->recording.subsystem_count; i++) {
         SubsystemRecording *subsystem = state->recording.subsystems + i;
         UI_SCOPE(page->context, subsystem);
         Label(page, subsystem->name, 20);
         MultiLineGraph(page, &subsystem->graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
      }

   } else {
      Label(page, "No run selected", 20);
   }
   
   element *recording_selector = VerticalList(SlidingSidePanel(full_page, 300, 5, 30, true));
   Background(recording_selector, V4(0.5, 0.5, 0.5, 0.5));
   
   for(FileListLink *file = state->ncrr_files; file; file = file->next) {
      if(_Button(POINTER_UI_ID(file), recording_selector, menu_button, file->name).clicked) {
         LoadRecording(&state->recording, file->name);
      }
   }
}

void DrawRobots(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page, RectMinMax(full_page->bounds.min, 
                                                      full_page->bounds.max - V2(60, 0)));

   
   if(state->robot_profile_helper.selected_profile) {
      RobotProfile *profile = state->robot_profile_helper.selected_profile;
      Label(page, profile->name, 20);
      
      for(u32 i = 0; i < profile->subsystem_count; i++) {
         RobotProfileSubsystem *subsystem = profile->subsystems + i;
         element *subsystem_page = Panel(page, ColumnLayout, V2(Size(page).x - 60, 400), V2(20, 0));
         Background(subsystem_page, V4(0.5, 0.5, 0.5, 0.5));

         Label(subsystem_page, subsystem->name, 20);
         
         Label(subsystem_page, "Parameters", 20, V2(0, 20));
         for(u32 j = 0; j < subsystem->param_count; j++) {
            RobotProfileParameter *param = subsystem->params + j;
            if(param->is_array) {
               Label(subsystem_page, Concat(param->name, Literal(": ")), 18, V2(20, 0));
               for(u32 k = 0; k < param->length; k++) {
                  Label(subsystem_page, ToString(param->values[k]), 18, V2(40, 0));
               }
            } else {
               Label(subsystem_page, Concat(param->name, Literal(": "), ToString(param->value)), 18, V2(20, 0));
            }
         }

         Label(subsystem_page, "Commands", 20, V2(0, 20));
         for(u32 j = 0; j < subsystem->command_count; j++) {
            RobotProfileCommand *command = subsystem->commands + j;
            //TODO: icon for command->type
            string params = (command->param_count > 0) ? command->params[0] : EMPTY_STRING;
            for(u32 k = 1; k < command->param_count; k++) {
               params = Concat(params, Literal(", "), command->params[k]);
            }
            Label(subsystem_page, Concat(command->name, Literal("("), params, Literal(")")), 18, V2(20, 0));  
         }
      }
   }

   element *run_selector = VerticalList(SlidingSidePanel(full_page, 300, 5, 30, true));
   Background(run_selector, V4(0.5, 0.5, 0.5, 0.5));

   if(state->robot.connected) {
      if(Button(run_selector, menu_button, "Connected Robot").clicked) {

      }
   }

   for(FileListLink *file = state->ncrp_files; file; file = file->next) {
      if(_Button(POINTER_UI_ID(file), run_selector, menu_button, file->name).clicked) {
         buffer loaded_file = ReadEntireFile(Concat(file->name, Literal(".ncrp")));
         if(loaded_file.data != NULL) {
            ParseProfileFile(&state->robot_profile_helper, loaded_file);
         }
         FreeEntireFile(&loaded_file);
      }
   }
}

//TODO: do we want to save settings & field data every time a change is made or have a "save" button?
void DrawSettings(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page, RectMinMax(full_page->bounds.min, 
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
      
      field_data_changed |= SettingsRow(page, "Field Mirrored (eg. Steamworks)", &state->field.flags, Field_Flags::MIRRORED, V2(20, 20)).clicked;
      field_data_changed |= SettingsRow(page, "Field Symmetric (eg. Power Up)", &state->field.flags, Field_Flags::SYMMETRIC, V2(20, 20)).clicked;

      ui_field_topdown field = FieldTopdown(page, state->field.image, state->field.size, Size(page->bounds).x);

      v2 robot_size_px = state->robot.connected ? FeetToPixels(&field, state->robot.size) : V2(20, 20);
      for(u32 i = 0; i < state->field.starting_position_count; i++) {
         Field_StartingPosition *starting_pos = state->field.starting_positions + i;
         UI_SCOPE(page->context, starting_pos);

         element *field_starting_pos = Panel(field.e, NULL, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px));
         v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                                 -sinf(starting_pos->angle * (PI32 / 180)));
         Background(field_starting_pos, RED);
         Line(field_starting_pos, Center(field_starting_pos->bounds),
                                  Center(field_starting_pos->bounds) + 10 * direction_arrow,
                                  BLACK);
         ui_drag drag_pos = DefaultDragInteraction(field_starting_pos);

         starting_pos->pos = ClampTo(starting_pos->pos + PixelsToFeet(&field, drag_pos.drag), 
                                     RectCenterSize(V2(0, 0), field.size_in_ft));
         
         element *starting_pos_panel = Panel(page, RowLayout, V2(Size(page->bounds).x, 40), V2(0, 0), V2(0, 5));
         Background(starting_pos_panel, BLUE); 

         Label(starting_pos_panel, "X: ", 20);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->pos.x, 20).valid_changed;
         Label(starting_pos_panel, "Y: ", 20);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->pos.y, 20).valid_changed;
         Label(starting_pos_panel, "Angle: ", 20);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->angle, 20).valid_changed;

         if(Button(starting_pos_panel, menu_button, "Delete").clicked) {
            for(u32 j = i; j < (state->field.starting_position_count - 1); j++) {
               state->field.starting_positions[j] = state->field.starting_positions[j + 1];
            }
            state->field.starting_position_count--;
            field_data_changed = true;
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
            state->field.starting_positions[state->field.starting_position_count - 1] = {};
            field_data_changed = true;
         }
      }

      if(field_data_changed) {
         WriteFieldFile(state);
      }
   } else {
      Label(page, Concat(state->field_name, Literal(".ncff not found")), 20);
   }

   element *field_selector = VerticalList(SlidingSidePanel(full_page, 300, 5, 50, true));
   Background(field_selector, V4(0.5, 0.5, 0.5, 0.5));
   
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
   HoverTooltip(top_bar, state->robot.connected ? "Connected" : "Not Connected");
   
   Background(top_bar, V4(0.5, 0.5, 0.5, 1));
   if(state->robot.connected) {
      Label(top_bar, state->robot.name, 20, V2(10, 0));
      Label(top_bar, Concat(Literal("Mode: "), ToString(state->mode)), 20, V2(10, 0));
   } else {
      Label(top_bar, "No Connection", 20, V2(5, 0));
   }
   Label(top_bar, Concat(Literal("Time: "), ToString((f32) root->context->curr_time)), 20, V2(10, 0));
   Label(top_bar, Concat(Literal("FPS: "), ToString((f32) root->context->fps)), 20, V2(10, 0));
   
   element *content = Panel(root, StackLayout, V2(Size(root->bounds).x, Size(root->bounds).y - 20));
   
   element *page = Panel(content, ColumnLayout, 
                         RectMinMax(content->bounds.min + V2(60, 0), content->bounds.max));
   switch(state->page) {
      case DashboardPage_Home: DrawHome(page, state); break;
      case DashboardPage_Subsystem: DrawSubsystem(page, state->selected_subsystem); break;
      case DashboardPage_Recordings: DrawRecordings(page, state); break;
      case DashboardPage_Robots: DrawRobots(page, state); break;
      case DashboardPage_Settings: DrawSettings(page, state); break;
   }

   element *menu_bar = VerticalList(SlidingSidePanel(content, 300, 5, 50, false));
   Background(menu_bar, V4(0.5, 0.5, 0.5, 0.5));
   
   if(Button(menu_bar, menu_button, "Home").clicked) {
      state->page = DashboardPage_Home;
   }

   if(state->robot.connected) {
      element *divider1 = Panel(menu_bar, NULL, V2(Size(menu_bar->bounds).x - 40, 5), V2(10, 0));
      Background(divider1, BLACK);

      for(u32 i = 0; i < state->robot.subsystem_count; i++) {
         ConnectedSubsystem *subsystem = state->robot.subsystems + i; 
         button_style style = menu_button;
         style.colour = V4(0.55, 0, 0, 0.5);
         if(_Button(POINTER_UI_ID(i), menu_bar, style, subsystem->name).clicked) {
            state->page = DashboardPage_Subsystem;
            state->selected_subsystem = subsystem;
         }
      }

      element *divider2 = Panel(menu_bar, NULL, V2(Size(menu_bar->bounds).x - 40, 5), V2(10, 0));
      Background(divider2, BLACK);
   }

   if(Button(menu_bar, menu_button, "Recordings").clicked) {
      state->page = DashboardPage_Recordings;
   }

   if(Button(menu_bar, menu_button, "Robots").clicked) {
      state->page = DashboardPage_Robots;
   }

   if(Button(menu_bar, menu_button, "Settings").clicked) {
      state->page = DashboardPage_Settings;
   }

   //TODO: end recordings when we disconnect
   //TODO: move recordings to their own thread

   if((state->mode == North_GameMode::Autonomous) && 
      (state->prev_mode != North_GameMode::Autonomous)) {
      BeginRecording(&state->auto_recorder);
   }

   if((state->mode != North_GameMode::Autonomous) && 
      (state->prev_mode == North_GameMode::Autonomous)) {
      EndRecording(&state->auto_recorder, Literal("auto_recording"));
   }

   state->mode = state->prev_mode;
}