#include "winsock2.h"
#include "Ws2tcpip.h"
#include "windows.h"
#include "gl/gl.h"
#include "wglext.h"
#include "glext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "lib/common.cpp"
#include "lib/ui_core.cpp"
#include "lib/ui_button.cpp"
#include "lib/ui_textbox.cpp"
#include "lib/ui_checkbox.cpp"
#include "lib/ui_vertical_list.cpp"
#include "lib/ui_horizontal_slider.cpp"
#include "lib/ui_multiline_graph.cpp"
#include "lib/ui_field_topdown.cpp"
#include "lib/ui_misc_utils.cpp"

#include "string_and_lexer.cpp"
#include "lib/ui_impl_win32_opengl.cpp"

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

v2 window_size = V2(0, 0);

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

#include "dashboard.cpp"
#include "network.cpp"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
   //NOTE: 
   //we load app-crucial files (eg. shaders, font) from the directory that the exe is in, 
   //we load user files (eg. settings, saves) from the working directory   

   __temp_arena = PlatformAllocArena(Megabyte(10));

   char exepath[MAX_PATH + 1];
   if(0 == GetModuleFileNameA(0, exepath, MAX_PATH + 1))
      Assert(false);
   exe_directory = Literal(exepath);
   for(u32 i = exe_directory.length - 1; i >= 0; i--) {
      if((exe_directory.text[i] == '\\') || (exe_directory.text[i] == '/'))
         break;

      exe_directory.length--;
   }

   ui_impl_win32_window window = createWindow("North Dashboard", WindowMessageEvent);
   
   //TODO: does this load exe-relative or from the working directory?
   HANDLE hIcon = LoadImageA(0, "icon.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
   if(hIcon) {
       SendMessageA(window.handle, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
       SendMessageA(window.handle, WM_SETICON, ICON_BIG, (LPARAM) hIcon);

       SendMessageA(GetWindow(window.handle, GW_OWNER), WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
       SendMessageA(GetWindow(window.handle, GW_OWNER), WM_SETICON, ICON_BIG, (LPARAM) hIcon);
   } else {
      MessageBox(window.handle, "Error", "Icon Not Found", 0);
   }

   bool running = true;

   ShowWindow(window.handle, nCmdShow);
   UpdateWindow(window.handle);

   initTheme();

   UIContext ui_context = {};
   ui_context.frame_arena = PlatformAllocArena(Megabyte(2));
   ui_context.persistent_arena = PlatformAllocArena(Megabyte(2));
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
         states[i] = {V2(1 + i, 0.5 * i), V2(1, 0), 0, i};
      }
      WriteArray(&test_auto_run, states, ArraySize(states));

      AutonomousRun_SubsystemDiagnostics subsystem_a = {11, 2};
      char subsystem_a_name[] = "subsystem_a";
      WriteStruct(&test_auto_run, &subsystem_a);
      WriteArray(&test_auto_run, subsystem_a_name, 11);

      //------------
      AutonomousRun_Diagnostic diag_1 = {5, 1, 10};
      char diag_1_name[] = "diag1";
      AutonomousRun_DiagnosticSample diag_1_samples[10] = {};
      for(u32 i = 0; i < 10; i++) {
         diag_1_samples[i] = {i, 2 * i};
      }

      AutonomousRun_Diagnostic diag_2 = {5, 1, 10};
      char diag_2_name[] = "diag2";
      AutonomousRun_DiagnosticSample diag_2_samples[10] = {};
      for(u32 i = 0; i < 10; i++) {
         diag_2_samples[i] = {i, i};
      }
      //------------

      WriteStruct(&test_auto_run, &diag_1);
      WriteArray(&test_auto_run, diag_1_name, 5);
      WriteArray(&test_auto_run, diag_1_samples, 10);

      WriteStruct(&test_auto_run, &diag_2);
      WriteArray(&test_auto_run, diag_2_name, 5);
      WriteArray(&test_auto_run, diag_2_samples, 10);

      AutonomousRun_SubsystemDiagnostics subsystem_b = {11, 2};
      char subsystem_b_name[] = "subsystem_b";
      WriteStruct(&test_auto_run, &subsystem_b);
      WriteArray(&test_auto_run, subsystem_b_name, 11);

      WriteStruct(&test_auto_run, &diag_1);
      WriteArray(&test_auto_run, diag_1_name, 5);
      WriteArray(&test_auto_run, diag_1_samples, 10);

      WriteStruct(&test_auto_run, &diag_2);
      WriteArray(&test_auto_run, diag_2_name, 5);
      WriteArray(&test_auto_run, diag_2_samples, 10);

      WriteEntireFile("test_auto_run.ncar", test_auto_run);
   }

   {
      buffer test_auto_proj = PushTempBuffer(Megabyte(1));

      FileHeader numbers = header(AUTONOMOUS_PROGRAM_MAGIC_NUMBER, AUTONOMOUS_PROGRAM_CURR_VERSION);
      AutonomousProgram_FileHeader header = {};
      header.variable_count = 3;
      WriteStruct(&test_auto_proj, &numbers);
      WriteStruct(&test_auto_proj, &header);

      AutonomousProgram_Variable vmax = {3, 10};
      char vmax_name[] = "vel";
      WriteStruct(&test_auto_proj, &vmax);
      WriteArray(&test_auto_proj, vmax_name, 3);

      AutonomousProgram_Variable accel = {5, 10};
      char accel_name[] = "accel";
      WriteStruct(&test_auto_proj, &accel);
      WriteArray(&test_auto_proj, accel_name, 5);

      AutonomousProgram_Variable deccel = {6, 10};
      char deccel_name[] = "deccel";
      WriteStruct(&test_auto_proj, &deccel);
      WriteArray(&test_auto_proj, deccel_name, 6);

      AutonomousProgram_Node start = {};
      start.pos = V2(-10, -10);
      start.command_count = 0;
      start.path_count = 1;
      WriteStruct(&test_auto_proj, &start);

      AutonomousProgram_Path path1 = {};
      path1.control_point_count = 1;
      WriteStruct(&test_auto_proj, &path1);

      AutonomousProgram_Value accel_var = { 1 };
      u8 accel_var_len = 5;
      char accel_var_name[] = "accel";
      WriteStruct(&test_auto_proj, &accel_var);
      WriteStruct(&test_auto_proj, &accel_var_len);
      WriteArray(&test_auto_proj, accel_var_name, 5);

      AutonomousProgram_Value deccel_var = { 1 };
      u8 deccel_var_len = 6;
      char deccel_var_name[] = "deccel";
      WriteStruct(&test_auto_proj, &deccel_var);
      WriteStruct(&test_auto_proj, &deccel_var_len);
      WriteArray(&test_auto_proj, deccel_var_name, 6);

      AutonomousProgram_Value max_vel_var = { 1 };
      u8 max_vel_var_len = 3;
      char max_vel_var_name[] = "vel";
      WriteStruct(&test_auto_proj, &max_vel_var);
      WriteStruct(&test_auto_proj, &max_vel_var_len);
      WriteArray(&test_auto_proj, max_vel_var_name, 3);

      v2 control_points[] = {V2(10, 10)};
      WriteArray(&test_auto_proj, control_points, ArraySize(control_points));

      AutonomousProgram_Node end = {};
      end.pos = V2(10, 2);
      WriteStruct(&test_auto_proj, &end);

      WriteEntireFile("test_auto_proj.ncap", test_auto_proj);
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
      ScreenToClient(window.handle, &cursor);
      
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

      //TODO: directory change notifications
      state.directory_changed = false;

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