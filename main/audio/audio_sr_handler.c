#include "audio_sr_handler.h"

static const char *TAG = "audio_sr_handler";

uint8_t Sleep_Mode[] = {0xAA, 0x06, 0x14, 0x02, 0x00, 0xBA};
uint8_t Relax_Mode[] = {0xAA, 0x06, 0x14, 0x04, 0x00, 0xBC};
uint8_t Deep_Sleep_Mode[] = {0xAA, 0x06, 0x14, 0x06, 0x00, 0xBE};
uint8_t Sleep_Aid_Mode[] = {0xAA, 0x06, 0x14, 0x0F, 0x00, 0xB7};
uint8_t Relaxation_Mode[] = {0xAA, 0x06, 0x14, 0x08, 0x00, 0xB0};
uint8_t Decompression_Mode[] = {0xAA, 0x06, 0x14, 0x11, 0x00, 0xA9};
uint8_t Head_Rise[] = {0xAA, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00, 0xAB};
uint8_t Head_Down[] = {0xAA, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0xA8};
uint8_t Foot_Rise[] = {0xAA, 0x08, 0x08, 0x00, 0x00, 0x01, 0x00, 0xAB};
uint8_t Foot_Down[] = {0xAA, 0x08, 0x08, 0x00, 0x00, 0x02, 0x00, 0xA8};
uint8_t Stop[] = {0xAA, 0x04, 0x28, 0x86};
uint8_t Both_Rise[] = {0xAA, 0x08, 0x08, 0x01, 0x00, 0x01, 0x00, 0xAA};
uint8_t Both_Down[] = {0xAA, 0x08, 0x08, 0x02, 0x00, 0x02, 0x00, 0xAA};
uint8_t Head_Massage_On[] = {0xAA, 0x06, 0x16, 0x01, 0x00, 0xBB};
uint8_t Head_Massage_Off[] = {0xAA, 0x06, 0x16, 0x02, 0x00, 0xB8};
uint8_t Foot_Massage_On[] = {0xAA, 0x06, 0x16, 0x00, 0x01, 0xBB};
uint8_t Foot_Massage_Off[] = {0xAA, 0x06, 0x16, 0x00, 0x02, 0xB8};
uint8_t Turn_On_Relaxation[] = {0xAA, 0x06, 0x16, 0x01, 0x01, 0xBA};
uint8_t Turn_Off_Relaxation[] = {0xAA, 0x06, 0x16, 0x02, 0x02, 0xBA};
uint8_t Massage_Mode_One[] = {0xAA, 0x05, 0x18, 0x00, 0xB7};
uint8_t Massage_Mode_Two[] = {0xAA, 0x05, 0x18, 0x01, 0xB6};
uint8_t Massage_Mode_Three[] = {0xAA, 0x05, 0x18, 0x02, 0xB5};
uint8_t Massage_10_Minutes[] = {0xAA, 0x05, 0x1A, 0x01, 0xB4};
uint8_t Massage_20_Minutes[] = {0xAA, 0x05, 0x1A, 0x02, 0xB7};
uint8_t Massage_30_Minutes[] = {0xAA, 0x05, 0x1A, 0x03, 0xB6};
uint8_t Light_On[] = {0xAA, 0x05, 0x1C, 0x01, 0xB2};
uint8_t Light_Off[] = {0xAA, 0x05, 0x1C, 0x00, 0xB3};
uint8_t Waist_Support_Mode[] = {0xAA, 0x06, 0x10, 0x64, 0x64, 0xBC};
uint8_t Left_Lumbar_Support_Mode[] = {0xAA, 0x06, 0x10, 0x64, 0x00, 0xD8};
uint8_t Right_Lumbar_Support_Mode[] = {0xAA, 0x06, 0x10, 0x00, 0x64, 0xD8};
uint8_t Softer_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x01, 0x01, 0xA2};
uint8_t Softer_Left_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x01, 0x00, 0xA3};
uint8_t Softer_Right_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x00, 0x01, 0xA3};
uint8_t Firmer_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x02, 0x02, 0xA2};
uint8_t Firmer_Left_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x02, 0x00, 0xA0};
uint8_t Firmer_Right_Lumbar_Support[] = {0xAA, 0x06, 0x0E, 0x00, 0x02, 0xA0};
uint8_t Turn_Off_Waist_Support[] = {0xAA, 0x06, 0x10, 0x05, 0x05, 0xBC};
uint8_t Left_Lumbar_Support_Off[] = {0xAA, 0x06, 0x10, 0x05, 0x00, 0xB9};
uint8_t Right_Lumbar_Support_Off[] = {0xAA, 0x06, 0x10, 0x00, 0x05, 0xB9};
uint8_t Heat_On[] = {0xAA, 0x05, 0x12, 0x01, 0xBC};
uint8_t Heat_Off[] = {0xAA, 0x05, 0x12, 0x00, 0xBD};
uint8_t Head_Zero[] = {0xAA, 0x08, 0x0C, 0x00, 0xFF, 0xFF, 0xFF, 0x51};
uint8_t Head_One[] = {0xAA, 0x08, 0x0C, 0x02, 0xFF, 0xFF, 0xFF, 0x53};
uint8_t Head_Two[] = {0xAA, 0x08, 0x0C, 0x04, 0xFF, 0xFF, 0xFF, 0x55};
uint8_t Head_Three[] = {0xAA, 0x08, 0x0C, 0x06, 0xFF, 0xFF, 0xFF, 0x57};
uint8_t Head_Four[] = {0xAA, 0x08, 0x0C, 0x08, 0xFF, 0xFF, 0xFF, 0x59};
uint8_t Head_Five[] = {0xAA, 0x08, 0x0C, 0x0A, 0xFF, 0xFF, 0xFF, 0x5B};
uint8_t Head_Six[] = {0xAA, 0x08, 0x0C, 0x0C, 0xFF, 0xFF, 0xFF, 0x5D};
uint8_t Head_Seven[] = {0xAA, 0x08, 0x0C, 0x0E, 0xFF, 0xFF, 0xFF, 0x5F};
uint8_t Head_Eight[] = {0xAA, 0x08, 0x0C, 0x10, 0xFF, 0xFF, 0xFF, 0x41};
uint8_t Head_Nine[] = {0xAA, 0x08, 0x0C, 0x12, 0xFF, 0xFF, 0xFF, 0x43};
uint8_t Head_Ten[] = {0xAA, 0x08, 0x0C, 0x14, 0xFF, 0xFF, 0xFF, 0x45};
uint8_t Foot_Zero[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x00, 0xFF, 0x51};
uint8_t Foot_One[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x02, 0xFF, 0x53};
uint8_t Foot_Two[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x04, 0xFF, 0x55};
uint8_t Foot_Three[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x06, 0xFF, 0x57};
uint8_t Foot_Four[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x08, 0xFF, 0x59};
uint8_t Foot_Five[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x0A, 0xFF, 0x5B};
uint8_t Foot_Six[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x0C, 0xFF, 0x5D};
uint8_t Foot_Seven[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x0E, 0xFF, 0x5F};
uint8_t Foot_Eight[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x10, 0xFF, 0x41};
uint8_t Foot_Nine[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x12, 0xFF, 0x43};
uint8_t Foot_Ten[] = {0xAA, 0x08, 0x0C, 0xFF, 0xFF, 0x14, 0xFF, 0x45};

