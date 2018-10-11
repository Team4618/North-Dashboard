void ReadSettingsFile(DashboardState *state) {
   Reset(&state->settings_arena);

   buffer settings_file = ReadEntireFile("settings.ncsf");
   if(settings_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&settings_file, FileHeader);
      
      if((file_numbers->magic_number == SETTINGS_MAGIC_NUMBER) && 
         (file_numbers->version_number == SETTINGS_CURR_VERSION))
      {
         Settings_FileHeader *header = ConsumeStruct(&settings_file, Settings_FileHeader);
         
         state->team_number = header->team_number;
         state->field_name = PushCopy(&state->settings_arena, ConsumeString(&settings_file, header->field_name_length));
      }

      FreeEntireFile(&settings_file);
   }

   buffer field_file = ReadEntireFile(Concat(state->field_name, Literal(".ncff")));
   state->field.loaded = (field_file.data != NULL);
   if(field_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&field_file, FileHeader);

      if((file_numbers->magic_number == FIELD_MAGIC_NUMBER) && 
         (file_numbers->version_number == FIELD_CURR_VERSION))
      {
         Field_FileHeader *header = ConsumeStruct(&field_file, Field_FileHeader);
         state->field.size = V2(header->width, header->height);
         state->field.flags = header->flags;

         u32 starting_positions_size = sizeof(Field_StartingPosition) * header->starting_position_count;
         Field_StartingPosition *starting_positions = ConsumeArray(&field_file, Field_StartingPosition,
                                                                   header->starting_position_count);
         
         //TODO: make some sort of error message instead?
         Assert(header->starting_position_count <= ArraySize(state->field.starting_positions));
         
         state->field.starting_position_count = header->starting_position_count;
         Copy(starting_positions, starting_positions_size, state->field.starting_positions);
         
         deleteTexture(state->field.image);
         u32 *image_texels = (u32 *) ConsumeSize(&field_file, header->image_width * header->image_height * sizeof(u32));
         state->field.image = createTexture(image_texels, header->image_width, header->image_height);
      }

      FreeEntireFile(&field_file);
   }
}

void WriteFieldFile(DashboardState *state) {
   image img = {};
   {
      buffer old_field_file = ReadEntireFile(Concat(state->field_name, Literal(".ncff")));
      ConsumeStruct(&old_field_file, FileHeader);
      Field_FileHeader *header = ConsumeStruct(&old_field_file, Field_FileHeader);
      ConsumeArray(&old_field_file, Field_StartingPosition, header->starting_position_count);
      img.width = header->image_width;
      img.height = header->image_height;
      img.texels = (u32 *) ConsumeSize(&old_field_file, img.width * img.height * 4);
   }

   FileHeader numbers = header(FIELD_MAGIC_NUMBER, FIELD_CURR_VERSION);
   Field_FileHeader header = {};
   header.width = state->field.size.x;
   header.height = state->field.size.y;
   header.flags = state->field.flags;
   header.starting_position_count = state->field.starting_position_count;
   header.image_width = img.width;
   header.image_height = img.height;

   u32 starting_positions_size = header.starting_position_count * sizeof(Field_StartingPosition);
   u32 image_size = img.width * img.height * 4;
   buffer field_file = PushTempBuffer(sizeof(numbers) + sizeof(header) +
                                      starting_positions_size + image_size);
   WriteStruct(&field_file, &numbers);
   WriteStruct(&field_file, &header);
   WriteSize(&field_file, state->field.starting_positions, starting_positions_size);
   WriteSize(&field_file, img.texels, image_size);
   
   WriteEntireFile(Concat(state->field_name, Literal(".ncff")), field_file);
   OutputDebugStringA("field saved\n");
}

void WriteNewFieldFile(DashboardState *state) {
   FileHeader numbers = header(FIELD_MAGIC_NUMBER, FIELD_CURR_VERSION);
   Field_FileHeader header = {};
   header.width = state->new_field.width;
   header.height = state->new_field.height;
   image img = ReadImage(GetText(state->new_field.image_file_box));
   header.image_width = img.valid ? img.width : 0;
   header.image_height = img.valid ? img.height : 0;

   u32 image_size = img.width * img.height * 4;

   buffer field_file = PushTempBuffer(sizeof(numbers) + sizeof(header) + image_size);
   WriteStruct(&field_file, &numbers);
   WriteStruct(&field_file, &header);
   if(img.valid)
      WriteSize(&field_file, img.texels, image_size);
   
   WriteEntireFile(Concat(GetText(state->new_field.name_box), Literal(".ncff")), field_file);
   OutputDebugStringA("new field written\n");
}

