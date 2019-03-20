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

#include "dashboard.cpp"
#include "network.cpp"

SOCKET tcp_socket;
bool was_connected = false;

void HandleConnectionStatus(DashboardState *state) {
   bool tcp_connected = true;
   if(recv(tcp_socket, NULL, 0, 0) == SOCKET_ERROR) {
      s32 wsa_error = WSAGetLastError();
      if(wsa_error == WSAENOTCONN) {
         tcp_connected = false;
      } else {
         //actual error or blocking
      }
   }
   
   if(was_connected && !tcp_connected) {
      HandleDisconnect(state);
   }

   was_connected = tcp_connected;

   if(!tcp_connected) {
      //TODO: check multiple possible address options
      struct sockaddr_in server_addr = {};
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = inet_addr("10.46.18.2"); 
      server_addr.sin_port = htons(5800);

      connect(tcp_socket, (SOCKADDR *) &server_addr, sizeof(server_addr));
   }
}

#include "test_file_writing.cpp"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
   //NOTE: 
   //we load app-crucial files (eg. shaders, font) from the directory that the exe is in, 
   //we load user files (eg. settings, saves) from the working directory   

   Win32CommonInit(PlatformAllocArena(Megabyte(10)));

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
   ui_context.frame_arena = PlatformAllocArena(Megabyte(2));
   ui_context.persistent_arena = PlatformAllocArena(Megabyte(2));
   ui_context.filedrop_arena = PlatformAllocArena(Megabyte(2));
   ui_context.font = &theme_font;

   DashboardState state = {};
   initDashboard(&state);   
      
   WSADATA winsock_data = {};
   WSAStartup(MAKEWORD(2, 2), &winsock_data);

   tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   {
      u_long non_blocking = true;
      ioctlsocket(tcp_socket, FIONBIO, &non_blocking);
   }

   //NOTE: test code, writes out a bunch of test files
   WriteTestFiles();

   Timer timer = InitTimer();
   
   while(PumpMessages(&window, &ui_context)) {
      f32 dt = GetDT(&timer);
      state.curr_time += dt;

      state.directory_changed = CheckFiles(&state.file_watcher);
      
      if(state.directory_changed) {
         OutputDebugStringA("Directory Changed\n");
      }

      Reset(&__temp_arena);

      {
         bool has_packets = true;
         while(has_packets) {
            //TODO: technically we can recieve like, half a header or something
            //      we should protect aginst that
            PacketHeader header = {};
            u32 recv_return = recv(tcp_socket, (char *) &header, sizeof(header), 0);
            
            if(recv_return == SOCKET_ERROR) {
               has_packets = false;
               s32 wsa_error = WSAGetLastError();

               if((wsa_error != 0) && (wsa_error != WSAEWOULDBLOCK)) {
                  //an actual error happened
               }
            } else {
               //TODO: what if the packet isnt fully sent yet or something
               buffer packet = PushTempBuffer(header.size);
               recv(tcp_socket, (char *) packet.data, packet.size, 0);

               HandlePacket(&state, (PacketType::type) header.type, packet);
            }
         }
      }
      
      HandleConnectionStatus(&state);

      Reset(&__temp_arena);
      
      element *root_element = beginFrame(window.size, &ui_context, dt);
      DrawUI(root_element, &state);
      endFrame(&window, root_element);
   }
   
   return 0;
}