struct NorthSettings {
   MemoryArena arena;
   
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
};

void ReadSettingsFile(NorthSettings *settings) {
   Reset(&settings->arena);

   buffer settings_file = ReadEntireFile("settings.ncsf");
   if(settings_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&settings_file, FileHeader);
      
      if((file_numbers->magic_number == SETTINGS_MAGIC_NUMBER) && 
         (file_numbers->version_number == SETTINGS_CURR_VERSION))
      {
         Settings_FileHeader *header = ConsumeStruct(&settings_file, Settings_FileHeader);
         
         settings->team_number = header->team_number;
         settings->field_name = PushCopy(&settings->arena, ConsumeString(&settings_file, header->field_name_length));
      }

      FreeEntireFile(&settings_file);
   }

   buffer field_file = ReadEntireFile(Concat(settings->field_name, Literal(".ncff")));
   settings->field.loaded = (field_file.data != NULL);
   if(field_file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&field_file, FileHeader);

      if((file_numbers->magic_number == FIELD_MAGIC_NUMBER) && 
         (file_numbers->version_number == FIELD_CURR_VERSION))
      {
         Field_FileHeader *header = ConsumeStruct(&field_file, Field_FileHeader);
         settings->field.size = V2(header->width, header->height);
         settings->field.flags = header->flags;

         u32 starting_positions_size = sizeof(Field_StartingPosition) * header->starting_position_count;
         Field_StartingPosition *starting_positions = ConsumeArray(&field_file, Field_StartingPosition,
                                                                   header->starting_position_count);
         
         //TODO: make some sort of error message instead?
         Assert(header->starting_position_count <= ArraySize(settings->field.starting_positions));
         
         settings->field.starting_position_count = header->starting_position_count;
         Copy(starting_positions, starting_positions_size, settings->field.starting_positions);
         
         deleteTexture(settings->field.image);
         u32 *image_texels = (u32 *) ConsumeSize(&field_file, header->image_width * header->image_height * sizeof(u32));
         settings->field.image = createTexture(image_texels, header->image_width, header->image_height);
      }

      FreeEntireFile(&field_file);
   }
}

void UpdateFieldFile(NorthSettings *settings) {
   image img = {};
   {
      buffer old_field_file = ReadEntireFile(Concat(settings->field_name, Literal(".ncff")));
      ConsumeStruct(&old_field_file, FileHeader);
      Field_FileHeader *header = ConsumeStruct(&old_field_file, Field_FileHeader);
      ConsumeArray(&old_field_file, Field_StartingPosition, header->starting_position_count);
      img.width = header->image_width;
      img.height = header->image_height;
      img.texels = (u32 *) ConsumeSize(&old_field_file, img.width * img.height * 4);
   }

   FileHeader numbers = header(FIELD_MAGIC_NUMBER, FIELD_CURR_VERSION);
   Field_FileHeader header = {};
   header.width = settings->field.size.x;
   header.height = settings->field.size.y;
   header.flags = settings->field.flags;
   header.starting_position_count = settings->field.starting_position_count;
   header.image_width = img.width;
   header.image_height = img.height;

   u32 starting_positions_size = header.starting_position_count * sizeof(Field_StartingPosition);
   u32 image_size = img.width * img.height * 4;
   buffer field_file = PushTempBuffer(sizeof(numbers) + sizeof(header) +
                                      starting_positions_size + image_size);
   WriteStruct(&field_file, &numbers);
   WriteStruct(&field_file, &header);
   WriteSize(&field_file, settings->field.starting_positions, starting_positions_size);
   WriteSize(&field_file, img.texels, image_size);
   
   WriteEntireFile(Concat(settings->field_name, Literal(".ncff")), field_file);
   OutputDebugStringA("field updated\n");
}

