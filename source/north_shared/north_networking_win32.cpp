SOCKET tcp_socket;
bool was_connected = false;
bool tcp_connected = false;
f32 last_recv_time = 0;

buffer tcp_send_buffer = {};
buffer tcp_recv_buffer = {};

void SendPacket(buffer packet) {
   s32 sent = send(tcp_socket, (char *)packet.data, packet.offset, 0);
   // OutputDebugStringA(ToCString(Concat(Literal("Sent "), ToString(sent), Literal(" bytes\n"))));
}

void CreateSocket() {
   tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   u_long non_blocking = true;
   ioctlsocket(tcp_socket, FIONBIO, &non_blocking);

   /*
   int rcvbuf_size = Kilobyte(512);
   int setsockopt_recv = setsockopt(tcp_socket, SOL_SOCKET, SO_RCVBUF,
                                    (char *)&rcvbuf_size, sizeof(rcvbuf_size));

   if(setsockopt_recv != 0) {
      s32 wsa_error = WSAGetLastError();
      u32 test = 0;
   }
   */
}

u32 sent_packet_count = 0;
u32 recv_data_size = 0;

bool HandleConnectionStatus(char *connection_target, f32 curr_time) {
   if(recv(tcp_socket, NULL, 0, 0) == SOCKET_ERROR) {
      s32 wsa_error = WSAGetLastError();
      if(wsa_error == WSAENOTCONN) {
         tcp_connected = false;
      } else {
         //actual error or blocking
      }
   }
   
   if((curr_time - last_recv_time) > 2) {
      tcp_connected = false;
   }

   bool disconnected = false;
   if(was_connected && !tcp_connected) {
      sent_packet_count = 0;
      recv_data_size = 0;

      disconnected = true;
      OutputDebugStringA(ToCString("Disconnecting, t = " + ToString(curr_time - last_recv_time) + "\n"));

      tcp_recv_buffer.offset = 0;

      closesocket(tcp_socket);
      CreateSocket();
   }

   was_connected = tcp_connected;

   if(!tcp_connected) {
      struct sockaddr_in server_addr = {};
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = inet_addr(connection_target); 
      server_addr.sin_port = htons(5800);

      if(connect(tcp_socket, (SOCKADDR *) &server_addr, sizeof(server_addr)) != 0) {
         // OutputDebugStringA(ToCString(Concat( ToString(WSAGetLastError()), Literal("\n") )));
      }
   } else {
      //NOTE: send to maintain connection
      PacketHeader heartbeat = {0, PacketType::Heartbeat};
      send(tcp_socket, (char *) &heartbeat, sizeof(heartbeat), 0);
   }

   return disconnected;
}

void DrainNetStream() {
   bool has_data = true;
   while(has_data) {
      if((tcp_recv_buffer.size - tcp_recv_buffer.offset) == 0) {
         OutputDebugStringA("TCP Buffer Full\n");
         //TODO: kill the connection
         break;
      }
      
      u32 recv_return = recv(tcp_socket, (char *) (tcp_recv_buffer.data + tcp_recv_buffer.offset), 
                                         (tcp_recv_buffer.size - tcp_recv_buffer.offset), 0);
      
      if(recv_return == SOCKET_ERROR) {
         s32 wsa_error = WSAGetLastError();

         if(wsa_error == 0) {

         } else if(wsa_error == WSAEWOULDBLOCK) {

         } else if(wsa_error == WSAENOTCONN) {

         } else {
            //an actual error happened
            u32 test = 0;
         }

         has_data = false;
      } else if(recv_return == 0) {
         has_data = false;
      } else {
         tcp_recv_buffer.offset += recv_return;
         recv_data_size += recv_return;
      }
   }

   // OutputDebugStringA(ToCString("Recieved" + ToString(recv_data_size) + "\n"));
}

void SendQueuedPackets() {
   //TODO: write into tcp_send_buffer
   //allocate more memory for the buffer if needed?
}

u64 Remaining(buffer b) {
   return b.size - b.offset;
}

