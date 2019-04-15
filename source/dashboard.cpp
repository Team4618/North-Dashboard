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

#define INCLUDE_DRAWSETTINGS
#include "north_settings_utils.cpp"
#include "robot_recording_utils.cpp"
#define INCLUDE_DRAWPROFILES
#include "robot_profile_utils.cpp"

#include "auto_project_utils.cpp"

struct ConnectedMessage {
   string text;
   North_MessageType::type type;
};

struct ConnectedMarker {
   string text;
   v2 pos;
};

struct ConnectedPath {
   string text;
   u32 control_point_count;
   North_HermiteControlPoint *control_points;
};

struct ConnectedGroup {
   ConnectedGroup *next_in_list;
   ConnectedGroup *next_in_hash;
   string name;
   
   bool hide_graph;
   MultiLineGraphData diagnostics_graph;

   bool hide_messages;
   u32 message_count;
   ConnectedMessage *messages;

   bool hide_markers;
   u32 marker_count;
   ConnectedMarker *markers;

   bool hide_paths;
   u32 path_count;
   ConnectedPath *paths;
};

struct ConnectedState {
   MemoryArena group_arena;
   MemoryArena messagelike_arena; //NOTE: gets reset every time we recieve a state packet

   RobotRecording_RobotStateSample pos_samples[1024];
   u32 curr_pos_sample;
   u32 pos_sample_count;

   u32 group_count;
   ConnectedGroup default_group;
   ConnectedGroup *first_group;
   ConnectedGroup *group_hash[64];

   North_GameMode::type prev_mode;
   North_GameMode::type mode;
};

void ResetConnectedState(ConnectedState *state) {
   Reset(&state->group_arena);
   Reset(&state->messagelike_arena);
   state->curr_pos_sample = 0;
   state->pos_sample_count = 0;

   state->group_count = 0;
   ZeroStruct(&state->default_group);
   state->default_group.diagnostics_graph = NewMultiLineGraph(PushArena(&state->group_arena, Megabyte(4)));

   state->first_group = NULL;
   ZeroStruct(&state->group_hash);
}

ConnectedGroup *GetOrCreateGroup(ConnectedState *state, string name) {
   if(name.length == 0)
      return &state->default_group;
   
   MemoryArena *arena = &state->group_arena;

   u32 hash = Hash(name) % ArraySize(state->group_hash);
   ConnectedGroup *result = NULL;
   for(ConnectedGroup *curr = state->group_hash[hash]; curr; curr = curr->next_in_hash) {
      if(curr->name == name) {
         result = curr;
      }
   }

   if(result == NULL) {
      auto *new_group = PushStruct(arena, ConnectedGroup);
      new_group->name = PushCopy(arena, name);
      
      new_group->next_in_hash = state->group_hash[hash];
      new_group->next_in_list = state->first_group;

      //TODO: make this better
      new_group->diagnostics_graph = NewMultiLineGraph(PushArena(&state->group_arena, Megabyte(4)));

      state->group_count++;
      state->group_hash[hash] = new_group;
      state->first_group = new_group;
      result = new_group;
   }
   
   return result;
}

void ParseStateGroup(f32 time, ConnectedState *state, buffer *packet) {
   MemoryArena *arena = &state->messagelike_arena;

   State_Group *header = ConsumeStruct(packet, State_Group);
   string group_name = ConsumeString(packet, header->name_length);

   ConnectedGroup *group = GetOrCreateGroup(state, group_name);

   for(u32 i = 0; i < header->diagnostic_count; i++) {
      State_Diagnostic *diag_header = ConsumeStruct(packet, State_Diagnostic);
      string diag_name = ConsumeString(packet, diag_header->name_length);

      AddEntry(&group->diagnostics_graph, diag_name, diag_header->value, time, (North_Unit::type) diag_header->unit);
   }

   group->message_count = header->message_count;
   group->messages = PushArray(arena, ConnectedMessage, header->message_count);
   for(u32 i = 0; i < header->message_count; i++) {
      State_Message *packet_message = ConsumeStruct(packet, State_Message);
      ConnectedMessage *msg = group->messages + i;

      msg->text = PushCopy(arena, ConsumeString(packet, packet_message->length));
      msg->type = (North_MessageType::type) msg->type;
   }

   group->marker_count = header->marker_count;
   group->markers = PushArray(arena, ConnectedMarker, header->marker_count);
   for(u32 i = 0; i < header->marker_count; i++) {
      State_Marker *packet_marker = ConsumeStruct(packet, State_Marker);
      ConnectedMarker *msg = group->markers + i;

      msg->text = PushCopy(arena, ConsumeString(packet, packet_marker->length));
      msg->pos = packet_marker->pos;
   }

   group->path_count = header->path_count;
   group->paths = PushArray(arena, ConnectedPath, header->path_count);
   for(u32 i = 0; i < header->path_count; i++) {
      State_Path *packet_path = ConsumeStruct(packet, State_Path);
      ConnectedPath *path = group->paths + i;

      path->text = PushCopy(arena, ConsumeString(packet, packet_path->length));
      path->control_point_count = packet_path->control_point_count;
      path->control_points = ConsumeAndCopyArray(arena, packet, North_HermiteControlPoint, packet_path->control_point_count);
   }
}

