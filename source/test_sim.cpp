//NOTE: angles are in radians (not canonicalized), positions are assumed to be driven at constant velocities
AutoRobotPose ForwardKinematics(AutoRobotPose a, f32 p_r, f32 p_l, f32 b) {
   //Handles "p_r - p_l != 0"
   if(abs(p_r - p_l) > 1e-6) { //TODO: figure out the precision on this, I chose 1e-6 arbitrarily
      f32 new_angle = (p_r - p_l) / b + a.angle;
      f32 radius = (b / 2.0) * ((p_r + p_l)/(p_r - p_l));
      f32 new_x =  radius * (sin(new_angle) - sin(a.angle));
      f32 new_y = -radius * (cos(new_angle) - cos(a.angle));

      OutputDebugStringA(ToCString("Radius = " + ToString(radius) + ", " + ToString(abs(p_r - p_l)) + "\n"));

      AutoRobotPose result = {};
      result.pos = a.pos + V2(new_x, new_y);
      result.angle = new_angle;
      return result;
   }
   
   OutputDebugStringA(ToCString("0 ~= " + ToString(abs(p_r - p_l)) + "\n"));

   //Handles "p_r - p_l == 0"
   AutoRobotPose result = {};
   result.pos = a.pos + p_r * V2(cos(a.angle), sin(a.angle));
   result.angle = a.angle;
   return result;
}

struct SplineResult {
   North_HermiteControlPoint *control_points;
   u32 control_point_count;
   
   InterpolatingMap<AutoRobotPose> arclength_pose_map;
   InterpolatingMap<v2> wheel_setpoint_map;
};

//From the summer 2019 jetson rover stuff
SplineResult SplineToWheelSetpoints(North_HermiteControlPoint *control_points, u32 control_point_count, u32 n_1, u32 n_2) {
   SplineResult result = {};
   result.control_points = control_points;
   result.control_point_count = control_point_count; 

   //Spline to poses
   f32 t_step = (f32)(control_point_count - 1) / (f32)(n_1 - 1);

   InterpolatingMapSample<AutoRobotPose> *arclength_pose_samples = PushTempArray(InterpolatingMapSample<AutoRobotPose>, n_1);

   f32 length = 0;
   v2 last_pos = control_points[0].pos;
   for(u32 i = 0; i < n_1; i++) {
      f32 t_full = t_step * i;
      u32 spline_i = Clamp(0, control_point_count - 2, (u32)t_full);

      North_HermiteControlPoint cp_a = control_points[spline_i];
      North_HermiteControlPoint cp_b = control_points[spline_i + 1]; 

      AutoRobotPose pose = {};
      pose.pos = CubicHermiteSpline(cp_a.pos, cp_a.tangent, cp_b.pos, cp_b.tangent, t_full - spline_i);
      pose.angle = ToRadians(Angle(CubicHermiteSplineTangent(cp_a.pos, cp_a.tangent, cp_b.pos, cp_b.tangent, t_full - spline_i)));
      
      length += Length(pose.pos - last_pos);
      last_pos = pose.pos;
      
      arclength_pose_samples[i].key = length;
      arclength_pose_samples[i].data = pose;
   }

   result.arclength_pose_map = BuildInterpolatingMap(arclength_pose_samples, n_1, __temp_arena, map_lerp_pose);

   //Poses to wheel setpoints
   f32 step = length / (n_2 - 1);
   bool is_reverse = false;
   f32 drivebase = 1;

   InterpolatingMapSample<v2> *wheel_setpoint_samples = PushTempArray(InterpolatingMapSample<v2>, n_2);

   v2 curr_setpoint = {};
   wheel_setpoint_samples[0].key = 0;
   wheel_setpoint_samples[0].data = curr_setpoint;

   for(u32 i = 1; i < n_2; i++) {
      f32 s = step * i;
      f32 dtheta = ShortestAngleBetween_Radians(MapLookup(&result.arclength_pose_map, step * (i - 1)).angle, MapLookup(&result.arclength_pose_map, s).angle);
      curr_setpoint = curr_setpoint + V2(step + (drivebase / 2) * dtheta, step - (drivebase / 2) * dtheta);

      wheel_setpoint_samples[i].key = s;
      wheel_setpoint_samples[i].data = (is_reverse ? -1 : 1) * curr_setpoint;
   }

   result.wheel_setpoint_map = BuildInterpolatingMap(wheel_setpoint_samples, n_2, __temp_arena, interpolation_map_v2_lerp);

   return result;
}