void UpdateSettingsFile(NorthSettings *settings) {
   FileHeader settings_numbers = header(SETTINGS_MAGIC_NUMBER, SETTINGS_CURR_VERSION);
   Settings_FileHeader settings_header = {};
   settings_header.team_number = (u16) settings->team_number;
   settings_header.field_name_length = (u16) settings->field_name.length;

   buffer settings_file = PushTempBuffer(sizeof(settings_numbers) + sizeof(settings_header) + settings->field_name.length);
   WriteStruct(&settings_file, &settings_numbers);
   WriteStruct(&settings_file, &settings_header);
   WriteSize(&settings_file, settings->field_name.text, settings->field_name.length);

   WriteEntireFile("settings.ncsf", settings_file);
   OutputDebugStringA("settings updated\n");
}

void WriteNewFieldFile(string name, f32 width, f32 height, string img_file_name) {
   FileHeader numbers = header(FIELD_MAGIC_NUMBER, FIELD_CURR_VERSION);
   Field_FileHeader header = {};
   header.width = width;
   header.height = height;
   image img = ReadImage(img_file_name);
   header.image_width = img.valid ? img.width : 0;
   header.image_height = img.valid ? img.height : 0;

   u32 image_size = img.width * img.height * 4;

   buffer field_file = PushTempBuffer(sizeof(numbers) + sizeof(header) + image_size);
   WriteStruct(&field_file, &numbers);
   WriteStruct(&field_file, &header);
   if(img.valid)
      WriteSize(&field_file, img.texels, image_size);
   
   WriteEntireFile(Concat(name, Literal(".ncff")), field_file);
   OutputDebugStringA("new field written\n");
}

