/**
 * @file vespa_ballistics.hpp
 * @brief Trajectory Prediction and Ballistics Solver.
 *
 * @section DESCRIPTION
 * Defines the `VespaBallistics` class.
 * Uses a Kalman Filter to smooth noisy detection data and predict 
 * future target position based on system latency.
 *
 * @section PHYSICS_MODEL
 * - Target: Vespa velutina (approx. constant velocity over short timeframe).
 * - Latency factors: Processing time + Turret mechanical lag + Laser travel time.
 */