//NOTE: requires common.cpp & the north defs

struct RobotProfileNamespace;
struct RobotProfileParameter {
   RobotProfileNamespace *parent;
   
   RobotProfileParameter *global_next;
   RobotProfileParameter *next_in_ns;

   string name;
   bool is_array;
   u32 length; //ignored if is_array is false
   union {
      f32 value; //is_array = false
      f32 *values; //is_array = true
   };
};

struct RobotProfileCommand {
   North_CommandExecutionType::type type;
   string name;
   u32 param_count;
   string *params;
   v4 colour;
};

namespace RobotProfileState {
   enum type {
      Invalid,
      Loaded,
      Connected
   };
};

struct RobotProfileNamespace {
   RobotProfileNamespace *parent;
   RobotProfileNamespace *next;
   string name;

   bool collapsed;

   RobotProfileNamespace *first_child;
   RobotProfileParameter *first_param;
};

struct RobotProfile {
   MemoryArena *arena; //NOTE: owned by RobotProfile

   RobotProfileState::type state;
   string name;
   v2 size;

   RobotProfileNamespace root_namespace;
   u32 param_count;
   RobotProfileParameter *first_param;

   u32 command_count;
   RobotProfileCommand *commands;

   u32 conditional_count;
   string *conditionals;
};

bool IsValid(RobotProfile *profile) {
   return profile->state != RobotProfileState::Invalid;
}

v4 ColourForName(string name) {
   u32 hash = Hash(name);
   v3 rgb = V3((f32)((hash << 0) & 0xFFFF) / (f32)0xFFFF, 
               (f32)((hash << 3) & 0xFFFF) / (f32)0xFFFF,
               (f32)((hash << 5) & 0xFFFF) / (f32)0xFFFF);
   rgb = Normalize(rgb);
   return V4(rgb.r, rgb.g, rgb.b, 1);
}

RobotProfileParameter *GetOrCreateParameter(RobotProfile *profile, string name) {
   MemoryArena *arena = profile->arena;
   SplitString path = split(name, '/');
   Assert(path.part_count > 0);

   RobotProfileNamespace *curr_namespace = &profile->root_namespace;
   for(u32 i = 0; i < path.part_count - 1; i++) {
      string ns_name = path.parts[i];
      bool found = false;
      
      for(RobotProfileNamespace *ns = curr_namespace->first_child; ns; ns = ns->next) {
         if(ns->name == ns_name) {
            curr_namespace = ns;
            found = true;
            break;
         }
      }

      if(!found) {
         RobotProfileNamespace *new_ns = PushStruct(arena, RobotProfileNamespace);
         new_ns->name = PushCopy(arena, ns_name);
         new_ns->collapsed = true;
         new_ns->parent = curr_namespace;
         new_ns->next = curr_namespace->first_child;
         curr_namespace->first_child = new_ns;
         
         curr_namespace = new_ns;
      }
   }

   RobotProfileParameter *result = NULL;
   string param_name = path.parts[path.part_count - 1];
   for(RobotProfileParameter *rec = curr_namespace->first_param; rec; rec = rec->next_in_ns) {
      if(rec->name == param_name) {
         result = rec;
         break;
      }
   }

   if(result == NULL) {
      result = PushStruct(arena, RobotProfileParameter);
      result->name = PushCopy(arena, param_name);
      result->parent = curr_namespace;
      result->next_in_ns = curr_namespace->first_param;
      result->global_next = profile->first_param;

      curr_namespace->first_param = result;
      profile->first_param = result;
      profile->param_count++;
   }

   return result;
}

string GetFullName(RobotProfileNamespace *ns) {
   return (ns->parent == NULL) ? EMPTY_STRING : Concat(GetFullName(ns->parent), Literal("/"), ns->name);
}

string GetFullName(RobotProfileParameter *param) {
   return Concat(GetFullName(param->parent), Literal("/"), param->name);
}

