void WriteTestFiles() {
   // {
   //    buffer test_auto_run = PushTempBuffer(Megabyte(1));
      
   //    FileHeader numbers = header(ROBOT_RECORDING_MAGIC_NUMBER, ROBOT_RECORDING_CURR_VERSION);
   //    RobotRecording_FileHeader header = {};
   //    header.robot_name_length = 4;
   //    header.robot_width = 2;
   //    header.robot_length = 2;
   //    header.subsystem_count = 2;
   //    header.robot_state_sample_count = 20;

   //    WriteStruct(&test_auto_run, &numbers);
   //    WriteStruct(&test_auto_run, &header);
      
   //    char name[] = "test";
   //    WriteArray(&test_auto_run, name, 4);
      
   //    RobotRecording_RobotStateSample states[20] = {};
   //    for(u32 i = 0; i < 20; i++) {
   //       states[i] = {V2(1.0 + i, 0.5 * i), 0, (f32)i};
   //    }
   //    WriteArray(&test_auto_run, states, ArraySize(states));

   //    RobotRecording_SubsystemDiagnostics subsystem_a = {11, 2};
   //    char subsystem_a_name[] = "subsystem_a";
   //    WriteStruct(&test_auto_run, &subsystem_a);
   //    WriteArray(&test_auto_run, subsystem_a_name, 11);

   //    //------------
   //    RobotRecording_Diagnostic diag_1 = {5, 1, 10};
   //    char diag_1_name[] = "diag1";
   //    RobotRecording_DiagnosticSample diag_1_samples[10] = {};
   //    for(u32 i = 0; i < 10; i++) {
   //       diag_1_samples[i] = {(f32)i, 2.0f * i};
   //    }

   //    RobotRecording_Diagnostic diag_2 = {5, 1, 10};
   //    char diag_2_name[] = "diag2";
   //    RobotRecording_DiagnosticSample diag_2_samples[10] = {};
   //    for(u32 i = 0; i < 10; i++) {
   //       diag_2_samples[i] = {(f32)i, (f32)i};
   //    }
   //    //------------

   //    WriteStruct(&test_auto_run, &diag_1);
   //    WriteArray(&test_auto_run, diag_1_name, 5);
   //    WriteArray(&test_auto_run, diag_1_samples, 10);

   //    WriteStruct(&test_auto_run, &diag_2);
   //    WriteArray(&test_auto_run, diag_2_name, 5);
   //    WriteArray(&test_auto_run, diag_2_samples, 10);

   //    RobotRecording_SubsystemDiagnostics subsystem_b = {11, 2};
   //    char subsystem_b_name[] = "subsystem_b";
   //    WriteStruct(&test_auto_run, &subsystem_b);
   //    WriteArray(&test_auto_run, subsystem_b_name, 11);

   //    WriteStruct(&test_auto_run, &diag_1);
   //    WriteArray(&test_auto_run, diag_1_name, 5);
   //    WriteArray(&test_auto_run, diag_1_samples, 10);

   //    WriteStruct(&test_auto_run, &diag_2);
   //    WriteArray(&test_auto_run, diag_2_name, 5);
   //    WriteArray(&test_auto_run, diag_2_samples, 10);

   //    WriteEntireFile("test_auto_run.ncrr", test_auto_run);
   // }

   {
      buffer test_robot = PushTempBuffer(Megabyte(1));

      FileHeader numbers = header(ROBOT_PROFILE_MAGIC_NUMBER, ROBOT_PROFILE_CURR_VERSION);
      RobotProfile_FileHeader header = {};
      header.robot_width = 28/12;
      header.robot_length = 28/12;
      header.group_count = 1;
      header.conditional_count = 1;
      header.command_count = 2;
      WriteStruct(&test_robot, &numbers);
      WriteStruct(&test_robot, &header);
      
      //NOTE: write conditional 
      char conditional_string[] = "cond_test";
      u8 conditional_length = (u8) ArraySize(conditional_string) - 1;
      WriteStruct(&test_robot, &conditional_length);
      WriteArray(&test_robot, conditional_string, conditional_length);

      //NOTE: write default group
      RobotProfile_Group def_group = {};
      def_group.name_length = 0;
      def_group.parameter_count = 1;
      WriteStruct(&test_robot, &def_group);
      
      {
         RobotProfile_Parameter test_param = {};
         char test_param_name[] = "test_param";
         test_param.name_length = ArraySize(test_param_name) - 1;
         f32 test_param_value = 1234;

         WriteStruct(&test_robot, &test_param);
         WriteArray(&test_robot, test_param_name, test_param.name_length);
         WriteStruct(&test_robot, &test_param_value);
      }

      //NOTE: write other group
      RobotProfile_Group test_group = {};
      char test_group_name[] = "test_group";
      test_group.name_length = ArraySize(test_group_name) - 1;
      test_group.parameter_count = 1;
      WriteStruct(&test_robot, &test_group);
      WriteArray(&test_robot, test_group_name, test_group.name_length);

      {
         RobotProfile_Parameter test_param = {};
         char test_param_name[] = "test_array_param";
         f32 test_param_values[] = {1, 2, 3};
         test_param.is_array = 1;
         test_param.name_length = ArraySize(test_param_name) - 1;
         test_param.value_count = ArraySize(test_param_values);
         
         WriteStruct(&test_robot, &test_param);
         WriteArray(&test_robot, test_param_name, test_param.name_length);
         WriteArray(&test_robot, test_param_values, ArraySize(test_param_values));
      }

      //NOTE: write command twice (command_count = 2 in header)
      RobotProfile_Command test_command = {};
      char test_command_name[] = "do_the_thing";
      test_command.name_length = ArraySize(test_command_name) - 1;
      test_command.param_count = 2;
      test_command.type = North_CommandExecutionType::NonBlocking;
      
      char param_name[] = "test_param";
      u8 param_name_length = ArraySize(param_name) - 1; 

      {
         WriteStruct(&test_robot, &test_command);
         WriteArray(&test_robot, test_command_name, test_command.name_length);

         WriteStruct(&test_robot, &param_name_length);
         WriteArray(&test_robot, param_name, param_name_length);
         WriteStruct(&test_robot, &param_name_length);
         WriteArray(&test_robot, param_name, param_name_length);
      }

      {
         WriteStruct(&test_robot, &test_command);
         WriteArray(&test_robot, test_command_name, test_command.name_length);

         WriteStruct(&test_robot, &param_name_length);
         WriteArray(&test_robot, param_name, param_name_length);
         WriteStruct(&test_robot, &param_name_length);
         WriteArray(&test_robot, param_name, param_name_length);
      }

      WriteEntireFile("test_robot.ncrp", test_robot);
   }
}