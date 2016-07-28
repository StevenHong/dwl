#include <dwl/simulation/PreviewLocomotion.h>


namespace dwl
{

namespace simulation
{

PreviewLocomotion::PreviewLocomotion() : robot_model_(false),
		sample_time_(0.001), gravity_(9.81), mass_(0.), num_feet_(0),
		step_height_(0.1), force_threshold_(0.)
{
	actual_system_com_.setZero();
}


PreviewLocomotion::~PreviewLocomotion()
{

}


void PreviewLocomotion::resetFromURDFFile(std::string urdf_file,
										  std::string system_file)
{
	resetFromURDFModel(urdf_model::fileToXml(urdf_file), system_file);
}


void PreviewLocomotion::resetFromURDFModel(std::string urdf_model,
										   std::string system_file)
{
	// Resetting the model of the floating-base system
	system_.resetFromURDFModel(urdf_model, system_file);

	// Initializing the dynamics and kinematics from the URDF model
	dynamics_.modelFromURDFModel(urdf_model, system_file);
	kinematics_.modelFromURDFModel(urdf_model, system_file);

	// Getting the gravity magnitude from the rigid-body dynamic model
	gravity_ = system_.getRBDModel().gravity.norm();

	// Getting the total mass of the system
	mass_ = system_.getTotalMass();

	// Getting the number of feet
	num_feet_ = system_.getNumberOfEndEffectors(model::FOOT);

	// Getting the feet names
	feet_names_ = system_.getEndEffectorNames(model::FOOT);

	// Getting the default joint position
	Eigen::VectorXd q0 = system_.getDefaultPosture();

	// Getting the default position of the CoM system
	actual_system_com_ = system_.getSystemCoM(rbd::Vector6d::Zero(),
											  q0);

	// Computing the stance posture using the default position
	kinematics_.computeForwardKinematics(stance_posture_,
										 rbd::Vector6d::Zero(),
										 q0,
										 feet_names_,
										 rbd::Linear);

	// Converting to the CoM frame
	for (rbd::BodyVector::iterator feet_it = stance_posture_.begin();
			feet_it != stance_posture_.end(); feet_it++) {
		std::string name = feet_it->first;
		Eigen::VectorXd stance = feet_it->second;

		stance_posture_[name] = stance - actual_system_com_;
	}

	// Setting up the cart-table model
	CartTableProperties model(mass_, gravity_);
	cart_table_.setModelProperties(model);

	robot_model_ = true;
}


void PreviewLocomotion::readPreviewSequence(ReducedBodyState& state,
											PreviewControl& control,
											std::string filename)
{
	// Checking that the robot model was initialized
	if (!robot_model_) {
		printf(RED "Error: the robot model was not initialized\n" COLOR_RESET);
		return;
	}

	YamlWrapper yaml_reader(filename);

	// All the preview sequence data have to be inside the state and
	// preview_control namespaces
	YamlNamespace state_ns = {"preview_sequence", "state"};
	YamlNamespace control_ns = {"preview_sequence", "preview_control"};


	// Reading the state
	if (!yaml_reader.read(state.com_pos, "com_pos", state_ns)) {
		printf(RED "Error: the CoM height was not found\n" COLOR_RESET);
		return;
	}
	if (!yaml_reader.read(state.com_vel, "com_vel", state_ns)) {
		printf(RED "Error: the CoM velocity was not found\n" COLOR_RESET);
		return;
	}
	if (!yaml_reader.read(state.cop, "cop", state_ns)) {
		printf(RED "Error: the CoP was not found\n" COLOR_RESET);
		return;
	}


	// Reading the preview control data
	// Reading the number of phases
	int num_phases;
	if (!yaml_reader.read(num_phases, "number_phase",  control_ns)) {
		printf(RED "Error: the number_phase was not found\n" COLOR_RESET);
		return;
	}
	control.params.resize(num_phases);

	// Reading the preview parameters per phase
	for (int k = 0; k < num_phases; k++) {
		// Getting the phase namespace
		YamlNamespace phase_ns = {"preview_sequence", "preview_control",
								  "phase_" + std::to_string(k)};

		// Reading the preview duration
		if (!yaml_reader.read(control.params[k].duration, "duration", phase_ns)) {
			printf(RED "Error: the duration of phase_%i was not found\n"
					COLOR_RESET, k);
			return;
		}

		// Reading the preview CoP shift
		if (yaml_reader.read(control.params[k].cop_shift, "cop_shift", phase_ns))
			control.params[k].phase.setTypeOfPhase(simulation::STANCE);

		// Reading the preview parameters for stance phase
		if (control.params[k].phase.getTypeOfPhase() == simulation::STANCE) {
			// Reading the heading acceleration
			if (!yaml_reader.read(control.params[k].head_acc, "head_acc", phase_ns)) {
				printf(RED "Error: the head_acc of phase_%i was not found\n"
						COLOR_RESET, k);
				return;
			}
		}

		// Reading the footstep shifts
		for (unsigned int f = 0; f < feet_names_.size(); f++) {
			std::string name = feet_names_[f];

			// Setting up if there is a foot shift in this phase
			Eigen::Vector2d foot_shift;
			if (yaml_reader.read(foot_shift, name, phase_ns)) {
				control.params[k].phase.feet.push_back(name);
				control.params[k].phase.setSwingFoot(name);
				control.params[k].phase.setFootShift(name, foot_shift);
			}
		}
	}
}


void PreviewLocomotion::setSampleTime(double sample_time)
{
	sample_time_ = sample_time;
}


void PreviewLocomotion::setStepHeight(double step_height)
{
	step_height_ = step_height;
}


void PreviewLocomotion::setForceThreshold(double force_threshold)
{
	force_threshold_ = force_threshold;
}


void PreviewLocomotion::multiPhasePreview(ReducedBodyTrajectory& trajectory,
										  const ReducedBodyState& state,
										  const PreviewControl& control,
										  bool full)
{
	// Checking that the robot model was initialized
	if (!robot_model_) {
		printf(RED "Error: the robot model was not initialized\n" COLOR_RESET);
		return;
	}

	// Updating the actual state
	actual_state_ = state;

	// Clearing the trajectory
	trajectory.clear();

	// Computing the preview for multi-phase
	ReducedBodyState actual_state;
	for (unsigned int k = 0; k < control.params.size(); k++) {
		ReducedBodyTrajectory phase_traj;

		// Getting the preview params of the actual phase
		PreviewParams preview_params = control.params[k];

		// Getting the actual preview state for this phase
		if (k == 0)
			actual_state = state;
		else {
			actual_state = trajectory.back();

			// Updating the support region for this phase
			if (preview_params.duration > sample_time_) {
				for (unsigned int f = 0; f < num_feet_; f++) {
					std::string name = feet_names_[f];

					// Removing the swing foot of the actual phase
					if (preview_params.phase.isSwingFoot(name))
						actual_state.support_region.erase(name);

					// Adding the foothold target of the previous phase
					if (control.params[k-1].phase.isSwingFoot(name) &&
							control.params[k-1].duration > sample_time_) {
						Eigen::Vector3d stance =
								stance_posture_.find(name)->second;

						// Getting the footshift control parameter
						Eigen::Vector2d footshift_2d =
								control.params[k-1].phase.getFootShift(name);
						Eigen::Vector3d footshift(footshift_2d(rbd::X),
												  footshift_2d(rbd::Y),
												  0.);

						// Computing the foothold position w.r.t. the world
						Eigen::Vector3d foothold = actual_state.com_pos +
								frame_tf_.fromBaseToWorldFrame(stance + footshift,
															   actual_state.getRPY_W());
						// Computing the footshift in z from the height map. In
						// case of no having the terrain height map, it assumes
						// flat terrain conditions. Note that, for those cases,
						// we compensate small drift between the actual and the
						// default postures, and the displacement of the CoM in z
						if (terrain_.isTerrainInformation()) {
							// Adding the terrain height given the terrain
							// height-map
							Eigen::Vector2d foothold_2d = foothold.head<2>();
							foothold(rbd::Z) = terrain_.getTerrainHeight(foothold_2d);
						} else {
							double comz_shift =
									actual_state.com_pos(rbd::Z) -
									actual_state_.com_pos(rbd::Z);
							double footshift_z =
									-(cart_table_.getPendulumHeight() +
											stance(rbd::Z));
							foothold(rbd::Z) = footshift_z - comz_shift;
						}

						actual_state.support_region[name] = foothold;
					}
				}
			}
		}

		// Computing the preview of the actual phase
		if (preview_params.phase.type == STANCE) {
			stancePreview(phase_traj, actual_state, preview_params, full);
		} else {
			flightPreview(phase_traj, actual_state, preview_params, full);
		}

		// Appending the actual phase trajectory
		trajectory.insert(trajectory.end(), phase_traj.begin(), phase_traj.end());

		// Sanity action: defining the actual state if there isn't a trajectory
		if (trajectory.size() == 0)
			trajectory.push_back(state);
	}

	// Adding the latest state
	actual_state = trajectory.back();
	PreviewParams end_control = control.params.back();
	for (unsigned int f = 0; f < num_feet_; f++) {
		std::string name = feet_names_[f];

		// Adding the foothold target of the current phase
		if (end_control.phase.isSwingFoot(name) &&
				end_control.duration > sample_time_) {
			Eigen::Vector3d stance =
					stance_posture_.find(name)->second.head<3>();

			// Getting the footshift control parameter
			Eigen::Vector2d footshift_2d =
					end_control.phase.getFootShift(name);
			Eigen::Vector3d footshift(footshift_2d(rbd::X),
									  footshift_2d(rbd::Y),
									  0.);

			// Computing the foothold position w.r.t. the world
			Eigen::Vector3d foothold = actual_state.com_pos +
					frame_tf_.fromBaseToWorldFrame(stance + footshift,
												   actual_state.getRPY_W());
			// Computing the footshift in z from the height map. In case of no
			// having the terrain height map, it assumes flat terrain conditions.
			// Note that, in those cases, we compensate small drift between the
			// actual and the default postures, and the displacement of the CoM
			// in z
			if (terrain_.isTerrainInformation()) {
				// Adding the terrain height given the terrain height-map
				Eigen::Vector2d foothold_2d = foothold.head<2>();
				foothold(rbd::Z) = terrain_.getTerrainHeight(foothold_2d);
			} else {
				double comz_shift =
						actual_state.com_pos(rbd::Z) - actual_state_.com_pos(rbd::Z);
				double footshift_z = -(cart_table_.getPendulumHeight() + stance(rbd::Z));
				foothold(rbd::Z) = footshift_z - comz_shift;
			}

			actual_state.support_region[name] = foothold;
		}
	}
	trajectory.push_back(actual_state);
}


void PreviewLocomotion::multiPhaseEnergy(Eigen::Vector3d& com_energy,
										 const ReducedBodyState& state,
										 const PreviewControl& control)
{
	// Checking that the robot model was initialized
	if (!robot_model_) {
		printf(RED "Error: the robot model was not initialized\n" COLOR_RESET);
		return;
	}

	// Updating the actual state
	actual_state_ = state;

	// Initializing the CoM energy vector
	com_energy.setZero();

	// Computing the energy for multi-phase
	ReducedBodyState actual_state = state;
	for (unsigned int k = 0; k < control.params.size(); k++) {
		// Getting the preview params of the actual phase
		PreviewParams preview_params = control.params[k];


		// Computing the CoM energy of this phase
		if (preview_params.phase.type == STANCE) {
			Eigen::Vector3d phase_energy;
			CartTableControlParams model_params(preview_params.duration,
											    preview_params.cop_shift);
			cart_table_.computeSystemEnergy(phase_energy,
											actual_state,
											model_params);
			com_energy += phase_energy;
		} else { // Flight phase
			// TODO compute the energy for flight phases
		}

		// Updating the actual state
		double time = actual_state.time + preview_params.duration;
		cart_table_.computeResponse(actual_state, time);
	}
}


void PreviewLocomotion::stancePreview(ReducedBodyTrajectory& trajectory,
									  const ReducedBodyState& state,
									  const PreviewParams& params,
									  bool full)
{
	// Checking the preview duration
	if (full && params.duration < sample_time_)
		return; // duration it's always positive, and makes sense when
				// is bigger than the sample time

	// Initialization of the Linear Controlled SLIP model
	CartTableControlParams model_params(params.duration,
										params.cop_shift);
	cart_table_.initResponse(state, model_params);

	// Computing the number of samples and initial index
	unsigned int num_samples = floor(params.duration / sample_time_);
	unsigned int idx;
	if (full) {
		idx = 0;
		trajectory.resize(num_samples + 1);

		// Initialization of the swing generator
		initSwing(state, params);
	} else {
		idx = num_samples;
		trajectory.resize(1);
	}

	// Adding the actual support region. Note that the support region
	// remains constant during this phase
	ReducedBodyState current_state = state;

	// Computing the preview trajectory
	double time;
	for (unsigned int k = idx; k < num_samples + 1; k++) {
		// Computing the current time of the preview trajectory
		if (k == num_samples)
			time = params.duration;
		else
			time = sample_time_ * (k + 1);
		current_state.time = state.time + time;

		// Computing the response of the Linear Controlled SLIP
		// dynamics
		cart_table_.computeResponse(current_state,
									current_state.time);

		// Computing the heading motion according to heading kinematic equation
//		current_state.head_pos = state.head_pos + state.head_vel * time +
//				0.5 * params.head_acc * time * time;
//		current_state.head_vel = state.head_vel + params.head_acc * time;
//		current_state.head_acc = params.head_acc; TODO think about it

		// Generating the swing trajectory
		if (full)
			generateSwing(current_state, current_state.time);

		// Appending the current state to the preview trajectory
		trajectory[k-idx] = current_state;
	}
}


void PreviewLocomotion::flightPreview(ReducedBodyTrajectory& trajectory,
						   	   	   	  const ReducedBodyState& state,
									  const PreviewParams& params,
									  bool full)
{
	// Checking the preview duration
	if (full && params.duration < sample_time_)
		return; // duration it's always positive, and makes sense when
				// is bigger than the sample time

	// Setting the gravity vector
	Eigen::Vector3d gravity_vec = Eigen::Vector3d::Zero();
	gravity_vec(rbd::Z) = -gravity_;

	// Computing the number of samples and initial index
	unsigned int num_samples = floor(params.duration / sample_time_);
	unsigned int idx;
	if (full) {
		idx = 0;
		trajectory.resize(num_samples);

		// Initialization of the swing generator
		initSwing(state, params);
	} else {
		idx = num_samples;
		trajectory.resize(1);
	}

	// Computing the preview trajectory
	double time;
	for (unsigned int k = idx; k < num_samples + 1; k++) {
		// Computing the current time of the preview trajectory
		if (k == num_samples)
			time = params.duration;
		else
			time = sample_time_ * (k + 1);


		// Computing the current time of the preview trajectory
		ReducedBodyState current_state;
		current_state.time = state.time + time;

		// Computing the CoM motion according to the projectile EoM
		current_state.com_pos = state.com_pos + state.com_vel * time +
				0.5 * gravity_vec * time * time;
		current_state.com_vel = state.com_vel + gravity_vec * time;
		current_state.com_acc = gravity_vec;

		// Computing the heading motion by assuming that there isn't
		// change in the angular momentum
//		current_state.head_pos = state.head_pos + state.head_vel * time;
//		current_state.head_vel = state.head_vel;
//		current_state.head_acc = 0.; TODO  think about it

		// Generating the swing trajectory
		if (full)
			generateSwing(current_state, current_state.time);

		// Appending the current state to the preview trajectory
		trajectory[k-idx] = current_state;
	}
}



void PreviewLocomotion::initSwing(const ReducedBodyState& state,
								  const PreviewParams& params)
{
	// Updating the phase state
	phase_state_ = state;

	// Computing the terminal CoM state for getting the foothold position
	ReducedBodyState terminal_state;
	cart_table_.computeResponse(terminal_state,
								state.time + params.duration);

	// Getting the swing shift per foot
	rbd::BodyPosition swing_shift;
	for (unsigned int j = 0; j < params.phase.feet.size(); j++) {
		std::string name = params.phase.feet[j];
		Eigen::Vector3d stance = stance_posture_.find(name)->second;

		// Getting the footshift control parameter
		Eigen::Vector2d footshift_2d = params.phase.getFootShift(name);
		Eigen::Vector3d footshift(footshift_2d(rbd::X),
								  footshift_2d(rbd::Y),
								  0.);

		// Computing the foothold position w.r.t. the world
		Eigen::Vector3d foothold = terminal_state.com_pos +
				frame_tf_.fromBaseToWorldFrame(stance + footshift,
											   terminal_state.getRPY_W());

		// Computing the footshift in z from the height map. In case of no
		// having the terrain height map, it assumes flat terrain conditions.
		// Note that, for those cases, we compensate small drift between the
		// actual and the default postures, and the displacement of the CoM in z
		if (terrain_.isTerrainInformation()) {
			// Adding the terrain height given the terrain height-map
			Eigen::Vector2d foothold_2d = foothold.head<2>();
			footshift(rbd::Z) = terrain_.getTerrainHeight(foothold_2d) -
					(terminal_state.com_pos(rbd::Z) + stance(rbd::Z));
		} else {
			double comz_shift =
					terminal_state.com_pos(rbd::Z) - actual_state_.com_pos(rbd::Z);
			double footshift_z = -(cart_table_.getPendulumHeight() + stance(rbd::Z));
			footshift(rbd::Z) = footshift_z - comz_shift;
		}

		swing_shift[name] = footshift;
	}

	// Adding the swing pattern
	swing_params_ = SwingParams(params.duration, swing_shift);

	// Generating the actual state for every feet
	feet_spline_generator_.clear();
	for (rbd::BodyVector::const_iterator foot_it = state.foot_pos.begin();
			foot_it != state.foot_pos.end(); foot_it++) {
		std::string name = foot_it->first;

		// Checking the feet that swing
		rbd::BodyPosition::const_iterator swing_it = swing_params_.feet_shift.find(name);
		if (swing_it != swing_params_.feet_shift.end()) {
			// Getting the actual position of the contact w.r.t the CoM frame
			Eigen::Vector3d actual_pos = foot_it->second;

			// Getting the target position of the contact w.r.t the CoM frame
			Eigen::Vector3d footshift = (Eigen::Vector3d) swing_it->second;
			Eigen::Vector3d stance_pos = stance_posture_.find(name)->second.head<3>();
			Eigen::Vector3d target_pos = stance_pos + footshift;

			// Initializing the foot pattern generator
			simulation::StepParameters step_params(params.duration,//num_samples * sample_time_,
												   step_height_);
			feet_spline_generator_[name].setParameters(state.time,
													   actual_pos,
													   target_pos,
													   step_params);
		}
	}
}


void PreviewLocomotion::generateSwing(ReducedBodyState& state,
									  double time)
{
	// Generating the actual state for every feet
	Eigen::Vector3d foot_pos, foot_vel, foot_acc;
	for (rbd::BodyVector::const_iterator foot_it = phase_state_.foot_pos.begin();
			foot_it != phase_state_.foot_pos.end(); foot_it++) {
		std::string name = foot_it->first;

		// Checking the feet that swing
		rbd::BodyPosition::const_iterator swing_it = swing_params_.feet_shift.find(name);
		if (swing_it != swing_params_.feet_shift.end()) {
			// Generating the swing positions, velocities and accelerations
			feet_spline_generator_[name].generateTrajectory(foot_pos,
															foot_vel,
															foot_acc,
															time);

			// Adding the swing state to the trajectory
			state.foot_pos[name] = foot_pos;
			state.foot_vel[name] = foot_vel;
			state.foot_acc[name] = foot_acc;
		} else {
			// There is not swing trajectory to generated (foot on ground).
			// Nevertheless, we have to updated their positions w.r.t the CoM frame
			// Getting the actual position of the contact w.r.t the CoM frame
			Eigen::Vector3d actual_pos = foot_it->second;

			// Getting the CoM position of the specific time
			Eigen::Vector3d com_pos = state.com_pos;
			Eigen::Vector3d com_vel = state.com_vel;
			Eigen::Vector3d com_acc = state.com_acc;

			// Adding the foot states w.r.t. the CoM frame
			Eigen::Vector3d com_disp = com_pos - phase_state_.com_pos;
			state.foot_pos[name] = actual_pos -
					frame_tf_.fromWorldToBaseFrame(com_disp, state.getRPY_W());
			state.foot_vel[name] =
					frame_tf_.fromWorldToBaseFrame(-com_vel, state.getRPY_W());
			state.foot_acc[name] =
					frame_tf_.fromWorldToBaseFrame(-com_acc, state.getRPY_W());
		}
	}
}


model::FloatingBaseSystem* PreviewLocomotion::getFloatingBaseSystem()
{
	return &system_;
}


model::WholeBodyDynamics* PreviewLocomotion::getWholeBodyDynamics()
{
	return &dynamics_;
}


environment::TerrainMap* PreviewLocomotion::getTerrainMap()
{
	return &terrain_;
}


double PreviewLocomotion::getSampleTime()
{
	return sample_time_;
}


void PreviewLocomotion::toWholeBodyState(WholeBodyState& full_state,
										 const ReducedBodyState& reduced_state)
{
	// Adding the time
	full_state.time = reduced_state.time;

	// From the preview model we do not know the joint states, so we neglect
	// the joint-related components of the CoM
	full_state.setBasePosition_W(reduced_state.com_pos - actual_system_com_);
	full_state.setBaseVelocity_W(reduced_state.com_vel);
	full_state.setBaseAcceleration_W(reduced_state.com_acc);

	full_state.setBaseRPY_W(reduced_state.angular_pos);
	full_state.setBaseRotationRate_W(reduced_state.angular_vel);
	full_state.setBaseRotAcceleration_W(reduced_state.angular_acc);


	// Adding the contact positions, velocities and accelerations
	// w.r.t the base frame
	dwl::rbd::BodyPosition feet_pos;
	for (rbd::BodyVector::const_iterator contact_it = reduced_state.foot_pos.begin();
			contact_it != reduced_state.foot_pos.end(); contact_it++) {
		std::string name = contact_it->first;
		Eigen::Vector3d foot_pos = contact_it->second + actual_system_com_W;

		full_state.contact_pos[name] = foot_pos;
		feet_pos[name] = foot_pos; // for IK computation
	}
	full_state.contact_vel = reduced_state.foot_vel;
	full_state.contact_acc = reduced_state.foot_acc;

	// Adding infinity contact force for active feet
	for (unsigned int f = 0; f < num_feet_; f++) {
		std::string name = feet_names_[f];

		rbd::BodyPosition::const_iterator support_it = reduced_state.support_region.find(name);
		if (support_it != reduced_state.support_region.end())
			full_state.setContactCondition(name, true);
		else
			full_state.setContactCondition(name, false);
	}


	// Adding the joint positions, velocities and accelerations
	full_state.joint_pos = Eigen::VectorXd::Zero(system_.getJointDoF());
	full_state.joint_vel = Eigen::VectorXd::Zero(system_.getJointDoF());
	full_state.joint_acc = Eigen::VectorXd::Zero(system_.getJointDoF());

	// Computing the joint positions
	kinematics_.computeInverseKinematics(full_state.joint_pos,
										 feet_pos);

	// Computing the joint velocities
	kinematics_.computeJointVelocity(full_state.joint_vel,
									 full_state.joint_pos,
									 full_state.contact_vel,
									 feet_names_);

	// Computing the joint accelerations
	kinematics_.computeJoinAcceleration(full_state.joint_acc,
										full_state.joint_pos,
										full_state.joint_vel,
										full_state.contact_vel,
										feet_names_);

	// Setting up the desired joint efforts equals to zero
	full_state.joint_eff = Eigen::VectorXd::Zero(system_.getJointDoF());
}


void PreviewLocomotion::fromWholeBodyState(ReducedBodyState& reduced_state,
										   const WholeBodyState& full_state)
{
	// Adding the actual time
	reduced_state.time = full_state.time;

	// Computing the CoM position, velocity and acceleration
	// Neglecting the joint accelerations components
	reduced_state.com_pos = system_.getSystemCoM(full_state.base_pos,
												 full_state.joint_pos);
	reduced_state.com_vel = system_.getSystemCoMRate(full_state.base_pos,
													 full_state.joint_pos,
													 full_state.base_vel,
													 full_state.joint_vel);
	reduced_state.com_acc = full_state.getBaseAcceleration_W();

	reduced_state.angular_pos = full_state.getBaseRPY_W();
	reduced_state.angular_vel = full_state.getBaseRotationRate_W();
	reduced_state.angular_acc = full_state.getBaseRotAcceleration_W();

	// Getting the world to base transformation
	Eigen::Vector3d base_traslation = full_state.getBasePosition_W();
	Eigen::Vector3d base_rpy = full_state.getBaseRPY_W();
	Eigen::Matrix3d base_rotation = math::getRotationMatrix(base_rpy);

	// Computing the CoP in the world frame
	Eigen::Vector3d cop_B;
	dynamics_.computeCenterOfPressure(cop_B,
									  full_state.contact_eff,
									  full_state.contact_pos,
									  feet_names_);
	reduced_state.cop = base_traslation + base_rotation * cop_B;

	// Getting the support region w.r.t the world frame. The support region
	// is defined by the active contacts
	rbd::BodySelector active_contacts;
	dynamics_.getActiveContacts(active_contacts,
								full_state.contact_eff,
								force_threshold_);
	reduced_state.support_region.clear();
	for (unsigned int i = 0; i < active_contacts.size(); i++) {
		std::string name = active_contacts[i];

		reduced_state.support_region[name] = base_traslation +
				base_rotation * full_state.getContactPosition_B(name);
	}

	// Adding the contact positions, velocities and accelerations
	// w.r.t the CoM frame
	for (rbd::BodyVector::const_iterator contact_it = full_state.contact_pos.begin();
			contact_it != full_state.contact_pos.end(); contact_it++) {
		std::string name = contact_it->first;
		reduced_state.foot_pos[name] = contact_it->second - actual_system_com_;
	}
	reduced_state.foot_vel = full_state.contact_vel;
	reduced_state.foot_acc = full_state.contact_acc;
}


void PreviewLocomotion::toWholeBodyTrajectory(WholeBodyTrajectory& full_traj,
											  const ReducedBodyTrajectory& reduced_traj)
{
	// Getting the number of points defined in the reduced-body trajectory
	unsigned int traj_size = reduced_traj.size();

	// Resizing the full trajectory vector
	full_traj.clear();
	full_traj.resize(traj_size);

	// Getting the full trajectory
	dwl::WholeBodyState full_state;
	for (unsigned int k = 0; k < traj_size; k++) {
		toWholeBodyState(full_state, reduced_traj[k]);

		full_traj[k] = full_state;
	}
}

} //@namespace simulation
} //@namespace dwl