RobotProfileCommand *GetCommand(RobotProfile *profile, string name) {
   for(u32 i = 0; i < profile->command_count; i++) {
      RobotProfileCommand *command = profile->commands + i;
      if(command->name == name) {
         return command;
      }
   }

   return NULL;
}

//File-Writing-------------------------------------
void EncodeProfileFile(RobotProfile *profile, buffer *file) {
   FileHeader magic_numbers = header(ROBOT_PROFILE_MAGIC_NUMBER, ROBOT_PROFILE_CURR_VERSION);
   WriteStruct(file, &magic_numbers);
   
   WriteStructData(file, RobotProfile_FileHeader, s, {
      s.conditional_count = profile->conditional_count;
      s.parameter_count = profile->param_count;
      s.command_count = profile->command_count;

      s.robot_width = profile->size.x;
      s.robot_length = profile->size.y;
   });
   
   ForEachArray(i, cond, profile->conditional_count, profile->conditionals, {
      u8 len = cond->length;
      WriteStruct(file, &len);
      WriteString(file, *cond);
   });

   for(RobotProfileParameter *param = profile->first_param; param; param = param->global_next) {
      string full_name = GetFullName(param);
      WriteStructData(file, RobotProfile_Parameter, s, {
         s.name_length = full_name.length; 
         s.is_array = param->is_array;
         s.value_count = param->is_array ? param->length : 1;
      });
      WriteString(file, full_name);
      if(param->is_array) {
         WriteArray(file, param->values, param->length);
      } else {
         WriteStruct(file, &param->value);
      }
   }

   ForEachArray(j, command, profile->command_count, profile->commands, {
      WriteStructData(file, RobotProfile_Command, s, {
         s.name_length = command->name.length;
         s.param_count = command->param_count;
         s.type = (u8) command->type;
      });
      WriteString(file, command->name);
      ForEachArray(k, param, command->param_count, command->params, {
         u8 length = param->length;
         WriteStruct(file, &length);
         WriteString(file, *param);
      });
   });
}

void UpdateProfileFile(RobotProfile *profile) {
   buffer file = PushTempBuffer(Megabyte(1));
   EncodeProfileFile(profile, &file);
   WriteEntireFile(Concat(profile->name, Literal(".ncrp")), file);
}

//Packet-Parsing-------------------------------------
void RecieveWelcomePacket(RobotProfile *profile, buffer packet) {
   MemoryArena *arena = profile->arena;
   
   Reset(arena);
   profile->first_param = NULL;
   profile->param_count = 0;
   ZeroStruct(&profile->root_namespace);

   profile->state = RobotProfileState::Connected;
   
   Welcome_PacketHeader *header = ConsumeStruct(&packet, Welcome_PacketHeader);
   profile->name = PushCopy(arena, ConsumeString(&packet, header->robot_name_length));
   profile->size = V2(header->robot_width, header->robot_length);
   profile->command_count = header->command_count;
   profile->commands = PushArray(arena, RobotProfileCommand, profile->command_count);
   profile->conditional_count = header->conditional_count;
   profile->conditionals = PushArray(arena, string, profile->conditional_count);

   for(u32 i = 0; i < header->conditional_count; i++) {
      u8 len = *ConsumeStruct(&packet, u8);
      profile->conditionals[i] = PushCopy(arena, ConsumeString(&packet, len));
   }

   for(u32 j = 0; j < header->command_count; j++) {
      Welcome_Command *command_desc = ConsumeStruct(&packet, Welcome_Command);
      RobotProfileCommand *command = profile->commands + j;
      
      command->name = PushCopy(arena, ConsumeString(&packet, command_desc->name_length));
      command->type = (North_CommandExecutionType::type) command_desc->type;
      command->param_count = command_desc->param_count;
      command->params = PushArray(arena, string, command->param_count);
      command->colour = ColourForName(command->name);

      for(u32 k = 0; k < command->param_count; k++) {
         u8 length = *ConsumeStruct(&packet, u8);
         command->params[k] = PushCopy(arena, ConsumeString(&packet, length));
      }
   }

   UpdateProfileFile(profile);
}

