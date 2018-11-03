#include "winsock2.h"
#include "Ws2tcpip.h"
#include "windows.h"
#include "gl/gl.h"
#include "lib/wglext.h"
#include "lib/glext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define COMMON_PLATFORM
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
v2 window_size = V2(0, 0);
#include "lib/ui_impl_win32_opengl.cpp"

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
      
      FileHeader numbers = header(ROBOT_RECORDING_MAGIC_NUMBER, ROBOT_RECORDING_CURR_VERSION);
      RobotRecording_FileHeader header = {};
      header.robot_name_length = 4;
      header.subsystem_count = 2;
      header.robot_state_sample_count = 20;

      WriteStruct(&test_auto_run, &numbers);
      WriteStruct(&test_auto_run, &header);
      
      char name[] = "test";
      WriteArray(&test_auto_run, name, 4);
      
      RobotRecording_RobotStateSample states[20] = {};
      for(u32 i = 0; i < 20; i++) {
         states[i] = {V2(1.0 + i, 0.5 * i), V2(1, 0), 0, (f32)i};
      }
      WriteArray(&test_auto_run, states, ArraySize(states));

      RobotRecording_SubsystemDiagnostics subsystem_a = {11, 2};
      char subsystem_a_name[] = "subsystem_a";
      WriteStruct(&test_auto_run, &subsystem_a);
      WriteArray(&test_auto_run, subsystem_a_name, 11);

      //------------
      RobotRecording_Diagnostic diag_1 = {5, 1, 10};
      char diag_1_name[] = "diag1";
      RobotRecording_DiagnosticSample diag_1_samples[10] = {};
      for(u32 i = 0; i < 10; i++) {
         diag_1_samples[i] = {(f32)i, 2.0f * i};
      }

      RobotRecording_Diagnostic diag_2 = {5, 1, 10};
      char diag_2_name[] = "diag2";
      RobotRecording_DiagnosticSample diag_2_samples[10] = {};
      for(u32 i = 0; i < 10; i++) {
         diag_2_samples[i] = {(f32)i, (f32)i};
      }
      //------------

      WriteStruct(&test_auto_run, &diag_1);
      WriteArray(&test_auto_run, diag_1_name, 5);
      WriteArray(&test_auto_run, diag_1_samples, 10);

      WriteStruct(&test_auto_run, &diag_2);
      WriteArray(&test_auto_run, diag_2_name, 5);
      WriteArray(&test_auto_run, diag_2_samples, 10);

      RobotRecording_SubsystemDiagnostics subsystem_b = {11, 2};
      char subsystem_b_name[] = "subsystem_b";
      WriteStruct(&test_auto_run, &subsystem_b);
      WriteArray(&test_auto_run, subsystem_b_name, 11);

      WriteStruct(&test_auto_run, &diag_1);
      WriteArray(&test_auto_run, diag_1_name, 5);
      WriteArray(&test_auto_run, diag_1_samples, 10);

      WriteStruct(&test_auto_run, &diag_2);
      WriteArray(&test_auto_run, diag_2_name, 5);
      WriteArray(&test_auto_run, diag_2_samples, 10);

      WriteEntireFile("test_auto_run.ncrr", test_auto_run);
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

   {
      buffer test_robot = PushTempBuffer(Megabyte(1));

      FileHeader numbers = header(ROBOT_PROFILE_MAGIC_NUMBER, ROBOT_PROFILE_CURR_VERSION);
      RobotProfile_FileHeader header = {};
      char robot_name[] = "test_bot";
      header.robot_name_length = ArraySize(robot_name) - 1;
      header.robot_width = 28;
      header.robot_length = 28;
      header.subsystem_count = 1;
      WriteStruct(&test_robot, &numbers);
      WriteStruct(&test_robot, &header);
      WriteArray(&test_robot, robot_name, header.robot_name_length);      

      RobotProfile_SubsystemDescription test_subsystem = {};
      char test_subsystem_name[] = "subsystem_a";
      test_subsystem.name_length = ArraySize(test_subsystem_name) - 1;
      test_subsystem.parameter_count = 3;
      test_subsystem.command_count = 1;
      WriteStruct(&test_robot, &test_subsystem);
      WriteArray(&test_robot, test_subsystem_name, test_subsystem.name_length);

      f32 test_value = 1;

      RobotProfile_Parameter test_p = {};
      char test_p_name[] = "P";
      test_p.name_length = ArraySize(test_p_name) - 1;
      WriteStruct(&test_robot, &test_p);
      WriteArray(&test_robot, test_p_name, test_p.name_length);
      WriteStruct(&test_robot, &test_value);

      RobotProfile_Parameter test_i = {};
      char test_i_name[] = "I";
      test_i.name_length = ArraySize(test_i_name) - 1;
      WriteStruct(&test_robot, &test_i);
      WriteArray(&test_robot, test_i_name, test_i.name_length);
      WriteStruct(&test_robot, &test_value);

      RobotProfile_Parameter test_d = {};
      char test_d_name[] = "D";
      test_d.name_length = ArraySize(test_d_name) - 1;
      WriteStruct(&test_robot, &test_d);
      WriteArray(&test_robot, test_d_name, test_d.name_length);
      WriteStruct(&test_robot, &test_value);

      RobotProfile_SubsystemCommand test_command = {};
      char test_command_name[] = "do_the_thing";
      test_command.name_length = ArraySize(test_command_name) - 1;
      test_command.param_count = 2;
      WriteStruct(&test_robot, &test_command);
      WriteArray(&test_robot, test_command_name, test_command.name_length);

      char param_name[] = "test_param";
      u8 param_name_length = ArraySize(param_name) - 1;
      WriteStruct(&test_robot, &param_name_length);
      WriteArray(&test_robot, param_name, param_name_length);
      WriteStruct(&test_robot, &param_name_length);
      WriteArray(&test_robot, param_name, param_name_length);

      WriteEntireFile("test_robot.ncrp", test_robot);
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