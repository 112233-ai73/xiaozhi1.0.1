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
uint8_t Volume_up[] = {0xAA, 0x05, 0x3D, 0x01, 0x93};
uint8_t Volume_down[] = {0xAA, 0x05, 0x3D, 0x02, 0x90};
uint8_t Memory_Mode_Flat[] = {0xAA, 0x06, 0x14, 0x01, 0x00, 0xB9};
uint8_t Memory_Leisure_Mode[] = {0xAA, 0x06, 0x14, 0x03, 0x00, 0xBB};
uint8_t Memory_DeepSleep_Mode[] = {0xAA, 0x06, 0x14, 0x05, 0x00, 0xBD};
uint8_t Memory_SleepAid_Mode[] = {0xAA, 0x06, 0x14, 0x0E, 0x00, 0xB6};
uint8_t Memory_Relax_Mode[] = {0xAA, 0x06, 0x14, 0x07, 0x00, 0xBF};
uint8_t Wakeup_Mode[] = {0xAA, 0x06, 0x14, 0x0C, 0x00, 0xB4};
uint8_t Temperature_Up[] = {0xAA, 0x05, 0x12, 0x10, 0x00, 0xAD};
uint8_t Temperature_Down[] = {0xAA, 0x05, 0x12, 0x20, 0x00, 0x9D};

