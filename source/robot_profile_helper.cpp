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
   MemoryArena arena;
   bool is_connected;
   RobotProfile connected_robot;
   RobotProfile loaded_robot;

   RobotProfile *selected_profile;
};

void InitRobotProfileHelper(RobotProfileHelper *state, MemoryArena arena) {
   state->arena = arena;

}

void RecieveWelcomePacket(RobotProfileHelper *state, buffer packet) {

}

void NetworkTimeout(RobotProfileHelper *state) {

}

void ParseProfileFile(RobotProfileHelper *state, buffer file) {

}