void ParseStatePacket(ConnectedState *state, buffer packet) {
   MemoryArena *arena = &state->messagelike_arena;
   Reset(arena);

   State_PacketHeader *header = ConsumeStruct(&packet, State_PacketHeader);
   state->mode = (North_GameMode::type) header->mode;
   f32 time = header->time;
   
   RobotRecording_RobotStateSample sample = { header->pos, header->angle, time };
   state->pos_samples[state->curr_pos_sample++ % ArraySize(state->pos_samples)] = sample;
   state->pos_sample_count = Clamp(0, ArraySize(state->pos_samples), state->pos_sample_count + 1);

   ParseStateGroup(time, state, &packet);
   for(u32 i = 0; i < header->group_count; i++) {
      ParseStateGroup(time, state, &packet);
   }
}

struct DashboardState {
   ConnectedState connected;

   NorthSettings settings;
   RobotProfiles profiles;
   
   LoadedRobotRecording recording;
   RobotRecorder auto_recorder;
   RobotRecorder manual_recorder;

   AutoProjectList auto_programs;

   //--------------------------------------------------------
   DashboardPage page;
   
   //AutoProjectLink *selected_auto_project; //NOTE: this doesnt survive a directory change!!!
   //AutoNode *selected_auto_node; //NOTE: this doesnt survive a directory change!!!
   
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
   state->connected.group_arena = PlatformAllocArena(Megabyte(10));
   state->connected.messagelike_arena = PlatformAllocArena(Megabyte(4));
   ResetConnectedState(&state->connected);

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
      DrawAutoPath(state, field, node->out_paths[i], preview);
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
      North_HermiteControlPoint *all_points = PushTempArray(North_HermiteControlPoint, point_count);
      
      all_points[0] = { path->in_node->pos, path->in_tangent };
      all_points[point_count - 1] = { path->out_node->pos, path->out_tangent };
      for(u32 i = 0; i < path->control_point_count; i++)
         all_points[i + 1] = path->control_points[i];

      for(u32 i = 0; i < point_count - 1; i++) {
         North_HermiteControlPoint a = all_points[i];
         North_HermiteControlPoint b = all_points[i + 1];
         
         CubicHermiteSpline(field, a.pos, a.tangent, b.pos, b.tangent, BLACK);
      }
   }

   DrawAutoNode(state, field, path->out_node, preview);
}

void DrawConnectedGroup(ConnectedGroup *group, element *page, ui_field_topdown *field) {
   UI_SCOPE(page, group);

   button_style hide_button = ButtonStyle(
      dark_grey, light_grey, BLACK,
      light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
      off_white, light_grey,
      20, V2(0, 0), V2(0, 0));

   bool has_graph_data = !IsEmpty(&group->diagnostics_graph);
   element *top_row = RowPanel(page, Size(Size(page).x, 20));
   Label(top_row, (group->name.length == 0) ? Literal("Default Group") : group->name, 20, BLACK);
   if(Button(top_row, group->hide_graph ? "  Gra+  " : "  Gra-  ", hide_button.IsEnabled(has_graph_data)).clicked) {
      group->hide_graph = !group->hide_graph;
   }
   if(Button(top_row, group->hide_messages ? "  Msg+  " : "  Msg-  ", hide_button).clicked) {
      group->hide_messages = !group->hide_messages;
   }
   if(Button(top_row, group->hide_markers ? "  Mrk+  " : "  Mrk-  ", hide_button).clicked) {
      group->hide_markers = !group->hide_markers;
   }
   if(Button(top_row, group->hide_paths ? "  Pth+  " : "  Pth-  ", hide_button).clicked) {
      group->hide_paths = !group->hide_paths;
   }

   if(!group->hide_graph && has_graph_data) {
      MultiLineGraph(page, &group->diagnostics_graph, V2(Size(page->bounds).x - 10, 400), V2(5, 5));
   }

   if(!group->hide_messages) {
      for(u32 i = 0; i < group->message_count; i++) {
         ConnectedMessage *msg = group->messages + i;
         Label(page, msg->text, 20, BLACK);
      }
   }

   if(!group->hide_markers) {
      for(u32 i = 0; i < group->marker_count; i++) {
         ConnectedMarker *msg = group->markers + i;
         UI_SCOPE(page, msg);
         
         v2 p = GetPoint(field, msg->pos);
         element *marker_panel = Panel(field->e, RectCenterSize(p, V2(5, 5)), 
                                       Captures(INTERACTION_HOT));
         Background(marker_panel, GREEN);

         if(IsHot(marker_panel)) {
            Outline(marker_panel, BLACK);
            //TODO: hover to see the marker text
         }
      }
   }

   if(!group->hide_paths) {

   }
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
                                            Clamp(0, Size(page->bounds).x, 700));

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

            ReadProjectsStartingAt(&state->auto_programs, 0, starting_pos->pos, profile);

            //TODO: send starting pos to robot
         }   
      }
      
      if(profile->state == RobotProfileState::Connected) {
         ConnectedState *conn_state = &state->connected;
         DrawConnectedGroup(&conn_state->default_group, page, &field);
         for(ConnectedGroup *curr_group = conn_state->first_group; curr_group; curr_group = curr_group->next_in_list) {
            DrawConnectedGroup(curr_group, page, &field);
         }
      }
      
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

   if(state->connected.mode != North_GameMode::Disabled)
   {
      Label(page, "Robot Messages", V2(Size(page).x, 80), 50, BLACK);
   }
}

