#include "winsock2.h"
#include "Ws2tcpip.h"
#include "windows.h"
#include "gl/gl.h"
#include "wglext.h"
#include "glext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "lib/common.cpp"
#include "string_and_lexer.cpp"
#include "lib/ui_core.cpp"
#include "lib/ui_button.cpp"
#include "lib/ui_textbox.cpp"
#include "lib/ui_vertical_list.cpp"
#include "lib/ui_horizontal_slider.cpp"
#include "lib/ui_multiline_graph.cpp"

MemoryArena VirtualAllocArena(u64 size) {
   return NewMemoryArena(VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE), size);
}

string exe_directory = {};
buffer ReadEntireFile(const char* path, bool in_exe_directory = false) {
   char full_path[MAX_PATH + 1];
   sprintf(full_path, "%.*s%s", exe_directory.length, exe_directory.text, path);

   HANDLE file_handle = CreateFileA(in_exe_directory ? full_path : path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
                                    
   buffer result = {};
   if(file_handle != INVALID_HANDLE_VALUE) {
      DWORD number_of_bytes_read;
      result.size = GetFileSize(file_handle, NULL);
      result.data = (u8 *) VirtualAlloc(0, result.size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      ReadFile(file_handle, result.data, result.size, &number_of_bytes_read, NULL);
      CloseHandle(file_handle);
   }

   return result;
}

buffer ReadEntireFile(string path, bool in_exe_directory = false) {
   return ReadEntireFile(ToCString(path), in_exe_directory);
}

void FreeEntireFile(buffer *file) {
   VirtualFree(file->data, 0, MEM_RELEASE);
   file->data = 0;
   file->size = 0;
}

void WriteEntireFile(const char* path, buffer file) {
   HANDLE file_handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
                                    
   if(file_handle != INVALID_HANDLE_VALUE) {
      DWORD number_of_bytes_written;
      WriteFile(file_handle, file.data, file.offset, &number_of_bytes_written, NULL);
      CloseHandle(file_handle);
   }
}

void WriteEntireFile(string path, buffer file) {
   return WriteEntireFile(ToCString(path), file);
}

struct FileListLink {
   FileListLink *next;
   string name;
};

FileListLink *ListFilesWithExtension(char *wildcard_extension, MemoryArena *arena = &__temp_arena) {
   WIN32_FIND_DATAA file = {};
   HANDLE handle = FindFirstFileA(wildcard_extension, &file);
   FileListLink *result = NULL;
   if(handle != INVALID_HANDLE_VALUE) {
      do {
         FileListLink *new_link = PushStruct(arena, FileListLink);
         string name = Literal(file.cFileName);
         for(u32 i = 0; i < name.length; i++) {
            if(name.text[i] == '.') {
               name.length = i;
               break;
            }
         }
         new_link->name = PushCopy(arena, name);
         new_link->next = result;
         result = new_link;
      } while(FindNextFileA(handle, &file));
   }
   return result;
}

PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

PFNGLCREATESHADERPROC glCreateShader;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

PFNGLCREATEBUFFERSPROC glCreateBuffers;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLNAMEDBUFFERDATAPROC glNamedBufferData;

PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLUNIFORM4FVPROC glUniform4fv;

PFNGLVERTEXARRAYVERTEXBUFFERPROC glVertexArrayVertexBuffer;
PFNGLENABLEVERTEXARRAYATTRIBPROC glEnableVertexArrayAttrib;
PFNGLDISABLEVERTEXARRAYATTRIBPROC glDisableVertexArrayAttrib;
PFNGLVERTEXARRAYATTRIBBINDINGPROC glVertexArrayAttribBinding;
PFNGLVERTEXARRAYATTRIBFORMATPROC glVertexArrayAttribFormat;
PFNGLVERTEXARRAYELEMENTBUFFERPROC glVertexArrayElementBuffer;

PFNGLUNIFORM1IPROC glUniform1i;
PFNGLBINDTEXTUREUNITPROC glBindTextureUnit;
PFNGLCREATETEXTURESPROC glCreateTextures;
PFNGLTEXTURESTORAGE2DPROC glTextureStorage2D;
PFNGLTEXTURESUBIMAGE2DPROC glTextureSubImage2D;
PFNGLTEXTUREPARAMETERIPROC glTextureParameteri;

PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;

struct image {
   u32 *texels;
   u32 width;
   u32 height;
   bool valid;
};

image ReadImage(char *path, bool in_exe_directory = false) {
   image result = {};
   buffer file = ReadEntireFile(path, in_exe_directory);

   if(file.data != NULL) {
      s32 width, height, channels;
      u32 *data = (u32 *) stbi_load_from_memory((u8 *) file.data, file.size,
                                                &width, &height, &channels, 0);
      result.width = width;
      result.height = height;
      result.texels = data;
      result.valid = true;

      FreeEntireFile(&file);
   }

   return result;
}

image ReadImage(string path, bool in_exe_directory = false) {
   return ReadImage(ToCString(path), in_exe_directory);
}

void FreeImage(image *i) {
   if(i->valid)
      stbi_image_free(i->texels);
}

texture loadTexture(char *path, bool in_exe_directory) {
   texture result = {};
   image img = ReadImage(path, in_exe_directory);
   if(img.valid)
      result = createTexture(img.texels, img.width, img.height);
   FreeImage(&img);
   return result;
}

texture createTexture(u32 *texels, u32 width, u32 height) {
   texture result = {};
   result.size = V2(width, height);
      
   glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);
   glTextureStorage2D(result.handle, 1, GL_RGBA8, width, height);
   glTextureSubImage2D(result.handle, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texels);
   glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   
   return result;
}

void deleteTexture(texture tex) {
   if(tex.handle != 0)
      glDeleteTextures(1, &tex.handle); 
}

sdfFont loadFont(char *metaPath, char *texturePath) {
   sdfFont result = {};
   result.sdfTexture = loadTexture(texturePath, true);
   
   buffer fontMeta = ReadEntireFile(metaPath, true);
   lexer metadata = { (char *) fontMeta.data };
   
   bool parsing = true;
   while(parsing) {
      token tkn = GetToken(&metadata);
      
      if(tkn.type == Token_End) {
         parsing = false;
      } else if(TokenIdentifier(tkn, Literal("char"))) {
         u32 id = 0;
         s32 x = 0;
         s32 y = 0;
         s32 width = 0;
         s32 height = 0;
         s32 xoffset = 0;
         s32 yoffset = 0;
         s32 xadvance = 0;
         
         while(true) {
            token first = GetToken(&metadata);
            
            if(first.type == Token_NewLine) {
               break;
            } else if(TokenIdentifier(first, Literal("id"))) {
               GetToken(&metadata); //Should always be =
               id = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("x"))) {
               GetToken(&metadata); //Should always be =
               x = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("y"))) {
               GetToken(&metadata); //Should always be =
               y = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("width"))) {
               GetToken(&metadata); //Should always be =
               width = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("height"))) {
               GetToken(&metadata); //Should always be =
               height = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("xoffset"))) {
               GetToken(&metadata); //Should always be =
               xoffset = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("yoffset"))) {
               GetToken(&metadata); //Should always be =
               yoffset = ParseS32(&metadata);
            } else if(TokenIdentifier(first, Literal("xadvance"))) {
               GetToken(&metadata); //Should always be =
               xadvance = ParseS32(&metadata);
            }
         }
         
         if(id < ArraySize(result.glyphs)) {
            glyphInfo glyph = {};
            glyph.textureLocation = V2(x, y);
            glyph.size = V2(width, height);
            glyph.offset = V2(xoffset, yoffset);
            glyph.xadvance = xadvance;
            result.glyphs[id] = glyph;

            result.max_char_width = Max(result.max_char_width, width);
         }
      } else if(TokenIdentifier(tkn, Literal("common"))) {
         while(true) {
            token first = GetToken(&metadata);
            
            if(first.type == Token_NewLine) {
               break;
            } else if(TokenIdentifier(first, Literal("lineHeight"))) {
               GetToken(&metadata); //Should always be =
               result.native_line_height = ParseS32(&metadata);
            }
         }
      } else if(TokenIdentifier(tkn, Literal("info"))) {
         
      }
   }
   
   FreeEntireFile(&fontMeta);
   return result;
}

