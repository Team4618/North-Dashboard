#include "winsock2.h"
#include "Ws2tcpip.h"
#include "windows.h"
#include "gl/gl.h"
#include "lib/wglext.h"
#include "lib/glext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "lib/stb_truetype.h"

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

#include "lib/ui_impl_win32_opengl.cpp"

#include "north_defs/north_common_definitions.h"
#include "north_defs/north_file_definitions.h"
#include "north_defs/north_network_definitions.h"

#include "north_shared/north_networking_win32.cpp"

#include "dashboard.cpp"
#include "network.cpp"

// #include "test_file_writing.cpp"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
   //NOTE: 
   //we load app-crucial files (eg. shaders, font) from the directory that the exe is in, 
   //we load user files (eg. settings, saves) from the working directory   

   Win32CommonInit(PlatformAllocArena(Megabyte(10), "Temp"));

   ui_impl_win32_window window = createWindow("North Dashboard");
   
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

   initTheme();

   UIContext ui_context = {};
   ui_context.frame_arena = PlatformAllocArena(Megabyte(2), "UI Frame");
   ui_context.persistent_arena = PlatformAllocArena(Megabyte(2), "UI Persistent");
   ui_context.filedrop_arena = PlatformAllocArena(Megabyte(2), "UI Filedrop");
   ui_context.font = &theme_font;

   DashboardState state = {};
   initDashboard(&state);   
      
   WSADATA winsock_data = {};
   WSAStartup(MAKEWORD(2, 2), &winsock_data);
   CreateSocket();

   MemoryArena *net_arena = PlatformAllocArena(Megabyte(2), "Net Arena");
   tcp_send_buffer = PushBuffer(net_arena, Megabyte(1));
   tcp_recv_buffer = PushBuffer(net_arena, Megabyte(1));

   //NOTE: test code, writes out a bunch of test files
   // WriteTestFiles();

   Timer timer = InitTimer();
   
   while(PumpMessages(&window, &ui_context)) {
      f32 dt = GetDT(&timer);
      state.curr_time += dt;

      state.directory_changed = CheckFiles(&state.file_watcher);
      
      if(state.directory_changed) {
         OutputDebugStringA("Directory Changed\n");
      }

      Reset(__temp_arena);
      DrainNetStream();
      SendQueuedPackets();

      PacketHeader header = {};
      buffer packet = {};
      while(HasPackets(ui_context.curr_time, &header, &packet)) {
         HandlePacket(&state, (PacketType::type) header.type, packet);
      }

      if(HandleConnectionStatus((state.settings.team_number == 0) ? "127.0.0.1" : "10.0.5.4", ui_context.curr_time)) {
         HandleDisconnect(&state);
      }

      Reset(__temp_arena);
      element *root_element = beginFrame(window.size, &ui_context, dt);
      DrawUI(root_element, &state);
      endFrame(&window, root_element);
   }
   
   return 0;
}