#ifdef INCLUDE_DRAWSETTINGS
//TODO: do we want to save settings & field data every time a change is made or have a "save" button?
void DrawSettings(element *full_page, NorthSettings *state, 
                  v2 robot_size_ft, string robot_size_label,
                  FileListLink *ncff_files)
{
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   Label(page, "Settings", 50, BLACK);
   
   bool settings_changed = false;
   settings_changed |= SettingsRow(page, "Team Number: ", &state->team_number).valid_enter;
   
   if(settings_changed) {
      UpdateSettingsFile(state);
   }

   if(state->field.loaded) {
      bool field_data_changed = false;

      Label(page, state->field_name, 20, BLACK);
      Label(page, "Field Dimensions", 20, BLACK, V2(0, 0), V2(0, 5));
      field_data_changed |= SettingsRow(page, "Field Width: ", &state->field.size.x, "ft").valid_enter;
      field_data_changed |= SettingsRow(page, "Field Height: ", &state->field.size.y, "ft").valid_enter;
      
      field_data_changed |= SettingsRow(page, "Field Mirrored (eg. Steamworks)", &state->field.flags, Field_Flags::MIRRORED, V2(20, 20)).clicked;
      field_data_changed |= SettingsRow(page, "Field Symmetric (eg. Power Up)", &state->field.flags, Field_Flags::SYMMETRIC, V2(20, 20)).clicked;

      ui_field_topdown field = FieldTopdown(page, state->field.image, state->field.size, Size(page->bounds).x);
      Label(page, robot_size_label, 20, BLACK);

      v2 robot_size_px =  FeetToPixels(&field, robot_size_ft);
      for(u32 i = 0; i < state->field.starting_position_count; i++) {
         Field_StartingPosition *starting_pos = state->field.starting_positions + i;
         UI_SCOPE(page->context, starting_pos);

         element *field_starting_pos = Panel(field.e, RectCenterSize(GetPoint(&field, starting_pos->pos), robot_size_px), Captures(INTERACTION_DRAG));
         v2 direction_arrow = V2(cosf(starting_pos->angle * (PI32 / 180)), 
                                 -sinf(starting_pos->angle * (PI32 / 180)));
         Background(field_starting_pos, RED);
         Line(field_starting_pos, Center(field_starting_pos),
                                  Center(field_starting_pos) + 10 * direction_arrow,
                                  BLACK);
         
         starting_pos->pos = ClampTo(starting_pos->pos + PixelsToFeet(&field, GetDrag(field_starting_pos)), 
                                     RectCenterSize(V2(0, 0), field.size_in_ft));
         
         element *starting_pos_panel = Panel(page, V2(Size(page).x, 40), Layout(RowLayout).Margin(0, 5));
         Background(starting_pos_panel, BLUE); 

         Label(starting_pos_panel, "X: ", 20, BLACK);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->pos.x, 20).valid_changed;
         Label(starting_pos_panel, "Y: ", 20, BLACK);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->pos.y, 20).valid_changed;
         Label(starting_pos_panel, "Angle: ", 20, BLACK);
         field_data_changed |= TextBox(starting_pos_panel, &starting_pos->angle, 20).valid_changed;

         if(Button(starting_pos_panel, "Delete", menu_button).clicked) {
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

         field_data_changed |= (GetDrag(field_starting_pos).x != 0) || (GetDrag(field_starting_pos).y != 0);
      }

      if((state->field.starting_position_count + 1) < ArraySize(state->field.starting_positions)) {
         element *add_starting_pos = RowPanel(page, V2(Size(page->bounds).x, 40), Margin(0, 5).Captures(INTERACTION_CLICK));
         Background(add_starting_pos, RED);
         if(WasClicked(add_starting_pos)) {
            state->field.starting_position_count++;
            state->field.starting_positions[state->field.starting_position_count - 1] = {};
            field_data_changed = true;
         }
      }

      if(field_data_changed) {
         UpdateFieldFile(state);
      }
   } else {
      Label(page, Concat(state->field_name, Literal(".ncff not found")), 20, BLACK);
   }

   //TODO: redo the settings ui
   // element *field_selector = VerticalList(SlidingSidePanel(full_page, 300, 5, 50, true));
   // Background(field_selector, V4(0.5, 0.5, 0.5, 0.5));
   
   // Label(field_selector, "Fields", 50); //TODO: center this
   
   // for(FileListLink *file = ncff_files; file; file = file->next) {
   //    if(_Button(POINTER_UI_ID(file), field_selector, menu_button, file->name).clicked) {
   //       //TODO: this is safe because the ReadSettingsFile allocates field_name in settings_arena
   //       //       its pretty ugly tho so clean up
   //       state->field_name = file->name;
   //       UpdateSettingsFile(state);
   //       ReadSettingsFile(state);
   //    }
   // }

   // //TODO: dont know if i like using static like this, especially for the textboxes
   // //      mainly because it takes up a name, SettingsRow is pretty bad too
   // static bool new_field_open = false; 
   // static f32 new_field_width = 0;
   // static f32 new_field_height = 0;
   // StaticTextBoxData(new_field_name, 15);
   // StaticTextBoxData(new_field_image_file, 15);

   // if(new_field_open) {
   //    element *create_game_window = Panel(field_selector, V2(Size(field_selector->bounds).x - 20, 300), Layout(ColumnLayout).Padding(10, 10));
   //    Background(create_game_window, menu_button.colour);

   //TODO: get rid of the image name_box & replace it with a file drop thing
   //    Label(create_game_window, "Create New Field File", 20);
   //    ui_textbox name_box = SettingsRow(create_game_window, "Name: ", &new_field_name);
   //    ui_numberbox width_box = SettingsRow(create_game_window, "Width: ", &new_field_width, "ft");
   //    ui_numberbox height_box = SettingsRow(create_game_window, "Height: ", &new_field_height, "ft");
   //    ui_textbox image_file_box = SettingsRow(create_game_window, "Image: ", &new_field_image_file);

   //    bool valid = (GetText(name_box).length > 0) &&
   //                 (GetText(image_file_box).length > 0) &&
   //                 width_box.valid && (new_field_width > 0) &&
   //                 height_box.valid && (new_field_height > 0);
      
   //    if(Button(create_game_window, menu_button, "Create", valid).clicked) {
   //       WriteNewFieldFile(GetText(name_box), 
   //                         new_field_width,
   //                         new_field_height,
   //                         GetText(image_file_box));
   //       new_field_open = false;
   //    }

   //    if(Button(create_game_window, menu_button, "Exit").clicked) {
   //       new_field_width = 0;
   //       new_field_height = 0;
   //       Clear(name_box);
   //       Clear(image_file_box);
   //       new_field_open = false;
   //    }
   // } else {
   //    if(Button(field_selector, menu_button, "Create Field").clicked) {
   //       new_field_open = true;
   //    }
   // }
}
#endif