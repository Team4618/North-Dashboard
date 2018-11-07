//NOTE: requires common.cpp & the north defs

enum RobotProfileCommandType {
   RobotProfileCommandType_NonBlocking,
   RobotProfileCommandType_Blocking,
   RobotProfileCommandType_Continuous
};

struct RobotProfileCommand {
   RobotProfileCommandType type;
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

struct RobotProfile {
   string name;
   v2 size;
   u32 subsystem_count;
   RobotProfileSubsystem *subsystems;
};

struct RobotProfileHelper {
   MemoryArena connected_arena;
   RobotProfile connected_robot;
   bool is_connected;

   MemoryArena loaded_arena;
   RobotProfile loaded_robot;

   RobotProfile *selected_profile;
};

void InitRobotProfileHelper(RobotProfileHelper *state, MemoryArena arena) {
   u32 size = arena.size/2;
   state->connected_arena = NewMemoryArena(PushSize(&arena, size), size);
   state->loaded_arena = NewMemoryArena(PushSize(&arena, size), size);
   state->selected_profile = NULL;
}

void EncodeProfileFile(RobotProfile *profile, buffer *file) {
   //TODO: do this, memory to file format
}

void UpdateConnectedProfile(RobotProfileHelper *state) {
   Assert(state->is_connected);

   buffer file = PushTempBuffer(Megabyte(1));
   EncodeProfileFile(&state->connected_robot, &file);
   WriteEntireFile(Concat(state->connected_robot.name, Literal(".ncrp")), file);
}

void RecieveWelcomePacket(RobotProfileHelper *state, buffer packet) {
   MemoryArena *arena = &state->connected_arena;
   RobotProfile *profile = &state->connected_robot;

   Reset(arena);
   Welcome_PacketHeader *header = ConsumeStruct(&packet, Welcome_PacketHeader);
   profile->name = PushCopy(arena, ConsumeString(&packet, header->robot_name_length));
   profile->size = V2(header->robot_width, header->robot_length);
   profile->subsystem_count = header->subsystem_count;
   profile->subsystems = PushArray(arena, RobotProfileSubsystem, profile->subsystem_count);

   for(u32 i = 0; i < header->subsystem_count; i++) {
      Welcome_SubsystemDescription *desc = ConsumeStruct(&packet, Welcome_SubsystemDescription);
      //TODO 
   }

   UpdateConnectedProfile(state);
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

void RecieveCurrentParametersPacket(RobotProfileHelper *state, buffer packet) {
   Assert(state->is_connected);
   MemoryArena *arena = &state->connected_arena;
   RobotProfile *profile = &state->connected_robot;

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

   UpdateConnectedProfile(state);
}

void NetworkTimeout(RobotProfileHelper *state) {
   state->is_connected = false;
   if(state->selected_profile == &state->connected_robot) {
      state->selected_profile = NULL;
   }
}

void ParseProfileFile(RobotProfileHelper *state, buffer file) {
   MemoryArena *arena = &state->loaded_arena;
   RobotProfile *profile = &state->loaded_robot;
   
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
         command->param_count = file_command->param_count;
         command->params = PushArray(arena, string, command->param_count);

         for(u32 k = 0; k < file_command->param_count; k++) {
            u8 length = *ConsumeStruct(&file, u8);
            command->params[k] = PushCopy(arena, ConsumeString(&file, length));
         }
      }
   }

   state->selected_profile = profile;
}