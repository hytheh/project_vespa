/**
 * @file main.cpp
 * @brief Entry Point for the V.E.S.P.A. Vision System.
 *
 * @section DESCRIPTION
 * Initializes the "Hunter" application. It orchestrates:
 * 1. The GStreamer/DeepStream Main Loop.
 * 2. The SocketCAN Communication Thread.
 * 3. The Unix Signal Handler (SIGINT/SIGTERM) for safe shutdown.
 *
 * @section FLOW
 * - Init CAN Interface -> Init Pipeline -> Start Main Loop -> Wait for EOS/Error.
 *
 * @note This file is kept minimal. Logic is delegated to `vespa_pipeline` class.
 */