void WriteSettingsFile(DashboardState *state) {
   FileHeader settings_numbers = header(SETTINGS_MAGIC_NUMBER, SETTINGS_CURR_VERSION);
   Settings_FileHeader settings_header = {};
   settings_header.team_number = (u16) state->team_number;
   settings_header.field_name_length = (u16) state->field_name.length;

   buffer settings_file = PushTempBuffer(sizeof(settings_numbers) + sizeof(settings_header) + state->field_name.length);
   WriteStruct(&settings_file, &settings_numbers);
   WriteStruct(&settings_file, &settings_header);
   WriteSize(&settings_file, state->field_name.text, state->field_name.length);

   WriteEntireFile("settings.ncsf", settings_file);
   OutputDebugStringA("settings saved\n");
}

void ReadAutonomousRun(DashboardState *state, string file_name) {
   Reset(&state->auto_run_arena);
   buffer auto_run_file = ReadEntireFile(Concat(file_name, Literal(".ncar")));
   state->auto_run.loaded = (auto_run_file.data != NULL);
   if(auto_run_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&auto_run_file, FileHeader);

      if((file_numbers->magic_number == AUTONOMOUS_RUN_MAGIC_NUMBER) && 
         (file_numbers->version_number == AUTONOMOUS_RUN_CURR_VERSION))
      {
         AutonomousRun_FileHeader *header = ConsumeStruct(&auto_run_file, AutonomousRun_FileHeader);
         
         string robot_name = ConsumeString(&auto_run_file, header->robot_name_length);

         AutonomousRun_RobotStateSample *robot_samples =
            ConsumeArray(&auto_run_file, AutonomousRun_RobotStateSample, header->robot_state_sample_count);
         
         if(header->robot_state_sample_count > 0) {
            state->auto_run.robot_sample_count = header->robot_state_sample_count;
            state->auto_run.robot_samples = (AutonomousRun_RobotStateSample *) PushCopy(&state->auto_run_arena, robot_samples,
                                             header->robot_state_sample_count * sizeof(AutonomousRun_RobotStateSample));
         
            state->auto_run.min_time = F32_MAX;
            state->auto_run.max_time = -F32_MAX;
            
            for(u32 i = 0; i < header->robot_state_sample_count; i++) {
               AutonomousRun_RobotStateSample *sample = robot_samples + i;
               state->auto_run.min_time = Min(state->auto_run.min_time, sample->time);
               state->auto_run.max_time = Max(state->auto_run.max_time, sample->time);
            }

            state->auto_run.curr_time = state->auto_run.min_time;
         }

         AutoRunSubsystem *subsystems = PushArray(&state->auto_run_arena, AutoRunSubsystem, header->subsystem_count);
         state->auto_run.subsystem_count = header->subsystem_count;
         state->auto_run.subsystems = subsystems;

         for(u32 i = 0; i < header->subsystem_count; i++) {
            AutoRunSubsystem *subsystem_graph = subsystems + i;
            AutonomousRun_SubsystemDiagnostics *subsystem = 
               ConsumeStruct(&auto_run_file, AutonomousRun_SubsystemDiagnostics);

            subsystem_graph->name = PushCopy(&state->auto_run_arena, 
                                             ConsumeString(&auto_run_file, subsystem->name_length));
            
            u32 graph_arena_size = Megabyte(2);
            MemoryArena graph_arena = NewMemoryArena(PushSize(&state->auto_run_arena, graph_arena_size), graph_arena_size); 

            subsystem_graph->graph = NewMultiLineGraph(graph_arena);
            SetDiagnosticsGraphUnits(&subsystem_graph->graph);

            for(u32 j = 0; j < subsystem->diagnostic_count; j++) {
               AutonomousRun_Diagnostic *line = ConsumeStruct(&auto_run_file, AutonomousRun_Diagnostic);
               string line_name = PushCopy(&state->auto_run_arena, ConsumeString(&auto_run_file, line->name_length));

               for(u32 k = 0; k < line->sample_count; k++) {
                  AutonomousRun_DiagnosticSample *sample = ConsumeStruct(&auto_run_file, AutonomousRun_DiagnosticSample);
                  AddEntry(&subsystem_graph->graph, line_name, sample->value, sample->time, line->unit);
               }
            }
         }
      }

      FreeEntireFile(&auto_run_file);
   }
}

