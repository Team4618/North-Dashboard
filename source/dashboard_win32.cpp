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
#include "lib/ui_misc_utils.cpp"

#include "string_and_lexer.cpp"
v2 window_size = V2(0, 0);
#include "lib/ui_impl_win32_opengl.cpp"

#include "dashboard.cpp"
#include "network.cpp"

SOCKET tcp_socket;
SOCKET udp_socket;
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
   ui_context.font = &font;
   
   DashboardState state = {};
   initDashboard(&state);   
      
   WSADATA winsock_data = {};
   WSAStartup(MAKEWORD(2, 2), &winsock_data);

   tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   {
      u_long non_blocking = true;
      ioctlsocket(tcp_socket, FIONBIO, &non_blocking);
   }

   udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   {
      u_long non_blocking = true;
      ioctlsocket(udp_socket, FIONBIO, &non_blocking);

      struct sockaddr_in client_info = {};
      client_info.sin_family = AF_INET;
      client_info.sin_addr.s_addr = htonl(INADDR_ANY);
      client_info.sin_port = htons(5801);

      bool bound = (bind(udp_socket, (struct sockaddr *) &client_info, sizeof(client_info)) != SOCKET_ERROR);
   }

   struct sockaddr_in server_info = {};
   
   //NOTE: test code, writes out a bunch of test files
   WriteTestFiles();

   LARGE_INTEGER frequency;
   QueryPerformanceFrequency(&frequency); 

   LARGE_INTEGER timer;
   QueryPerformanceCounter(&timer);

   while(PumpMessages(&window, &ui_context)) {
      LARGE_INTEGER new_time;
      QueryPerformanceCounter(&new_time);
      f32 dt = (f32)(new_time.QuadPart - timer.QuadPart) / (f32)frequency.QuadPart;
      timer = new_time;

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
      
      {
         bool has_packets = true;
         while(has_packets) {
            //TODO: 
            /*
            this is super janky, we can get half packets and stuff like that,
            we might not even be getting 2 packets from the same persion
            */
            PacketHeader header = {};
            struct sockaddr_in sender_info = {};
            
            u32 recv_return = recvfrom(udp_socket, (char *) &header, sizeof(header), 0,
                                       (struct sockaddr *) &sender_info, NULL);
            
            if(recv_return == SOCKET_ERROR) {
               has_packets = false;
               s32 wsa_error = WSAGetLastError();

               if((wsa_error != 0) && (wsa_error != WSAEWOULDBLOCK)) {
                  //an actual error happened
               }
            } else {
               buffer packet = PushTempBuffer(header.size);
               recvfrom(udp_socket, (char *) packet.data, packet.size, 0, NULL, NULL);

               HandlePacket(&state, (PacketType::type) header.type, packet);
            }
         }
      }
      
      HandleConnectionStatus(&state);

      Reset(&__temp_arena);
      
      element *root_element = beginFrame(window_size, &ui_context, dt);
      DrawUI(root_element, &state);
      endFrame(root_element);
   }
   
   return 0;
}