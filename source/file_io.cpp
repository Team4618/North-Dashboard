
void UpdateFieldFile(DashboardState *state) {
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