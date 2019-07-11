//-------------------------PARSERS-------------------------
void parseVertexData(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_VertexData *vdata = ConsumeStruct(file, RobotRecording_VertexData);
   v2 *points = ConsumeArray(file, v2, vdata->point_count);

   if(counting) {
      recording->vdata.vdata_count++;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->vdata.vdata == NULL) {
         recording->vdata.vdata = PushArray(arena, VertexDataRecording, recording->vdata.vdata_count);
         recording->vdata.vdata_count = 0;
      }

      VertexDataRecording *vdata_rec = recording->vdata.vdata + recording->vdata.vdata_count++;
      vdata_rec->point_count = vdata->point_count;
      vdata_rec->points = PushArrayCopy(arena, v2, points, vdata->point_count);
      vdata_rec->colour = vdata->colour;
      vdata_rec->begin_time = vdata->begin_time;
      vdata_rec->end_time = vdata->end_time;

      state->min_time = Min(state->min_time, vdata->begin_time);
      state->max_time = Max(state->max_time, vdata->end_time);
   }
}

void parseRobotPose(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_RobotPose *pose_data = ConsumeStruct(file, RobotRecording_RobotPose);
   RobotRecording_RobotStateSample *samples = ConsumeArray(file, RobotRecording_RobotStateSample, pose_data->sample_count);

   if(counting) {
      recording->pose.sample_count += pose_data->sample_count;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->pose.samples == NULL) {
         recording->pose.samples = PushArray(arena, RobotRecording_RobotStateSample, recording->pose.sample_count);
         recording->pose.sample_count = 0;
      }

      Copy(samples, pose_data->sample_count * sizeof(RobotRecording_RobotStateSample), 
           recording->pose.samples + recording->pose.sample_count);

      recording->pose.sample_count += pose_data->sample_count;

      for(u32 i = 0; i < pose_data->sample_count; i++) {
         state->min_time = Min(state->min_time, samples[i].time);
         state->max_time = Max(state->max_time, samples[i].time);
      }
   }
}

void parseDiagnostic(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_Diagnostic *diag_data = ConsumeStruct(file, RobotRecording_Diagnostic);
   string unit_name = ConsumeString(file, diag_data->unit_length);
   RobotRecording_DiagnosticSample *samples = ConsumeArray(file, RobotRecording_DiagnosticSample, diag_data->sample_count);

   if(counting) {
      recording->diag.sample_count += diag_data->sample_count;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->diag.samples == NULL) {
         recording->diag.unit = PushCopy(arena, unit_name);
         recording->diag.samples = PushArray(arena, RobotRecording_DiagnosticSample, recording->diag.sample_count);
         recording->diag.sample_count = 0;
      }

      Copy(samples, diag_data->sample_count * sizeof(RobotRecording_DiagnosticSample), 
           recording->diag.samples + recording->diag.sample_count);

      recording->diag.sample_count += diag_data->sample_count;

      for(u32 i = 0; i < diag_data->sample_count; i++) {
         state->min_time = Min(state->min_time, samples[i].time);
         state->max_time = Max(state->max_time, samples[i].time);
      }
   }
}

void parseMessage(LoadedRobotRecording *state, LoadedRecording *recording, buffer *file, bool counting) {
   RobotRecording_Message *msg = ConsumeStruct(file, RobotRecording_Message);
   string text = ConsumeString(file, msg->text_length);

   if(counting) {
      recording->msg.message_count++;
   } else {
      MemoryArena *arena = state->arena;
      if(recording->msg.messages == NULL) {
         recording->msg.messages = PushArray(arena, MessageRecording, recording->msg.message_count);
         recording->msg.message_count = 0;
      }

      MessageRecording *msg_rec = recording->msg.messages + recording->msg.message_count++;
      msg_rec->text = PushCopy(arena, text);
      msg_rec->begin_time = msg->begin_time;
      msg_rec->end_time = msg->end_time;

      state->min_time = Min(state->min_time, msg->begin_time);
      state->max_time = Max(state->max_time, msg->end_time);
   }
}

entry_parser_callback entry_parsers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   parseVertexData, //Polyline = 1
   parseVertexData, //Triangles = 2
   parseVertexData, //Points = 3
   parseRobotPose, //RobotPose = 4
   parseDiagnostic, //Diagnostic = 5
   parseMessage, //Message = 6
};
//---------------------------------------------------------

//------------------------WRITERS--------------------------
u32 WriteVertexData(buffer *chunk, RecorderEntry *entry) {
   Assert((entry->type == North_VisType::Polyline) ||
          (entry->type == North_VisType::Triangles) ||
          (entry->type == North_VisType::Points));

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderVertexData *vdata = entry->curr_vdata; vdata; vdata = vdata->next) {
      RobotRecording_VertexData file_vdata = {};
      file_vdata.point_count = vdata->point_count;
      file_vdata.colour = vdata->colour;
      file_vdata.begin_time = vdata->begin_time;
      file_vdata.end_time = vdata->end_time;
      WriteStruct(chunk, &file_vdata);
      WriteArray(chunk, vdata->points, vdata->point_count);
      
      recording_count++;
   }

   entry->curr_vdata = NULL;
   return recording_count;
}

