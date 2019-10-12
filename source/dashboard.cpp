#include "theme.cpp"

#define INCLUDE_DRAWSETTINGS
#include "north_shared/north_settings_utils.cpp"
#define INCLUDE_DRAWPROFILES
#include "north_shared/robot_profile_utils.cpp"
#define INCLUDE_DRAWRECORDINGS
#include "north_shared/robot_recording_utils.cpp"

#include "north_shared/auto_project_utils.cpp"

#include "dashboard_live_view.cpp"
#include "auto_editor/auto_editor.cpp"

enum DashboardPage {
   DashboardPage_Home,
   DashboardPage_Auto,
   DashboardPage_Recordings,
   DashboardPage_Robots,
   DashboardPage_Settings
};

struct DashboardState {
   ConnectedState connected;

   //TODO: clean this stuff up
      NorthSettings settings;
      RobotProfiles profiles;
      
      LoadedRobotRecording recording;
      RobotRecorder auto_recorder;
      RobotRecorder manual_recorder;

      AutoProjectList auto_programs;

      EditorState editor;

   //--------------------------------------------------------
   DashboardPage page;
   
   //AutoProjectLink *selected_auto_project; //NOTE: this doesnt survive a directory change!!!
   //AutoNode *selected_auto_node; //NOTE: this doesnt survive a directory change!!!
   
   //--------------------------------------------------------

   bool directory_changed;
   MemoryArena *file_lists_arena;
   FileListLink *ncff_files; //Field 
   FileListLink *ncrr_files; //Recording
   FileListLink *ncrp_files; //Robot Profile
   // FileListLink *ncap_files; //Auto Programs
   FileWatcher file_watcher;
   
   f32 curr_time;
};

void reloadFiles(DashboardState *state) {
   Reset(state->file_lists_arena);
   state->ncff_files = ListFilesWithExtension("*.ncff", state->file_lists_arena);
   state->ncrr_files = ListFilesWithExtension("*.ncrr", state->file_lists_arena);
   state->ncrp_files = ListFilesWithExtension("*.ncrp", state->file_lists_arena);
   // state->ncap_files = ListFilesWithExtension("*.ncap", state->file_lists_arena);

   ReadSettingsFile(&state->settings);
   ReadAllProjects(&state->editor.auto_programs);
}

