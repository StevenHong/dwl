#include <environment/BodyOrientationFeature.h>


namespace dwl
{

namespace environment
{

BodyOrientationFeature::BodyOrientationFeature() : flat_threshold_(0.0 * (M_PI / 180.0)), roll_threshold_(30.0 * (M_PI / 180.0)),
		pitch_threshold_(30.0 * (M_PI / 180.0))
{
	name_ = "Body Orientation";
}


BodyOrientationFeature::~BodyOrientationFeature()
{

}


void BodyOrientationFeature::computeReward(double& reward_value, RobotAndTerrain info) //TODO Finish this feature
{
	// Setting the resolution of the terrain
	space_discretization_.setEnvironmentResolution(info.resolution, true);

	// Getting the potential foothold
	std::vector<Contact> potential_footholds = info.current_contacts;
	potential_footholds.push_back(info.potential_contact);

	// Computing the potential stance
	std::vector<Eigen::Vector3f> stance;
	Eigen::Vector3f leg_position;
	for (int i = 0; i < potential_footholds.size(); i++) {
		float foothold_x = potential_footholds[i].position(0);
		float foothold_y = potential_footholds[i].position(1);
		float foothold_z = potential_footholds[i].position(2);

		leg_position << foothold_x, foothold_y, foothold_z;
		stance.push_back(leg_position);
	}

	// Computing the plane parameters
	utils::Math math;
	Eigen::Vector3d normal;
	math.computePlaneParameters(normal, stance);

	// Computing the roll and pitch angles
	Eigen::Quaterniond normal_quaternion;
	Eigen::Vector3d origin;
	origin << 0, 0, 1;
	normal_quaternion.setFromTwoVectors(origin, normal);

	double r, p, y;
	Orientation orientation(normal_quaternion);
	orientation.getRPY(r, p, y);

	// Computing the reward value
	double roll_reward, pitch_reward;
	r = fabs(r);
	p = fabs(p);
	if (r < flat_threshold_)
		roll_reward = 0.0;
	else {
		if (r < roll_threshold_) {
			roll_reward = log((roll_threshold_ - r) / (roll_threshold_ - flat_threshold_));
			if (max_reward_ > roll_reward)
				roll_reward = max_reward_;
		} else
			roll_reward = max_reward_;
	}

	if (p < flat_threshold_)
		pitch_reward = 0.0;
	else {
		if (p < pitch_threshold_) {
			pitch_reward = log(fabs((pitch_threshold_ - p) / (pitch_threshold_ - flat_threshold_)));
			if (max_reward_ > roll_reward)
				pitch_reward = max_reward_;
		} else
			pitch_reward = max_reward_;
	}

	reward_value = roll_reward + pitch_reward;
}

} //@namespace environment
} //@namespace dwl