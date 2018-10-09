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

         u32 starting_positions_size = sizeof(v2) * header->starting_position_count;
         v2 *starting_positions = (v2 *) ConsumeSize(&field_file, starting_positions_size);
         
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
      ConsumeArray(&old_field_file, v2, header->starting_position_count);
      img.width = header->image_width;
      img.height = header->image_height;
      img.texels = (u32 *) ConsumeSize(&old_field_file, img.width * img.height * 4);
   }

   FileHeader numbers = header(FIELD_MAGIC_NUMBER, FIELD_CURR_VERSION);
   Field_FileHeader header = {};
   header.width = state->field.size.x;
   header.height = state->field.size.y;
   header.starting_position_count = state->field.starting_position_count;
   header.image_width = img.width;
   header.image_height = img.height;

   u32 starting_positions_size = header.starting_position_count * sizeof(v2);
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