void DrawRecordingGroup(element *page, LoadedRobotRecording *recording, LoadedRecordingGroup *group,
                        bool field_loaded, ui_field_topdown *field)
{
   UI_SCOPE(page, group);
   button_style hide_button = ButtonStyle(
      dark_grey, light_grey, BLACK,
      light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
      off_white, light_grey,
      20, V2(0, 0), V2(0, 0));

   element *top_row = RowPanel(page, Size(Size(page).x, 20));
   Label(top_row, (group->name.length == 0) ? Literal("Default Group") : group->name, 20, BLACK);
   if(Button(top_row, group->collapsed ? "  +  " : "  -  ", hide_button).clicked) {
      group->collapsed = !group->collapsed;
   }

   if(!group->collapsed) {
      if(group->diagnostic_count != 0) {
         element *graph = ImmutableMultiLineGraph(page, &group->graph, V2(Size(page->bounds).x - 30, 400), V2(5, 5));
         f32 x = GetXFromTime(&group->graph, graph->bounds, recording->curr_time);
         Line(graph, BLACK, 2, V2(x, graph->bounds.min.y), V2(x, graph->bounds.max.y));
      }
      
      //TODO: messagelike timeline
      // f32 start_time = recording->min_time;
      // f32 total_time = recording->max_time - recording->min_time;

      for(u32 i = 0; i < group->message_count; i++) {
         MessageRecording *msg = group->messages + i;
         // f32 min_x = ((msg->begin_time - start_time) / total_time) * Size(page->bounds).x;
         // f32 max_x = ((msg->end_time - start_time) / total_time) * Size(page->bounds).x;
         
         // element *msg_panel = Panel(page, Size(max_x - min_x, 40).Padding(min_x, 0));
         // Background(msg_panel, BLUE);
         // Text(msg_panel, msg->text, msg_panel->bounds.min, 20, BLACK);

         if((msg->begin_time <= recording->curr_time) && (recording->curr_time <= msg->end_time)) {
            Label(page, msg->text, 20, BLACK);
         }
      }
   }

   for(u32 i = 0; i < group->marker_count; i++) {
      MarkerRecording *msg = group->markers + i;
      UI_SCOPE(page, msg);
      
      if((msg->begin_time <= recording->curr_time) && (recording->curr_time <= msg->end_time)) {
         v2 p = GetPoint(field, msg->pos);
         element *marker_panel = Panel(field->e, RectCenterSize(p, V2(5, 5)), 
                                       Captures(INTERACTION_HOT));
         Background(marker_panel, GREEN);

         if(IsHot(marker_panel)) {
            Outline(marker_panel, BLACK);
            //TODO: hover to see the marker text
         }
      }
   }

   //TODO: draw paths
}

