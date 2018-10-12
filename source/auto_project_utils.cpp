//NOTE: needs common & north_file_definitions
struct AutoCommand {
   string subsystem_name;
   string command_name;
   u32 param_count;
   f32 *params;
};

struct AutoPath;
struct AutoNode {
   v2 pos;
   
   u32 command_count;
   AutoCommand *commands;
   
   u32 path_count;
   AutoPath *out_paths;
};

struct AutoContinuousEventSample {
   f32 distance; 
   f32 value;
};

struct AutoContinuousEvent {
   string subsystem_name;
   string command_name;
   u32 sample_count;
   AutoContinuousEventSample *samples;
};

struct AutoDiscreteEvent {
   f32 distance;
   AutoCommand command;
};

struct AutoValue {
   bool is_variable;
   union {
      f32 value;
      string variable;
   };
};

struct AutoPath {
   AutoNode *in_node;
   AutoNode *out_node;

   AutoValue accel;
   AutoValue deccel;
   AutoValue max_vel;

   string conditional;

   u32 continuous_event_count;
   AutoContinuousEvent *continuous_events;

   u32 discrete_event_count;
   AutoDiscreteEvent *discrete_events;

   u32 control_point_count;
   v2 *control_points;
};

struct AutoVariable {
   string name;
   f32 value;
};

struct AutoProjectLink {
   AutoNode *starting_node;
   string name;

   u32 variable_count;
   AutoVariable *variables;

   AutoProjectLink *next;
};

AutoCommand CreateCommand(MemoryArena *arena, string subsystem_name, string command_name,
                           u32 param_count, f32 *params) 
{
   AutoCommand result = {};
   result.subsystem_name = PushCopy(arena, subsystem_name);
   result.command_name = PushCopy(arena, command_name);
   result.param_count = param_count;
   result.params = PushArray(arena, f32, param_count);
   Copy(params, sizeof(f32) * param_count, result.params);
   return result;
}

void ParseAutoPath(buffer *file, MemoryArena *arena, AutoPath *path);
AutoNode *ParseAutoNode(buffer *file, MemoryArena *arena) {
   AutoNode *result = PushStruct(arena, AutoNode);
   AutonomousProgram_Node *file_node = ConsumeStruct(file, AutonomousProgram_Node);
   
   result->pos = file_node->pos;
   result->path_count = file_node->path_count;
   result->out_paths = PushArray(arena, AutoPath, file_node->path_count);
   result->command_count = file_node->command_count;
   result->commands = PushArray(arena, AutoCommand, result->command_count);

   for(u32 i = 0; i < file_node->command_count; i++) {
      AutonomousProgram_Command *file_command = ConsumeStruct(file, AutonomousProgram_Command);
      string subsystem_name = ConsumeString(file, file_command->subsystem_name_length);
      string command_name = ConsumeString(file, file_command->command_name_length);
      f32 *params = ConsumeArray(file, f32, file_command->parameter_count);
      result->commands[i] = CreateCommand(arena, subsystem_name, command_name, file_command->parameter_count, params);
   }
   
   for(u32 i = 0; i < file_node->path_count; i++) {
      AutoPath *path = result->out_paths + i;
      ParseAutoPath(file, arena, path);
      path->in_node = result;
   }

   return result;
}

AutoValue ParseAutoValue(buffer *file, MemoryArena *arena) {
   AutonomousProgram_Value *file_value = ConsumeStruct(file, AutonomousProgram_Value);
   AutoValue result = {};
   result.is_variable = file_value->is_variable;
   if(file_value->is_variable) {
      u8 *variable_length = ConsumeStruct(file, u8);
      result.variable = PushCopy(arena, ConsumeString(file, *variable_length));
   } else {
      result.value = *ConsumeStruct(file, f32);
   }
   return result;
}

void ParseAutoPath(buffer *file, MemoryArena *arena, AutoPath *path) {
   AutonomousProgram_Path *file_path = ConsumeStruct(file, AutonomousProgram_Path);

   path->accel = ParseAutoValue(file, arena);
   path->deccel = ParseAutoValue(file, arena);
   path->max_vel = ParseAutoValue(file, arena);
   
   if(file_path->conditional_length == 0) {
      path->conditional = EMPTY_STRING;
   } else {
      path->conditional =  PushCopy(arena, ConsumeString(file, file_path->conditional_length));
   }

   path->control_point_count = file_path->control_point_count;
   v2 *control_points = ConsumeArray(file, v2, file_path->control_point_count);
   path->control_points = (v2 *) PushCopy(arena, control_points, file_path->control_point_count * sizeof(v2));

   path->continuous_event_count = file_path->continuous_event_count;
   path->continuous_events = PushArray(arena, AutoContinuousEvent, path->continuous_event_count);
   for(u32 i = 0; i < file_path->continuous_event_count; i++) {
      AutonomousProgram_ContinuousEvent *file_event = ConsumeStruct(file, AutonomousProgram_ContinuousEvent);
      AutoContinuousEvent *event = path->continuous_events + i;

      event->subsystem_name = PushCopy(arena, ConsumeString(file, file_event->subsystem_name_length));
      event->command_name = PushCopy(arena, ConsumeString(file, file_event->command_name_length));
      event->sample_count = file_event->datapoint_count;
      event->samples = PushArray(arena, AutoContinuousEventSample, event->sample_count);
      AutonomousProgram_DataPoint *data_points = ConsumeArray(file, AutonomousProgram_DataPoint, file_event->datapoint_count);
      for(u32 j = 0; j < event->sample_count; j++) {
         event->samples[j].distance = data_points[j].distance;
         event->samples[j].value = data_points[j].value;
      }
   }

   path->discrete_event_count = file_path->discrete_event_count;
   path->discrete_events = PushArray(arena, AutoDiscreteEvent, path->discrete_event_count);
   for(u32 i = 0; i < file_path->discrete_event_count; i++) {
      AutonomousProgram_DiscreteEvent *file_event = ConsumeStruct(file, AutonomousProgram_DiscreteEvent);
      AutoDiscreteEvent *event = path->discrete_events + i;
      
      string subsystem_name = ConsumeString(file, file_event->subsystem_name_length);
      string command_name = ConsumeString(file, file_event->command_name_length);
      f32 *params = ConsumeArray(file, f32, file_event->parameter_count);
      
      event->command = CreateCommand(arena, subsystem_name, command_name, 
                                     file_event->parameter_count, params);
      event->distance = file_event->distance;
   }

   path->out_node = ParseAutoNode(file, arena);
}

AutoProjectLink *ReadAutoProject(string file_name, MemoryArena *arena) {
   buffer file = ReadEntireFile(Concat(file_name, Literal(".ncap")));
   if(file.data != NULL) {
      FileHeader *file_numbers = ConsumeStruct(&file, FileHeader);
      AutonomousProgram_FileHeader *header = ConsumeStruct(&file, AutonomousProgram_FileHeader);

      AutoVariable *vars = PushArray(arena, AutoVariable, header->variable_count);
      for(u32 i = 0; i < header->variable_count; i++) {
         AutonomousProgram_Variable *file_var = ConsumeStruct(&file, AutonomousProgram_Variable);
         AutoVariable *var = vars + i;
         var->name = PushCopy(arena, ConsumeString(&file, file_var->name_length));
         var->value = file_var->value;
      }

      AutoProjectLink *result = PushStruct(arena, AutoProjectLink);
      result->name = PushCopy(arena, file_name);
      result->variable_count = header->variable_count;
      result->variables = vars;
      result->starting_node = ParseAutoNode(&file, arena);
      return result;
   }

   return NULL;
}