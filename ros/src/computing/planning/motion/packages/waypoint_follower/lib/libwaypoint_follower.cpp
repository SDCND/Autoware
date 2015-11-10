/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "waypoint_follower/libwaypoint_follower.h"

int WayPoints::getSize() const
{
	if (current_waypoints_.waypoints.empty())
		return 0;
	else
		return current_waypoints_.waypoints.size();
}

double WayPoints::getInterval() const
{
  if(current_waypoints_.waypoints.empty())
    return 0;

	//interval between 2 waypoints
	tf::Vector3 v1(current_waypoints_.waypoints[0].pose.pose.position.x, current_waypoints_.waypoints[0].pose.pose.position.y, 0);

	tf::Vector3 v2(current_waypoints_.waypoints[1].pose.pose.position.x, current_waypoints_.waypoints[1].pose.pose.position.y, 0);
	return tf::tfDistance(v1, v2);
}

geometry_msgs::Point WayPoints::getWaypointPosition(int waypoint) const
{
	geometry_msgs::Point p;
	p.x = current_waypoints_.waypoints[waypoint].pose.pose.position.x;
	p.y = current_waypoints_.waypoints[waypoint].pose.pose.position.y;
	p.z = current_waypoints_.waypoints[waypoint].pose.pose.position.z;
	return p;
}

geometry_msgs::Quaternion WayPoints::getWaypointOrientation(int waypoint) const
{
	geometry_msgs::Quaternion q;
	q.x = current_waypoints_.waypoints[waypoint].pose.pose.orientation.x;
	q.y = current_waypoints_.waypoints[waypoint].pose.pose.orientation.y;
	q.z = current_waypoints_.waypoints[waypoint].pose.pose.orientation.z;
	q.w = current_waypoints_.waypoints[waypoint].pose.pose.orientation.w;
	return q;
}

double WayPoints::getWaypointVelocityMPS(int waypoint)const
{
	return current_waypoints_.waypoints[waypoint].twist.twist.linear.x;
}

double DecelerateVelocity(double distance, double prev_velocity)
{

    double decel_ms = 1.0; // m/s
    double decel_velocity_ms = sqrt(2 * decel_ms * distance);

    std::cout << "velocity/prev_velocity :" << decel_velocity_ms << "/" << prev_velocity << std::endl;
    if (decel_velocity_ms < prev_velocity) {
        return decel_velocity_ms;
    } else {
        return prev_velocity;
    }

}

//calculation relative coordinate of point from current_pose frame
geometry_msgs::Point calcRelativeCoordinate(geometry_msgs::Point point, geometry_msgs::Pose current_pose)
{
  tf::Transform inverse;
  tf::poseMsgToTF(current_pose, inverse);
  tf::Transform transform = inverse.inverse();

  tf::Vector3 v = point2vector(point);
  tf::Vector3 tf_v = transform * v;

  return vector2point(tf_v);
}

//calculation absolute coordinate of point on current_pose frame
geometry_msgs::Point calcAbsoluteCoordinate(geometry_msgs::Point point, geometry_msgs::Pose current_pose)
{
  tf::Transform inverse;
  tf::poseMsgToTF(current_pose, inverse);

  tf::Vector3 v = point2vector(point);
  tf::Vector3 tf_v = inverse * v;

  return vector2point(tf_v);
}

//distance between target 1 and target2 in 2-D
double getPlaneDistance(geometry_msgs::Point target1, geometry_msgs::Point target2)
{
  tf::Vector3 v1 = point2vector(target1);
  v1.setZ(0);
  tf::Vector3 v2 = point2vector(target2);
  v2.setZ(0);
  return tf::tfDistance(v1,v2) ;
}

//get closest waypoint from current pose
int getClosestWaypoint(const waypoint_follower::lane &current_path,geometry_msgs::Pose current_pose )
{

  if (!current_path.waypoints.size()){
    return -1;
  }

  int waypoint_min = -1;
  double distance_min = DBL_MAX;
  //get closest waypoint
  for(int i = 1 ; i <  static_cast<int>(current_path.waypoints.size()); i++)
  {
    //skip waypoint behind vehicle
    double x = calcRelativeCoordinate(current_path.waypoints[i].pose.pose.position, current_pose).x;
    //ROS_INFO("waypoint = %d, x = %lf",i,x);
    if (x < 0)
      continue;

    //calc waypoint angle
    double waypoint_yaw;
    if (i == static_cast<int>(current_path.waypoints.size()) - 1)
    {
      waypoint_yaw = atan2(
          current_path.waypoints[i - 1].pose.pose.position.y - current_path.waypoints[i].pose.pose.position.y,
          current_path.waypoints[i - 1].pose.pose.position.x - current_path.waypoints[i].pose.pose.position.x);
      waypoint_yaw -= M_PI;
    }
    else
    {
      waypoint_yaw = atan2(
          current_path.waypoints[i + 1].pose.pose.position.y - current_path.waypoints[i].pose.pose.position.y,
          current_path.waypoints[i + 1].pose.pose.position.x - current_path.waypoints[i].pose.pose.position.x);
    }
    if(waypoint_yaw < 0)
      waypoint_yaw += 2 * M_PI;

    //calc pose angle
    tf::Quaternion q(current_pose.orientation.x, current_pose.orientation.y, current_pose.orientation.z, current_pose.orientation.w);
    double dummy1,dummy2,pose_yaw;
    tf::Matrix3x3(q).getRPY(dummy1, dummy2, pose_yaw);
    if(pose_yaw < 0)
      pose_yaw += 2 * M_PI;

    //skip waypoint which direction is reverse against current_pose
    double direction_sub = (waypoint_yaw - pose_yaw) * 180 / M_PI; //degree
    //ROS_INFO("waypoint = %d, waypoint_yaw = %lf, pose_yaw = %lf, direction sub = %lf",i,waypoint_yaw,pose_yaw,direction_sub);
    if(fabs(direction_sub) > 90)
     continue;

    double distance = getPlaneDistance(current_path.waypoints[i].pose.pose.position, current_pose.position);
    //ROS_INFO("waypoint = %d , distance = %lf , distance_min = %lf",i,distance,distance_min);
    if(distance_min > distance){

      //waypoint_min = el;
      waypoint_min = i;
      distance_min = distance;
    }
  }

  //ROS_INFO("waypoint = %d",waypoint_min);
  return waypoint_min;
}
