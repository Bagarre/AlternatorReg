// regulateFieldPWM.h
#ifndef REGULATE_FIELD_PWM_H
#define REGULATE_FIELD_PWM_H

#include <algorithm>

/**
 * @brief A simple PID controller for alternator field PWM regulation.
 *
 * This class computes a PWM duty cycle based on voltage error,
 * using Proportional-Integral-Derivative control.
 */
class RegulateFieldPWM {
public:
    /**
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     * @param dt Time delta between updates (in seconds)
     */
    RegulateFieldPWM(float kp, float ki, float kd, float dt);

    /**
     * @brief Set the desired output voltage target
     * @param targetVoltage Desired system voltage (e.g., 14.2V)
     */
    void setTargetVoltage(float targetVoltage);

    /**
     * @brief Compute the new PWM duty cycle given current measurements
     * @param currentVoltage Measured output voltage
     * @return Duty cycle (0.0 to 1.0) to apply to the field coil
     */
    float update(float currentVoltage);

private:
    float kp_;             // proportional coefficient
    float ki_;             // integral coefficient
    float kd_;             // derivative coefficient
    float dt_;             // update interval in seconds

    float targetVoltage_;  // setpoint voltage
    float prevError_;      // error from previous cycle
    float integral_;       // accumulated integral term
};

#endif // REGULATE_FIELD_PWM_H


// regulateFieldPWM.cpp
#include "regulateFieldPWM.h"

/**
 * Constructor initializes PID gains and clears state
 */
RegulateFieldPWM::RegulateFieldPWM(float kp, float ki, float kd, float dt)
    : kp_(kp), ki_(ki), kd_(kd), dt_(dt),
      targetVoltage_(0.0f), prevError_(0.0f), integral_(0.0f) {}

void RegulateFieldPWM::setTargetVoltage(float targetVoltage) {
    targetVoltage_ = targetVoltage;
    // Reset integral and derivative terms on new setpoint
    prevError_ = 0.0f;
    integral_ = 0.0f;
}

float RegulateFieldPWM::update(float currentVoltage) {
    // Calculate error between desired and actual voltage
    float error = targetVoltage_ - currentVoltage;

    // Accumulate integral term
    integral_ += error * dt_;

    // Calculate derivative term
    float derivative = (error - prevError_) / dt_;
    prevError_ = error;

    // Compute raw PID output
    float output = kp_ * error + ki_ * integral_ + kd_ * derivative;

    // Convert to duty cycle (0.0 to 1.0)
    float duty = output;
    // Clamp duty cycle to valid range
    duty = std::clamp(duty, 0.0f, 1.0f);

    return duty;
}
