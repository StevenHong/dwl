#ifndef DWL_WholeBodyDynamics_H
#define DWL_WholeBodyDynamics_H

#include <model/WholeBodyKinematics.h>
#include <rbdl/rbdl.h>
#include <rbdl/addons/urdfreader/urdfreader.h>
#include <utils/RigidBodyDynamics.h>
#include <utils/Math.h>
#include <utils/utils.h>


namespace dwl
{

namespace model
{

/**
 * @brief WholeBodyDynamics class implements the dynamics methods for a floating-base robot
 */
class WholeBodyDynamics
{
	public:
		/** @bried Constructor function */
		WholeBodyDynamics();

		/** @brief Destructor function */
		~WholeBodyDynamics();

		/**
		 * @brief Build the model rigid-body system from an URDF file
		 * @param std::string URDF file
		 * @param struct rbd::ReducedFloatingBase* Defined only when it's not fully floating-base, i.e. a floating-
		 * base with physical constraints
		 * @param Print model information
		 */
		void modelFromURDF(std::string file, struct rbd::ReducedFloatingBase* reduce_base = NULL, bool info = false);

		/**
		 * @brief Computes the whole-body inverse dynamics, assuming a fully actuated robot, using the
		 * Recursive Newton-Euler Algorithm (RNEA). An applied external force is defined for a certain body, movable or
		 * fixed body, where a fixed body is considered a fixed point of a movable one. These forces are represented as
		 * Cartesian forces applied to the body, where the first three elements are the moments and the last three
		 * elements are the linear forces. In general a point only has linear forces, but with this representation we
		 * can model the forces applied by a surface of contact in the center of pressure of it.
		 * @param rbd::Vector6d& Base wrench
		 * @param Eigen::VectorXd& Joint forces
		 * @param const rbd::Vector6d& Base position
		 * @param const Eigen::VectorXd& Joint position
		 * @param const rbd::Vector6d& Base velocity
		 * @param const Eigen::VectorXd& Joint velocity
		 * @param const rbd::Vector6d& Base acceleration with respect to a gravity field
		 * @param const Eigen::VectorXd& Joint acceleration
		 * @param const rbd::BodyForce External force applied to a certain body of the robot
		 */
		void computeInverseDynamics(rbd::Vector6d& base_wrench,
									Eigen::VectorXd& joint_forces,
									const rbd::Vector6d& base_pos,
									const Eigen::VectorXd& joint_pos,
									const rbd::Vector6d& base_vel,
									const Eigen::VectorXd& joint_vel,
									const rbd::Vector6d& base_acc,
									const Eigen::VectorXd& joint_acc,
									const rbd::BodyForce& ext_force = rbd::BodyForce());
		/**
		 * @brief Computes the whole-body inverse dynamics using the Recursive Newton-Euler Algorithm (RNEA) for a
		 * floating-base robot (RX,RY,RZ,TX,TY,TZ). An applied external force is defined for a certain body, movable
		 * or fixed body, where a fixed body is considered a fixed point of a movable one. These forces are represented
		 * as Cartesian forces applied to the body, where the first three elements are the moments and the last three
		 * elements are the linear forces. In general a point only has linear forces, but with this representation we
		 * can model the forces applied by a surface of contact in the center of pressure of it.
		 * @param rbd::Vector6d& Base acceleration with respect to a gravity field
		 * @param Eigen::VectorXd& Joint forces
		 * @param const rbd::Vector6d& Base position
		 * @param const Eigen::VectorXd& Joint position
		 * @param const rbd::Vector6d& Base velocity
		 * @param const Eigen::VectorXd& Joint velocity
		 * @param const Eigen::VectorXd& Joint acceleration
		 * @param const rbd::BodyForce External force applied to a certain body of the robot
		 */
		void computeFloatingBaseInverseDynamics(rbd::Vector6d& base_acc,
												Eigen::VectorXd& joint_forces,
												const rbd::Vector6d& base_pos,
												const Eigen::VectorXd& joint_pos,
												const rbd::Vector6d& base_vel,
												const Eigen::VectorXd& joint_vel,
												const Eigen::VectorXd& joint_acc,
												const rbd::BodyForce& ext_force = rbd::BodyForce());

		/**
		 * @brief Computes the constrained whole-body inverse dynamics using the Recursive Newton-Euler
		 * Algorithm (RNEA). Constrained are defined by the contacts of the robot. Contacts could be defined
		 * for movable and fixed bodies, where a fixed body is considered a fixed point of a movable one.
		 * Thus, this approach allows us to compute the inverse dynamic when we have a predefined set of
		 * contacts, and without specific information of the contact forces of these contacts. Here we are
		 * assuming that the desired movement is realizable without base wrench (i.e. the hand's God).
		 * @param Eigen::VectorXd& Joint forces
		 * @param const rbd::Vector6d& Base position
		 * @param const Eigen::VectorXd& Joint position
		 * @param const rbd::Vector6d& Base velocity
		 * @param const Eigen::VectorXd& Joint velocity
		 * @param const rbd::Vector6d& Base acceleration with respect to a gravity field
		 * @param const Eigen::VectorXd& Joint acceleration
		 * @param const rbd::BodyForce External force applied to a certain body of the robot
		 */
		void computeConstrainedFloatingBaseInverseDynamics(Eigen::VectorXd& joint_forces,
														   const rbd::Vector6d& base_pos,
														   const Eigen::VectorXd& joint_pos,
														   const rbd::Vector6d& base_vel,
														   const Eigen::VectorXd& joint_vel,
														   const rbd::Vector6d& base_acc,
														   const Eigen::VectorXd& joint_acc,
														   const rbd::BodySelector& contacts);


	private:
		/**
		 * Converts the applied external forces to RBDL format
		 * @param std::vector<RigidBodyDynamcis::Math::SpatialVector>& RBDL external forces format
		 * @param const rbd::BodyForce& External forces
		 * @param const Eigen::VectorXd& Generalized joint position
		 */
		void convertAppliedExternalForces(std::vector<RigidBodyDynamics::Math::SpatialVector>& f_ext,
										  const rbd::BodyForce& ext_force,
										  const Eigen::VectorXd& generalized_joint_pos);

		/** @brief Model of the rigid-body system */
		RigidBodyDynamics::Model robot_model_;

		/* @brief Body ids */
		rbd::BodyID body_id_;

		/** @brief Kinematic model */
		WholeBodyKinematics kinematics_;

		/** @brief Defines the type of dynamic system, e.g. fixed or floating-base system */
		enum rbd::TypeOfSystem type_of_system_;

		/** @brief A floating-base definition only used for reduced floating-base systems */
		rbd::ReducedFloatingBase* reduced_base_;
};

} //@namespace model
} //@namespace dwl

#endif