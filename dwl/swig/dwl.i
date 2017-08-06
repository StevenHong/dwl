%module dwl
%{
#include <Python.h>
#include <dwl/WholeBodyState.h>
#include <dwl/model/FloatingBaseSystem.h>
#include <dwl/model/WholeBodyKinematics.h>
%}


/// Data structure in the target language holding data
#ifdef SWIGPYTHON
#define GUESTOBJECT PyObject
#elif defined(SWIGMATLAB)
#define GUESTOBJECT mxArray
#else
#define GUESTOBJECT void
#endif

// typemaps.i is a built-in swig interface that lets us map c++ types to other
// types in our language of choice. We'll use it to map Eigen matrices to
// Numpy arrays.
%include <typemaps.i>
%include <std_string.i>
%include <std_vector.i>
%include <std_map.i>

// eigen.i is found in ../swig/ and contains specific definitions to convert
// Eigen matrices into Numpy arrays.
%include <eigen.i>


%template(Matrix3d_List) std::vector<Eigen::Matrix3d>;
%template(Matrix4d_List) std::vector<Eigen::Matrix4d>;
%template(MatrixXd_List) std::vector<Eigen::MatrixXd>;
%template(Vector2d_List) std::vector<Eigen::Vector2d>;
%template(Vector3d_List) std::vector<Eigen::Vector3d>;
%template(VectorXd_List) std::vector<Eigen::VectorXd>;
//%template(Vector3d_Dict) std::map<std::string,Eigen::Vector3d>;
//%template(VectorXd_Dict) std::map<std::string,Eigen::VectorXd>;

// Since Eigen uses templates, we have to declare exactly which types we'd
// like to generate mappings for.
%eigen_typemaps(Eigen::Vector2d)
%eigen_typemaps(Eigen::Vector3d)
%eigen_typemaps(dwl::rbd::Vector6d)
%eigen_typemaps(Eigen::VectorXd)
%eigen_typemaps(Eigen::Matrix3d)
%eigen_typemaps(Eigen::Matrix4d)
%eigen_typemaps(Eigen::MatrixXd)
//%eigen_typemaps(Eigen::Quaterniond) TODO it doesn't work yet
// Even though Eigen::MatrixXd is just a typedef for Eigen::Matrix<double,
// Eigen::Dynamic, Eigen::Dynamic>, our templatedInverse function doesn't
// compile correctly unless we also declare typemaps for Eigen::Matrix<double,
// Eigen::Dynamic, Eigen::Dynamic>. Not totally sure why that is.
//%eigen_typemaps(Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>)

%template(string_List) std::vector<std::string>;
%template(string_uint) std::map<std::string,unsigned int>;
%template(string_jointLimits) std::map<std::string,urdf::JointLimits>;


// Renaming orientation methods to get rid of the ambiguity
%rename(getQuaternion_RM) getQuaternion(const Eigen::Matrix3d& rotation_mtx);
%rename(getQuaternion_RPY) getQuaternion(const Eigen::Vector3d& rpy);
%rename(getInverseEulerAnglesRatesMatrix_RPY) getInverseEulerAnglesRatesMatrix(const Eigen::Vector3d& rpy);
%rename(getInverseEulerAnglesRatesMatrix_RM) getInverseEulerAnglesRatesMatrix(const Eigen::Matrix3d& rotation_mtx);
%rename(getInverseEulerAnglesRatesMatrix_Q) getInverseEulerAnglesRatesMatrix(const Eigen::Quaterniond& quaternion);
%rename(getEulerAnglesRatesMatrix_RPY) getEulerAnglesRatesMatrix(const Eigen::Vector3d& rpy);
%rename(getEulerAnglesRatesMatrix_RM) getEulerAnglesRatesMatrix(const Eigen::Matrix3d& rotation_mtx);
%rename(getEulerAnglesRatesMatrix_Q) getEulerAnglesRatesMatrix(const Eigen::Quaterniond& quaternion);

// Ignoring two methods of the WholeBodyKinematic class that generate
// ambiguity
%ignore computeJointPosition(Eigen::VectorXd&,
							 const rbd::BodyVector3d&);
%ignore computeInverseKinematics(rbd::Vector6d&,
								 Eigen::VectorXd&,
								 const rbd::BodyVector3d&);

%rename(urdf_Joint) urdf::Joint;
%rename(urdf_Pose) urdf::Pose;
%include <dwl/utils/RigidBodyDynamics.h>
%include <dwl/WholeBodyState.h>
%include <urdf_model/pose.h>
%include <urdf_model/joint.h>
%include <dwl/utils/URDF.h>
%include <dwl/utils/Orientation.h>
%include <dwl/model/FloatingBaseSystem.h>
%include <dwl/model/WholeBodyKinematics.h>
