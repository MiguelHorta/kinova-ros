/**
 *      _____
 *     /  _  \
 *    / _/ \  \
 *   / / \_/   \
 *  /  \_/  _   \  ___  _    ___   ___   ____   ____   ___   _____  _   _
 *  \  / \_/ \  / /  _\| |  | __| / _ \ | ++ \ | ++ \ / _ \ |_   _|| | | |
 *   \ \_/ \_/ /  | |  | |  | ++ | |_| || ++ / | ++_/| |_| |  | |  | +-+ |
 *    \  \_/  /   | |_ | |_ | ++ |  _  || |\ \ | |   |  _  |  | |  | +-+ |
 *     \_____/    \___/|___||___||_| |_||_| \_\|_|   |_| |_|  |_|  |_| |_|
 *             ROBOTICS™
 *
 *  File: jaco_comm.cpp
 *  Desc: Class for moving/querying jaco arm.
 *  Auth: Alex Bencz, Jeff Schmidt
 *
 *  Copyright (c) 2013, Clearpath Robotics, Inc.
 *  All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Clearpath Robotics, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CLEARPATH ROBOTICS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Please send comments, questions, or patches to skynet@clearpathrobotics.com
 *
 */

#include <ros/ros.h>
#include "jaco_driver/jaco_comm.h"

// TODO: Remove me when done (used for syscall(SYS_gettid))
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>


namespace jaco {

template<class T>
void printRaw(const T& something) {
    void const *qs = static_cast<void const *>(&something);
    unsigned char const *p = static_cast<unsigned char const *>(qs);
    for (size_t i=0; i<sizeof(something); i++) {
        printf("%02x ", p[i]);
    }
    putchar('\n');
    fflush(stdout);
}

JacoComm::JacoComm(JacoAngles home)
    : is_software_stop_(false), home_position_(home)
{
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO("Initializing API");
    assert(jaco_api_.initAPI() == 1);

    // On a cold boot the arm may not respond to commands from the API right away.
    // This kick-starts the Control API so that it's ready to go.
    assert(jaco_api_.startControlAPI() == 1);
    ros::Duration(3.0).sleep();
    //assert(jaco_api_.stopControlAPI() == 1);

    QuickStatus qs;
    getQuickStatus(qs);

    ros::Duration(3.0).sleep();
    getQuickStatus(qs);

    ros::Duration(3.0).sleep();
    getQuickStatus(qs);

    initFingers();

    ros::Duration(3.0).sleep();
    getQuickStatus(qs);
}

JacoComm::~JacoComm() {
    jaco_api_.closeAPI();
}

/*!
 * \brief Determines whether the arm has returned to its "Home" state.
 *
 * Checks the current joint angles, then compares them to the known "Home"
 * joint angles.
 */
bool JacoComm::isHomed(void) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);

    QuickStatus quick_status;
    ROS_INFO_STREAM("Getting quick status to check if arm is homed");
    getQuickStatus(quick_status);

    if (quick_status.RetractType == 1) {
        ROS_INFO("Arm is homed");
        return 1;
    } else {
        ROS_INFO("Arm is not homed");
        return 0;
    }
}

/*!
 * \brief Send the arm to the "home" position.
 *
 * The code replicates the function of the "home" button on the user controller
 * by "pressing" the home button long enough for the arm to return to the home
 * position.
 *
 * Fingers are homed by manually opening them fully, then returning them to a
 * half-open position.
 */
void JacoComm::homeArm(void) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("Arm is stopped, cannot home");
        return;
    } else if (isHomed()) {
        ROS_INFO("Arm is already in \"home\" position");
        return;
    }

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();

    ROS_INFO("Homing the arm");
    jaco_api_.moveHome();
}

/*!
 * \brief Initialize finger actuators.
 *
 * Move fingers to the full-open position to initialize them for use.
 * Note, The this routine requires firmware version 5.05.x (or higher?).
 */
void JacoComm::initFingers(void) {
    ROS_INFO("Initializing fingers...this will take a few seconds");
    jaco_api_.initFingers();

    QuickStatus qs;
    getQuickStatus(qs);
    FingerAngles finger_angles;
    finger_angles.Finger1 = 50.0;
    finger_angles.Finger2 = 50.0;
    finger_angles.Finger3 = 50.0;
    setFingerPositions(finger_angles, 5.0);
    ros::Duration(2.0).sleep();

    getQuickStatus(qs);
    finger_angles.Finger1 = 0.25;
    finger_angles.Finger2 = 0.25;
    finger_angles.Finger3 = 0.25;
    setFingerPositions(finger_angles, 5.0);
    ros::Duration(2.0).sleep();

    getQuickStatus(qs);
    // Set the fingers to "half-open"
    finger_angles.Finger1 = 25.0;
    finger_angles.Finger2 = 25.0;
    finger_angles.Finger3 = 25.0;
    setFingerPositions(finger_angles, 5.0);
    ros::Duration(2.0).sleep();

    return;
}

