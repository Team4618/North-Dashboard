template <typename T>
struct InterpolatingMapSample {
   f32 key;
   T data;
};

template <typename T>
struct InterpolatingMapNode {
   bool is_leaf;
   f32 midpoint;
   
   union {
      struct {
         InterpolatingMapNode<T> *lesser;
         InterpolatingMapNode<T> *greater;
      };
      u32 val_index;
   };
};

template <typename T> 
struct InterpolatingMap {
   typedef T (*InterpolatingMapCallback) (T a, T b, f32 t);
   
   InterpolatingMapCallback lerp_callback;

   InterpolatingMapSample<T> *samples;
   u32 sample_count;

   InterpolatingMapNode<T> *root;
};

template <typename T>
InterpolatingMapNode<T> *CreateNode(u32 index_offset, InterpolatingMapSample<T> *sorted_samples, u32 sample_count, MemoryArena *arena) {
   Assert(sample_count > 0);
   InterpolatingMapNode<T> *result = PushStruct(arena, InterpolatingMapNode<T>);
   
   if(sample_count == 1) {
      result->is_leaf = true;
      result->val_index = index_offset;
   } else {
      result->is_leaf = false;

      u32 half_sample_count = sample_count / 2;
      result->midpoint = (sorted_samples[half_sample_count - 1 + index_offset].key + sorted_samples[half_sample_count + index_offset].key) / 2.0f;
      result->lesser =  CreateNode(index_offset,                     sorted_samples, half_sample_count,                arena);
      result->greater = CreateNode(index_offset + half_sample_count, sorted_samples, sample_count - half_sample_count, arena);
   }
   
   return result;
}

//NOTE: doesnt take ownership of "arena", just uses it for allocation in this function, not even stored
//NOTE: "sorted_samples" must be sorted by key in increasing value 
template <typename T>
InterpolatingMap<T> BuildInterpolatingMap(InterpolatingMapSample<T> *sorted_samples, u32 sample_count, 
                                          MemoryArena *arena, T (*callback) (T a, T b, f32 t))
{
   InterpolatingMap<T> result = {};
   result.lerp_callback = callback;
   result.samples = PushArrayCopy(arena, InterpolatingMapSample<T>, sorted_samples, sample_count);
   result.sample_count = sample_count;
   result.root = CreateNode(0, result.samples, sample_count, arena);

   return result;
}

template <typename T>
T MapLookup(InterpolatingMap<T> *map, f32 key) {
   InterpolatingMapNode<T> *curr_node = map->root;
   while(!curr_node->is_leaf) {
      curr_node = (key < curr_node->midpoint) ? curr_node->lesser : curr_node->greater;
   }

   u32 a_i;
   u32 b_i;

   u32 center_leaf_i = curr_node->val_index;
   if(key > map->samples[center_leaf_i].key) {
      a_i = Clamp(0, map->sample_count - 1, center_leaf_i);
      b_i = Clamp(0, map->sample_count - 1, center_leaf_i + 1);
   } else {
      a_i = Clamp(0, map->sample_count - 1, center_leaf_i - 1);
      b_i = Clamp(0, map->sample_count - 1, center_leaf_i);
   }

   InterpolatingMapSample<T> *leaf_a = map->samples + a_i;
   InterpolatingMapSample<T> *leaf_b = map->samples + b_i;

   f32 t = (key - leaf_a->key) / (leaf_b->key - leaf_a->key); 
   return map->lerp_callback(leaf_a->data, leaf_b->data, t);
}

v2 interpolation_map_v2_lerp(v2 a, v2 b, f32 t) {
   return lerp(a, t, b);
}