void ParseAutoPath(buffer *file, MemoryArena *arena, AutoPath *path);
AutoNode *ParseAutoNode(buffer *file, MemoryArena *arena) {
   AutoNode *result = PushStruct(arena, AutoNode);
   AutonomousProgram_Node *file_node = ConsumeStruct(file, AutonomousProgram_Node);
   
   result->pos = file_node->pos;
   result->path_count = file_node->path_count;
   result->out_paths = PushArray(arena, AutoPath, file_node->path_count);

   for(u32 i = 0; i < file_node->command_count; i++) {
      AutonomousProgram_Command *command = ConsumeStruct(file, AutonomousProgram_Command);
      ConsumeString(file, command->subsystem_name_length);
      ConsumeString(file, command->command_name_length);
      ConsumeArray(file, f32, command->parameter_count);
   }

   for(u32 i = 0; i < file_node->path_count; i++) {
      AutoPath *path = result->out_paths + i;
      ParseAutoPath(file, arena, path);
      path->in_node = result;
   }

   return result;
}

f32 ParseAutoValue(buffer *file, MemoryArena *arena) {
   AutonomousProgram_Value *file_value = ConsumeStruct(file, AutonomousProgram_Value);
   f32 value = 0;
   if(file_value->is_variable) {
      u8 *variable_length = ConsumeStruct(file, u8);
      string variable_name = ConsumeString(file, *variable_length);
   } else {
      value = *ConsumeStruct(file, f32);
   }
   return value;
}

void ParseAutoPath(buffer *file, MemoryArena *arena, AutoPath *path) {
   AutonomousProgram_Path *file_path = ConsumeStruct(file, AutonomousProgram_Path);

   f32 accel = ParseAutoValue(file, arena);
   f32 deccel = ParseAutoValue(file, arena);
   f32 max_vel = ParseAutoValue(file, arena);

   //--
   string conditional = ConsumeString(file, file_path->conditional_length);
   //--
   
   path->control_point_count = file_path->control_point_count;
   v2 *control_points = ConsumeArray(file, v2, file_path->control_point_count);
   path->control_points = (v2 *) PushCopy(arena, control_points, file_path->control_point_count * sizeof(v2));

   //--
   for(u32 i = 0; i < file_path->continuous_event_count; i++) {
      AutonomousProgram_ContinuousEvent *file_event = ConsumeStruct(file, AutonomousProgram_ContinuousEvent);
      string subsystem_name = ConsumeString(file, file_event->subsystem_name_length);
      string command_name = ConsumeString(file, file_event->command_name_length);
      AutonomousProgram_DataPoint *data_points = ConsumeArray(file, AutonomousProgram_DataPoint, file_event->datapoint_count);
   }

   for(u32 i = 0; i < file_path->discrete_event_count; i++) {
      AutonomousProgram_DiscreteEvent *file_event = ConsumeStruct(file, AutonomousProgram_DiscreteEvent);
      string subsystem_name = ConsumeString(file, file_event->subsystem_name_length);
      string command_name = ConsumeString(file, file_event->command_name_length);
      f32 *params = ConsumeArray(file, f32, file_event->parameter_count);
   }
   //--

   path->out_node = ParseAutoNode(file, arena);
}

AutoProjectLink *ReadAutoProject(string file_name, MemoryArena *arena) {
   buffer file = ReadEntireFile(Concat(file_name, Literal(".ncap")));
   if(file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);
      AutonomousProgram_FileHeader *header = ConsumeStruct(&file, AutonomousProgram_FileHeader);

      for(u32 i = 0; i < header->variable_count; i++) {
         AutonomousProgram_Variable *var = ConsumeStruct(&file, AutonomousProgram_Variable);
         ConsumeArray(&file, char, var->name_length);
      }

      AutoProjectLink *result = PushStruct(arena, AutoProjectLink);
      result->starting_node = ParseAutoNode(&file, arena);
      return result;
   }

   return NULL;
}