/*!
 * \brief Sends a joint angle command to the Jaco arm.
 *
 * Waits until the arm has stopped moving before releasing control of the API.
 */
void JacoComm::setJointAngles(JacoAngles &angles, int timeout, bool push) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("The angles could not be set because the arm is stopped");
        return;
    }

    TrajectoryPoint jaco_position;
    jaco_position.InitStruct();
    memset(&jaco_position, 0, sizeof(jaco_position));  // zero structure

    if (push) {
        jaco_api_.eraseAllTrajectories();
    }

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
    jaco_api_.setAngularControl();

    //Jaco_Position.LimitationsActive = false;
    jaco_position.Position.Delay = 0.0;
    jaco_position.Position.Type = ANGULAR_POSITION;
    jaco_position.Position.Actuators = angles;

    jaco_api_.sendAdvanceTrajectory(jaco_position);
}

/*!
 * \brief Sends a cartesian coordinate trajectory to the Jaco arm.
 *
 * Waits until the arm has stopped moving before releasing control of the API.
 */
void JacoComm::setCartesianPosition(JacoPose &position, int timeout, bool push) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("The position could not be set because the arm is stopped");
        return;
    }

    TrajectoryPoint jaco_position;
    jaco_position.InitStruct();
    memset(&jaco_position, 0, sizeof(jaco_position));  // zero structure

    if (push) {
        jaco_api_.eraseAllTrajectories();
    }

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
    jaco_api_.setCartesianControl();

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    jaco_position.Position.Delay = 0.0;
    jaco_position.Position.Type = CARTESIAN_POSITION;
    jaco_position.Position.HandMode = HAND_NOMOVEMENT;

    // These values will not be used but are initialized anyway.
    jaco_position.Position.Actuators.Actuator1 = 0.0f;
    jaco_position.Position.Actuators.Actuator2 = 0.0f;
    jaco_position.Position.Actuators.Actuator3 = 0.0f;
    jaco_position.Position.Actuators.Actuator4 = 0.0f;
    jaco_position.Position.Actuators.Actuator5 = 0.0f;
    jaco_position.Position.Actuators.Actuator6 = 0.0f;

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    jaco_position.Position.CartesianPosition = position;
    //Jaco_Position.Position.CartesianPosition.ThetaZ += 0.0001; // A workaround for a bug in the Kinova API


    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    jaco_api_.sendBasicTrajectory(jaco_position);

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

}

/*!
 * \brief Sets the finger positions
 */
void JacoComm::setFingerPositions(FingerAngles &fingers, int timeout, bool push) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("The fingers could not be set because the arm is stopped");
        return;
    }

    TrajectoryPoint jaco_position;
    jaco_position.InitStruct();
    memset(&jaco_position, 0, sizeof(jaco_position));  // zero structure

    if (push) {
        jaco_api_.eraseAllTrajectories();
    }

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();

    // Initialize Cartesian control of the fingers
    jaco_position.Position.HandMode = POSITION_MODE;
    jaco_position.Position.Type = CARTESIAN_POSITION;
    jaco_position.Position.Fingers = fingers;
    jaco_position.Position.Delay = 0.0;
    jaco_position.LimitationsActive = 0;

    // These values will not be used but are initialized anyway.
    jaco_position.Position.Actuators.Actuator1 = 0.0f;
    jaco_position.Position.Actuators.Actuator2 = 0.0f;
    jaco_position.Position.Actuators.Actuator3 = 0.0f;
    jaco_position.Position.Actuators.Actuator4 = 0.0f;
    jaco_position.Position.Actuators.Actuator5 = 0.0f;
    jaco_position.Position.Actuators.Actuator6 = 0.0f;

    // When loading a cartesian position for the fingers, values are required for the arm joints
    // as well or the arm goes nuts.  Grab the current position and feed it back to the arm.
    JacoPose pose;
    getCartesianPosition(pose);

    jaco_position.Position.CartesianPosition.X = pose.X;
    jaco_position.Position.CartesianPosition.Y = pose.Y;
    jaco_position.Position.CartesianPosition.Z = pose.Z;
    jaco_position.Position.CartesianPosition.ThetaX = pose.ThetaX;
    jaco_position.Position.CartesianPosition.ThetaY = pose.ThetaY;
    jaco_position.Position.CartesianPosition.ThetaZ = pose.ThetaZ;

    jaco_api_.sendAdvanceTrajectory(jaco_position);
}