u32 WriteRobotPose(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::RobotPose);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderPoses *poses = entry->curr_poses; poses; poses = poses->next) {
      RobotRecording_RobotPose file_poses = {};
      file_poses.sample_count = poses->sample_count;
      WriteStruct(chunk, &file_poses);
      WriteArray(chunk, poses->samples, poses->sample_count);
      
      recording_count++;
   }

   entry->curr_poses = NULL;
   return recording_count;
}

u32 WriteDiagnostics(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::Diagnostic);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderDiagnostic *diags = entry->curr_diag; diags; diags = diags->next) {
      RobotRecording_Diagnostic file_diags = {};
      file_diags.unit_length = diags->unit.length;
      file_diags.sample_count = diags->sample_count;
      WriteStruct(chunk, &file_diags);
      WriteString(chunk, diags->unit);
      WriteArray(chunk, diags->samples, diags->sample_count);
      
      recording_count++;
   }

   entry->curr_diag = NULL;
   return recording_count;
}

u32 WriteMessage(buffer *chunk, RecorderEntry *entry) {
   Assert(entry->type == North_VisType::Message);

   //TODO: this writes these in reverse chronological order
   u32 recording_count = 0;
   for(RecorderMessage *msg = entry->curr_msg; msg; msg = msg->next) {
      RobotRecording_Message file_msg = {};
      file_msg.text_length = msg->text.length;
      file_msg.begin_time = msg->begin_time;
      file_msg.end_time = msg->end_time;
      WriteStruct(chunk, &file_msg);
      WriteString(chunk, msg->text);
      
      recording_count++;
   }

   entry->curr_msg = NULL;
   return recording_count;
}

entry_writer_callback entry_writers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   WriteVertexData, //Polyline = 1
   WriteVertexData, //Triangles = 2
   WriteVertexData, //Points = 3
   WriteRobotPose, //RobotPose = 4
   WriteDiagnostics, //Diagnostic = 5
   WriteMessage, //Message = 6
};
//---------------------------------------------------------

//------------------------RECIEVERS------------------------
void RecieveVertexData(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_VertexData *vdata = ConsumeStruct(packet, Vis_VertexData);
   v2 *points = ConsumeArray(packet, v2, vdata->point_count);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, true);
   bool equivalent = true; //TODO: Compare hashes instead of comparing each point
   if(entry->curr_vdata != NULL) {
      equivalent = (vdata->colour == entry->curr_vdata->colour) && 
                   (vdata->point_count == entry->curr_vdata->point_count);

      if(equivalent) {
         for(u32 i = 0; i < vdata->point_count; i++) {
            if(points[i] != entry->curr_vdata->points[i]) {
               equivalent = false;
               break;
            }
         }
      }
   }

   if((entry->curr_vdata == NULL) || !equivalent || entry->ended_recording) {
      if(entry->curr_vdata != NULL)
         entry->curr_vdata->end_time = curr_time;
      
      RecorderVertexData *new_vdata = PushStruct(arena, RecorderVertexData);
      new_vdata->point_count = vdata->point_count;
      new_vdata->points = PushArrayCopy(arena, v2, points, vdata->point_count);
      new_vdata->colour = vdata->colour;
      new_vdata->begin_time = curr_time;
      new_vdata->end_time = curr_time;

      new_vdata->next = entry->curr_vdata;
      entry->curr_vdata = new_vdata;
      entry->ended_recording = false;
   } else if(equivalent) {
      entry->curr_vdata->end_time = curr_time;
   }
}

void RecieveVisRobotPose(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_RobotPose *pose = ConsumeStruct(packet, Vis_RobotPose);
   
   RecorderEntry *entry = GetOrCreateEntry(state, name, type, false);
   if((entry->curr_poses == NULL) || (entry->curr_poses->sample_count == ArraySize(entry->curr_poses->samples))) {
      RecorderPoses *new_poses = PushStruct(arena, RecorderPoses);
      new_poses->next = entry->curr_poses;
      entry->curr_poses = new_poses;
   }

   entry->curr_poses->samples[entry->curr_poses->sample_count].time = curr_time;
   entry->curr_poses->samples[entry->curr_poses->sample_count].pos = pose->pos;
   entry->curr_poses->samples[entry->curr_poses->sample_count].angle = pose->angle;
   entry->curr_poses->sample_count++;
}

