//NOTE: requires common.cpp & the north defs

struct RobotProfileCommand {
   North_CommandType::type type;
   string name;
   u32 param_count;
   string *params;
};

struct RobotProfileParameter {
   string name;
   bool is_array;
   u32 length; //ignored if is_array is false
   union {
      f32 value; //is_array = false
      f32 *values; //is_array = true
   };
};

struct RobotProfileSubsystem {
   string name;
   u32 command_count;
   RobotProfileCommand *commands;
   u32 param_count;
   RobotProfileParameter *params;
};

namespace RobotProfileState {
   enum type {
      Invalid,
      Loaded,
      Connected
   };
};

struct RobotProfile {
   MemoryArena arena;

   RobotProfileState::type state;
   string name;
   v2 size;
   
   u32 subsystem_count;
   RobotProfileSubsystem *subsystems;
};

bool IsValid(RobotProfile *profile) {
   return profile->state != RobotProfileState::Invalid;
}

void EncodeProfileFile(RobotProfile *profile, buffer *file) {
   WriteStructData(file, RobotProfile_FileHeader, s, {
      s.robot_name_length = profile->name.length;
      s.subsystem_count = profile->subsystem_count;
      s.robot_width = profile->size.x;
      s.robot_length = profile->size.y;
   });
   WriteString(file, profile->name);
   
   ForEachArray(i, subsystem, profile->subsystem_count, profile->subsystems, {
      WriteStructData(file, RobotProfile_SubsystemDescription, s, {
         s.name_length = subsystem->name.length;
         s.parameter_count = subsystem->param_count;
         s.command_count = subsystem->command_count; 
      });
      WriteString(file, subsystem->name);

      ForEachArray(j, param, subsystem->param_count, subsystem->params, {
         WriteStructData(file, RobotProfile_Parameter, s, {
            s.name_length = param->name.length; 
            s.is_array = param->is_array;
            s.value_count = param->is_array ? param->length : 1;
         });
         WriteString(file, param->name);
         if(param->is_array) {
            WriteArray(file, param->values, param->length);
         } else {
            WriteStruct(file, &param->value);
         }
      });
      
      ForEachArray(j, command, subsystem->command_count, subsystem->commands, {
         WriteStructData(file, RobotProfile_SubsystemCommand, s, {
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
   });
}

void UpdateProfileFile(RobotProfile *profile) {
   buffer file = PushTempBuffer(Megabyte(1));
   EncodeProfileFile(profile, &file);
   WriteEntireFile(Concat(profile->name, Literal(".ncrp")), file);
}

void RecieveWelcomePacket(RobotProfile *profile, buffer packet) {
   MemoryArena *arena = &profile->arena;
   
   Reset(arena);
   profile->state = RobotProfileState::Connected;
   
   Welcome_PacketHeader *header = ConsumeStruct(&packet, Welcome_PacketHeader);
   profile->name = PushCopy(arena, ConsumeString(&packet, header->robot_name_length));
   profile->size = V2(header->robot_width, header->robot_length);
   profile->subsystem_count = header->subsystem_count;
   profile->subsystems = PushArray(arena, RobotProfileSubsystem, profile->subsystem_count);

   for(u32 i = 0; i < header->subsystem_count; i++) {
      Welcome_SubsystemDescription *desc = ConsumeStruct(&packet, Welcome_SubsystemDescription);
      RobotProfileSubsystem *subsystem = profile->subsystems + i;
      
      subsystem->name = PushCopy(arena, ConsumeString(&packet, desc->name_length));
      subsystem->command_count = desc->command_count;
      subsystem->commands = PushArray(arena, RobotProfileCommand, subsystem->command_count);

      for(u32 j = 0; j < desc->command_count; j++) {
         Welcome_SubsystemCommand *command_desc = ConsumeStruct(&packet, Welcome_SubsystemCommand);
         RobotProfileCommand *command = subsystem->commands + j;
         
         command->name = PushCopy(arena, ConsumeString(&packet, command_desc->name_length));
         command->type = (North_CommandType::type) command_desc->type;
         command->param_count = command_desc->param_count;
         command->params = PushArray(arena, string, command->param_count);

         for(u32 k = 0; k < command->param_count; k++) {
            u8 length = *ConsumeStruct(&packet, u8);
            command->params[k] = PushCopy(arena, ConsumeString(&packet, length));
         }
      }
   }

   UpdateProfileFile(profile);
}

RobotProfileSubsystem *GetSubsystem(RobotProfile *profile, string name) {
   for(u32 i = 0; i < profile->subsystem_count; i++) {
      RobotProfileSubsystem *subsystem = profile->subsystems + i;
      if(subsystem->name == name) {
         return subsystem;
      }
   }

   return NULL;
}

RobotProfileParameter *GetParameter(RobotProfileSubsystem *subsystem, string name) {
   for(u32 i = 0; i < subsystem->param_count; i++) {
      RobotProfileParameter *param = subsystem->params + i;
      if(param->name == name) {
         return param;
      }
   }

   return NULL;
}

void RecieveCurrentParametersPacket(RobotProfile *profile, buffer packet) {
   Assert(profile->state == RobotProfileState::Connected);
   MemoryArena *arena = &profile->arena;
   
   CurrentParameters_PacketHeader *header = ConsumeStruct(&packet, CurrentParameters_PacketHeader);
   for(u32 i = 0; i < header->subsystem_count; i++) {
      CurrentParameters_SubsystemParameters *subsystem_params = ConsumeStruct(&packet, CurrentParameters_SubsystemParameters);
      string name = ConsumeString(&packet, subsystem_params->name_length);
      RobotProfileSubsystem *subsystem = GetSubsystem(profile, name);
      Assert(subsystem != NULL);

      if(subsystem->params == NULL) {
         //NOTE: we're getting CurrentParameters for the first time
         
         subsystem->param_count = subsystem_params->param_count;
         subsystem->params = PushArray(arena, RobotProfileParameter, subsystem->param_count);   
      
         for(u32 j = 0; j < subsystem_params->param_count; j++) {
            RobotProfileParameter *param = subsystem->params + j;
            CurrentParameters_Parameter *packet_param = ConsumeStruct(&packet, CurrentParameters_Parameter);
            param->name = PushCopy(arena, ConsumeString(&packet, packet_param->name_length));
            param->is_array = packet_param->is_array;
            f32 *values = ConsumeArray(&packet, f32, param->is_array ? packet_param->value_count : 1);
            if(param->is_array) {
               param->length = packet_param->value_count;
               param->values = (f32 *) PushCopy(arena, values, param->length * sizeof(f32));
            } else {
               param->value = *values;
            }
         }
      } else {
         //NOTE: parameter values got changed

         for(u32 j = 0; j < subsystem_params->param_count; j++) {
            CurrentParameters_Parameter *packet_param = ConsumeStruct(&packet, CurrentParameters_Parameter);
            string name = ConsumeString(&packet, packet_param->name_length);
            f32 *values = ConsumeArray(&packet, f32, packet_param->is_array ? packet_param->value_count : 1);
            
            RobotProfileParameter *param = GetParameter(subsystem, name);
            Assert(param != NULL);

            if(param->is_array) {
               param->length = packet_param->value_count;
               param->values = (f32 *) PushCopy(arena, values, param->length * sizeof(f32));
            } else {
               param->value = *values;
            }
         }
      }
   }

   UpdateProfileFile(profile);
}

void ParseProfileFile(RobotProfile *profile, buffer file) {
   MemoryArena *arena = &profile->arena;
   
   profile->state = RobotProfileState::Loaded;
   Reset(arena);

   FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);
   RobotProfile_FileHeader *header = ConsumeStruct(&file, RobotProfile_FileHeader);
   profile->name = PushCopy(arena, ConsumeString(&file, header->robot_name_length));
   profile->size = V2(header->robot_width, header->robot_length);
   profile->subsystem_count = header->subsystem_count;
   profile->subsystems = PushArray(arena, RobotProfileSubsystem, header->subsystem_count);

   for(u32 i = 0; i < header->subsystem_count; i++) {
      RobotProfile_SubsystemDescription *file_subsystem = ConsumeStruct(&file, RobotProfile_SubsystemDescription);
      RobotProfileSubsystem *subsystem = profile->subsystems + i;
      
      subsystem->name = PushCopy(arena, ConsumeString(&file, file_subsystem->name_length));
      subsystem->param_count = file_subsystem->parameter_count;
      subsystem->params = PushArray(arena, RobotProfileParameter, subsystem->param_count);
      subsystem->command_count = file_subsystem->command_count;
      subsystem->commands = PushArray(arena, RobotProfileCommand, subsystem->command_count);

      for(u32 j = 0; j < file_subsystem->parameter_count; j++) {
         RobotProfile_Parameter *file_param = ConsumeStruct(&file, RobotProfile_Parameter);
         RobotProfileParameter *param = subsystem->params + j;

         param->name = PushCopy(arena, ConsumeString(&file, file_param->name_length));
         param->is_array = file_param->is_array;
         f32 *values = ConsumeArray(&file, f32, file_param->is_array ? file_param->value_count : 1);
         if(param->is_array) {
            param->length = file_param->value_count;
            param->values = (f32 *) PushCopy(arena, values, param->length * sizeof(f32));
         } else {
            param->value = *values;
         }
      }

      for(u32 j = 0; j < file_subsystem->command_count; j++) {
         RobotProfile_SubsystemCommand *file_command = ConsumeStruct(&file, RobotProfile_SubsystemCommand);
         RobotProfileCommand *command = subsystem->commands + j;
         
         command->name = PushCopy(arena, ConsumeString(&file, file_command->name_length));
         command->type = (North_CommandType::type) file_command->type;
         command->param_count = file_command->param_count;
         command->params = PushArray(arena, string, command->param_count);

         for(u32 k = 0; k < file_command->param_count; k++) {
            u8 length = *ConsumeStruct(&file, u8);
            command->params[k] = PushCopy(arena, ConsumeString(&file, length));
         }
      }
   }
}

void LoadProfileFile(RobotProfile *profile, string file_name) {
   buffer loaded_file = ReadEntireFile(Concat(file_name, Literal(".ncrp")));
   if(loaded_file.data != NULL) 
      ParseProfileFile(profile, loaded_file);
   
   FreeEntireFile(&loaded_file);
}