#include "ui_field_topdown.cpp"
#include "dashboard.cpp"
#include "network.cpp"

GLuint loadShader(char *path, GLenum shaderType) {
   buffer shader_src = ReadEntireFile(path, true);
   GLuint shader = glCreateShader(shaderType);
   glShaderSource(shader, 1, (char **) &shader_src.data, (GLint *) &shader_src.size);
   glCompileShader(shader);
   
   GLint compiled = 0;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
   OutputDebugStringA(path);
   OutputDebugStringA(compiled ? " Compiled\n" : " Didn't Compile\n");
   if(compiled == GL_FALSE)
   {
      GLint log_length = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

      char *compile_log = (char *) malloc(sizeof(char) * log_length);
      glGetShaderInfoLog(shader, log_length, &log_length, compile_log);
      OutputDebugStringA(compile_log);
   }
   
   FreeEntireFile(&shader_src);
   return shader;
}

GLuint shaderProgram(GLuint vert, GLuint frag) {
   GLuint shader_program = glCreateProgram();
   glAttachShader(shader_program, vert);
   glAttachShader(shader_program, frag);
   glLinkProgram(shader_program);
   
   GLint shader_linked = 0;
   glGetProgramiv(shader_program, GL_LINK_STATUS, &shader_linked);
   OutputDebugStringA(shader_linked ? "Shader Program Linked\n" : "Shader Program Didn't Link\n");
   if(shader_linked == GL_FALSE)
   {
      GLint log_length = 0;
      glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

      char *link_log = (char *) malloc(sizeof(char) * log_length);
      glGetProgramInfoLog(shader_program, log_length, &log_length, link_log);
      OutputDebugStringA(link_log);
   }
   
   return shader_program;
}

