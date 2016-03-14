/*
 * Copyright (C) 2014 Roman Bapst, CVG, ETH Zurich, Switzerland
 *
 * This software is released to the Contestants of the european
 * robotics challenges (EuRoC) for the use in stage 1. (Re)-distribution, whether
 * in parts or entirely, is NOT PERMITTED.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "rotors_gazebo_plugins/gazebo_wing_model.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdio.h>

#include <boost/bind.hpp>

namespace gazebo {

GazeboWingModelPlugin::GazeboWingModelPlugin()
    : ModelPlugin() {
}

GazeboWingModelPlugin::~GazeboWingModelPlugin() {
  event::Events::DisconnectWorldUpdateBegin(updateConnection_);
}

void GazeboWingModelPlugin::Load(physics::ModelPtr _model, sdf::ElementPtr _sdf) {
  // Store the pointer to the model and the world
  model_ = _model;
  world_ = model_->GetWorld();

  // Get the robot namespace
  if (_sdf->HasElement("robotNamespace"))
    namespace_ = _sdf->GetElement("robotNamespace")->Get<std::string>();
  else
    gzerr << "[gazebo_wing_model] Please specify a robotNamespace.\n";

  // Get the link name
  std::string link_name;
  if (_sdf->HasElement("linkName"))
    link_name = _sdf->GetElement("linkName")->Get<std::string>();
  else
    gzerr << "[gazebo_wing_model] Please specify a linkName.\n";
  // Get the pointer to the link
  this->link_ = this->model_->GetLink(link_name);
  if (link_ == NULL)
    gzthrow("[gazebo_wing_model] Couldn't find specified link \"" << link_name << "\".");

  // Retrieve the rest of the SDF parameters
  getSdfParam<float>(_sdf, "alpha0", alpha_0_, kDefaultAlpha0);
  getSdfParam<float>(_sdf, "alphaStall", alpha_stall_, kDefaultAlphaStall);
  getSdfParam<float>(_sdf, "cLAlpha", c_L_alpha_, kDefaultCLAlpha);
  getSdfParam<float>(_sdf, "airDensity", air_density_, kDefaultAirDensity);
  getSdfParam<float>(_sdf, "wingArea", wing_area_, kDefaultWingArea);

  alpha_0_ *= M_PI / 180.0;
  alpha_stall_ *= M_PI / 180.0;

  // Listen to the update event. This event is broadcast every simulation iteration
  this->updateConnection_ =
      event::Events::ConnectWorldUpdateBegin(
          boost::bind(&GazeboWingModelPlugin::OnUpdate, this, _1));
}

void GazeboWingModelPlugin::OnUpdate(const common::UpdateInfo& _info) {
  // Get the link velocity and pose in the world frame
  math::Vector3 vel_W = link_->GetWorldLinearVel();
  math::Quaternion rot = link_->GetWorldPose().rot;

  // Rotate the velocity into the body frame
  math::Vector3 body_vel = rot.RotateVector(vel_W);

  // Compute the overall force acting on the wing and rotate them into the body frame
  math::Vector3 forces = ComputeAerodynamicForces(body_vel);
  math::Vector3 forces_body = rot.RotateVector(forces);

  this->link_->AddForce(forces_body);
  //this->link_->AddRelativeTorque(aerodynamic_moments);
}

math::Vector3 GazeboWingModelPlugin::ComputeAerodynamicForces(math::Vector3 vel) {
  // Compute angle of attack
  double alpha = -atan2(vel.z,vel.x);

  // Compute the coefficients of lift and drag
  float c_L = 0.0;
  float c_D = 0.0;

  if (alpha <= alpha_stall_ && alpha >= -alpha_stall_) {
    c_L = c_L_alpha_ * (alpha - alpha_0_);
    c_D = 0.2;
  }

  // Compute the speed of the aircraft in the lift-drag plane
  float speedInLiftDragPlane = sqrt(pow(vel.x, 2.0) + pow(vel.z, 2.0));

  // Compute the forces
  float lift = 0.5 * air_density_ * c_L * wing_area_ * pow(speedInLiftDragPlane, 2.0);
  float drag = 0.5 * air_density_ * c_D * wing_area_ * pow(speedInLiftDragPlane, 2.0);

  // Return the total force acting on the wing
  return math::Vector3(-drag, 0, lift);
}

/*math::Vector3 GazeboWingModelPlugin::get_aerodynamic_moments(math::Vector3 &vel) {
  // compute lift
  math::Vector3 lift = get_aerodynamic_forces(vel);

  // compute moment due to lift

  // compute moment due to elevons

  // compute total moment
  return math::Vector3(0.0f,0.0f,0.0f);
}*/

GZ_REGISTER_MODEL_PLUGIN(GazeboWingModelPlugin);
}
