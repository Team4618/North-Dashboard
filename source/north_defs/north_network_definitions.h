#pragma pack(push, 1)

//NOTE: Everything is sent over TCP socket 5800 

struct PacketHeader {
   //NOTE: this lets packets be really big, our practical limit is much smaller than the 4 gb limit
   //NOTE: this size excludes the 5 byte header
   u32 size;
   u8 type; 
};

namespace PacketType {
   enum type {
      Heartbeat = 0,             // <->
      SetConnectionFlags = 1,    //  ->
      Welcome = 2,               // <-
      CurrentParameters = 3,     // <-
      State = 4,                 // <-
      ParameterOp = 5,           //  ->
      SetState = 6,              //  ->
      UploadAutonomous = 7,      //  ->
      //NOTE: if we change a packet just make a new type instead 
      //eg. "Welcome" becomes "Welcome_V1" & we create "Welcome_V2"
   };
};

//------------------------------------------
namespace SetConnectionFlags_Flags {
   enum type {
      WANTS_STATE = (1 << 0),
   };
};

struct SetConnectionFlags_PacketHeader {
   u8 flags;
};
//------------------------------------------

//------------------------------------------
//NOTE: everything in Welcome can only change if the robot was rebooted
//      (eg. commands)

struct Welcome_Command {
   u8 name_length;
   u8 param_count;
   u8 type; //NOTE: North_CommandExecutionType
   //char name[name_length]
   //{ u8 length; char [length]; } [param_count]
};

//NOTE: unused
/*
namespace Welcome_Flags {
   enum type {

   };
};
*/

struct Welcome_PacketHeader {
   u8 robot_name_length;
   u8 conditional_count;
   u8 command_count;
   f32 robot_width;
   f32 robot_length;
   u32 flags;
   //char name[robot_name_length]
   //{ u8 length; char [length]; } [conditional_count]
   //Welcome_Command [command_count]
};
//-----------------------------------------

//-----------------------------------------
struct CurrentParameters_Parameter {
   u8 is_array;
   u8 name_length;
   u8 value_count; //Ignored if is_array is false
   //char name[name_length]
   //f32 [value_count]
};

struct CurrentParameters_PacketHeader {
   u16 parameter_count;
   // CurrentParameters_Parameter [parameter_count]
};
//-----------------------------------------

//-----------------------------------------
struct Vis_VertexData {
   u32 point_count; //NOTE: must be a multiple of 3 if drawing triangles
   v4 colour;

   //v2 points[point_count]
};

struct Vis_RobotPose {
   v2 pos;
   f32 angle;
};

struct Vis_Diagnostic {
   f32 value;
   u8 unit_name_length;

   //char unit_name[unit_name_length]
};

struct Vis_Message {
   u16 text_length;
   //char text[text_length]
};

struct Vis_Entry {
   u8 type; //North_VisType
   u16 name_length;

   u32 size; //size of the following block of data (not including the name)
   //char name[name_length]
   //Vis_[type]   
};

struct VisHeader {
   u64 frame_number; //so we know when to get rid of stuff
   f32 time;

   u32 entry_count;
   //Vis_Entry [entry_count]
};
//-----------------------------------------

//-----------------------------------------
namespace ParameterOp_Type {
   enum type {
      SetValue = 1, //NOTE: for is_array = false
      AddValue = 2, //NOTE: for is_array = true
      RemoveValue = 3, //NOTE: for is_array = true
   };
};

struct ParameterOp_PacketHeader {
   u8 type;
   u8 param_name_length;

   f32 value; //NOTE: only used by SetValue & AddValue
   u32 index; //NOTE: ignored if is_array = false
   
   //char [param_name_length]
};
//-----------------------------------------

struct SetState_PacketHeader {
   v2 pos;
   f32 angle;
};

//-------------------------------------
struct UploadAutonomous_PacketHeader {
   f32 starting_angle;
   //AutonomousProgram_Node begining_node //NOTE: AutonomousProgram_Node is in north_file_definitions.h
};

#pragma pack(pop)