void initDashboard(DashboardState *state) {
   state->connected.persistent_arena = PlatformAllocArena(Megabyte(10), "Connected Group");
   state->connected.frame_arena = PlatformAllocArena(Megabyte(4), "Connected Messagelike");
   ResetConnectedState(&state->connected);

   state->settings.arena = PlatformAllocArena(Megabyte(1), "Settings");
   state->recording.arena = PlatformAllocArena(Megabyte(20), "Loaded Recording");
   state->auto_programs.arena = PlatformAllocArena(Megabyte(20), "Auto Programs");
   state->file_lists_arena = PlatformAllocArena(Megabyte(10), "File Lists");
   state->auto_recorder.arena = PlatformAllocArena(Megabyte(30), "Auto Recorder");
   state->manual_recorder.arena = PlatformAllocArena(Megabyte(30), "Manual Recorder");
   state->profiles.current.arena = PlatformAllocArena(Megabyte(10), "Current Profile");
   state->profiles.loaded.arena = PlatformAllocArena(Megabyte(10), "Loaded Profile");
   state->page = DashboardPage_Home;
   
   InitFileWatcher(&state->file_watcher, PlatformAllocArena(Kilobyte(512), "File Watcher"), "*.*");
   initEditor(&state->editor);
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

#include "test_sim.cpp"

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

#if 1
      //draw test robot
      static AutoRobotPose test_pose = {};
      DrawRobot(&field, V2(1, 1), test_pose.pos, test_pose.angle, BLACK);
      
      static f32 test_pr = 0;
      static f32 test_pl = 0; 
      Label(page, "Right", 40, BLACK);
      HorizontalSlider(page, &test_pr, -2, 2, V2(200, 40));
      Label(page, "Left", 40, BLACK);
      HorizontalSlider(page, &test_pl, -2, 2, V2(200, 40));
      if(Button(page, "Move", menu_button).clicked) {
         test_pose = ForwardKinematics(test_pose, test_pr, test_pl, 1);
      }

      u32 shadow_count = 10;
      f32 shadow_step = 1.0 / (shadow_count - 1);
      for(u32 i = 0; i < shadow_count; i++) {
         AutoRobotPose shadow_pose = ForwardKinematics(test_pose, shadow_step * i * test_pr, shadow_step * i * test_pl, 1);
         DrawRobot(&field, V2(1, 1), shadow_pose.pos, shadow_pose.angle, BLACK);
      }
#endif

#if 0
      static North_HermiteControlPoint test_spline[] = {
         {V2(0, 0), V2(8, 0)},
         {V2(3, 2), V2(8, 0)}
      }; 

      SplineResult test_spline_gen = SplineToWheelSetpoints(test_spline, ArraySize(test_spline), 20, 20);

      for(u32 i = 0; i < test_spline_gen.arclength_pose_map.sample_count; i++) {
         AutoRobotPose shadow_pose = test_spline_gen.arclength_pose_map.samples[i].data;
         DrawRobot(&field, V2(1, 1), shadow_pose.pos, shadow_pose.angle, BLACK);
      }

      CubicHermiteSpline(&field, test_spline[0].pos, test_spline[0].tangent, test_spline[1].pos, test_spline[1].tangent, BLACK);

      static AutoRobotPose test_pose = {};
      DrawRobot(&field, V2(1, 1), test_pose.pos, test_pose.angle, RED);

      static u32 wheel_setpoint_i = 1;
      if(Button(page, "Move", menu_button).clicked) {
         if(wheel_setpoint_i < test_spline_gen.wheel_setpoint_map.sample_count) {
            v2 delta_p = test_spline_gen.wheel_setpoint_map.samples[wheel_setpoint_i].data - test_spline_gen.wheel_setpoint_map.samples[wheel_setpoint_i - 1].data;
            test_pose = ForwardKinematics(test_pose, delta_p.x, delta_p.y, 1);  
            wheel_setpoint_i++;
         }
      }
#endif

#if 0
      static MultiLineGraphData spline_test_graph = NewMultiLineGraph(PlatformAllocArena(Megabyte(1), "Test Spline"));
      ResetMultiLineGraph(&spline_test_graph);

      SplineResult spline_gen2 = SplineToWheelSetpoints(test_spline, ArraySize(test_spline), 20, 2);
      SplineResult spline_gen10 = SplineToWheelSetpoints(test_spline, ArraySize(test_spline), 20, 10);
      SplineResult spline_gen20 = SplineToWheelSetpoints(test_spline, ArraySize(test_spline), 20, 20);

      static f32 setpoint_n = 20;
      HorizontalSlider(page, &setpoint_n, 1, 40, V2(200, 40));
      Label(page, ToString((u32)setpoint_n), 40, BLACK);
      
      SplineResult spline_gen_n = SplineToWheelSetpoints(test_spline, ArraySize(test_spline), 20, (u32)setpoint_n);

      for(u32 i = 0; i < spline_gen2.wheel_setpoint_map.sample_count; i++) {
         AddEntry(&spline_test_graph, "2x", spline_gen2.wheel_setpoint_map.samples[i].data.x, spline_gen2.wheel_setpoint_map.samples[i].key, "m");
         // AddEntry(&spline_test_graph, "2y", spline_gen2.wheel_setpoint_map.samples[i].data.y, spline_gen2.wheel_setpoint_map.samples[i].key, "m");
      }

      for(u32 i = 0; i < spline_gen10.wheel_setpoint_map.sample_count; i++) {
         AddEntry(&spline_test_graph, "10x", spline_gen10.wheel_setpoint_map.samples[i].data.x, spline_gen10.wheel_setpoint_map.samples[i].key, "m");
         // AddEntry(&spline_test_graph, "10y", spline_gen10.wheel_setpoint_map.samples[i].data.y, spline_gen10.wheel_setpoint_map.samples[i].key, "m");
      }

      for(u32 i = 0; i < spline_gen20.wheel_setpoint_map.sample_count; i++) {
         AddEntry(&spline_test_graph, "20x", spline_gen20.wheel_setpoint_map.samples[i].data.x, spline_gen20.wheel_setpoint_map.samples[i].key, "m");
         // AddEntry(&spline_test_graph, "20y", spline_gen20.wheel_setpoint_map.samples[i].data.y, spline_gen20.wheel_setpoint_map.samples[i].key, "m");
      }

      for(u32 i = 0; i < spline_gen_n.wheel_setpoint_map.sample_count; i++) {
         AddEntry(&spline_test_graph, "Nx", spline_gen_n.wheel_setpoint_map.samples[i].data.x, spline_gen_n.wheel_setpoint_map.samples[i].key, "m");
         // AddEntry(&spline_test_graph, "Ny", spline_gen_n.wheel_setpoint_map.samples[i].data.y, spline_gen_n.wheel_setpoint_map.samples[i].key, "m");
      }

      MultiLineGraph(page, &spline_test_graph, V2(Size(page).x - 40, 400));
#endif

#if 0
      //TODO: robot kinematics simulator (simulate acceleration profiles, see what constant accel looks like)
      v2 wheel_ps[] = {};

      

      AutoRobotPose sim_test_poses[] = {};
      for(u32 i = 1; i < ArraySize(wheel_ps); i++) {
         v2 wheel_diff = wheel_ps[i] - wheel_ps[i - 1];
         sim_test_poses[i] = ForwardKinematics(sim_test_poses[i - 1], wheel_diff.x, wheel_diff.y, 1);
      }
#endif

      // v2 robot_size_px =  FeetToPixels(&field, profile->size);
      // for(u32 i = 0; i < state->settings.field.starting_position_count; i++) {
      //    Field_StartingPosition *starting_pos = state->settings.field.starting_positions + i;
      //    UI_SCOPE(page->context, starting_pos);
         
      //    element *field_starting_pos = Panel(field.e, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px),
      //                                        Captures(INTERACTION_CLICK));
         
      //    bool is_selected = starting_pos_selected && (Length(selected_starting_pos.pos - starting_pos->pos) < 0.01);    
      //    v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
      //                            -sinf(starting_pos->angle * (PI32 / 180)));
      //    Background(field_starting_pos, is_selected ? GREEN : RED);
      //    Line(field_starting_pos, BLACK, 2,
      //         Center(field_starting_pos->bounds),
      //         Center(field_starting_pos->bounds) + 10 * direction_arrow);           

      //    if(WasClicked(field_starting_pos)) {
      //       starting_pos_selected = true;
      //       selected_starting_pos = *starting_pos;

      //       ReadProjectsStartingAt(&state->auto_programs, 0, starting_pos->pos, profile);

      //       //TODO: send starting pos to robot
      //    }   
      // }
      
      if(profile->state == RobotProfileState::Connected) {
         ConnectedState *conn_state = &state->connected;
         DrawLiveNamespace(page, &field, conn_state, &conn_state->root_namespace);
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
}

//TODO: icons for pages
#define PageButton(...) _PageButton(GEN_UI_ID, __VA_ARGS__)
void _PageButton(ui_id id, element *parent, char *name, DashboardPage page, DashboardState *state) {
   button_style style = menu_button.IsSelected(state->page == page);
   if(_Button(id, parent, name, style).clicked) {
      state->page = page;
   }
}

#include "gradient_descent.cpp"

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

      v3 test_pos[] = { V3(40, 100, 0), V3(100, 100, 0), V3(100, 180, 0) };
      v2 test_uvs[] = { V2(0, 0), V2(0, 0), V2(0, 0) };
      v4 test_colours[] = { RED, BLUE, GREEN };
      DrawTriangles(root, 3, test_pos, test_uvs, test_colours);
   }

   if(state->directory_changed) {
      reloadFiles(state);
   }

   element *status_bar = RowPanel(root, Size(Size(root).x, status_bar_height));
   Background(status_bar, dark_grey);
   if(state->profiles.current.state == RobotProfileState::Connected) {
      Label(status_bar, state->profiles.current.name, 20, WHITE, V2(10, 0));
      
      //TODO: print the message at "status"
      // Label(status_bar, Concat(Literal("Mode: "), ToString(state->connected.mode)), 20, WHITE, V2(10, 0));
   } else if(state->profiles.current.state == RobotProfileState::Loaded) {
      Label(status_bar, Concat(state->profiles.current.name, Literal(" (loaded from file)")), 20, WHITE, V2(10, 0));
   } else {
      Label(status_bar, "No Robot", 20, WHITE, V2(5, 0));
   }
   
   element *page_tabs = RowPanel(root, Size(Size(root).x, page_tab_height));
   Background(page_tabs, dark_grey);
   PageButton(page_tabs, "Home", DashboardPage_Home, state);
   PageButton(page_tabs, "Auto", DashboardPage_Auto, state);
   PageButton(page_tabs, "Recordings", DashboardPage_Recordings, state);
   PageButton(page_tabs, "Robots", DashboardPage_Robots, state);
   PageButton(page_tabs, "Settings", DashboardPage_Settings, state);

   element *page = ColumnPanel(root, RectMinMax(root->bounds.min + V2(0, status_bar_height + page_tab_height), root->bounds.max));
   switch(state->page) {
      case DashboardPage_Home: DrawHome(page, state); break;
      case DashboardPage_Auto: DrawAutoEditor(page, &state->editor, &state->settings, &state->profiles.current); break;
      case DashboardPage_Recordings: DrawRecordings(page, state->ncrr_files, &state->recording, &state->settings, &state->profiles); break;
      case DashboardPage_Robots: DrawProfiles(page, &state->profiles, state->ncrp_files); break;
      case DashboardPage_Settings: {
         v2 robot_size_ft = V2(2, 2);
         string robot_size_label = Literal("No robot loaded, defaulting to 2x2ft");
         DrawSettings(page, &state->settings, robot_size_ft, robot_size_label, state->ncff_files);
      } break;
   }

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

   // if(state->manual_recorder.recording) {
   //    Label(status_bar, ToString(state->manual_recorder.sample_count), 20, WHITE, V2(10, 0));
   // } 

   //TODO: move recordings to their own thread
   // if((state->connected.mode == North_GameMode::Autonomous) && 
   //    (state->connected.prev_mode != North_GameMode::Autonomous) &&
   //    !state->auto_recorder.recording) {
   //    BeginRecording(&state->auto_recorder);
   // }

   // if((state->connected.mode != North_GameMode::Autonomous) && 
   //    (state->connected.prev_mode == North_GameMode::Autonomous) &&
   //    state->auto_recorder.recording) {
   //    EndRecording(&state->auto_recorder, Literal("auto_recording"));
   // }

   // state->connected.mode = state->connected.prev_mode;
}