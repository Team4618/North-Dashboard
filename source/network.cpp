#include "north/north_network_definitions.h"

void ResetDashboardState(DashboardState *state) {
   Reset(&state->state_arena);

   if(state->page == DashboardPage_Subsystem)
      state->page = DashboardPage_Home;
   state->selected_subsystem = NULL;
   
   state->robot.connected = false;
   state->robot.subsystems = NULL;
   state->robot.subsystem_count = 0;
   state->robot.name = EMPTY_STRING;
}

void PacketHandler_Welcome(buffer *packet, DashboardState *state) {
   Welcome_PacketHeader *header = ConsumeStruct(packet, Welcome_PacketHeader);
   string robot_name = ConsumeString(packet, header->robot_name_length);

   ResetDashboardState(state);
   state->robot.connected = true;
   state->robot.name = PushCopy(&state->state_arena, robot_name);
   state->robot.size = V2(header->robot_width, header->robot_height);

   state->robot.subsystems = PushArray(&state->state_arena, Subsystem, header->subsystem_count);
   state->robot.subsystem_count = header->subsystem_count;

   for(u32 i = 0; i < header->subsystem_count; i++) {
      Welcome_SubsystemDescription *desc = ConsumeStruct(packet, Welcome_SubsystemDescription);
      string subystem_name = ConsumeString(packet, desc->name_length);
      
      Subsystem *subsystem = state->robot.subsystems + i;
      subsystem->name = PushCopy(&state->state_arena, subystem_name);
      u32 diagnostics_graph_arena_size = Megabyte(2);
      MemoryArena diagnostics_graph_arena = NewMemoryArena(PushSize(&state->state_arena, diagnostics_graph_arena_size), 
                                                                    diagnostics_graph_arena_size); 
      subsystem->diagnostics_graph = NewMultiLineGraph(diagnostics_graph_arena);
      SetDiagnosticsGraphUnits(&subsystem->diagnostics_graph);

      //TODO: save commands to state
      for(u32 j = 0; j < desc->command_count; j++) {
         Welcome_SubsystemCommand *command = ConsumeStruct(packet, Welcome_SubsystemCommand);
         string command_name = ConsumeString(packet, command->name_length);

         for(u32 k = 0; k < command->param_count; k++) {
            u8 param_name_length = *ConsumeStruct(packet, u8);
            string param_name = ConsumeString(packet, param_name_length);
         }
      }
   }
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   CurrentParameters_PacketHeader *header = ConsumeStruct(packet, CurrentParameters_PacketHeader);

   for(u32 i = 0; i < header->subsystem_count; i++) {
      SubsystemParameters *params = ConsumeStruct(packet, SubsystemParameters);
      string subystem_name = ConsumeString(packet, params->name_length);

      for(u32 j = 0; j < params->param_count; j++) {
         Parameter *param = ConsumeStruct(packet, Parameter);
         string param_name = ConsumeString(packet, param->name_length);
      }
   }
}

void PacketHandler_State(buffer *packet, DashboardState *state) {

}

void PacketHandler_CurrentAutoPath(buffer *packet, DashboardState *state) {
   
}

bool HandlePacket(buffer packet, DashboardState *state) {
   u8 type = *PeekStruct(&packet, u8);
   
   state->last_recieve_time = state->curr_time;
   
   if(type == PacketType::Welcome) {
      PacketHandler_Welcome(&packet, state);
      return true;
   } else if(type == PacketType::CurrentParameters) {
      PacketHandler_CurrentParameters(&packet, state);
   } else if(type == PacketType::State) {
      PacketHandler_State(&packet, state);
   } else if(type == PacketType::CurrentAutoPath) {
      PacketHandler_CurrentAutoPath(&packet, state);
   }

   return false;
}

//TODO: 5 seconds is a really long time
#define DISCONNECT_TIMEOUT 5
#define PING_THRESHOLD 5

void HandleConnectionStatus(DashboardState *state) {
   if(state->robot.connected) {
      if((state->curr_time - state->last_recieve_time) > DISCONNECT_TIMEOUT) {
         state->robot.connected = false;
         ResetDashboardState(state);
      }
   }
   
   if(!state->robot.connected) {
      //attempt to connect
   }

   if((state->curr_time - state->last_send_time) > PING_THRESHOLD) {
      //send ping
   }
}