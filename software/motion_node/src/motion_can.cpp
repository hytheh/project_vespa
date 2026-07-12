/**
 * @file motion_can.cpp
 * @author Lead Software Comm Developer / Motion Node Lead
 * @brief Implementation of the STM32G431 FDCAN wrapper.
 */

#include "motion_can.h"

MotionCan::MotionCan(uint32_t rxPin, uint32_t txPin) : _rxPin(rxPin), _txPin(txPin) {
    _hfdcan.Instance = FDCAN1;
}

bool MotionCan::begin() {
    setupHardwarePins();

    // Route the FDCAN Kernel Clock to PCLK1 to power the Message RAM
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        return false;
    }

    __HAL_RCC_FDCAN_CLK_ENABLE();

    _hfdcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    _hfdcan.Init.FrameFormat  = FDCAN_FRAME_CLASSIC; 
    _hfdcan.Init.Mode         = FDCAN_MODE_NORMAL;
    _hfdcan.Init.AutoRetransmission = ENABLE;
    _hfdcan.Init.TransmitPause = DISABLE;
    _hfdcan.Init.ProtocolException = DISABLE;

    // 500 kbps Math with 85% Sample Point
    _hfdcan.Init.NominalPrescaler = 17; 
    _hfdcan.Init.NominalSyncJumpWidth = 1;
    _hfdcan.Init.NominalTimeSeg1 = 16; 
    _hfdcan.Init.NominalTimeSeg2 = 3;  

    // Declare 1 filter so hardware dynamically allocates the FDCAN RAM
    _hfdcan.Init.StdFiltersNbr = 1; 
    _hfdcan.Init.ExtFiltersNbr = 0;
    _hfdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION; 

    if (HAL_FDCAN_Init(&_hfdcan) != HAL_OK) {
        return false;
    }

    if (!configureFilters()) {
        return false;
    }

    if (HAL_FDCAN_Start(&_hfdcan) != HAL_OK) {
        return false;
    }

    return true;
}

void MotionCan::setupHardwarePins() {
    PinName rx = digitalPinToPinName(_rxPin);
    PinName tx = digitalPinToPinName(_txPin);
    pin_function(rx, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF9_FDCAN1));
    pin_function(tx, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF9_FDCAN1));
}

bool MotionCan::configureFilters() {
    FDCAN_FilterTypeDef filterConfig;
    filterConfig.IdType = FDCAN_STANDARD_ID;
    filterConfig.FilterIndex = 0;
    filterConfig.FilterType = FDCAN_FILTER_MASK;
    filterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filterConfig.FilterID1 = CAN_ID_TARGET_VEC; 
    filterConfig.FilterID2 = 0x7FF; 

    if (HAL_FDCAN_ConfigFilter(&_hfdcan, &filterConfig) != HAL_OK) return false;
    HAL_FDCAN_ConfigGlobalFilter(&_hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
    
    return true;
}

bool MotionCan::sendFrame(const CAN_Frame_t* frame) {
    if (!frame) return false;

    FDCAN_TxHeaderTypeDef txHeader = {0}; 
    txHeader.Identifier = frame->id;
    txHeader.IdType = FDCAN_STANDARD_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = frame->dlc; // Removed bitshift for modern HAL
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_OFF;
    txHeader.FDFormat = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0;

    if (HAL_FDCAN_GetTxFifoFreeLevel(&_hfdcan) == 0) return false; 

    // Aligned buffer to prevent ARM Cortex-M Memory UsageFault
    uint32_t aligned_data[2] = {0}; 
    memcpy(aligned_data, frame->data, frame->dlc);

    if (HAL_FDCAN_AddMessageToTxFifoQ(&_hfdcan, &txHeader, (uint8_t*)aligned_data) != HAL_OK) {
        return false;
    }

    return true;
}

bool MotionCan::readFrame(CAN_Frame_t* frame) {
    if (!frame) return false;
    if (HAL_FDCAN_GetRxFifoFillLevel(&_hfdcan, FDCAN_RX_FIFO0) == 0) return false;

    FDCAN_RxHeaderTypeDef rxHeader;
    uint32_t aligned_data[2] = {0}; 

    if (HAL_FDCAN_GetRxMessage(&_hfdcan, FDCAN_RX_FIFO0, &rxHeader, (uint8_t*)aligned_data) != HAL_OK) {
        return false;
    }

    frame->id = rxHeader.Identifier;
    frame->dlc = rxHeader.DataLength; // Removed bitshift for modern HAL
    memcpy(frame->data, aligned_data, frame->dlc);

    return true;
}