/*!
 * \brief Set the angular velocity of the joints
 */
void JacoComm::setVelocities(AngularInfo joint_vel) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("The velocities could not be set because the arm is stopped");
        return;
    }

    TrajectoryPoint jaco_velocity;
    jaco_velocity.InitStruct();

    memset(&jaco_velocity, 0, sizeof(jaco_velocity)); // zero structure

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
    jaco_velocity.Position.Type = ANGULAR_VELOCITY;

    // confusingly, velocity is passed in the position struct
    jaco_velocity.Position.Actuators = joint_vel;
    jaco_api_.sendAdvanceTrajectory(jaco_velocity);
}

/*!
 * \brief Set the cartesian velocity of the tool tip
 */
void JacoComm::setCartesianVelocities(CartesianInfo velocities) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    if (isStopped()) {
        ROS_INFO("The cartesian velocities could not be set because the arm is stopped");
        jaco_api_.eraseAllTrajectories();
        return;
    }

    TrajectoryPoint jaco_velocity;
    jaco_velocity.InitStruct();

    memset(&jaco_velocity, 0, sizeof(jaco_velocity));  // zero structure

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
    jaco_velocity.Position.Type = CARTESIAN_VELOCITY;

    // confusingly, velocity is passed in the position struct
    jaco_velocity.Position.CartesianPosition = velocities;
    jaco_api_.sendAdvanceTrajectory(jaco_velocity);
}

/*!
 * \brief Obtains the current arm configuration.
 *
 * This is the configuration which are stored on the arm itself. Many of these
 * configurations may be set using the Windows interface.
 */
void JacoComm::setConfig(ClientConfigurations config) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    jaco_api_.setClientConfigurations(config);
}

/*!
 * \brief API call to obtain the current angular position of all the joints.
 */
void JacoComm::getJointAngles(JacoAngles &angles) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    AngularPosition jaco_angles;
    memset(&jaco_angles, 0, sizeof(jaco_angles));  // zero structure
    jaco_api_.getAngularPosition(jaco_angles);
    angles = jaco_angles.Actuators;
}

/*!
 * \brief API call to obtain the current cartesian position of the arm.
 */
void JacoComm::getCartesianPosition(JacoPose &position) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    CartesianPosition jaco_cartesian_position;
    memset(&jaco_cartesian_position, 0, sizeof(jaco_cartesian_position));  // zero structure
    jaco_api_.getCartesianPosition(jaco_cartesian_position);
    position = JacoPose(jaco_cartesian_position.Coordinates);
}

/*!
 * \brief API call to obtain the current finger positions.
 */
void JacoComm::getFingerPositions(FingerAngles &fingers) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    CartesianPosition jaco_cartesian_position;
    memset(&jaco_cartesian_position, 0, sizeof(jaco_cartesian_position));  // zero structure
    jaco_api_.getCartesianPosition(jaco_cartesian_position);
    fingers = jaco_cartesian_position.Fingers;
}

/*!
 * \brief API call to obtain the current client configuration.
 */
void JacoComm::getConfig(ClientConfigurations &config) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    memset(&config, 0, sizeof(config));  // zero structure
    jaco_api_.getClientConfigurations(config);
}

/*!
 * \brief API call to obtain the current "quick status".
 */
void JacoComm::getQuickStatus(QuickStatus &quick_status) {
    boost::recursive_mutex::scoped_lock lock(api_mutex_);
//    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    memset(&quick_status, 0, sizeof(quick_status));  // zero structure
    int result = jaco_api_.getQuickStatus(quick_status);
    printf("Quick status: ");
    printRaw(quick_status);
}

/*!
 * \brief Dumps the current joint angles onto the screen.
 */
void JacoComm::printAngles(JacoAngles &angles) {
//    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    ROS_INFO("Joint angles (deg) -- J1: %f, J2: %f J3: %f, J4: %f, J5: %f, J6: %f",
             angles.Actuator1, angles.Actuator2, angles.Actuator3,
             angles.Actuator4, angles.Actuator5, angles.Actuator6);
}

/*!
 * \brief Dumps the current cartesian positions onto the screen.
 */