void opengl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                           GLsizei length, const GLchar* message, const void* userParam)
{
   /*
   OutputDebugStringA("OGL Debug: ");
   OutputDebugStringA(message);
   OutputDebugStringA("\n");
   */
}

const GLuint position_slot = 0;
const GLuint uv_slot = 1;
const GLuint colour_slot = 2;

struct openglContext {
   HDC dc;
   GLuint vao;
   
   struct {
      GLuint handle;
      GLint matrix_uniform;
      GLint colour_uniform;
   } col;
   
   struct {
      GLuint handle;
      GLint matrix_uniform; 
      GLint texture_uniform;
   } tex;
   
   struct {
      GLuint handle;
      GLint matrix_uniform; 
      GLint texture_uniform;
      GLint colour_uniform; 
   } sdf;
   
   GLuint buffers[3];
};

v2 window_size = V2(0, 0);

/**
TODO: RENDERER REWRITE
   freelist of opengl buffers?
   seperate lists for opaque & transparent geometry
      opaque geometry can be drawn in one pass
      transparent geometry must be sorted and drawn in multiple passes
   
   seperate threads for:
      window message handling (wait on GetMessage)
      rendering
      ui
*/

void DrawRenderCommandBuffer(RenderCommand *first_command, rect2 bounds, mat4 transform, openglContext *gl) {
   glScissor(bounds.min.x, window_size.y - bounds.max.y, Size(bounds).x, Size(bounds).y);

   for(RenderCommand *command = first_command; command; command = command->next) {
      switch(command->type) {
         case RenderCommand_SDF: {
            glUseProgram(gl->sdf.handle);
            glUniformMatrix4fv(gl->sdf.matrix_uniform, 1, GL_FALSE, transform.e);
            glUniform4fv(gl->sdf.colour_uniform, 1, command->drawSDF.colour.e);
            glUniform1i(gl->sdf.texture_uniform, 0);
            glBindTextureUnit(0, command->drawSDF.sdf.handle);
            
            glEnableVertexArrayAttrib(gl->vao, position_slot);
            glEnableVertexArrayAttrib(gl->vao, uv_slot);
            glDisableVertexArrayAttrib(gl->vao, colour_slot);
            
            glVertexArrayVertexBuffer(gl->vao, position_slot, gl->buffers[0], 0, sizeof(v2));
            glVertexArrayVertexBuffer(gl->vao, uv_slot, gl->buffers[1], 0, sizeof(v2));
            
            rect2 bounds = command->drawSDF.bounds;
            v2 verts[6] = {
               bounds.min, bounds.min + YOf(Size(bounds)), bounds.max, 
               bounds.min, bounds.min + XOf(Size(bounds)), bounds.max
            };
            
            rect2 uvBounds = command->drawSDF.uvBounds;
            v2 uvs[6] = {
               uvBounds.min, uvBounds.min + YOf(Size(uvBounds)), uvBounds.max, 
               uvBounds.min, uvBounds.min + XOf(Size(uvBounds)), uvBounds.max
            };
            
            for(u32 i = 0; i < 6; i++)
               uvs[i] = uvs[i] / command->drawSDF.sdf.size;
            
            glNamedBufferData(gl->buffers[0], 2 * 3 * sizeof(v2), verts, GL_STREAM_DRAW);
            glNamedBufferData(gl->buffers[1], 2 * 3 * sizeof(v2), uvs, GL_STREAM_DRAW);
            
            glDrawArrays(GL_TRIANGLES, 0, 2 * 3);
         } break;
         
         case RenderCommand_Texture: {
            glUseProgram(gl->tex.handle);
            glUniformMatrix4fv(gl->tex.matrix_uniform, 1, GL_FALSE, transform.e);
            glUniform1i(gl->tex.texture_uniform, 0);
            glBindTextureUnit(0, command->drawTexture.tex.handle);
            
            glEnableVertexArrayAttrib(gl->vao, position_slot);
            glEnableVertexArrayAttrib(gl->vao, uv_slot);
            glDisableVertexArrayAttrib(gl->vao, colour_slot);
            
            glVertexArrayVertexBuffer(gl->vao, position_slot, gl->buffers[0], 0, sizeof(v2));
            glVertexArrayVertexBuffer(gl->vao, uv_slot, gl->buffers[1], 0, sizeof(v2));
            
            rect2 bounds = command->drawTexture.bounds;
            v2 verts[6] = {
               bounds.min, bounds.min + YOf(Size(bounds)), bounds.max, 
               bounds.min, bounds.min + XOf(Size(bounds)), bounds.max
            };
            
            rect2 uvBounds = command->drawTexture.uvBounds;
            v2 uvs[6] = {
               uvBounds.min, uvBounds.min + YOf(Size(uvBounds)), uvBounds.max, 
               uvBounds.min, uvBounds.min + XOf(Size(uvBounds)), uvBounds.max
            };
            
            for(u32 i = 0; i < 6; i++)
               uvs[i] = uvs[i] / command->drawTexture.tex.size;
            
            glNamedBufferData(gl->buffers[0], 2 * 3 * sizeof(v2), verts, GL_STREAM_DRAW);
            glNamedBufferData(gl->buffers[1], 2 * 3 * sizeof(v2), uvs, GL_STREAM_DRAW);
            
            glDrawArrays(GL_TRIANGLES, 0, 2 * 3);
         } break;
         
         case RenderCommand_Rectangle: {
            glUseProgram(gl->col.handle);
            glUniformMatrix4fv(gl->col.matrix_uniform, 1, GL_FALSE, transform.e);
            glUniform4fv(gl->col.colour_uniform, 1, command->drawRectangle.colour.e);
            
            glEnableVertexArrayAttrib(gl->vao, position_slot);
            glDisableVertexArrayAttrib(gl->vao, uv_slot);
            glDisableVertexArrayAttrib(gl->vao, colour_slot);
            
            glVertexArrayVertexBuffer(gl->vao, position_slot, gl->buffers[0], 0, sizeof(v2));
            
            rect2 bounds = command->drawRectangle.bounds;
            v2 verts[6] = {
               bounds.min, bounds.min + YOf(Size(bounds)), bounds.max, 
               bounds.min, bounds.min + XOf(Size(bounds)), bounds.max
            };
            
            glNamedBufferData(gl->buffers[0], 2 * 3 * sizeof(v2), verts, GL_STREAM_DRAW);
            
            glDrawArrays(GL_TRIANGLES, 0, 2 * 3);
         } break;
         
         case RenderCommand_Line: {
            glUseProgram(gl->col.handle);
            glUniformMatrix4fv(gl->col.matrix_uniform, 1, GL_FALSE, transform.e);
            glUniform4fv(gl->col.colour_uniform, 1, command->drawLine.colour.e);
            glLineWidth(command->drawLine.thickness);
            
            glEnableVertexArrayAttrib(gl->vao, position_slot);
            glDisableVertexArrayAttrib(gl->vao, uv_slot);
            glDisableVertexArrayAttrib(gl->vao, colour_slot);
            
            glVertexArrayVertexBuffer(gl->vao, position_slot, gl->buffers[0], 0, sizeof(v2));
            
            v2 verts[2] = { command->drawLine.a, command->drawLine.b };
            
            glNamedBufferData(gl->buffers[0], 2 * sizeof(v2), verts, GL_STREAM_DRAW);
            
            glDrawArrays(GL_LINES, 0, 2);
         } break;
      }
   }
}

