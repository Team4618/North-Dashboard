#ifndef COMMON_STRINGS
#define COMMON_STRINGS

#include "common_types.h"

u32 StringLength(const char *text) {
   u32 length = 0;
   while(*text) {
      length++;
      text++;
   }

   return length;
}

//TODO: utf-8
struct string {
   char *text;
   u32 length;

   string () {
      this->text = 0;
      this->length = NULL;
   }

   string (const char *c_str) {
      //TODO: casting this away because clang
      this->text = (char *)c_str;
      this->length = StringLength(c_str);
   }

   string& operator= (const char *c_str) {
      //TODO: casting this away because clang
      this->text = (char *)c_str;
      this->length = (c_str == NULL) ? 0 : StringLength(c_str);
      return *this;
   }
};

const string EMPTY_STRING = {};

string String(const char *text, u32 length) {
   string result = {};

   //TODO: casting this away because clang
   result.text = (char *)text;
   result.length = length;
   return result;
}

//TODO: get rid of this (or just rename it to String & get rid of most of its uses)
string Literal(const char *text) {
   return String(text, StringLength(text));
}

//TODO: better hash function
u32 Hash(string s) {
   u32 result = 1;
   for(u32 i = 0; i < s.length; i++) {
      result = 7 * s.text[i] + 3 * result; 
   }
   return result;
}

bool operator== (string a, string b) {
   if(a.length != b.length)
      return false;

   for(u32 i = 0; i < a.length; i++) {
      if(a.text[i] != b.text[i])
         return false;
   }

   return true;
} 

bool operator!=(string a, string b) {
   return !(a == b);
}

bool IsWhitespace(char c) {
   return ((c == ' ') || (c == '\r') || (c == '\t'));
}

bool IsAlpha(char c) {
   return (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')));
}

bool IsNumeric(char c) {
   return ((c >= '0') && (c <= '9'));
}

bool IsNumber(string s) {
   bool has_dot = false;
   bool hit_whitespace = false;
   for(u32 i = 0; i < s.length; i++) {
      char c = s.text[i];
      bool is_whitespace = IsWhitespace(c) || (c == '\0');
      
      if(hit_whitespace && !is_whitespace)
         return false;

      if(c == '-') {
         if((i != 0) || (s.length == 1))
            return false;
      } else if(c == '.') {
         if(has_dot)
            return false;

         if(i == (s.length - 1))
            return false;

         has_dot = true;
      } else if(is_whitespace) {
         hit_whitespace = true;
      } else if(!IsNumeric(c)) {
         return false;
      }
   }
   return true;
}

bool IsUInt(string s) {
   bool hit_whitespace = false;
   for(u32 i = 0; i < s.length; i++) {
      char c = s.text[i];
      bool is_whitespace = IsWhitespace(c) || (c == '\0');
      
      if(hit_whitespace && !is_whitespace)
         return false;

      if(is_whitespace) {
         hit_whitespace = true;
      } else if(!IsNumeric(c)) {
         return false;
      }
   }
   return true;
}

#ifdef COMMON_MEMORY 
string PushCopy(MemoryArena *arena, string s) {
   string result = {};
   result.length = s.length;
   result.text = (char *) PushSize(arena, s.length);
   Copy((u8 *) s.text, s.length, (u8 *) result.text);
   return result;
}

//TODO: make concatenation & conversion from c-strings to len-strings nicer
string Concat(string *inputs, u32 input_count) {
   string result = {};
   
   for(u32 i = 0; i < input_count; i++) {
      result.length += inputs[i].length;
   }

   result.text = PushTempArray(char, result.length);
   u8 *dest = (u8 *) result.text;
   for(u32 i = 0; i < input_count; i++) {
      string s = inputs[i];
      Copy((u8 *) s.text, s.length, dest);
      dest += s.length;
   }

   return result;
}

string Concat(string a, string b, string c = EMPTY_STRING, string d = EMPTY_STRING,
              string e = EMPTY_STRING, string f = EMPTY_STRING) {
   string inputs[] = {a, b, c, d, e, f};
   return Concat(inputs, ArraySize(inputs));
}

string operator+ (string a, string b) {
	return Concat(a, b);
}

struct SplitString {
   string *parts;
   u32 part_count;
};

SplitString split(string s, char split_c, MemoryArena *arena = __temp_arena) {
   for(u32 i = 0; (i < s.length) && (s.text[0] == split_c); i++) {
      s.text++;
      s.length--;
   }

   while((s.length > 0) && (s.text[s.length - 1] == split_c)) {
      s.length--;
   }
   
   u32 part_count = (s.length > 0);
   for(u32 i = 0; i < s.length; i++) {
      if((s.text[i] == split_c) && (s.text[i + 1] != split_c)) {
         part_count++;
      }
   }

   string *parts = PushArray(arena, string, part_count);
   u32 part_i = 0;
   u32 char_i = 0;
   while(char_i < s.length) {
      if(s.text[char_i] != split_c) {
         u32 length = 1;
         while(((char_i + length) < s.length) && (s.text[char_i + length] != split_c)) {
            length++;
         }
         parts[part_i++] = PushCopy(arena, String(s.text + char_i, length));
         char_i += length;
      } else {
         char_i++;
      }
   }

   SplitString result = {};
   result.part_count = part_count;
   result.parts = parts;
   return result;
}

#include "stdio.h"
string ToString(f32 value) {
   //TODO: clean this up
   char buffer[128] = {};
   sprintf(buffer, "%f", value);
   string result = PushTempCopy(Literal(buffer));
   u32 length = 0;
   for(u32 i = result.length - 1; i > 0; i--) {
      if(result.text[i] == '.') {
         length = i;
         break;
      }
      
      if(result.text[i] != '0') {
         length = i + 1;
         break;
      }
   }
   result.length = Max(1, length);
   return result;
}

f32 ToF32(string number) {
   char number_buffer[256] = {};
   sprintf(number_buffer, "%.*s", number.length, number.text);
   return atof(number_buffer);
}

string ToString(u32 value) {
   char buffer[128] = {};
   sprintf(buffer, "%u", value);
   return PushTempCopy(Literal(buffer));
}

u32 ToU32(string number) {
   char number_buffer[256] = {};
   sprintf(number_buffer, "%.*s", number.length, number.text);
   return (u32) strtol(number_buffer, NULL, 10);
}

string ToString(s32 value) {
   char buffer[128] = {};
   sprintf(buffer, "%i", value);
   return PushTempCopy(Literal(buffer));
}

char *ToCString(string str) {
   char null_terminated[256] = {};
   sprintf(null_terminated, "%.*s", Min(str.length, 255), str.text);
   return (char *) PushCopy(__temp_arena, null_terminated, ArraySize(null_terminated));
}
#endif

#endif