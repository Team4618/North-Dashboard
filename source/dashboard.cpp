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

#include "theme.cpp"

#include "auto_project_utils.cpp"
#define INCLUDE_DRAWSETTINGS
#include "north_settings_utils.cpp"
#include "robot_recording_utils.cpp"
#define INCLUDE_DRAWPROFILES
#include "robot_profile_utils.cpp"

struct ConnectedParameter;
struct ConnectedParameterValue {
   ConnectedParameter *parent;
   ConnectedParameterValue *next;
   ConnectedParameterValue *prev;
   
   u32 index;
   f32 value;
};

struct ConnectedSubsystem;
struct ConnectedParameter {
   ConnectedSubsystem *subsystem;
   string name;
   bool is_array;

   ConnectedParameterValue sentinel;
};

struct ConnectedSubsystem {
   //TODO: we're being super infefficient with memory right now, fix eventually
   MemoryArena param_arena;

   string name;
   MultiLineGraphData diagnostics_graph;
   
   u32 param_count;
   ConnectedParameter *params;
};

void SetParamValue(ConnectedParameterValue *param, f32 value) {
   buffer packet = PushTempBuffer(Megabyte(1));

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::SetValue;
   header.subsystem_name_length = param->parent->subsystem->name.length;
   header.param_name_length = param->parent->name.length;

   header.index = param->index;
   header.value = value;
   param->value = value; 
}

void RecalculateIndicies(ConnectedParameter *param) {
   u32 i = 0;
   for(ConnectedParameterValue *curr = param->sentinel.next;
       curr != &param->sentinel; curr = curr->next)
   {
      curr->index = i++;
   }
}

void AddParamValue(ConnectedParameter *param) {
   Assert(param->is_array);
   MemoryArena *arena = &param->subsystem->param_arena;
   ConnectedParameterValue *sentinel = &param->sentinel;

   ConnectedParameterValue *new_value = PushStruct(arena, ConnectedParameterValue);
   new_value->value = 0;
   new_value->parent = param;
   new_value->next = sentinel;
   new_value->prev = sentinel->prev;

   new_value->next->prev = new_value;
   new_value->prev->next = new_value; 
   RecalculateIndicies(param);

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::AddValue;
   header.subsystem_name_length = param->subsystem->name.length;
   header.param_name_length = param->name.length;
   header.index = new_value->index;
   header.value = 0;
}

void RemoveParamValue(ConnectedParameterValue *param) {
   Assert(param->parent->is_array);
   ConnectedParameter *parent = param->parent;
   
   param->next->prev = param->prev;
   param->prev->next = param->next;
   RecalculateIndicies(param->parent);

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::RemoveValue;
   header.subsystem_name_length = parent->subsystem->name.length;
   header.param_name_length = parent->name.length;
   header.index = param->index;
   header.value = 0;
}

struct DashboardState {
   struct {
      MemoryArena arena;
      ConnectedSubsystem *subsystems;
      u32 subsystem_count;
   } connected;

   RobotRecording_RobotStateSample pos_samples[1024];
   u32 curr_pos_sample;
   u32 pos_sample_count;

   //TODO: current auto path

   NorthSettings settings;
   RobotProfiles profiles;
   
   LoadedRobotRecording recording;
   RobotRecorder auto_recorder;
   RobotRecorder manual_recorder;

   AutoProjectList auto_programs;

   //--------------------------------------------------------
   DashboardPage page;
   ConnectedSubsystem *selected_subsystem; //NOTE: this doesnt survive a reconnect!!
   //AutoProjectLink *selected_auto_project; //NOTE: this doesnt survive a directory change!!!
   //AutoNode *selected_auto_node; //NOTE: this doesnt survive a directory change!!!
   
   North_GameMode::type prev_mode;
   North_GameMode::type mode;
   //--------------------------------------------------------

   bool directory_changed;
   MemoryArena file_lists_arena;
   FileListLink *ncff_files; //Field 
   FileListLink *ncrr_files; //Recording
   FileListLink *ncrp_files; //Robot Profile
   FileWatcher file_watcher;
   
   f32 curr_time;
};

void reloadFiles(DashboardState *state) {
   Reset(&state->file_lists_arena);
   state->ncff_files = ListFilesWithExtension("*.ncff", &state->file_lists_arena);
   state->ncrr_files = ListFilesWithExtension("*.ncrr", &state->file_lists_arena);
   state->ncrp_files = ListFilesWithExtension("*.ncrp", &state->file_lists_arena);

   ReadSettingsFile(&state->settings);
}

