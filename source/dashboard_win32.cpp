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

#if 0
SOCKET tcp_socket;
bool was_connected = false;
bool tcp_connected = false;
f32 last_recv_time = 0;

void SendPacket(buffer packet) {
   s32 sent = send(tcp_socket, (char *)packet.data, packet.offset, 0);
   // OutputDebugStringA(ToCString(Concat(Literal("Sent "), ToString(sent), Literal(" bytes\n"))));
}
#endif

#include "north_defs/north_common_definitions.h"
#include "north_defs/north_file_definitions.h"
#include "north_defs/north_network_definitions.h"

#include "north_shared/north_networking_win32.cpp"

#include "dashboard.cpp"
#include "network.cpp"

#if 0
void CreateSocket() {
   tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   u_long non_blocking = true;
   ioctlsocket(tcp_socket, FIONBIO, &non_blocking);
}

u32 sent_packet_count = 0;
void HandleConnectionStatus(DashboardState *state) {
   if(recv(tcp_socket, NULL, 0, 0) == SOCKET_ERROR) {
      s32 wsa_error = WSAGetLastError();
      if(wsa_error == WSAENOTCONN) {
         tcp_connected = false;
      } else {
         //actual error or blocking
      }
   }
   
   if((state->curr_time - last_recv_time) > 20 /*2*/) {
      tcp_connected = false;
   }

   if(was_connected && !tcp_connected) {
      sent_packet_count = 0;
      
      HandleDisconnect(state);
      OutputDebugStringA(ToCString(Literal("Disconnecting, t = ") + ToString(state->curr_time - last_recv_time) + Literal("\n")));
      
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
         // server_addr.sin_addr.s_addr = inet_addr("10.46.18.2");
         server_addr.sin_addr.s_addr = inet_addr("10.0.5.4");
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

void HandleNetworking(DashboardState *state) {
   bool has_packets = true;
   while(has_packets) {
      PacketHeader header = {};
      u32 recv_return = recv(tcp_socket, (char *) &header, sizeof(header), MSG_PEEK);
      
      if(recv_return == SOCKET_ERROR) {
         has_packets = false;
         s32 wsa_error = WSAGetLastError();

         if((wsa_error != 0) && (wsa_error != WSAEWOULDBLOCK)) {
            //an actual error happened
         }
      } else if(recv_return == sizeof(PacketHeader)) {
         buffer packet = PushTempBuffer(header.size + sizeof(PacketHeader));
         recv_return = recv(tcp_socket, (char *) packet.data, packet.size, MSG_PEEK);
         
         //--------------------------
         if(recv_return == packet.size) {
            sent_packet_count++;
         } else {
            OutputDebugStringA(ToCString(ToString(recv_return) + Literal(" Pending\n")));
         }
         OutputDebugStringA(ToCString(Concat(Literal((recv_return == packet.size) ? "Recv" : "Icmp"), ToString((u32)(header.size + sizeof(PacketHeader))), Literal(" ") + ToString(header.type), Literal(" #"), ToString(sent_packet_count), Literal("\n") )));
         if(header.type > 4) {
            int test = 12;
         }
         //--------------------------
         
         if(recv_return == packet.size) {
            recv(tcp_socket, (char *) packet.data, packet.size, 0);
            
            ConsumeStruct(&packet, PacketHeader);
            HandlePacket(state, (PacketType::type) header.type, packet);
         
            tcp_connected = true;
            last_recv_time = state->curr_time;
         } else {
            has_packets = false;
         }
      }
   }

   HandleConnectionStatus(state);
}
#endif

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
   // tcp_buffer = Buffer(???);

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