/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, University of Colorado, Boulder
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Univ of CO, Boulder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Dave Coleman
   Desc:   Example ros_control hardware interface that performs a perfect control loop for
   simulation
*/

#include <ros_control_boilerplate/sim_hw_interface.h>
#include <control_toolbox/filters.h>

namespace ros_control_boilerplate
{
SimHWInterface::SimHWInterface(ros::NodeHandle &nh, urdf::Model *urdf_model) : GenericHWInterface(nh, urdf_model)
{
  ROS_INFO_NAMED("generic_hw_interface", "Loaded sim_hw_interface.");

  // Resize vectors
  joint_position_prev_.resize(num_joints_, 0.0);
}

void SimHWInterface::read(ros::Duration &elapsed_time)
{
  // No need to read since our write() command populates our state for us
}

void SimHWInterface::write(ros::Duration &elapsed_time)
{
  // Safety
  enforceLimits(elapsed_time);

  // Send commands in different modes
  int joint_mode = 0;  // TODO

  // NOTE: the following is a "simuation" example so that this boilerplate can be run without being
  // connected to hardware
  // When converting to your robot, remove the built-in PID loop and instead let the higher leverl
  // ros_control controllers take
  // care of PID loops for you. This P-controller is only intended to mimic the delay in real
  // hardware, somewhat like a simualator
  for (std::size_t joint_id = 0; joint_id < num_joints_; ++joint_id)
  {
    switch (joint_mode)
    {
      case 0:  // hardware_interface::MODE_POSITION:
        positionControlSimulation(elapsed_time, joint_id);
        break;

      case 1:  // hardware_interface::MODE_VELOCITY:
        // Position - Move all the states to the commanded set points slowly
        joint_position_[joint_id] += joint_velocity_[joint_id] * elapsed_time.toSec();

        // Velocity - Move all the states to the commanded set points slowly
        v_error_ = joint_velocity_command_[joint_id] - joint_velocity_[joint_id];
        // scale the rate it takes to achieve velocity by a factor that is invariant to the feedback
        // loop
        joint_velocity_[joint_id] += v_error_ * VELOCITY_STEP_FACTOR;
        break;

      case 2:  // hardware_interface::MODE_EFFORT:
        ROS_ERROR_STREAM_NAMED("generic_hw_interface", "Effort not implemented yet");
        break;
    }
  }
}

void SimHWInterface::positionControlSimulation(ros::Duration &elapsed_time, const std::size_t joint_id)
{
  const double max_delta_pos = joint_velocity_limits_[joint_id] * elapsed_time.toSec();

  // Move all the states to the commanded set points at max velocity
  p_error_ = joint_position_command_[joint_id] - joint_position_[joint_id];
  const double delta_pos = std::max(std::min(p_error_, max_delta_pos), -max_delta_pos);

  // if (joint_id == 0)
  //   std::cout << "max: " << max_delta_pos << "\t vel lim: " << joint_velocity_limits_[joint_id] << "\t elapsed time: " << elapsed_time.toSec()
  //             << "\t p_error: " << p_error_ << "\t delta_pos: " << delta_pos << std::endl;

  joint_position_[joint_id] += delta_pos;

  // Bypass max velocity stuff:
  //joint_position_[joint_id] = joint_position_command_[joint_id];

  // Calculate velocity based on change in positions, using an exponential smoothing filter.
  // Alpha is between 0 and 1. Values closer to 0 weight the last smoothed value more heavily
  static const double ALPHA = 0.5;
  if (elapsed_time.toSec() > 0)
  {
    const double value = (joint_position_[joint_id] - joint_position_prev_[joint_id]) / elapsed_time.toSec();
    joint_velocity_[joint_id] = filters::exponentialSmoothing(value, joint_velocity_[joint_id], ALPHA);
  }
  else
    joint_velocity_[joint_id] = 0;

  // Save last position
  joint_position_prev_[joint_id] = joint_position_[joint_id];
}

}  // namespace