void initDashboard(DashboardState *state) {
   state->connected.arena = PlatformAllocArena(Megabyte(24));
   state->settings.arena = PlatformAllocArena(Megabyte(1));
   state->recording.arena = PlatformAllocArena(Megabyte(20));
   state->auto_programs.arena = PlatformAllocArena(Megabyte(20));
   state->file_lists_arena = PlatformAllocArena(Megabyte(10));
   state->auto_recorder.arena = PlatformAllocArena(Megabyte(30));
   state->manual_recorder.arena = PlatformAllocArena(Megabyte(30));
   state->profiles.current.arena = PlatformAllocArena(Megabyte(10));
   state->profiles.loaded.arena = PlatformAllocArena(Megabyte(10));
   state->page = DashboardPage_Home;
   
   InitFileWatcher(&state->file_watcher, PlatformAllocArena(Kilobyte(512)), "*.*");
}

void DrawAutoPath(DashboardState *state, ui_field_topdown *field, AutoPath *path, bool preview);
void DrawAutoNode(DashboardState *state, ui_field_topdown *field, AutoNode *node, bool preview) {
   v2 p = GetPoint(field, node->pos);
   
   if((node->path_count > 1) && !preview) {
      UI_SCOPE(field->e->context, node);
      element *node_selector = Panel(field->e, RectCenterSize(p, V2(10, 10)), Captures(INTERACTION_CLICK));
      Background(node_selector, RED);
      if(WasClicked(node_selector)) {
         //state->selected_auto_node = node;
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
      Line(field->e, BLACK, 2,
           GetPoint(field, path->in_node->pos), 
           GetPoint(field, path->out_node->pos));
   } else {
      //TODO: Not a fan of having to put this in a temp array
      u32 point_count = path->control_point_count + 2;
      AutonomousProgram_ControlPoint *all_points = PushTempArray(AutonomousProgram_ControlPoint, point_count);
      
      all_points[0] = { path->in_node->pos, path->in_tangent };
      all_points[point_count - 1] = { path->out_node->pos, path->out_tangent };
      for(u32 i = 0; i < path->control_point_count; i++)
         all_points[i + 1] = path->control_points[i];

      for(u32 i = 0; i < point_count - 1; i++) {
         AutonomousProgram_ControlPoint a = all_points[i];
         AutonomousProgram_ControlPoint b = all_points[i + 1];
         
         CubicHermiteSpline(field, a.pos, a.tangent, b.pos, b.tangent, BLACK);
      }
   }

   DrawAutoNode(state, field, path->out_node, preview);
}

void DrawHome(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   static bool starting_pos_selected = false;
   static Field_StartingPosition selected_starting_pos = {};
   // static AutoProjectLink *selected_auto_project = NULL;

   if(state->settings.field.loaded && IsValid(&state->profiles.current)) {
      RobotProfile *profile = &state->profiles.current;
      
      ui_field_topdown field = FieldTopdown(page, state->settings.field.image, state->settings.field.size,
                                            Size(page->bounds).x);

      v2 robot_size_px =  FeetToPixels(&field, profile->size);
      for(u32 i = 0; i < state->settings.field.starting_position_count; i++) {
         Field_StartingPosition *starting_pos = state->settings.field.starting_positions + i;
         UI_SCOPE(page->context, starting_pos);
         
         element *field_starting_pos = Panel(field.e, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px),
                                             Captures(INTERACTION_CLICK));
         
         bool is_selected = starting_pos_selected && (Length(selected_starting_pos.pos - starting_pos->pos) < 0.01);    
         v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                                 -sinf(starting_pos->angle * (PI32 / 180)));
         Background(field_starting_pos, is_selected ? GREEN : RED);
         Line(field_starting_pos, BLACK, 2,
              Center(field_starting_pos->bounds),
              Center(field_starting_pos->bounds) + 10 * direction_arrow);           

         if(WasClicked(field_starting_pos)) {
            starting_pos_selected = true;
            selected_starting_pos = *starting_pos;

            ReadProjectsStartingAt(&state->auto_programs, 0, starting_pos->pos);

            //TODO: send starting pos to robot
         }   
      }

      if(state->pos_sample_count > 0) {
         //TODO: draw live field tracking samples
      }

      //TODO: draw current auto path
      
      if(starting_pos_selected) {
         bool drawing_auto_preview = false;
         Label(page, "Autos", 20, off_white);
         //TODO: dont load on file change & show all autos
         //      load when a position is selected & check if it starts there 
         for(AutoProjectLink *auto_proj = state->auto_programs.first; auto_proj; auto_proj = auto_proj->next) {
            UI_SCOPE(page->context, auto_proj);

            ui_button btn = Button(page, auto_proj->name, menu_button);

            if(btn.clicked) {
               //state->selected_auto_project = auto_proj;
            }

            if(IsHot(btn.e)) {
               DrawAutoNode(state, &field, auto_proj->starting_node, true);
               drawing_auto_preview = true;
            }
         }

         if(false /*state->selected_auto_node*/) {
            //TODO: reorder path priorities
         }

         if(false /*state->selected_auto_node*/) {
            //DrawAutoNode(state, &field, state->selected_auto_project->starting_node, false);
            if(Button(page, "Upload Auto", menu_button).clicked) {
               //TODO: upload auto
            }
         }
      }

   } else {
      if(!state->settings.field.loaded)
         Label(page, "No Field", V2(Size(page).x, 80), 50, BLACK);
      
      if(!IsValid(&state->profiles.current))
         Label(page, "No Robot", V2(Size(page).x, 80), 50, BLACK);
   }

   if(state->mode != North_GameMode::Disabled)
   {
      Label(page, "Robot Messages", V2(Size(page).x, 80), 50, BLACK);
   }
}

