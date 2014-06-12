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
 *  File: jaco_types.h
 *  Desc: Wrappers around Kinova types.
 *  Auth: Alex Bencz
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

#ifndef _JACO_TYPES_H_
#define _JACO_TYPES_H_

#include <KinovaTypes.h>
#include <geometry_msgs/Pose.h>
#include <jaco_msgs/JointAngles.h>
#include <jaco_msgs/FingerPosition.h>


namespace jaco {
// A note on style in the following classes: these classes inherit
// from the Kinova API. That API follows a different style for naming
// (e.g., for methods and members). In these classes we try to
// transition to the ROS style; however, there will be cases where the
// Kinova style is visible. When using these classes, one should only
// see the ROS style.

class JacoPose : public CartesianInfo {
 public:
    JacoPose() {}
    JacoPose(const geometry_msgs::Pose &pose);
    JacoPose(const CartesianInfo &pose);

    geometry_msgs::Pose constructPoseMsg();
    bool compareToOther(const JacoPose &, float tolerance) const;
};

class JacoAngles : public AngularInfo {
 public:
    JacoAngles() {}
    JacoAngles(const jaco_msgs::JointAngles &angles);
    JacoAngles(const AngularInfo &angles);

    jaco_msgs::JointAngles constructAnglesMsg();
    bool compareToOther(const JacoAngles &, float tolerance) const;
};

class FingerAngles : public FingersPosition {
 public:
    FingerAngles() {}
    FingerAngles(const jaco_msgs::FingerPosition &position);
    FingerAngles(const FingersPosition &angle);

    jaco_msgs::FingerPosition constructFingersMsg();
    bool compareToOther(const FingerAngles &, float tolerance) const;
};

}
#endif  // _JACO_TYPES_H_
