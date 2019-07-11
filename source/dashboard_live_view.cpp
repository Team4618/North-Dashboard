struct ConnectedEntry {
   ConnectedEntry *next;
   North_VisType::type type;
   string name;

   bool has_data;

   union {
      string msg;
      
      struct {
         v4 colour;
         v2 *points;
         u32 point_count;
      } vdata;

      RobotRecording_RobotStateSample pose;
      RobotRecording_DiagnosticSample diag;
   };
};

struct ConnectedNamespace {
   ConnectedNamespace *next;
   string name;

   bool collapsed;

   ConnectedNamespace *first_child;
   ConnectedEntry *first_entry;
};

struct ConnectedLiveGraph {
   ConnectedLiveGraph *next;

   MultiLineGraphData graph;
   // u32 selected_entry_count;
   //TODO: array of strings?
};

struct ConnectedState {
   //NOTE: both owned by ConnectedState
   MemoryArena *persistent_arena;
   MemoryArena *frame_arena; //NOTE: gets reset every time we recieve a state packet with a new frame number

   u32 curr_frame;

   ConnectedNamespace root_namespace;

   //TODO: graphs
   // ConnectedLiveGraph *first_graph;
};

void ResetConnectedState(ConnectedState *state) {
   Reset(state->persistent_arena);
   Reset(state->frame_arena);

   state->curr_frame = 0;
   ZeroStruct(&state->root_namespace);
}

ConnectedEntry *GetOrCreateEntry(ConnectedState *state, string name, bool create = true) {
   MemoryArena *arena = state->persistent_arena;
   SplitString path = split(name, '/');
   Assert(path.part_count > 0);

   ConnectedNamespace *curr_namespace = &state->root_namespace;
   for(u32 i = 0; i < path.part_count - 1; i++) {
      string ns_name = path.parts[i];
      bool found = false;
      
      for(ConnectedNamespace *ns = curr_namespace->first_child; ns; ns = ns->next) {
         if(ns->name == ns_name) {
            curr_namespace = ns;
            found = true;
            break;
         }
      }

      if(!found) {
         if(!create)
            return NULL;

         ConnectedNamespace *new_ns = PushStruct(arena, ConnectedNamespace);
         new_ns->name = PushCopy(arena, ns_name);
         new_ns->next = curr_namespace->first_child;
         curr_namespace->first_child = new_ns;
         
         curr_namespace = new_ns;
      }
   }

   ConnectedEntry *result = NULL;
   string rec_name = path.parts[path.part_count - 1];
   for(ConnectedEntry *entry = curr_namespace->first_entry; entry; entry = entry->next) {
      if(entry->name == rec_name) {
         result = entry;
         break;
      }
   }

   if((result == NULL) && create) {
      result = PushStruct(arena, ConnectedEntry);
      result->name = PushCopy(arena, rec_name);
      
      result->next = curr_namespace->first_entry;
      curr_namespace->first_entry = result;
   }

   return result;
}

ConnectedEntry *GetEntry(ConnectedState *state, string name) {
   return GetOrCreateEntry(state, name, false);
} 

void ClearNamespace(ConnectedNamespace *ns) {
   for(ConnectedEntry *entry = ns->first_entry; entry; entry = entry->next) {
      entry->has_data = false;
   }

   for(ConnectedNamespace *child = ns->first_child; child; child = child->next) {
      ClearNamespace(child);
   }
}

//---------------------------------
void parseVertexData(buffer *packet, ConnectedState *state, ConnectedEntry *entry) {
   MemoryArena *arena = state->frame_arena;
   Vis_VertexData *vdata = ConsumeStruct(packet, Vis_VertexData);
   v2 *points = ConsumeArray(packet, v2, vdata->point_count);

   entry->vdata.point_count = vdata->point_count;
   entry->vdata.points = PushArrayCopy(arena, v2, points, vdata->point_count);
   entry->vdata.colour = vdata->colour;
   entry->has_data = true;
}

void parseDiag(buffer *packet, ConnectedState *state, ConnectedEntry *entry) {
   Vis_Diagnostic *diag = ConsumeStruct(packet, Vis_Diagnostic);
   string unit_name = ConsumeString(packet, diag->unit_name_length);

   entry->diag.value = diag->value;
   entry->has_data = true;
}

void parseRobotPose(buffer *packet, ConnectedState *state, ConnectedEntry *entry) {
   Vis_RobotPose *pose = ConsumeStruct(packet, Vis_RobotPose);
   
   entry->pose.pos = pose->pos;
   entry->pose.angle = pose->angle;
   entry->has_data = true;
}
//---------------------------------