void RecieveParameter(RobotProfile *profile, buffer *packet) {
   MemoryArena *arena = profile->arena;

   CurrentParameters_Parameter *param_data = ConsumeStruct(packet, CurrentParameters_Parameter);
   string name = ConsumeString(packet, param_data->name_length); 
   f32 *values = ConsumeArray(packet, f32, param_data->is_array ? param_data->value_count : 1);

   RobotProfileParameter *param = GetOrCreateParameter(profile, name);
   if(param->name.text == NULL) {
      param->name = PushCopy(arena, name);
      param->is_array = param_data->is_array;
   }
   
   if(param->is_array) {
      param->length = param_data->value_count;
      param->values = (f32 *) PushCopy(arena, values, param->length * sizeof(f32));
   } else {
      param->value = *values;
   }
}

void RecieveCurrentParametersPacket(RobotProfile *profile, buffer packet) {
   Assert(profile->state == RobotProfileState::Connected);
   MemoryArena *arena = profile->arena;
   
   CurrentParameters_PacketHeader *header = ConsumeStruct(&packet, CurrentParameters_PacketHeader);
   
   for(u32 i = 0; i < header->parameter_count; i++) {
      RecieveParameter(profile, &packet);
   }

   UpdateProfileFile(profile);
}

//File-Reading----------------------------------------
void ParseProfileFile(RobotProfile *profile, buffer file, string name) {
   MemoryArena *arena = profile->arena;
   
   Reset(arena);
   profile->first_param = NULL;
   profile->param_count = 0;
   ZeroStruct(&profile->root_namespace);

   profile->state = RobotProfileState::Loaded;

   FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);
   Assert(file_numbers->magic_number == ROBOT_PROFILE_MAGIC_NUMBER);
   Assert(file_numbers->version_number == ROBOT_PROFILE_CURR_VERSION);

   RobotProfile_FileHeader *header = ConsumeStruct(&file, RobotProfile_FileHeader);
   profile->name = PushCopy(arena, name);
   profile->size = V2(header->robot_width, header->robot_length);
   profile->command_count = header->command_count;
   profile->commands = PushArray(arena, RobotProfileCommand, header->command_count);
   profile->conditional_count = header->conditional_count;
   profile->conditionals = PushArray(arena, string, header->conditional_count);

   for(u32 i = 0; i < header->conditional_count; i++) {
      u8 len = *ConsumeStruct(&file, u8);
      profile->conditionals[i] = PushCopy(arena, ConsumeString(&file, len));
   }

   for(u32 i = 0; i < header->parameter_count; i++) {
      RobotProfile_Parameter *file_param = ConsumeStruct(&file, RobotProfile_Parameter);
      string param_name = ConsumeString(&file, file_param->name_length);
      RobotProfileParameter *param = GetOrCreateParameter(profile, param_name);

      param->is_array = file_param->is_array;
      f32 *values = ConsumeArray(&file, f32, file_param->is_array ? file_param->value_count : 1);
      if(param->is_array) {
         param->length = file_param->value_count;
         param->values = (f32 *) PushCopy(arena, values, param->length * sizeof(f32));
      } else {
         param->value = *values;
      }
   }

   for(u32 j = 0; j < profile->command_count; j++) {
      RobotProfile_Command *file_command = ConsumeStruct(&file, RobotProfile_Command);
      RobotProfileCommand *command = profile->commands + j;
      
      command->name = PushCopy(arena, ConsumeString(&file, file_command->name_length));
      command->type = (North_CommandExecutionType::type) file_command->type;
      command->param_count = file_command->param_count;
      command->params = PushArray(arena, string, command->param_count);
      command->colour = ColourForName(command->name);

      for(u32 k = 0; k < file_command->param_count; k++) {
         u8 length = *ConsumeStruct(&file, u8);
         command->params[k] = PushCopy(arena, ConsumeString(&file, length));
      }
   }
}

void LoadProfileFile(RobotProfile *profile, string file_name) {
   buffer loaded_file = ReadEntireFile(Concat(file_name, Literal(".ncrp")));
   if(loaded_file.data != NULL) 
      ParseProfileFile(profile, loaded_file, file_name);
}