LRESULT CALLBACK WindowMessageEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
   switch(message)
	{
		case WM_CLOSE:
			DestroyWindow(window);
         break;
		
		case WM_DESTROY:
			PostQuitMessage(0);
         break;
        
      case WM_SIZE:
      {
         RECT client_rect = {};
         GetClientRect(window, &client_rect);
         window_size = V2(client_rect.right, client_rect.bottom);
         glViewport(0, 0, window_size.x, window_size.y);
      } break;
	}
   
   return DefWindowProc(window, message, wParam, lParam);
}

void DrawElement(element *e, mat4 transform, openglContext *gl) {
   DrawRenderCommandBuffer(e->first_command, e->cliprect, transform, gl);
   
   UIContext *context = e->context;
   v2 cursor = context->input_state.pos;

   if(IsActive(e)) {
      context->active_element_refreshed = true;
      context->active.last_pos = cursor;
   }

   if(IsSelected(e)) {
      context->selected_element_refreshed = true;
      context->selected.last_pos = cursor;
   }

   for(element *child = e->first_child; child; child = child->next) {
      DrawElement(child, transform, gl);
   }
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
   //NOTE: 
   //we load app-crucial files (eg. shaders, font) from the directory that the exe is in, 
   //we load user files (eg. settings, saves) from the working directory   

   char exepath[MAX_PATH + 1];
   if(0 == GetModuleFileNameA(0, exepath, MAX_PATH + 1))
      Assert(false);
   exe_directory = Literal(exepath);
   for(u32 i = exe_directory.length - 1; i >= 0; i--) {
      if((exe_directory.text[i] == '\\') || (exe_directory.text[i] == '/'))
         break;

      exe_directory.length--;
   }

   WNDCLASSEX window_class = {};
   window_class.cbSize = sizeof(window_class);
   window_class.style = CS_OWNDC;
   window_class.lpfnWndProc = WindowMessageEvent;
   window_class.hInstance = hInstance;
   window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
   window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
   window_class.lpszClassName = "WindowClass";
   
   RegisterClassExA(&window_class);
   
   HWND window = CreateWindowExA(WS_EX_CLIENTEDGE, "WindowClass", "North Dashboard",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                                 hInstance, NULL);
   
   //TODO: does this load exe-relative or from the working directory?
   HANDLE hIcon = LoadImageA(0, "icon.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
   if(hIcon) {
       SendMessageA(window, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
       SendMessageA(window, WM_SETICON, ICON_BIG, (LPARAM) hIcon);

       SendMessageA(GetWindow(window, GW_OWNER), WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
       SendMessageA(GetWindow(window, GW_OWNER), WM_SETICON, ICON_BIG, (LPARAM) hIcon);
   } else {
      MessageBox(window, "Error", "Icon Not Found", 0);
   }

   openglContext gl = {};
   gl.dc = GetDC(window);
   
   PIXELFORMATDESCRIPTOR pixel_format = {sizeof(pixel_format), 1};
   pixel_format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   pixel_format.iPixelType = PFD_TYPE_RGBA;
   pixel_format.cColorBits = 32;
   
   s32 pixel_format_index = ChoosePixelFormat(gl.dc, &pixel_format);
   SetPixelFormat(gl.dc, pixel_format_index, &pixel_format);
   
   HGLRC temp_gl_context = wglCreateContext(gl.dc);
   wglMakeCurrent(gl.dc, temp_gl_context);
   
   PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
      (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");
   
   const int gl_attribs[] = 
   {
      WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
      WGL_CONTEXT_MINOR_VERSION_ARB, 5,
      WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
      WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
      0
   };
      
   HGLRC gl_context = wglCreateContextAttribsARB(gl.dc, 0, gl_attribs);
   wglMakeCurrent(gl.dc, gl_context);
   wglDeleteContext(temp_gl_context);
   
   OutputDebugStringA((char *) glGetString(GL_VERSION));
   OutputDebugStringA("\n");
   
   glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC) wglGetProcAddress("glCreateVertexArrays");
   glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) wglGetProcAddress("glDeleteVertexArrays");
   glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) wglGetProcAddress("glBindVertexArray");
   
   glCreateShader = (PFNGLCREATESHADERPROC) wglGetProcAddress("glCreateShader");
   glDeleteShader = (PFNGLDELETESHADERPROC) wglGetProcAddress("glDeleteShader");
   glShaderSource = (PFNGLSHADERSOURCEPROC) wglGetProcAddress("glShaderSource");
   glCompileShader = (PFNGLCOMPILESHADERPROC) wglGetProcAddress("glCompileShader");
   glGetShaderiv = (PFNGLGETSHADERIVPROC) wglGetProcAddress("glGetShaderiv");
   glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) wglGetProcAddress("glGetShaderInfoLog");

   glCreateProgram = (PFNGLCREATEPROGRAMPROC) wglGetProcAddress("glCreateProgram");
   glDeleteProgram = (PFNGLDELETEPROGRAMPROC) wglGetProcAddress("glDeleteProgram");
   glAttachShader = (PFNGLATTACHSHADERPROC) wglGetProcAddress("glAttachShader");
   glDetachShader = (PFNGLDETACHSHADERPROC) wglGetProcAddress("glDetachShader");
   glLinkProgram = (PFNGLLINKPROGRAMPROC) wglGetProcAddress("glLinkProgram");
   glUseProgram = (PFNGLUSEPROGRAMPROC) wglGetProcAddress("glUseProgram");
   glGetProgramiv = (PFNGLGETPROGRAMIVPROC) wglGetProcAddress("glGetProgramiv");
   glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) wglGetProcAddress("glGetProgramInfoLog");
   
   glCreateBuffers = (PFNGLCREATEBUFFERSPROC) wglGetProcAddress("glCreateBuffers");
   glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) wglGetProcAddress("glDeleteBuffers");
   glBindBuffer = (PFNGLBINDBUFFERPROC) wglGetProcAddress("glBindBuffer");
   glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC) wglGetProcAddress("glNamedBufferData");

   glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) wglGetProcAddress("glGetUniformLocation");
   glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) wglGetProcAddress("glUniformMatrix4fv");
   glUniform4fv = (PFNGLUNIFORM4FVPROC) wglGetProcAddress("glUniform4fv");
   
   glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC) wglGetProcAddress("glVertexArrayVertexBuffer");
   glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC) wglGetProcAddress("glEnableVertexArrayAttrib");
   glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC) wglGetProcAddress("glDisableVertexArrayAttrib");
   glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC) wglGetProcAddress("glVertexArrayAttribBinding");
   glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC) wglGetProcAddress("glVertexArrayAttribFormat");
   glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC) wglGetProcAddress("glVertexArrayElementBuffer");
   
   glUniform1i = (PFNGLUNIFORM1IPROC) wglGetProcAddress("glUniform1i");
   glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC) wglGetProcAddress("glBindTextureUnit");
   
   glCreateTextures = (PFNGLCREATETEXTURESPROC) wglGetProcAddress("glCreateTextures");
   glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC) wglGetProcAddress("glTextureStorage2D");
   glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC) wglGetProcAddress("glTextureSubImage2D");
   glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC) wglGetProcAddress("glTextureParameteri");

   glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) wglGetProcAddress("glDebugMessageCallback");
   glDebugMessageCallback((GLDEBUGPROC) opengl_debug_callback, NULL); //NOTE: this crashes in 32bit mode
   
   GLuint transformVert = loadShader("transform.vert", GL_VERTEX_SHADER);
   GLuint transformWithUVVert = loadShader("transform_UV.vert", GL_VERTEX_SHADER);
   GLuint colourFragment = loadShader("colour.frag", GL_FRAGMENT_SHADER);
   GLuint textureFragment = loadShader("texture.frag", GL_FRAGMENT_SHADER);
   GLuint sdfFragment = loadShader("sdf.frag", GL_FRAGMENT_SHADER);
   
   gl.col.handle = shaderProgram(transformVert, colourFragment);
   gl.col.matrix_uniform = glGetUniformLocation(gl.col.handle, "Matrix");
   gl.col.colour_uniform = glGetUniformLocation(gl.col.handle, "Colour");
   
   gl.tex.handle = shaderProgram(transformWithUVVert, textureFragment);
   gl.tex.matrix_uniform = glGetUniformLocation(gl.tex.handle, "Matrix");
   gl.tex.texture_uniform = glGetUniformLocation(gl.tex.handle, "Texture");
   
   gl.sdf.handle = shaderProgram(transformWithUVVert, sdfFragment);
   gl.sdf.matrix_uniform = glGetUniformLocation(gl.sdf.handle, "Matrix");
   gl.sdf.texture_uniform = glGetUniformLocation(gl.sdf.handle, "Texture");
   gl.sdf.colour_uniform = glGetUniformLocation(gl.sdf.handle, "Colour");
   
   glCreateVertexArrays(1, &gl.vao);
   glBindVertexArray(gl.vao);
   
   glVertexArrayAttribFormat(gl.vao, position_slot, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(gl.vao, position_slot, position_slot);
   
   glVertexArrayAttribFormat(gl.vao, uv_slot, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(gl.vao, uv_slot, uv_slot);
   
   glVertexArrayAttribFormat(gl.vao, colour_slot, 4, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(gl.vao, colour_slot, colour_slot);
   
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_SCISSOR_TEST);    

   bool running = true;
   ShowWindow(window, nCmdShow);
   UpdateWindow(window);
   
   initTheme();
   
   glCreateBuffers(3, gl.buffers);
   
   __temp_arena = VirtualAllocArena(Megabyte(4));

   UIContext ui_context = {};
   ui_context.frame_arena = VirtualAllocArena(Megabyte(2));
   ui_context.persistent_arena = VirtualAllocArena(Megabyte(2));
   ui_context.font = &font;
   
   DashboardState state = {};
   initDashboard(&state);   
      
   WSADATA winsock_data = {};
   WSAStartup(MAKEWORD(2, 2), &winsock_data);

   SOCKET _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   {
      struct sockaddr_in client_info = {};
      client_info.sin_family = AF_INET;
      client_info.sin_addr.s_addr = htonl(INADDR_ANY);
      client_info.sin_port = htons(5800);

      u_long non_blocking = true;
      ioctlsocket(_socket, FIONBIO, &non_blocking);
      bool bound = (bind(_socket, (struct sockaddr *) &client_info, sizeof(client_info)) != SOCKET_ERROR);
   }

   struct sockaddr_in server_info = {};
   
   //NOTE: test code, writes out a test auto run
   {
      buffer test_auto_run = PushTempBuffer(Megabyte(1));
      
      FileHeader numbers = header(AUTONOMOUS_RUN_MAGIC_NUMBER, AUTONOMOUS_RUN_CURR_VERSION);
      AutonomousRun_FileHeader header = {};
      header.robot_name_length = 4;
      header.subsystem_count = 2;
      header.robot_state_sample_count = 20;

      WriteStruct(&test_auto_run, &numbers);
      WriteStruct(&test_auto_run, &header);
      
      char name[] = "test";
      WriteArray(&test_auto_run, name, 4);
      
      AutonomousRun_RobotStateSample states[20] = {};
      for(u32 i = 0; i < 20; i++) {
         states[i] = {V2(1, 0), V2(1, 0), 0, i};
      }
      WriteArray(&test_auto_run, states, ArraySize(states));

      AutonomousRun_SubsystemDiagnostics subsystem_a = {11, 0};
      char subsystem_a_name[] = "subsystem_a";
      WriteStruct(&test_auto_run, &subsystem_a);
      WriteArray(&test_auto_run, subsystem_a_name, 11);

      AutonomousRun_SubsystemDiagnostics subsystem_b = {11, 0};
      char subsystem_b_name[] = "subsystem_b";
      WriteStruct(&test_auto_run, &subsystem_b);
      WriteArray(&test_auto_run, subsystem_b_name, 11);

      WriteEntireFile("test_auto_run.ncar", test_auto_run);
   }

   LARGE_INTEGER frequency;
   QueryPerformanceFrequency(&frequency); 

   LARGE_INTEGER timer;
   QueryPerformanceCounter(&timer);

   while(running) {
      LARGE_INTEGER new_time;
      QueryPerformanceCounter(&new_time);
      f32 dt = (f32)(new_time.QuadPart - timer.QuadPart) / (f32)frequency.QuadPart;
      timer = new_time;

      state.curr_time += dt;

      POINT cursor = {};
      GetCursorPos(&cursor);
      ScreenToClient(window, &cursor);
      
      ui_context.input_state.pos.x = cursor.x;
      ui_context.input_state.pos.y = cursor.y;
      ui_context.input_state.scroll = 0;
   
      ui_context.input_state.left_up = false;
      ui_context.input_state.right_up = false;

      ui_context.input_state.key_char = 0;
      ui_context.input_state.key_backspace = false;
      ui_context.input_state.key_enter = false;
      ui_context.input_state.key_up_arrow = false;
      ui_context.input_state.key_down_arrow = false;
      ui_context.input_state.key_left_arrow = false;
      ui_context.input_state.key_right_arrow = false;

      MSG msg = {};
      while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
         switch(msg.message) {
            case WM_QUIT:
               running = false;
               break;
         
            case WM_LBUTTONUP:
               ui_context.input_state.left_down = false;
               ui_context.input_state.left_up = true;
               break;
         
            case WM_LBUTTONDOWN:
               ui_context.input_state.left_down = true;
               ui_context.input_state.left_up = false;
               break;
               
            case WM_RBUTTONUP:
               ui_context.input_state.right_down = false;
               ui_context.input_state.right_up = true;
               break;
         
            case WM_RBUTTONDOWN:
               ui_context.input_state.right_down = true;
               ui_context.input_state.right_up = false;
               break;   

            case WM_CHAR: {
               switch(msg.wParam) {
                  case 0x08:
                     ui_context.input_state.key_backspace = true;
                     break;
                  case 0x0D:
                     ui_context.input_state.key_enter = true;
                     break;
                  case 0x0A:  // linefeed 
                  case 0x1B:  // escape 
                  case 0x09:  // tab 
                     break;
                  default:
                     ui_context.input_state.key_char = (char) msg.wParam;
                     break;
               }
            } break;

            case WM_KEYDOWN: {
               switch(msg.wParam) {
                  case VK_UP:
                     ui_context.input_state.key_up_arrow = true;
                     break;
                  case VK_DOWN:
                     ui_context.input_state.key_down_arrow = true;
                     break;
                  case VK_LEFT:
                     ui_context.input_state.key_left_arrow = true;
                     break;
                  case VK_RIGHT:
                     ui_context.input_state.key_right_arrow = true;
                     break;
               }
            } break;

            case WM_MOUSEWHEEL: {
               ui_context.input_state.scroll = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            } break;
         }
         
         TranslateMessage(&msg);
         DispatchMessageA(&msg);
      }

      bool has_packets = true;
      while(has_packets) {
         u8 buffer[1024] = {};
         struct sockaddr_in sender_info = {};
         s32 sender_info_size = sizeof(sender_info);

         s32 recv_return = recvfrom(_socket, (char *)buffer, ArraySize(buffer), 0,
                                    (struct sockaddr *) &sender_info, (int *)&sender_info_size);

         //TODO: check that sender_info equals server_info if connected to something

         if(recv_return == SOCKET_ERROR) {
            s32 wsa_error = WSAGetLastError();

            if(wsa_error == WSAEWOULDBLOCK) {
               has_packets = false;
            } else if(wsa_error != 0) {
               //an actual error happened
            }
         } else {
            if(HandlePacket(Buffer(ArraySize(buffer), buffer), &state)) {
               server_info = sender_info;
            }
         }
      }

      HandleConnectionStatus(&state);

      Reset(&__temp_arena);

      glScissor(0, 0, window_size.x, window_size.y);
      glClearColor(1, 1, 1, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
      mat4 transform = Orthographic(0, window_size.y, 0, window_size.x, 100, -100);
      
      element *root_element = beginFrame(window_size, &ui_context, dt);
      DrawUI(root_element, &state);
      DrawElement(root_element, transform, &gl);
      
      if(ui_context.tooltip.length > 0) {
         element tooltip_element = {};
         tooltip_element.context = &ui_context;
         
         Text(&tooltip_element, ui_context.tooltip, ui_context.input_state.pos - V2(0, 20), 20);
         DrawRenderCommandBuffer(tooltip_element.first_command,
                                 RectMinSize(V2(0, 0), window_size), transform, &gl);
      }

      SwapBuffers(gl.dc);
   }
   
   return 0;
}