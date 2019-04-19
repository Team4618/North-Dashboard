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

SOCKET tcp_socket;
bool was_connected = false;
bool tcp_connected = false;
f32 last_recv_time = 0;

void SendPacket(buffer packet) {
   s32 sent = send(tcp_socket, (char *)packet.data, packet.offset, 0);
   // OutputDebugStringA(ToCString(Concat(Literal("Sent "), ToString(sent), Literal(" bytes\n"))));
}

#include "dashboard.cpp"
#include "network.cpp"

void CreateSocket() {
   tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   u_long non_blocking = true;
   ioctlsocket(tcp_socket, FIONBIO, &non_blocking);
}

void HandleConnectionStatus(DashboardState *state) {
   if(recv(tcp_socket, NULL, 0, 0) == SOCKET_ERROR) {
      s32 wsa_error = WSAGetLastError();
      if(wsa_error == WSAENOTCONN) {
         tcp_connected = false;
      } else {
         //actual error or blocking
      }
   }
   
   if((state->curr_time - last_recv_time) > 2)
      tcp_connected = false; 

   if(was_connected && !tcp_connected) {
      HandleDisconnect(state);
      
      closesocket(tcp_socket);
      CreateSocket();
   }

   was_connected = tcp_connected;

   if(!tcp_connected) {
      string team_number_string = ToString(state->settings.team_number);
      
      if(state->settings.team_number == 0) {
         struct sockaddr_in server_addr = {};
         server_addr.sin_family = AF_INET;
         server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
         server_addr.sin_port = htons(5800);

         if(connect(tcp_socket, (SOCKADDR *) &server_addr, sizeof(server_addr)) != 0) {
            // OutputDebugStringA(ToCString(Concat( ToString(WSAGetLastError()), Literal("\n") )));
         }
      } else if(team_number_string.length == 4) {
         //TODO: check multiple possible address options
         struct sockaddr_in server_addr = {};
         server_addr.sin_family = AF_INET;
         //TODO: actually generate the ip from the team number string
         server_addr.sin_addr.s_addr = inet_addr("10.46.18.2"); 
         server_addr.sin_port = htons(5800);

         if(connect(tcp_socket, (SOCKADDR *) &server_addr, sizeof(server_addr)) != 0) {
            // OutputDebugStringA(ToCString(Concat( ToString(WSAGetLastError()), Literal("\n") )));
         }
      }
      
      //TODO: support non 4 digit team numbers
   } else {
      //NOTE: send to maintain connection
      PacketHeader heartbeat = {0, PacketType::Heartbeat};
      send(tcp_socket, (char *) &heartbeat, sizeof(heartbeat), 0);
   }
}

#include "test_file_writing.cpp"

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

      Reset(__temp_arena);

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
               // OutputDebugStringA(ToCString(Concat(Literal("Recieved Packet, Size = "), ToString(header.size), Literal("\n") )));

               buffer packet = PushTempBuffer(header.size);
               recv(tcp_socket, (char *) packet.data, packet.size, 0);

               HandlePacket(&state, (PacketType::type) header.type, packet);
               
               tcp_connected = true;
               last_recv_time = state.curr_time;
            }
         }
      }
      
      HandleConnectionStatus(&state);

      Reset(__temp_arena);
      
      element *root_element = beginFrame(window.size, &ui_context, dt);
      DrawUI(root_element, &state);
      endFrame(&window, root_element);
   }
   
   return 0;
}