void sr_handler_task(void *pvParam)
{
    QueueHandle_t xQueue = (QueueHandle_t)pvParam;

    while (true)
    {
        sr_result_t result;
        xQueueReceive(xQueue, &result, portMAX_DELAY);

        ESP_LOGI(TAG, "cmd:%d, wakemode:%d,state:%d", result.command_id, result.wakenet_mode, result.state);

        if (ESP_MN_STATE_TIMEOUT == result.state)
        {
            ESP_LOGI(TAG, "timeout");
            continue;
        }

        if (WAKENET_DETECTED == result.wakenet_mode)
        {
            ESP_LOGI(TAG, "wakeword detected");
            continue;
        }

        if (ESP_MN_STATE_DETECTED == result.state)
        {
            ESP_LOGI(TAG, "mn detected");

            switch (result.command_id)
            {
            case 0:
                usart_send_data(Head_Zero);
                break;
            case 1:
                usart_send_data(Head_One);
                break;
            case 2:
                usart_send_data(Head_Two);
                break;
            case 3:
                usart_send_data(Head_Three);
                break;
            case 4:
                usart_send_data(Head_Four);
                break;
            case 5:
                usart_send_data(Head_Five);
                break;
            case 6:
                usart_send_data(Head_Six);
                break;
            case 7:
                usart_send_data(Head_Seven);
                break;
            case 8:
                usart_send_data(Head_Eight);
                break;
            case 9:
                usart_send_data(Head_Nine);
                break;
            case 10:
                usart_send_data(Head_Ten);
                break;
            case 11:
                usart_send_data(Foot_Zero);
                break;
            case 12:
                usart_send_data(Foot_One);
                break;
            case 13:
                usart_send_data(Foot_Two);
                break;
            case 14:
                usart_send_data(Foot_Three);
                break;
            case 15:
                usart_send_data(Foot_Four);
                break;
            case 16:
                usart_send_data(Foot_Five);
                break;
            case 17:
                usart_send_data(Foot_Six);
                break;
            case 18:
                usart_send_data(Foot_Seven);
                break;
            case 19:
                usart_send_data(Foot_Eight);
                break;
            case 20:
                usart_send_data(Foot_Nine);
                break;
            case 21:
                usart_send_data(Foot_Ten);
                break;
            case 22:
                usart_send_data(Head_Rise);
                break;
            case 23:
                usart_send_data(Head_Down);
                break;
            case 24:
                usart_send_data(Foot_Rise);
                break;
            case 25:
                usart_send_data(Foot_Down);
                break;
            case 26:
                usart_send_data(Both_Rise);
                break;
            case 27:
                usart_send_data(Both_Down);
                break;
            case 28:

                break;
            case 29:

                break;
            case 30:
                usart_send_data(Light_On);
                break;
            case 31:
                usart_send_data(Light_Off);
                break;
            case 32:
                usart_send_data(Light_On);
                break;
            case 33:
                usart_send_data(Light_Off);
                break;
            case 34:
                usart_send_data(Sleep_Mode);
                break;
            case 35:

                break;
            case 36:
                usart_send_data(Relax_Mode);
                break;
            case 37:

                break;
            case 38:
                usart_send_data(Deep_Sleep_Mode);
                break;
            case 39:

                break;
            case 40:
                usart_send_data(Sleep_Aid_Mode);
                break;
            case 41:

                break;
            case 42:
                usart_send_data(Relaxation_Mode);
                break;
            case 43:

                break;
            case 44:

                break;
            case 45:
                usart_send_data(Waist_Support_Mode);
                break;
            case 46:
                usart_send_data(Softer_Lumbar_Support);
                break;
            case 47:
                usart_send_data(Firmer_Lumbar_Support);
                break;
            case 48:
                usart_send_data(Left_Lumbar_Support_Mode);
                break;
            case 49:
                usart_send_data(Right_Lumbar_Support_Mode);
                break;
            case 50:
                usart_send_data(Softer_Left_Lumbar_Support);
                break;
            case 51:
                usart_send_data(Softer_Right_Lumbar_Support);
                break;
            case 52:
                usart_send_data(Firmer_Left_Lumbar_Support);
                break;
            case 53:
                usart_send_data(Firmer_Right_Lumbar_Support);
                break;
            case 54:
                usart_send_data(Turn_Off_Waist_Support);
                break;
            case 55:
                usart_send_data(Left_Lumbar_Support_Off);
                break;
            case 56:
                usart_send_data(Right_Lumbar_Support_Off);
                break;
            case 57:
                usart_send_data(Heat_On);
                break;
            case 58:
                usart_send_data(Heat_Off);
                break;
            case 59:

                break;
            case 60:

                break;
            case 61:
                usart_send_data(Stop);
                break;
            case 62:
                usart_send_data(Head_Massage_On);
                break;
            case 63:
                usart_send_data(Head_Massage_On);
                break;
            case 64:
                usart_send_data(Head_Massage_Off);
                break;
            case 65:
                usart_send_data(Foot_Massage_On);
                break;
            case 66:
                usart_send_data(Foot_Massage_On);
                break;
            case 67:
                usart_send_data(Foot_Massage_Off);
                break;
            case 68:
                usart_send_data(Turn_On_Relaxation);
                break;
            case 69:
                usart_send_data(Turn_Off_Relaxation);
                break;
            case 70:
                usart_send_data(Massage_Mode_One);
                break;
            case 71:
                usart_send_data(Massage_Mode_Two);
                break;
            case 72:
                usart_send_data(Massage_Mode_Three);
                break;
            case 73:
                usart_send_data(Massage_10_Minutes);
                break;
            case 74:
                usart_send_data(Massage_Mode_One);
                break;
            case 75:
                usart_send_data(Massage_Mode_One);
                break;
            default:
                break;
            }
            /* **************** REGISTER COMMAND CALLBACK HERE **************** */
        }
    }

    vTaskDelete(NULL);
}