typedef void (*live_entry_parser_callback)(buffer *packet, ConnectedState *state, ConnectedEntry *entry);
live_entry_parser_callback live_entry_parsers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   parseVertexData, //Polyline = 1
   parseVertexData, //Triangles = 2
   parseVertexData, //Points = 3
   parseRobotPose, //RobotPose = 4
   parseDiag, //Diagnostic = 5
   NULL, //Message = 6
};

live_entry_parser_callback getLiveEntryReciever(u8 type) {
   return (type < ArraySize(live_entry_parsers)) ? live_entry_parsers[type] : NULL;
}

void ParseVisEntry(ConnectedState *state, buffer *packet) {
   Vis_Entry *entry_header = ConsumeStruct(packet, Vis_Entry);
   string name = ConsumeString(packet, entry_header->name_length);

   live_entry_parser_callback parser = getLiveEntryReciever(entry_header->type);
   if(parser) {
      ConnectedEntry *entry = GetOrCreateEntry(state, name);
      entry->type = (North_VisType::type) entry_header->type; 
      
      parser(packet, state, entry);
   } else {
      //TODO: report unknown entry type?
      ConsumeSize(packet, entry_header->size);
   }
}

void ParseStatePacket(ConnectedState *state, buffer packet) {
   MemoryArena *arena = state->frame_arena;

   VisHeader *header = ConsumeStruct(&packet, VisHeader);
   if(state->curr_frame != header->frame_number) {
      Reset(arena);
      ClearNamespace(&state->root_namespace);
      state->curr_frame = header->frame_number;
   }

   for(u32 i = 0; i < header->entry_count; i++) {
      ParseVisEntry(state, &packet);
   }
}

//------------------------------------
void DrawLivePoints(element *page, ui_field_topdown *field, ConnectedState *state, ConnectedEntry *entry) {
   if(page) {
      Label(page, entry->name + " Point count = " + ToString(entry->vdata.point_count), 20, BLACK);
   }

   // if(!entry->hidden) {
      //Replace this with one call that takes the points & a transform matrix
      for(u32 j = 0; j < entry->vdata.point_count; j++) {
         v2 p = GetPoint(field, entry->vdata.points[j]);
         Rectangle(field->e, RectCenterSize(p, V2(5, 5)), entry->vdata.colour);
      }
      //------------------------------
   // }
}

void DrawLiveDiagnostic(element *page, ui_field_topdown *field, ConnectedState *state, ConnectedEntry *entry) {
   if(page) {
      Label(page, entry->name + " = " + ToString(entry->diag.value), 20, BLACK);
   }
}

void DrawLiveRobotPose(element *page, ui_field_topdown *field, ConnectedState *state, ConnectedEntry *entry) {
   if(page) {
      Label(page, entry->name, 20, BLACK);
   }

   DrawRobot(field, V2(0.5, 0.5), entry->pose.pos, entry->pose.angle, BLACK);
}
//------------------------------------

typedef void (*draw_live_entry_callback)(element *page, ui_field_topdown *field, ConnectedState *state, ConnectedEntry *entry);
draw_live_entry_callback live_entry_drawers[North_VisType::TypeCount] = {
   NULL, //Invalid 0
   NULL, //Polyline = 1
   NULL, //Triangles = 2
   DrawLivePoints, //Points = 3
   DrawLiveRobotPose, //RobotPose = 4
   DrawLiveDiagnostic, //Diagnostic = 5
   NULL, //Message = 6
};

draw_live_entry_callback getLiveEntryDrawer(North_VisType::type type) {
   return (type < ArraySize(live_entry_drawers)) ? live_entry_drawers[type] : NULL;
}

void DrawLiveNamespace(element *page, ui_field_topdown *field, ConnectedState *state, ConnectedNamespace *ns) {
   element *child_page = NULL;
   
   if(page) {
      UI_SCOPE(page, ns);
      button_style hide_button = ButtonStyle(
         dark_grey, light_grey, BLACK,
         light_grey, V4(120/255.0, 120/255.0, 120/255.0, 1), WHITE, 
         off_white, light_grey,
         20, V2(0, 0), V2(0, 0));

      element *top_row = RowPanel(page, Size(Size(page).x, 20));
      Label(top_row, (ns->name.length == 0) ? Literal("/") : ns->name, 20, BLACK);
      if(Button(top_row, ns->collapsed ? "  +  " : "  -  ", hide_button).clicked) {
         ns->collapsed = !ns->collapsed;
      }

      child_page = ns->collapsed ? NULL : page;
   }
   
   for(ConnectedEntry *entry = ns->first_entry; entry; entry = entry->next) {
      draw_live_entry_callback draw_func = getLiveEntryDrawer(entry->type);
      if(entry->has_data && draw_func) {
         draw_func(child_page, field, state, entry);
      }
   }
   
   for(ConnectedNamespace *child = ns->first_child; child; child = child->next) {
      DrawLiveNamespace(child_page, field, state, child);
   }
}