void sr_handler_task(void *pvParam)
{
    QueueHandle_t xQueue = (QueueHandle_t)pvParam;

    while (true)
    {
        sr_result_t result;
        xQueueReceive(xQueue, &result, portMAX_DELAY);

        if (ESP_MN_STATE_TIMEOUT == result.state)
        {
            ESP_LOGI(TAG, "timeout");
            continue;
        }

        if (ESP_MN_STATE_DETECTED == result.state)
        {
            ESP_LOGI(TAG, "mn detected");
            com_status_change(SPEAKING);
            if (is_awake == false)
            {
                switch (result.command_id)
                {
                case 0:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 1:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 2:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                default:
                    break;
                }
            }
            else
            {
                com_set_awake(true);
                switch (result.command_id)
                {
                case 0:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 1:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 2:
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 3:
                    com_set_awake(false);
                    audio_mp3_play_file_async("106.mp3");
                    com_status_change(IDLE);
                    break;
                case 4:
                    usart_send_data(Head_Zero);
                    audio_mp3_play_file_async("109.mp3");
                    break;
                case 5:
                    usart_send_data(Head_One);
                    audio_mp3_play_file_async("110.mp3");
                    break;
                case 6:
                    usart_send_data(Head_Two);
                    audio_mp3_play_file_async("111.mp3");
                    break;
                case 7:
                    usart_send_data(Head_Three);
                    audio_mp3_play_file_async("112.mp3");
                    break;
                case 8:
                    usart_send_data(Head_Four);
                    audio_mp3_play_file_async("113.mp3");
                    break;
                case 9:
                    usart_send_data(Head_Five);
                    audio_mp3_play_file_async("114.mp3");
                    break;
                case 10:
                    usart_send_data(Head_Six);
                    audio_mp3_play_file_async("115.mp3");
                    break;
                case 11:
                    usart_send_data(Head_Seven);
                    audio_mp3_play_file_async("116.mp3");
                    break;
                case 12:
                    usart_send_data(Head_Eight);
                    audio_mp3_play_file_async("117.mp3");
                    break;
                case 13:
                    usart_send_data(Head_Nine);
                    audio_mp3_play_file_async("118.mp3");
                    break;
                case 14:
                    usart_send_data(Head_Ten);
                    audio_mp3_play_file_async("119.mp3");
                    break;
                case 15:
                    usart_send_data(Foot_Zero);
                    audio_mp3_play_file_async("163.mp3");
                    break;
                case 16:
                    usart_send_data(Foot_One);
                    audio_mp3_play_file_async("164.mp3");
                    break;
                case 17:
                    usart_send_data(Foot_Two);
                    audio_mp3_play_file_async("165.mp3");
                    break;
                case 18:
                    usart_send_data(Foot_Three);
                    audio_mp3_play_file_async("166.mp3");
                    break;
                case 19:
                    usart_send_data(Foot_Four);
                    audio_mp3_play_file_async("167.mp3");
                    break;
                case 20:
                    usart_send_data(Foot_Five);
                    audio_mp3_play_file_async("168.mp3");
                    break;
                case 21:
                    usart_send_data(Foot_Six);
                    audio_mp3_play_file_async("169.mp3");
                    break;
                case 22:
                    usart_send_data(Foot_Seven);
                    audio_mp3_play_file_async("170.mp3");
                    break;
                case 23:
                    usart_send_data(Foot_Eight);
                    audio_mp3_play_file_async("171.mp3");
                    break;
                case 24:
                    usart_send_data(Foot_Nine);
                    audio_mp3_play_file_async("172.mp3");
                    break;
                case 25:
                    usart_send_data(Foot_Ten);
                    audio_mp3_play_file_async("173.mp3");
                    break;
                case 26:
                    usart_send_data(Head_Rise);
                    audio_mp3_play_file_async("108.mp3");
                    break;
                case 27:
                    usart_send_data(Head_Down);
                    audio_mp3_play_file_async("120.mp3");
                    break;
                case 28:
                    usart_send_data(Foot_Rise);
                    audio_mp3_play_file_async("121.mp3");
                    break;
                case 29:
                    usart_send_data(Foot_Down);
                    audio_mp3_play_file_async("122.mp3");
                    break;
                case 30:
                    usart_send_data(Both_Rise);
                    audio_mp3_play_file_async("123.mp3");
                    break;
                case 31:
                    usart_send_data(Both_Down);
                    audio_mp3_play_file_async("124.mp3");
                    break;
                case 32:
                    usart_send_data(Volume_up);
                    audio_mp3_play_file_async("125.mp3");
                    break;
                case 33:
                    usart_send_data(Volume_down);
                    audio_mp3_play_file_async("126.mp3");
                    break;
                case 34:
                    usart_send_data(Light_On);
                    audio_mp3_play_file_async("127.mp3");
                    break;
                case 35:
                    usart_send_data(Light_Off);
                    audio_mp3_play_file_async("128.mp3");
                    break;
                case 36:
                    usart_send_data(Light_On);
                    audio_mp3_play_file_async("127.mp3");
                    break;
                case 37:
                    usart_send_data(Light_Off);
                    audio_mp3_play_file_async("128.mp3");
                    break;
                case 38:
                    usart_send_data(Sleep_Mode);
                    audio_mp3_play_file_async("129.mp3");
                    break;
                case 39:
                    usart_send_data(Memory_Mode_Flat);

                    break;
                case 40:
                    usart_send_data(Relax_Mode);
                    audio_mp3_play_file_async("130.mp3");
                    break;
                case 41:
                    usart_send_data(Memory_Leisure_Mode);
                    audio_mp3_play_file_async("131.mp3");
                    break;
                case 42:
                    usart_send_data(Deep_Sleep_Mode);
                    audio_mp3_play_file_async("132.mp3");
                    break;
                case 43:
                    usart_send_data(Memory_DeepSleep_Mode);
                    audio_mp3_play_file_async("133.mp3");
                    break;
                case 44:
                    usart_send_data(Sleep_Aid_Mode);
                    audio_mp3_play_file_async("134.mp3");
                    break;
                case 45:
                    usart_send_data(Memory_SleepAid_Mode);
                    audio_mp3_play_file_async("135.mp3");
                    break;
                case 46:
                    usart_send_data(Relaxation_Mode);
                    audio_mp3_play_file_async("136.mp3");
                    break;
                case 47:
                    usart_send_data(Memory_Relax_Mode);
                    audio_mp3_play_file_async("137.mp3");
                    break;
                case 48:
                    usart_send_data(Wakeup_Mode);
                    audio_mp3_play_file_async("138.mp3");
                    break;
                case 49:
                    usart_send_data(Waist_Support_Mode);
                    audio_mp3_play_file_async("139.mp3");
                    break;
                case 50:
                    usart_send_data(Softer_Lumbar_Support);
                    break;
                case 51:
                    usart_send_data(Firmer_Lumbar_Support);
                    break;
                case 52:
                    usart_send_data(Left_Lumbar_Support_Mode);
                    break;
                case 53:
                    usart_send_data(Right_Lumbar_Support_Mode);
                    break;
                case 54:
                    usart_send_data(Softer_Left_Lumbar_Support);
                    break;
                case 55:
                    usart_send_data(Softer_Right_Lumbar_Support);
                    break;
                case 56:
                    usart_send_data(Firmer_Left_Lumbar_Support);
                    break;
                case 57:
                    usart_send_data(Firmer_Right_Lumbar_Support);
                    break;
                case 58:
                    usart_send_data(Turn_Off_Waist_Support);
                    break;
                case 59:
                    usart_send_data(Left_Lumbar_Support_Off);
                    audio_mp3_play_file_async("144.mp3");
                    break;
                case 60:
                    usart_send_data(Right_Lumbar_Support_Off);
                    audio_mp3_play_file_async("145.mp3");
                    break;
                case 61:
                    usart_send_data(Heat_On);
                    audio_mp3_play_file_async("146.mp3");
                    break;
                case 62:
                    usart_send_data(Heat_Off);
                    audio_mp3_play_file_async("147.mp3");
                    break;
                case 63:
                    usart_send_data(Temperature_Up);
                    audio_mp3_play_file_async("148.mp3");
                    break;
                case 64:
                    usart_send_data(Temperature_Down);
                    audio_mp3_play_file_async("149.mp3");
                    break;
                case 65:
                    usart_send_data(Stop);
                    audio_mp3_play_file_async("150.mp3");
                    break;
                case 66:
                    usart_send_data(Head_Massage_On);
                    audio_mp3_play_file_async("151.mp3");
                    break;
                case 67:
                    usart_send_data(Head_Massage_On);
                    audio_mp3_play_file_async("151.mp3");
                    break;
                case 68:
                    usart_send_data(Head_Massage_Off);
                    audio_mp3_play_file_async("152.mp3");
                    break;
                case 69:
                    usart_send_data(Foot_Massage_On);
                    audio_mp3_play_file_async("153.mp3");
                    break;
                case 70:
                    usart_send_data(Foot_Massage_On);
                    audio_mp3_play_file_async("153.mp3");
                    break;
                case 71:
                    usart_send_data(Foot_Massage_Off);
                    audio_mp3_play_file_async("154.mp3");
                    break;
                case 72:
                    usart_send_data(Turn_On_Relaxation);
                    audio_mp3_play_file_async("155.mp3");
                    break;
                case 73:
                    usart_send_data(Turn_Off_Relaxation);
                    audio_mp3_play_file_async("156.mp3");
                    break;
                case 74:
                    usart_send_data(Massage_Mode_One);
                    audio_mp3_play_file_async("157.mp3");
                    break;
                case 75:
                    usart_send_data(Massage_Mode_Two);
                    audio_mp3_play_file_async("158.mp3");
                    break;
                case 76:
                    usart_send_data(Massage_Mode_Three);
                    audio_mp3_play_file_async("159.mp3");
                    break;
                case 77:
                    usart_send_data(Massage_10_Minutes);
                    audio_mp3_play_file_async("160.mp3");
                    break;
                case 78:
                    usart_send_data(Massage_Mode_One);
                    audio_mp3_play_file_async("161.mp3");
                    break;
                case 79:
                    usart_send_data(Massage_Mode_One);
                    audio_mp3_play_file_async("162.mp3");
                    break;
                default:
                    break;
                }
            }
            /* **************** REGISTER COMMAND CALLBACK HERE **************** */
        }
    }

    vTaskDelete(NULL);
}