bool HasPackets(f32 curr_time, PacketHeader *header, buffer *packet) {
   buffer buf = Buffer(tcp_recv_buffer.offset, tcp_recv_buffer.data);
   
   if(Remaining(buf) >= sizeof(PacketHeader)) {
      *header = *PeekStruct(&buf, PacketHeader);
      u32 total_size = sizeof(PacketHeader) + header->size;

      //--------------------------
      if(Remaining(buf) >= total_size) {
         sent_packet_count++;
      } else {
         OutputDebugStringA(ToCString(ToString((u32)Remaining(buf)) + Literal(" Pending, ")));
      }
      OutputDebugStringA(ToCString((Remaining(buf) >= total_size ? "Recv " : "Icmp ") + ToString((u32)(header->size + sizeof(PacketHeader))) + " " + ToString(header->type) + " #" + ToString(sent_packet_count) + "\n"));
      if((header->type > 4) || (header->type == 0)) {
         int test = 12;
      }
      //--------------------------

      if(Remaining(buf) >= total_size) {
         *packet = Buffer(total_size, PushCopy(__temp_arena, 
                          ConsumeSize(&buf, total_size), total_size));
         
         ConsumeStruct(packet, PacketHeader);

         Advance(&tcp_recv_buffer, total_size);
         tcp_connected = true;
         last_recv_time = curr_time;
         return true;
      }
   }

   return false;
   
   // u32 recv_return = recv(tcp_socket, (char *) header, sizeof(PacketHeader), MSG_PEEK);
   
   // if(recv_return == SOCKET_ERROR) {
   //    s32 wsa_error = WSAGetLastError();

   //    if((wsa_error != 0) && (wsa_error != WSAEWOULDBLOCK)) {
   //       //an actual error happened
   //    }

   //    return false;
   // } else if(recv_return == sizeof(PacketHeader)) {
   //    *packet = PushTempBuffer(header->size + sizeof(PacketHeader));
   //    recv_return = recv(tcp_socket, (char *) packet->data, packet->size, MSG_PEEK);
      
   //    //--------------------------
   //    if(recv_return == packet->size) {
   //       sent_packet_count++;
   //    } else {
   //       OutputDebugStringA(ToCString(ToString(recv_return) + Literal(" Pending\n")));
   //    }
   //    OutputDebugStringA(ToCString((recv_return == packet->size ? "Recv" : "Icmp") + ToString((u32)(header->size + sizeof(PacketHeader))) + " " + ToString(header->type) + " #" + ToString(sent_packet_count) + "\n"));
   //    if(header->type > 4) {
   //       int test = 12;
   //    }
   //    //--------------------------

   //    if(recv_return == packet->size) {
   //       recv(tcp_socket, (char *) packet->data, packet->size, 0);
         
   //       ConsumeStruct(packet, PacketHeader);
         
   //       tcp_connected = true;
   //       last_recv_time = curr_time;
   //       return true;
   //    } else {
   //       return false;
   //    }
   // }

   // return false;
}

#if 0
bool HasPackets(f32 curr_time, PacketHeader *header, buffer *packet) {
   u32 recv_return = recv(tcp_socket, (char *) header, sizeof(PacketHeader), MSG_PEEK);
   
   if(recv_return == SOCKET_ERROR) {
      s32 wsa_error = WSAGetLastError();

      if((wsa_error != 0) && (wsa_error != WSAEWOULDBLOCK)) {
         //an actual error happened
      }

      return false;
   } else if(recv_return == sizeof(PacketHeader)) {
      *packet = PushTempBuffer(header->size + sizeof(PacketHeader));
      recv_return = recv(tcp_socket, (char *) packet->data, packet->size, MSG_PEEK);
      
      //--------------------------
      if(recv_return == packet->size) {
         sent_packet_count++;
      } else {
         OutputDebugStringA(ToCString(ToString(recv_return) + Literal(" Pending\n")));
      }
      OutputDebugStringA(ToCString((recv_return == packet->size ? "Recv" : "Icmp") + ToString((u32)(header->size + sizeof(PacketHeader))) + " " + ToString(header->type) + " #" + ToString(sent_packet_count) + "\n"));
      if(header->type > 4) {
         int test = 12;
      }
      //--------------------------

      if(recv_return == packet->size) {
         recv(tcp_socket, (char *) packet->data, packet->size, 0);
         
         ConsumeStruct(packet, PacketHeader);
         
         tcp_connected = true;
         last_recv_time = curr_time;
         return true;
      } else {
         return false;
      }
   }

   return false;
}
#endif