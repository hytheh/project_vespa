/**
 * @file motion_can.cpp
 * @author Lead Software Comm Developer
 * @brief Implementation of the STM32G431 FDCAN wrapper.
 * @date 2026-03-16
 */

#include "motion_can.h"

MotionCan::MotionCan(uint32_t rxPin, uint32_t txPin) : _rxPin(rxPin), _txPin(txPin) {
    _hfdcan.Instance = FDCAN1;
}

bool MotionCan::begin() {
    setupHardwarePins();

    // Enable FDCAN Clock
    __HAL_RCC_FDCAN_CLK_ENABLE();

    // Configure FDCAN1 for Classic CAN 2.0 mode, Target: 500 kbps
    // Note: Assuming a 170MHz SYSCLK and FDCAN clock source from PCLK1.
    // Time Quanta calculation will vary based on your exact clock tree.
    // These nominal values are standard for 500kbps on 170MHz G4.
    _hfdcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    _hfdcan.Init.FrameFormat  = FDCAN_FRAME_CLASSIC; // STRICTLY Classic CAN for ESP32 compatibility
    _hfdcan.Init.Mode         = FDCAN_MODE_NORMAL;
    _hfdcan.Init.AutoRetransmission = ENABLE;
    _hfdcan.Init.TransmitPause = DISABLE;
    _hfdcan.Init.ProtocolException = DISABLE;

    // Nominal Bit Timing (500 kbps)
    _hfdcan.Init.NominalPrescaler = 17; 
    _hfdcan.Init.NominalSyncJumpWidth = 1;
    _hfdcan.Init.NominalTimeSeg1 = 13;
    _hfdcan.Init.NominalTimeSeg2 = 2;

    if (HAL_FDCAN_Init(&_hfdcan) != HAL_OK) {
        return false;
    }

    if (!configureFilters()) {
        return false;
    }

    // Start the FDCAN module
    if (HAL_FDCAN_Start(&_hfdcan) != HAL_OK) {
        return false;
    }

    return true;
}

void MotionCan::setupHardwarePins() {
    // Convert Arduino Pin names to STM32 Port/Pin
    PinName rx = digitalPinToPinName(_rxPin);
    PinName tx = digitalPinToPinName(_txPin);

    // Initialize pins to Alternate Function 9 (FDCAN1)
    pin_function(rx, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF9_FDCAN1));
    pin_function(tx, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF9_FDCAN1));
}

bool MotionCan::configureFilters() {
    FDCAN_FilterTypeDef filterConfig;

    // We only want to accept messages meant for the motion node: CAN_ID_TARGET_VEC (0x020)
    filterConfig.IdType = FDCAN_STANDARD_ID;
    filterConfig.FilterIndex = 0;
    filterConfig.FilterType = FDCAN_FILTER_MASK;
    filterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filterConfig.FilterID1 = CAN_ID_TARGET_VEC; 
    filterConfig.FilterID2 = 0x7FF; // Exact match mask

    if (HAL_FDCAN_ConfigFilter(&_hfdcan, &filterConfig) != HAL_OK) {
        return false;
    }

    // Reject everything else to save CPU cycles
    HAL_FDCAN_ConfigGlobalFilter(&_hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
    
    return true;
}

bool MotionCan::sendFrame(const CAN_Frame_t* frame) {
    if (!frame) return false;

    FDCAN_TxHeaderTypeDef txHeader;
    txHeader.Identifier = frame->id;
    txHeader.IdType = FDCAN_STANDARD_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = frame->dlc; // e.g., FDCAN_DLC_BYTES_4 for ERR messages
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_OFF;
    txHeader.FDFormat = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0;

    // Convert standard integer DLC to FDCAN macro encoding
    // Map 0-8 to FDCAN_DLC_BYTES_0 to FDCAN_DLC_BYTES_8
    txHeader.DataLength = frame->dlc << 16; 

    // Wait until there is space in the TX FIFO
    if (HAL_FDCAN_GetTxFifoFreeLevel(&_hfdcan) == 0) {
        return false; 
    }

    if (HAL_FDCAN_AddMessageToTxFifoQ(&_hfdcan, &txHeader, (uint8_t*)frame->data) != HAL_OK) {
        return false;
    }

    return true;
}

bool MotionCan::readFrame(CAN_Frame_t* frame) {
    if (!frame) return false;

    // Check if RX FIFO 0 has new messages
    if (HAL_FDCAN_GetRxFifoFillLevel(&_hfdcan, FDCAN_RX_FIFO0) == 0) {
        return false;
    }

    FDCAN_RxHeaderTypeDef rxHeader;
    if (HAL_FDCAN_GetRxMessage(&_hfdcan, FDCAN_RX_FIFO0, &rxHeader, frame->data) != HAL_OK) {
        return false;
    }

    frame->id = rxHeader.Identifier;
    // Decode FDCAN DLC macro back to integer length
    frame->dlc = rxHeader.DataLength >> 16; 

    return true;
}