void DrawSubsystem(element *page, ConnectedSubsystem *subsystem) {
   Label(page, subsystem->name, 50, BLACK);
   MultiLineGraph(page, &subsystem->diagnostics_graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
}

void DrawRecordings(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   element *top_bar = RowPanel(page, V2(Size(page).x - 10, page_tab_height), Padding(5, 5));
   Background(top_bar, dark_grey);
   static bool selector_open = false;

   if(Button(top_bar, "Open Recording", menu_button.IsSelected(selector_open)).clicked) {
      selector_open = !selector_open;
   }

   if(selector_open) {
      //TODO: readd this
      // if(DefaultClickInteraction(recording_selector).clicked) {
      //    state->recording.loaded = false;
      // }

      for(FileListLink *file = state->ncrr_files; file; file = file->next) {
         UI_SCOPE(page->context, file);
         
         if(Button(page, file->name, menu_button).clicked) {
            LoadRecording(&state->recording, file->name);
            selector_open = false;
         }
      }
   } else {
      if(state->recording.loaded) {
         if(state->settings.field.loaded) {
            ui_field_topdown field = FieldTopdown(page, state->settings.field.image, state->settings.field.size, 
                                                Size(page->bounds).x);

            for(s32 i = 0; i < state->recording.robot_sample_count; i++) {
               RobotRecording_RobotStateSample *sample = state->recording.robot_samples + i;
               v2 p = GetPoint(&field, sample->pos);
               Rectangle(field.e, RectCenterSize(p, V2(5, 5)), RED);
            }
         } else {
            Label(page, "No field loaded", 20, BLACK);
         }

         //TODO: automatically center this somehow, maybe make a CenterColumnLayout?
         HorizontalSlider(page, &state->recording.curr_time, state->recording.min_time, state->recording.max_time,
                        V2(Size(page->bounds).x - 40, 40), V2(20, 20));
         
         //TODO: highlight robot at current slider time
         //TODO: draw vertical bar in multiline graph at t=curr_time

         for(u32 i = 0; i < state->recording.subsystem_count; i++) {
            SubsystemRecording *subsystem = state->recording.subsystems + i;
            UI_SCOPE(page->context, subsystem);
            Label(page, subsystem->name, 20, BLACK);
            MultiLineGraph(page, &subsystem->graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
         }

      } else {   
         selector_open = true;
      }
   }
}

//TODO: icons for pages
#define PageButton(...) _PageButton(GEN_UI_ID, __VA_ARGS__)
void _PageButton(ui_id id, element *parent, char *name, DashboardPage page, DashboardState *state) {
   button_style style = menu_button.IsSelected(state->page == page);
   if(_Button(id, parent, name, style).clicked) {
      state->page = page;
   }
}

void DrawUI(element *root, DashboardState *state) {
   //OutputDebugStringA(ToCString(Concat(Literal("fps "), ToString(root->context->fps), Literal("\n"))));
   ColumnLayout(root);
   Background(root, light_grey);
   Texture(root, logoTexture, RectCenterSize(Center(root->bounds), logoTexture.size));
   
   if(root->context->debug_mode) {
      Line(root, RED, 10, V2(40, 100), V2(500, 140), V2(650, 200));
      Rectangle(root, RectCenterSize(V2(40, 100), V2(5, 5)), BLACK);
      Rectangle(root, RectCenterSize(V2(500, 140), V2(5, 5)), BLACK);
      Rectangle(root, RectCenterSize(V2(650, 200), V2(5, 5)), BLACK);
   }

   if(state->directory_changed) {
      reloadFiles(state);
   }

   element *status_bar = RowPanel(root, V2(Size(root).x, status_bar_height));
   Background(status_bar, dark_grey);
   if(state->profiles.current.state == RobotProfileState::Connected) {
      Label(status_bar, state->profiles.current.name, 20, WHITE, V2(10, 0));
      Label(status_bar, Concat(Literal("Mode: "), ToString(state->mode)), 20, WHITE, V2(10, 0));
   } else if(state->profiles.current.state == RobotProfileState::Loaded) {
      Label(status_bar, Concat(state->profiles.current.name, Literal(" (loaded from file)")), 20, WHITE, V2(10, 0));
   } else {
      Label(status_bar, "No Robot", 20, WHITE, V2(5, 0));
   }
   
   element *page_tabs = RowPanel(root, V2(Size(root).x, page_tab_height));
   Background(page_tabs, dark_grey);
   PageButton(page_tabs, "Home", DashboardPage_Home, state);
   PageButton(page_tabs, "Recordings", DashboardPage_Recordings, state);
   PageButton(page_tabs, "Robots", DashboardPage_Robots, state);
   PageButton(page_tabs, "Settings", DashboardPage_Settings, state);

   element *page = ColumnPanel(root, RectMinMax(root->bounds.min + V2(0, status_bar_height + page_tab_height), root->bounds.max));
   switch(state->page) {
      case DashboardPage_Home: DrawHome(page, state); break;
      case DashboardPage_Subsystem: DrawSubsystem(page, state->selected_subsystem); break;
      case DashboardPage_Recordings: DrawRecordings(page, state); break;
      case DashboardPage_Robots: DrawProfiles(page, &state->profiles, state->ncrp_files); break;
      case DashboardPage_Settings: {
         v2 robot_size_ft = V2(2, 2);
         string robot_size_label = Literal("No robot loaded, defaulting to 2x2ft");
         DrawSettings(page, &state->settings, robot_size_ft, robot_size_label, state->ncff_files);
      } break;
   }

   //TODO: where do we put these now?
   // if(state->connected.subsystem_count > 0) {
   //    element *divider1 = Panel(menu_bar, V2(Size(menu_bar->bounds).x - 40, 5), Padding(10, 0));
   //    Background(divider1, BLACK);

   //    for(u32 i = 0; i < state->connected.subsystem_count; i++) {
   //       ConnectedSubsystem *subsystem = state->connected.subsystems + i; 
   //       button_style style = menu_button;
   //       style.colour = V4(0.55, 0, 0, 0.5);
   //       if(_Button(POINTER_UI_ID(i), menu_bar, style, subsystem->name).clicked) {
   //          state->page = DashboardPage_Subsystem;
   //          state->selected_subsystem = subsystem;
   //       }
   //    }

   //    element *divider2 = Panel(menu_bar, V2(Size(menu_bar->bounds).x - 40, 5), Padding(10, 0));
   //    Background(divider2, BLACK);
   // }
   
   if(state->profiles.current.state == RobotProfileState::Connected) {
      string toggle_recording_text = Concat(state->manual_recorder.recording ? Literal("Stop") : Literal("Start"), Literal(" Manual Recording"));
      if(Button(page_tabs, toggle_recording_text, menu_button).clicked) {
         if(state->manual_recorder.recording) {
            //TODO: actually generate a name for these
            EndRecording(&state->manual_recorder, Literal("name"));
         } else {
            BeginRecording(&state->manual_recorder);
         }
      }
   }

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