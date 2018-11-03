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

void ReadRecording(DashboardState *state, string file_name) {
   Reset(&state->recording_arena);
   buffer recording_file = ReadEntireFile(Concat(file_name, Literal(".ncrr")));
   state->recording.loaded = (recording_file.data != NULL);
   if(recording_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&recording_file, FileHeader);

      if((file_numbers->magic_number == ROBOT_RECORDING_MAGIC_NUMBER) && 
         (file_numbers->version_number == ROBOT_RECORDING_CURR_VERSION))
      {
         RobotRecording_FileHeader *header = ConsumeStruct(&recording_file, RobotRecording_FileHeader);
         
         state->recording.robot_name = PushCopy(&state->recording_arena,
            ConsumeString(&recording_file, header->robot_name_length));

         RobotRecording_RobotStateSample *robot_samples =
            ConsumeArray(&recording_file, RobotRecording_RobotStateSample, header->robot_state_sample_count);
         
         if(header->robot_state_sample_count > 0) {
            state->recording.robot_sample_count = header->robot_state_sample_count;
            state->recording.robot_samples = (RobotRecording_RobotStateSample *) PushCopy(&state->recording_arena, robot_samples,
                                             header->robot_state_sample_count * sizeof(RobotRecording_RobotStateSample));
         
            state->recording.min_time = F32_MAX;
            state->recording.max_time = -F32_MAX;
            
            for(u32 i = 0; i < header->robot_state_sample_count; i++) {
               RobotRecording_RobotStateSample *sample = robot_samples + i;
               state->recording.min_time = Min(state->recording.min_time, sample->time);
               state->recording.max_time = Max(state->recording.max_time, sample->time);
            }

            state->recording.curr_time = state->recording.min_time;
         }

         SubsystemRecording *subsystems = PushArray(&state->recording_arena, SubsystemRecording, header->subsystem_count);
         state->recording.subsystem_count = header->subsystem_count;
         state->recording.subsystems = subsystems;

         for(u32 i = 0; i < header->subsystem_count; i++) {
            SubsystemRecording *subsystem_graph = subsystems + i;
            RobotRecording_SubsystemDiagnostics *subsystem = 
               ConsumeStruct(&recording_file, RobotRecording_SubsystemDiagnostics);

            subsystem_graph->name = PushCopy(&state->recording_arena, 
                                             ConsumeString(&recording_file, subsystem->name_length));
            
            u32 graph_arena_size = Megabyte(2);
            MemoryArena graph_arena = NewMemoryArena(PushSize(&state->recording_arena, graph_arena_size), graph_arena_size); 

            subsystem_graph->graph = NewMultiLineGraph(graph_arena);
            SetDiagnosticsGraphUnits(&subsystem_graph->graph);

            for(u32 j = 0; j < subsystem->diagnostic_count; j++) {
               RobotRecording_Diagnostic *line = ConsumeStruct(&recording_file, RobotRecording_Diagnostic);
               string line_name = PushCopy(&state->recording_arena, ConsumeString(&recording_file, line->name_length));

               for(u32 k = 0; k < line->sample_count; k++) {
                  RobotRecording_DiagnosticSample *sample = ConsumeStruct(&recording_file, RobotRecording_DiagnosticSample);
                  AddEntry(&subsystem_graph->graph, line_name, sample->value, sample->time, line->unit);
               }
            }
         }
      }

      FreeEntireFile(&recording_file);
   }
}