void RecieveVisDiagnostic(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_Diagnostic *diag = ConsumeStruct(packet, Vis_Diagnostic);
   string unit_name = ConsumeString(packet, diag->unit_name_length);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, false);
   if((entry->curr_diag == NULL) || (entry->curr_diag->sample_count == ArraySize(entry->curr_diag->samples))) {
      RecorderDiagnostic *new_diag = PushStruct(arena, RecorderDiagnostic);
      new_diag->unit = PushCopy(arena, unit_name);

      new_diag->next = entry->curr_diag;
      entry->curr_diag = new_diag;
   }

   entry->curr_diag->samples[entry->curr_diag->sample_count].time = curr_time;
   entry->curr_diag->samples[entry->curr_diag->sample_count].value = diag->value;
   entry->curr_diag->sample_count++;
}

void RecieveVisMessage(buffer *packet, RobotRecorder *state, string name, f32 curr_time, North_VisType::type type) {
   MemoryArena *arena = state->arena;
   Vis_Message *msg = ConsumeStruct(packet, Vis_Message);
   string text = ConsumeString(packet, msg->text_length);

   RecorderEntry *entry = GetOrCreateEntry(state, name, type, true/*time based*/);
   if((entry->curr_msg == NULL) || (entry->curr_msg->text != text) || entry->ended_recording) {
      if(entry->curr_msg != NULL)
         entry->curr_msg->end_time = curr_time;
      
      RecorderMessage *new_recording = PushStruct(arena, RecorderMessage);
      new_recording->text = PushCopy(arena, text);
      new_recording->begin_time = curr_time;
      new_recording->end_time = curr_time;

      new_recording->next = entry->curr_msg;
      entry->curr_msg = new_recording;
      entry->ended_recording = false;
   } else if(entry->curr_msg->text == text) {
      entry->curr_msg->end_time = curr_time;
   }
}

entry_reciever_callback entry_recievers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   RecieveVertexData, //Polyline = 1
   RecieveVertexData, //Triangles = 2
   RecieveVertexData, //Points = 3
   RecieveVisRobotPose, //RobotPose = 4
   RecieveVisDiagnostic, //Diagnostic = 5
   RecieveVisMessage, //Message = 6
};
//---------------------------------------------------------

#ifdef INCLUDE_DRAWRECORDINGS
//-----------------DRAWERS------------------
void DrawPointsRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }

   if(!rec->hidden) {
      for(u32 i = 0; i < rec->vdata.vdata_count; i++) {
         VertexDataRecording *vdata = rec->vdata.vdata + i;
         if((vdata->begin_time <= state->curr_time) && (state->curr_time <= vdata->end_time)) {
            //Replace this with one call that takes the points & a transform matrix
            for(u32 j = 0; j < vdata->point_count; j++) {
               v2 p = GetPoint(field, vdata->points[j]);
               Rectangle(field->e, RectCenterSize(p, V2(5, 5)), vdata->colour);
            }
            //------------------------------
         }
      }
   }
}

void DrawPolylineRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }

   for(u32 i = 0; i < rec->vdata.vdata_count; i++) {
      VertexDataRecording *vdata = rec->vdata.vdata + i;
      if((vdata->begin_time <= state->curr_time) && (state->curr_time <= vdata->end_time)) {
         
         v2 *transformed_points = PushTempArray(v2, vdata->point_count);
         for(u32 j = 0; j < vdata->point_count; j++) {
            transformed_points[j] = GetPoint(field, vdata->points[j]);
         }
         
         _Line(field->e, vdata->colour, 3, transformed_points, vdata->point_count);
      }
   }
}

void DrawRobotPoseRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
   }
   
   for(u32 i = 0; i < rec->pose.sample_count; i++) {
      RobotRecording_RobotStateSample sample = rec->pose.samples[i];
      if(sample.time >= state->curr_time) {
         DrawRobot(field, V2(0.5, 0.5), sample.pos, sample.angle, BLACK);
         break;
      }
   }
}

void DrawMessageRecording(element *page, ui_field_topdown *field, LoadedRobotRecording *state, LoadedRecording *rec) {
   if(page) {
      Label(page, rec->name, 20, BLACK);
      
      for(u32 i = 0; i < rec->msg.message_count; i++) {
         MessageRecording *msg = rec->msg.messages + i;
         if((msg->begin_time <= state->curr_time) && (state->curr_time <= msg->end_time)) {
            Label(page, msg->text, 20, BLACK, V2(20, 0));
         }
      }
   }
}

recording_draw_callback recording_drawers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   DrawPolylineRecording, //Polyline = 1
   NULL, //Triangles = 2
   DrawPointsRecording, //Points = 3
   DrawRobotPoseRecording, //RobotPose = 4
   NULL, //Diagnostic = 5
   DrawMessageRecording, //Message = 6
};
//-----------------------------------
#endif