//Sending-Packets--------------------------------
buffer MakeParamOpPacket(RobotProfileParameter *param, ParameterOp_Type::type type, f32 value, u32 index) {
   buffer packet = PushTempBuffer(Kilobyte(10));

   u32 size = sizeof(ParameterOp_PacketHeader) + param->name.length;
   PacketHeader p_header = { size, (u8)PacketType::ParameterOp };

   ParameterOp_PacketHeader header = {};
   header.type = (u8) type;
   header.param_name_length = param->name.length;

   header.value = value;
   header.index = index;

   WriteStruct(&packet, &p_header);
   WriteStruct(&packet, &header);
   WriteString(&packet, param->name);

   return packet;
}

void SendParamSetValue(RobotProfileParameter *param, f32 value, u32 index = 0) {
   buffer packet = MakeParamOpPacket(param, ParameterOp_Type::SetValue, value, index);
   SendPacket(packet);
}

void SendParamRemoveValue(RobotProfileParameter *param, u32 index) {
   buffer packet = MakeParamOpPacket(param, ParameterOp_Type::RemoveValue, 0, index);
   SendPacket(packet);
}

void SendParamAddValue(RobotProfileParameter *param, f32 value) {
   buffer packet = MakeParamOpPacket(param, ParameterOp_Type::AddValue, value, 0);
   SendPacket(packet);
}

//UI--------------------------------------------------
#ifdef INCLUDE_DRAWPROFILES 
struct RobotProfiles {
   RobotProfile current;
   RobotProfile loaded; //NOTE: this is a file we're looking at
   RobotProfile *selected; //??
};

void DrawProfiles_DrawParam(element *group_page, RobotProfile *profile, RobotProfileParameter *param) {
   UI_SCOPE(group_page, param);

   button_style hide_button = ButtonStyle(
      dark_grey, light_grey, BLACK,
      light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
      off_white, light_grey,
      20, V2(0, 0), V2(0, 0));

   if(profile->state == RobotProfileState::Connected) {
      if(param->is_array) {
         Label(group_page, Concat(param->name, Literal(": ")), 18, BLACK, V2(20, 0));
         for(u32 i = 0; i < param->length; i++) {
            UI_SCOPE(group_page, i);

            element *param_row = RowPanel(group_page, Size(Size(group_page).x, 18));
            Label(param_row, Concat(ToString(i), Literal(": ")), 18, BLACK, V2(40, 0));
            ui_numberbox param_box = TextBox(param_row, param->values[i], 18);
            
            if(param_box.valid_enter) {
               SendParamSetValue(param, param_box.f32_value, i);
            }

            if(Button(param_row, "Remove", hide_button.Padding(5, 0).IsEnabled(param->length > 1)).clicked) {
               SendParamRemoveValue(param, i);
            }   
         }

         if(Button(group_page, "Add", hide_button.Padding(40, 0)).clicked) {
            SendParamAddValue(param, 0);
         }
      } else {
         element *param_row = RowPanel(group_page, Size(Size(group_page).x, 18));
         Label(param_row, Concat(param->name, Literal(": ")), 18, BLACK, V2(20, 0));
         ui_numberbox param_box = TextBox(param_row, param->value, 18);
         
         if(param_box.valid_enter) {
            SendParamSetValue(param, param_box.f32_value);
         }
      }
   } else {
      if(param->is_array) {
         Label(group_page, Concat(param->name, Literal(": ")), 18, BLACK, V2(20, 0));
         for(u32 i = 0; i < param->length; i++) {
            Label(group_page, Concat(ToString(i), Literal(": "), ToString(param->values[i])), 18, BLACK, V2(40, 0));
         }
      } else {
         Label(group_page, Concat(param->name, Literal(": "), ToString(param->value)), 18, BLACK, V2(20, 0));
      }
   }  
}