void DrawRecordings(element *full_page, DashboardState *state) {
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   element *top_bar = RowPanel(page, Size(Size(page).x - 10, page_tab_height).Padding(5, 5));
   Background(top_bar, dark_grey);
   static bool selector_open = false;

   if(Button(top_bar, "Open Recording", menu_button.IsSelected(selector_open)).clicked) {
      selector_open = !selector_open;
   }

   if(selector_open) {
      for(FileListLink *file = state->ncrr_files; file; file = file->next) {
         UI_SCOPE(page->context, file);
         
         if(Button(page, file->name, menu_button).clicked) {
            LoadRecording(&state->recording, file->name);
            selector_open = false;
         }
      }
   } else {
      if(state->recording.loaded) {
         ui_field_topdown field = {};

         if(state->settings.field.loaded) {
            field = FieldTopdown(page, state->settings.field.image, state->settings.field.size, 
                                 Clamp(0, Size(page->bounds).x, 700));

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
                        V2(Size(page->bounds).x - 60, 40), V2(20, 20));
         
         DrawRecordingGroup(page, &state->recording, &state->recording.default_group, state->settings.field.loaded, &field);
         for(u32 i = 0; i < state->recording.group_count; i++) {
            DrawRecordingGroup(page, &state->recording, state->recording.groups + i, state->settings.field.loaded, &field);
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

MultiLineGraphData test_graph = NewMultiLineGraph(PlatformAllocArena(Kilobyte(20)));

void DrawUI(element *root, DashboardState *state) {
   //OutputDebugStringA(ToCString(Concat(Literal("fps "), ToString(root->context->fps), Literal("\n"))));
   ColumnLayout(root);
   Background(root, light_grey);
   Texture(root, logoTexture, RectCenterSize(Center(root->bounds), logoTexture.size));

   if(root->context->debug_mode != UIDebugMode_Disabled) {
      Line(root, RED, 10, V2(40, 100), V2(500, 140), V2(650, 200));
      Rectangle(root, RectCenterSize(V2(40, 100), V2(5, 5)), BLACK);
      Rectangle(root, RectCenterSize(V2(500, 140), V2(5, 5)), BLACK);
      Rectangle(root, RectCenterSize(V2(650, 200), V2(5, 5)), BLACK);
   }

   if(state->directory_changed) {
      reloadFiles(state);
   }

   element *status_bar = RowPanel(root, Size(Size(root).x, status_bar_height));
   Background(status_bar, dark_grey);
   if(state->profiles.current.state == RobotProfileState::Connected) {
      Label(status_bar, state->profiles.current.name, 20, WHITE, V2(10, 0));
      Label(status_bar, Concat(Literal("Mode: "), ToString(state->connected.mode)), 20, WHITE, V2(10, 0));
   } else if(state->profiles.current.state == RobotProfileState::Loaded) {
      Label(status_bar, Concat(state->profiles.current.name, Literal(" (loaded from file)")), 20, WHITE, V2(10, 0));
   } else {
      Label(status_bar, "No Robot", 20, WHITE, V2(5, 0));
   }
   
   element *page_tabs = RowPanel(root, Size(Size(root).x, page_tab_height));
   Background(page_tabs, dark_grey);
   PageButton(page_tabs, "Home", DashboardPage_Home, state);
   
   PageButton(page_tabs, "Recordings", DashboardPage_Recordings, state);
   PageButton(page_tabs, "Robots", DashboardPage_Robots, state);
   PageButton(page_tabs, "Settings", DashboardPage_Settings, state);

   element *page = ColumnPanel(root, RectMinMax(root->bounds.min + V2(0, status_bar_height + page_tab_height), root->bounds.max));
   switch(state->page) {
      case DashboardPage_Home: DrawHome(page, state); break;
      case DashboardPage_Recordings: DrawRecordings(page, state); break;
      case DashboardPage_Robots: DrawProfiles(page, &state->profiles, state->ncrp_files); break;
      case DashboardPage_Settings: {
         v2 robot_size_ft = V2(2, 2);
         string robot_size_label = Literal("No robot loaded, defaulting to 2x2ft");
         DrawSettings(page, &state->settings, robot_size_ft, robot_size_label, state->ncff_files);
      } break;
   }

   //NOTE: TEST CODE----------------------------
   test_graph.arena.alloc_block = NULL;
   AddEntry(&test_graph, Literal("test"), sinf(state->curr_time), state->curr_time, 0);
   MultiLineGraph(page, &test_graph, V2(Size(page->bounds).x - 10, 400));
   
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

   if(state->manual_recorder.recording) {
      Label(status_bar, ToString(state->manual_recorder.sample_count), 20, WHITE, V2(10, 0));
   } 

   //TODO: move recordings to their own thread
   if((state->connected.mode == North_GameMode::Autonomous) && 
      (state->connected.prev_mode != North_GameMode::Autonomous) &&
      !state->auto_recorder.recording) {
      BeginRecording(&state->auto_recorder);
   }

   if((state->connected.mode != North_GameMode::Autonomous) && 
      (state->connected.prev_mode == North_GameMode::Autonomous) &&
      state->auto_recorder.recording) {
      EndRecording(&state->auto_recorder, Literal("auto_recording"));
   }

   state->connected.mode = state->connected.prev_mode;
}