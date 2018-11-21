void WriteTestFiles() {
   {
      buffer test_auto_run = PushTempBuffer(Megabyte(1));
      
      FileHeader numbers = header(ROBOT_RECORDING_MAGIC_NUMBER, ROBOT_RECORDING_CURR_VERSION);
      RobotRecording_FileHeader header = {};
      header.robot_name_length = 4;
      header.robot_width = 2;
      header.robot_length = 2;
      header.subsystem_count = 2;
      header.robot_state_sample_count = 20;

      WriteStruct(&test_auto_run, &numbers);
      WriteStruct(&test_auto_run, &header);
      
      char name[] = "test";
      WriteArray(&test_auto_run, name, 4);
      
      RobotRecording_RobotStateSample states[20] = {};
      for(u32 i = 0; i < 20; i++) {
         states[i] = {V2(1.0 + i, 0.5 * i), 0, (f32)i};
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

   // {
   //    buffer test_auto_proj = PushTempBuffer(Megabyte(1));

   //    FileHeader numbers = header(AUTONOMOUS_PROGRAM_MAGIC_NUMBER, AUTONOMOUS_PROGRAM_CURR_VERSION);
   //    AutonomousProgram_FileHeader header = {};
   //    header.variable_count = 3;
   //    WriteStruct(&test_auto_proj, &numbers);
   //    WriteStruct(&test_auto_proj, &header);

   //    AutonomousProgram_Node start = {};
   //    start.pos = V2(-10, -10);
   //    start.command_count = 0;
   //    start.path_count = 1;
   //    WriteStruct(&test_auto_proj, &start);

   //    AutonomousProgram_Path path1 = {};
   //    path1.control_point_count = 1;
   //    WriteStruct(&test_auto_proj, &path1);

   //    AutonomousProgram_Value accel_var = { 1 };
   //    u8 accel_var_len = 5;
   //    char accel_var_name[] = "accel";
   //    WriteStruct(&test_auto_proj, &accel_var);
   //    WriteStruct(&test_auto_proj, &accel_var_len);
   //    WriteArray(&test_auto_proj, accel_var_name, 5);

   //    AutonomousProgram_Value deccel_var = { 1 };
   //    u8 deccel_var_len = 6;
   //    char deccel_var_name[] = "deccel";
   //    WriteStruct(&test_auto_proj, &deccel_var);
   //    WriteStruct(&test_auto_proj, &deccel_var_len);
   //    WriteArray(&test_auto_proj, deccel_var_name, 6);

   //    AutonomousProgram_Value max_vel_var = { 1 };
   //    u8 max_vel_var_len = 3;
   //    char max_vel_var_name[] = "vel";
   //    WriteStruct(&test_auto_proj, &max_vel_var);
   //    WriteStruct(&test_auto_proj, &max_vel_var_len);
   //    WriteArray(&test_auto_proj, max_vel_var_name, 3);

   //    v2 control_points[] = {V2(10, 10)};
   //    WriteArray(&test_auto_proj, control_points, ArraySize(control_points));

   //    AutonomousProgram_Node end = {};
   //    end.pos = V2(10, 2);
   //    WriteStruct(&test_auto_proj, &end);

   //    WriteEntireFile("test_auto_proj.ncap", test_auto_proj);
   // }

   {
      buffer test_robot = PushTempBuffer(Megabyte(1));

      FileHeader numbers = header(ROBOT_PROFILE_MAGIC_NUMBER, ROBOT_PROFILE_CURR_VERSION);
      RobotProfile_FileHeader header = {};
      char robot_name[] = "test_robot";
      header.robot_name_length = ArraySize(robot_name) - 1;
      header.robot_width = 28/12;
      header.robot_length = 28/12;
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
}