void DrawProfiles_DrawNamespace(element *page, RobotProfile *profile, RobotProfileNamespace *ns) {
   UI_SCOPE(page, ns);
   element *group_page = ColumnPanel(page, Width(Size(page).x - 60).Padding(20, 0));
   
   button_style hide_button = ButtonStyle(
      dark_grey, light_grey, BLACK,
      light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
      off_white, light_grey,
      20, V2(0, 0), V2(0, 0));

   element *top_row = RowPanel(group_page, Size(Size(group_page).x, 20));
   Label(top_row, (ns->name.length == 0) ? Literal("/") : ns->name, 20, BLACK);
   if(Button(top_row, ns->collapsed ? "  +  " : "  -  ", hide_button).clicked) {
      ns->collapsed = !ns->collapsed;
   }

   if(!ns->collapsed) {
      for(RobotProfileParameter *param = ns->first_param; param; param = param->next_in_ns) {
         DrawProfiles_DrawParam(group_page, profile, param);
      }

      for(RobotProfileNamespace *child_ns = ns->first_child; child_ns; child_ns = child_ns->next) {
         DrawProfiles_DrawNamespace(group_page, profile, child_ns);
      }
   }
}

void DrawProfiles(element *full_page, RobotProfiles *profiles, FileListLink *ncrp_files) {
   StackLayout(full_page);
   element *page = VerticalList(full_page);
   
   element *top_bar = RowPanel(page, Size(Size(page).x - 10, page_tab_height).Padding(5, 5));
   Background(top_bar, dark_grey);

   static bool selector_open = false;

   if(Button(top_bar, "Open Profile", menu_button.IsSelected(selector_open)).clicked) {
      selector_open = !selector_open;
   }

   if(selector_open) {
      if(profiles->current.state == RobotProfileState::Connected) {
         if(Button(page, "Connected Robot", menu_button).clicked) {
            profiles->selected = &profiles->current;
            selector_open = false;
         }

         element *divider = Panel(page, Size(Size(page->bounds).x - 40, 5).Padding(10, 0));
         Background(divider, BLACK);
      }

      for(FileListLink *file = ncrp_files; file; file = file->next) {
         UI_SCOPE(page->context, file);
         
         if((profiles->current.state == RobotProfileState::Connected) && 
            (profiles->current.name == file->name))
         {
            continue;
         }
         
         if(Button(page, file->name, menu_button).clicked) {
            LoadProfileFile(&profiles->loaded, file->name);

            if(IsValid(&profiles->loaded)) {
               profiles->selected = &profiles->loaded;
               selector_open = false;
            }
         }
      }
   } else {
      if(profiles->selected) {
         RobotProfile *profile = profiles->selected;
         Label(page, profile->name, 20, BLACK);
         
         if(Button(page, "Load", menu_button.IsEnabled(profiles->current.name != profile->name)).clicked) {
            LoadProfileFile(&profiles->current, profile->name);
         }

         element *params_panel = ColumnPanel(page, Width( Size(page).x ));
         Background(params_panel, V4(0.5, 0.5, 0.5, 0.5));
         Label(params_panel, "Params", 20, BLACK);
         
         DrawProfiles_DrawNamespace(params_panel, profile, &profile->root_namespace);
         
         FinalizeLayout(params_panel);

         Label(page, "Commands", 20, BLACK);
         for(u32 j = 0; j < profile->command_count; j++) {
            RobotProfileCommand *command = profile->commands + j;
            //TODO: icon for command->type
            string params = (command->param_count > 0) ? command->params[0] : EMPTY_STRING;
            for(u32 k = 1; k < command->param_count; k++) {
               params = Concat(params, Literal(", "), command->params[k]);
            }
            Label(page, Concat(command->name, Literal("("), params, Literal(")")), 18, BLACK, V2(20, 0));  
         }

         Label(page, "Conditionals", 20, BLACK, V2(0, 20));
         for(u32 j = 0; j < profile->conditional_count; j++) {
            Label(page, profile->conditionals[j], 18, BLACK, V2(20, 0));
         }
      } else {
         selector_open = true;
      }
   }
}
#endif