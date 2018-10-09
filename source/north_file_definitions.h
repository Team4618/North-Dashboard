#pragma pack(push, 1)

struct FileHeader {
   u32 magic_number;
   u32 version_number;
};

FileHeader header(u32 magic_number, u32 version_number) {
   FileHeader result = { magic_number, version_number };
   return result;
}

//-----------------------------------------------------------
//NOTE: unused right now
/*
namespace SettingFlags {
   enum type {

   };
};
*/

struct Settings_FileHeader {
#define SETTINGS_MAGIC_NUMBER RIFF_CODE("NCSF") 
#define SETTINGS_CURR_VERSION 0
   u16 team_number;
   u8 field_name_length;
   u32 flags;
   //char field_name[field_name_length]
};
//-----------------------------------------------------------

//-----------------------------------------------------------

struct Field_StartingPosition {
   v2 pos;
   f32 angle;
};

struct Field_FileHeader {
#define FIELD_MAGIC_NUMBER RIFF_CODE("NCFF") 
#define FIELD_CURR_VERSION 0
   f32 width; //in ft
   f32 height; //in ft
   u32 flags;
   u8 starting_position_count;
   u16 image_width;
   u16 image_height;
   //Field_StartingPosition [starting_position_count]
   //u32 field_image [image_width * image_height] RGBA encoded
};
//-----------------------------------------------------------

//-----------------------------------------------------------
namespace RobotProfile_CommandType {
   enum type {
      NonBlocking = 0,
      Blocking = 1,
      Continuous = 2,
   };
};

struct RobotProfile_SubsystemCommand {
   u8 name_length;
   u8 param_count;
   u8 type;
   //char name[name_length]
   //{ u8 length; char [length]; } [param_count]
};

struct RobotProfile_SubsystemDescription {
   u8 name_length;
   u8 command_count;
   //char name[name_length]
   //RobotProfile_SubsystemCommand [command_count]
};

//NOTE: unused
/*
namespace RobotProfile_Flags {
   enum type {

   };
};
*/

struct RobotProfile_FileHeader {
#define ROBOT_PROFILE_MAGIC_NUMBER RIFF_CODE("NCRP") 
#define ROBOT_PROFILE_CURR_VERSION 0
   u8 robot_name_length;
   u8 subsystem_count;
   f32 robot_width;
   f32 robot_height;
   u32 flags;
   //char robot_name[robot_name_length]
   //RobotProfile_SubsystemDescription [subsystem_count]
};
//-----------------------------------------------------------

//-----------------------------------------------------------
struct AutonomousRun_SubsystemDiagnostics {
   u8 name_length;
   u8 diagnostic_count;
   //char name[name_length]
   //AutonomousRun_Diagnostic [diagnostic_count]
};

struct AutonomousRun_Diagnostic {
   enum unit_type {
      Unitless = 0,
      Feet = 1,
      FeetPerSecond = 2,
      Degrees = 3,
      DegreesPerSecond = 4,
      Seconds = 5,
      Percent = 6,
      Amp = 7,
      Volt = 8,
   };

   u8 name_length;
   u8 unit;
   u32 sample_count;
   //char name[name_length]
   //AutonomousRun_DiagnosticSample [sample_count]
};

struct AutonomousRun_DiagnosticSample {
   f32 value;
   f32 time;
};

struct AutonomousRun_RobotStateSample {
   v2 pos;
   v2 vel;
   f32 angle;
   f32 time;
};

struct AutonomousRun_FileHeader {
#define AUTONOMOUS_RUN_MAGIC_NUMBER RIFF_CODE("NCAR") 
#define AUTONOMOUS_RUN_CURR_VERSION 0
   u64 timestamp;
   u8 robot_name_length;
   u8 subsystem_count;
   u32 robot_state_sample_count;
   //char [robot_name_length]
   //AutonomousRun_RobotStateSample [robot_state_sample_count]
   //AutonomousRun_SubsystemDiagnostics [subsystem_count]
};
//-----------------------------------------------------------

//-----------------------------------------------------------
struct AutonomousProgram_FileHeader {
#define AUTONOMOUS_PROGRAM_MAGIC_NUMBER RIFF_CODE("NCAP") 
#define AUTONOMOUS_PROGRAM_CURR_VERSION 0
};
//-----------------------------------------------------------

#pragma pack(pop)