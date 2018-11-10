void ResetDashboardState(DashboardState *state) {
   //TODO: Redo, this probably isnt correct anymore
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
   /**
   TODO: Dashboard codepath
   Welcome_PacketHeader *header = ConsumeStruct(packet, Welcome_PacketHeader);
   string robot_name = ConsumeString(packet, header->robot_name_length);

   ResetDashboardState(state);
   state->robot.connected = true;
   state->robot.name = PushCopy(&state->state_arena, robot_name);
   state->robot.size = V2(header->robot_width, header->robot_length);

   state->robot.subsystems = PushArray(&state->state_arena, ConnectedSubsystem, header->subsystem_count);
   state->robot.subsystem_count = header->subsystem_count;

   for(u32 i = 0; i < header->subsystem_count; i++) {
      Welcome_SubsystemDescription *desc = ConsumeStruct(packet, Welcome_SubsystemDescription);
      string subystem_name = ConsumeString(packet, desc->name_length);
      
      ConnectedSubsystem *subsystem = state->robot.subsystems + i;
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
   **/

   RecieveWelcomePacket(&state->robot_profile_helper, *packet);
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   /**
   TODO: Dashboard codepath
   CurrentParameters_PacketHeader *header = ConsumeStruct(packet, CurrentParameters_PacketHeader);

   for(u32 i = 0; i < header->subsystem_count; i++) {
      CurrentParameters_SubsystemParameters *params = ConsumeStruct(packet, CurrentParameters_SubsystemParameters);
      string subystem_name = ConsumeString(packet, params->name_length);

      for(u32 j = 0; j < params->param_count; j++) {
         CurrentParameters_Parameter *param = ConsumeStruct(packet, CurrentParameters_Parameter);
         string param_name = ConsumeString(packet, param->name_length);
      }
   }
   */

   RecieveCurrentParametersPacket(&state->robot_profile_helper, *packet);
}

void PacketHandler_State(buffer *packet, DashboardState *state) {
   //TODO: Dashboard codepath
   RecieveStatePacket(&state->auto_recorder, *packet);
   RecieveStatePacket(&state->manual_recorder, *packet);
}

void PacketHandler_CurrentAutoPath(buffer *packet, DashboardState *state) {
   //TODO: Dashboard codepath
}

bool HandlePacket(DashboardState *state, PacketType::type type, buffer packet) {
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

/**
TODO: 
   -redo networking
   -we're split between TCP & UDP now
   -no more ping packets
   -packet types are now in a seperate header (the dispatcher will consume the header)
**/