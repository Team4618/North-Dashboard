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

void RecieveWelcomePacket(RobotProfileHelper *state, buffer packet) {

}

void NetworkTimeout(RobotProfileHelper *state) {

}

void ParseProfileFile(RobotProfileHelper *state, buffer file) {
   MemoryArena *arena = &state->loaded_arena;
   RobotProfile *profile = &state->loaded_robot;
   
   Reset(arena);
   FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);
   RobotProfile_FileHeader *header = ConsumeStruct(&file, RobotProfile_FileHeader);
   profile->name = PushCopy(arena, ConsumeString(&file, header->robot_name_length));
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