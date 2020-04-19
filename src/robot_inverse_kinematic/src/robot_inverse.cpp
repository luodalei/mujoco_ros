#include <ros/ros.h>
#include <iostream>
#include <cmath>
#include <Eigen/Eigen>
#include <stdio.h>
#include <robot_inverse.h>
#include <robot_msgs/omega.h>
#include <robot_msgs/ik.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class teleoperation
{
private:
  ros::NodeHandle nh;
  ros::Subscriber sub;
  ros::Publisher pub;
  HomoKinematics kinematics_;
  Eigen::Vector3d master_pos_zero;
  Eigen::Vector3d master_rpy_zero;
  Eigen::Vector3d master_pos;
  Eigen::Vector3d master_rpy;
  Eigen::Affine3d frame_end_zero_position;
  Eigen::Affine3d frame_end_zero_rotation;
  Eigen::Affine3d frame_end;

  Eigen::Vector3d slave_desire_pos;
  Eigen::Matrix<double,3,3> slave_desire_rotation;
  Eigen::Matrix<double,3,3> slave_rotation_zero;
  Eigen::Matrix<double,3,3> frame_end_constant;
  Eigen::Vector3d slave_desire_rpy_r_increase;
  Eigen::Vector3d slave_desire_rpy_p_increase;
  Eigen::Vector3d slave_desire_rpy_y_increase;

  Eigen::Vector3d slave_pos_zero;
  Eigen::Vector3d slave_pos_zero_com;
  Eigen::VectorXd joint_values;

  double direction_pos_x;
  double direction_pos_y;
  double direction_pos_z;
  double direction_rpy_r;
  double direction_rpy_p;
  double direction_rpy_y;
  double scale_p_x;
  double scale_p_y;
  double scale_p_z;
  double scale_r_x;
  double scale_r_y;
  double scale_r_z;
  double yaw;
  double pitch;
  double roll;
  double rotate_angle;
  double roll_angle;
  double clip_angle;

  double omega_button_zero;
  double omega_button;
  bool omega_button_is_open;
  bool is_first_;
public:
  teleoperation():
    is_first_(true)
  {
    direction_pos_x = 1;
    direction_pos_y = 1;
    direction_pos_z = 1;
    direction_rpy_r = 1;
    direction_rpy_p = 1;
    direction_rpy_y = 1;
    scale_p_x = 0.3;
    scale_p_y = 0.3;
    scale_p_z = 0.3;
    scale_r_x = 0.3;
    scale_r_y = 0.3;
    scale_r_z = 0.3;

    std::cout<<"teleoperation start ..."<<std::endl;
    pub = nh.advertise<robot_msgs::ik>("/ik", 100, true);
    sub = nh.subscribe("/omega_pose", 100, &teleoperation::operationCallback, this);
  }

  ~teleoperation(){}

  void operationCallback(const robot_msgs::omega::ConstPtr& omega7_msg)
  {
    if(is_first_)
    {
      for(unsigned int i=0;i<3;i++)
          master_pos_zero[i] = omega7_msg->data[i];
      for(unsigned int i=0;i<3;i++)
          master_rpy_zero[i] = omega7_msg->data[i+3];

      omega_button_zero = omega7_msg->button[0];
      kinematics_.getTransformAtIndex(10, frame_end_zero_position);
      kinematics_.getTransformAtIndex_rotation(11, frame_end_zero_rotation);
      slave_pos_zero = frame_end_zero_position.translation();
      slave_rotation_zero = frame_end_zero_rotation.rotation();
      is_first_=false;
      return;
    }
    for(unsigned int i=0;i<3;i++)
        master_pos[i] = omega7_msg->data[i];
    for(unsigned int i=0;i<3;i++)
        master_rpy[i] = omega7_msg->data[i+3];
    omega_button = omega7_msg->button[0];

    slave_desire_pos[0] = direction_pos_x * (master_pos[0]-master_pos_zero[0]) * scale_p_x + slave_pos_zero[0];
    slave_desire_pos[1] = direction_pos_y * (master_pos[1]-master_pos_zero[1]) * scale_p_y + slave_pos_zero[1];
    slave_desire_pos[2] = direction_pos_z * (master_pos[2]-master_pos_zero[2]) * scale_p_y + slave_pos_zero[2];

    slave_desire_rpy_r_increase[0] = direction_rpy_r * scale_r_x*(master_rpy[0]-master_rpy_zero[0]);
    slave_desire_rpy_p_increase[1] = direction_rpy_p * scale_r_y*(master_rpy[1]-master_rpy_zero[1]);
    slave_desire_rpy_y_increase[2] = direction_rpy_y * scale_r_z*(master_rpy[2]-master_rpy_zero[2]);

    roll   = slave_desire_rpy_r_increase[0];
    pitch = slave_desire_rpy_p_increase[1];
    yaw  = slave_desire_rpy_y_increase[2];

    slave_desire_rotation = (Eigen::AngleAxisd(yaw,Eigen::Vector3d::UnitZ()))*(Eigen::AngleAxisd(pitch,
                                   Eigen::Vector3d::UnitY()))*(Eigen::AngleAxisd(roll, Eigen::Vector3d::UnitX()))
                                  *slave_rotation_zero;

    frame_end = Eigen::Translation3d(slave_desire_pos);

    joint_values.resize(6);
    kinematics_.getIk(frame_end, joint_values);

    frame_end_constant = slave_desire_rotation.inverse();

    double rotate_angle = std::atan2(frame_end_constant(2,0), frame_end_constant(2,1));
    double roll_angle = std::atan2((frame_end_constant(2,1) * std::cos(rotate_angle) - frame_end_constant(2,0)*std::sin(rotate_angle)), frame_end_constant(2,2));
    double clip_angle = std::acos(frame_end_constant(0,0) * std::cos(rotate_angle) + frame_end_constant(0,1)*std::sin(rotate_angle));

    joint_values[3] = rotate_angle;
    joint_values[4] = roll_angle;
    joint_values[5] = clip_angle;

    robot_msgs::ik ik_msg;
    ik_msg.data.resize(6);
    ik_msg.data[0] = joint_values[0];
    ik_msg.data[1] = joint_values[1];
    ik_msg.data[2] = joint_values[2];
//    ik_msg.data[3] = joint_values[3] - 1.5708;
//    ik_msg.data[4] = joint_values[4] + 1.5708;
//    ik_msg.data[5] = joint_values[5];

    pub.publish(ik_msg);

    std::cout << "ik_msg: " << std::endl<<ik_msg <<std::endl;

  }
  void start(void)
  {
      std::cout<<"remote operation start ..."<<std::endl;
  }
};


int main(int argc, char **argv)
{
    ros::init (argc, argv, "robot_inverse");
    ros::AsyncSpinner spinner(1);
    spinner.start();
    teleoperation operation;
//    operation.start();
    std::cout << "ok" << std::endl;
    ros::waitForShutdown();
    return 0;
}































