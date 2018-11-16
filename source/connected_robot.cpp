struct ConnectedParameter;
struct ConnectedParameterValue {
   ConnectedParameter *parent;
   ConnectedParameterValue *next;
   ConnectedParameterValue *prev;
   
   u32 index;
   f32 value;
};

struct ConnectedSubsystem;
struct ConnectedParameter {
   ConnectedSubsystem *subsystem;
   string name;
   bool is_array;

   ConnectedParameterValue sentinel;
};

struct ConnectedSubsystem {
   //TODO: we're being super infefficient with memory right now, fix eventually
   MemoryArena param_arena;

   string name;
   MultiLineGraphData diagnostics_graph;
   
   u32 param_count;
   ConnectedParameter *params;
};

void SetParamValue(ConnectedParameterValue *param, f32 value) {
   buffer packet = PushTempBuffer(Megabyte(1));

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::SetValue;
   header.subsystem_name_length = param->parent->subsystem->name.length;
   header.param_name_length = param->parent->name.length;

   header.index = param->index;
   header.value = value;
   param->value = value; 
}

void RecalculateIndicies(ConnectedParameter *param) {
   u32 i = 0;
   for(ConnectedParameterValue *curr = param->sentinel.next;
       curr != &param->sentinel; curr = curr->next)
   {
      curr->index = i++;
   }
}

void AddParamValue(ConnectedParameter *param) {
   Assert(param->is_array);
   MemoryArena *arena = &param->subsystem->param_arena;
   ConnectedParameterValue *sentinel = &param->sentinel;

   ConnectedParameterValue *new_value = PushStruct(arena, ConnectedParameterValue);
   new_value->value = 0;
   new_value->parent = param;
   new_value->next = sentinel;
   new_value->prev = sentinel->prev;

   new_value->next->prev = new_value;
   new_value->prev->next = new_value; 
   RecalculateIndicies(param);

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::AddValue;
   header.subsystem_name_length = param->subsystem->name.length;
   header.param_name_length = param->name.length;
   header.index = new_value->index;
   header.value = 0;
}

void RemoveParamValue(ConnectedParameterValue *param) {
   Assert(param->parent->is_array);
   ConnectedParameter *parent = param->parent;
   
   param->next->prev = param->prev;
   param->prev->next = param->next;
   RecalculateIndicies(param->parent);

   ParameterOp_PacketHeader header = {};
   header.type = ParameterOp_Type::RemoveValue;
   header.subsystem_name_length = parent->subsystem->name.length;
   header.param_name_length = parent->name.length;
   header.index = param->index;
   header.value = 0;
}