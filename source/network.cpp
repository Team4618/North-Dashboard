void PacketHandler_Welcome(buffer *packet, DashboardState *state) {
   //TODO: Dashboard codepath
   RecieveWelcomePacket(&state->robot_profile_helper, *packet);
}

void PacketHandler_CurrentParameters(buffer *packet, DashboardState *state) {
   //TODO: Dashboard codepath
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

void HandlePacket(DashboardState *state, PacketType::type type, buffer packet) {
   if(type == PacketType::Welcome) {
      PacketHandler_Welcome(&packet, state);
   } else if(type == PacketType::CurrentParameters) {
      PacketHandler_CurrentParameters(&packet, state);
   } else if(type == PacketType::State) {
      PacketHandler_State(&packet, state);
   } else if(type == PacketType::CurrentAutoPath) {
      PacketHandler_CurrentAutoPath(&packet, state);
   }
}

void HandleDisconnect(DashboardState *state) {
   state->robot.connected = false;
   NetworkTimeout(&state->robot_profile_helper);

   if(state->auto_recorder.recording)
      EndRecording(&state->auto_recorder, Literal("auto_recording"));

   if(state->manual_recorder.recording)
      EndRecording(&state->manual_recorder, Literal("manual_recording"));
}