void JacoComm::printPosition(JacoPose &position) {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    ROS_INFO("Arm position\n"
             "\tposition (m) -- x: %f, y: %f z: %f\n"
             "\trotation (rad) -- theta_x: %f, theta_y: %f, theta_z: %f",
             position.X, position.Y, position.Z,
             position.ThetaX, position.ThetaY, position.ThetaZ);
}

/*!
 * \brief Dumps the current finger positions onto the screen.
 */
void JacoComm::printFingers(FingersPosition fingers) {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    ROS_INFO("Finger positions -- F1: %f, F2: %f, F3: %f",
    fingers.Finger1, fingers.Finger2, fingers.Finger3);
}

/*!
 * \brief Dumps the client configuration onto the screen.
 */
void JacoComm::printConfig(ClientConfigurations config) {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    ROS_INFO_STREAM("Arm configuration:\n"
                    "\tClientID: " << config.ClientID <<
                    "\n\tClientName: " << config.ClientName <<
                    "\n\tOrganization: " << config.Organization <<
                    "\n\tSerial:" << config.Serial <<
                    "\n\tModel: " << config.Model <<
                    "\n\tMaxForce: " << config.MaxForce <<
                    "\n\tSensibility: " <<  config.Sensibility <<
                    "\n\tDrinkingHeight: " << config.DrinkingHeight <<
                    "\n\tComplexRetractActive: " << config.ComplexRetractActive <<
                    "\n\tRetractedPositionAngle: " << config.RetractedPositionAngle <<
                    "\n\tRetractedPositionCount: " << config.RetractedPositionCount <<
                    "\n\tDrinkingDistance: " << config.DrinkingDistance <<
                    "\n\tFingers2and3Inverted: " << config.Fingers2and3Inverted <<
                    "\n\tDrinkingLength: " << config.DrinkingLenght <<
                    "\n\tDeletePreProgrammedPositionsAtRetract: " <<
                    config.DeletePreProgrammedPositionsAtRetract <<
                    "\n\tEnableFlashErrorLog: " << config.EnableFlashErrorLog <<
                    "\n\tEnableFlashPositionLog: " << config.EnableFlashPositionLog);
}


void JacoComm::stop() {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    is_software_stop_ = true;

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
    jaco_api_.eraseAllTrajectories();

    JoystickCommand home_command;
    memset(&home_command, 0, sizeof(home_command));  // zero structure

    home_command.ButtonValue[2] = 1;
    jaco_api_.sendJoystickCommand(home_command);
    jaco_api_.eraseAllTrajectories();
    ros::Duration(0.05).sleep();

    home_command.ButtonValue[2] = 0;
    jaco_api_.sendJoystickCommand(home_command);

    ROS_INFO_STREAM("file: " << __FILE__ << ", line: " << __LINE__ << ", "
                    "process: " << getpid() << ", thread: " << syscall(SYS_gettid));
}

void JacoComm::start() {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);

    boost::recursive_mutex::scoped_lock lock(api_mutex_);
    is_software_stop_ = false;

    jaco_api_.stopControlAPI();
    jaco_api_.startControlAPI();
}

bool JacoComm::isStopped() {
    return is_software_stop_;
}

/*!
 * \brief Wait for the arm to reach the "home" position.
 *
 * \param timeout Timeout after which to give up waiting for arm to finish "homing".
 */
void JacoComm::waitForHome(int timeout) {
    ROS_INFO_STREAM("File: " << __FILE__ << ", line: " << __LINE__ << ", function: " << __PRETTY_FUNCTION__);
    double start_secs;
    double current_secs;

    // If ros is still running use rostime, else use system time
    if (ros::ok()) {
        start_secs = ros::Time::now().toSec();
        current_secs = ros::Time::now().toSec();
    } else {
        start_secs = (double) time(NULL);
        current_secs = (double) time(NULL);
    }

    // while we have not timed out
    while ((current_secs - start_secs) < timeout) {
        ros::Duration(0.5).sleep();

        //If ros is still running use rostime, else use system time
        if (ros::ok()) {
            current_secs = ros::Time::now().toSec();
            ros::spinOnce();
        } else {
            current_secs = (double) time(NULL);
        }

        if (isHomed()) {
            ROS_INFO("Arm has homed");
            ros::Duration(1.0).sleep();  // Grants a bit more time for the arm to "settle"
            return;
        } else {
            ROS_INFO("Still homing the arm");
        }
    }

    ROS_WARN("Timed out waiting for arm to return home");
}

}  // namespace jaco
