#include "app_sr_handler.h"
#include "com/com_debug.h"

//static const char *TAG = "app_sr_handler";

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
uint8_t Memory_Leisure_Mode[] = {0xAA, 0x06, 0x14, 0x03, 0x00, 0xBB};
uint8_t Memory_DeepSleep_Mode[] = {0xAA, 0x06, 0x14, 0x05, 0x00, 0xBD};
uint8_t Memory_SleepAid_Mode[] = {0xAA, 0x06, 0x14, 0x0E, 0x00, 0xB6};
uint8_t Memory_Relax_Mode[] = {0xAA, 0x06, 0x14, 0x07, 0x00, 0xBF};
uint8_t Wakeup_Mode[] = {0xAA, 0x06, 0x14, 0x0C, 0x00, 0xB4};
uint8_t Temperature_Up[] = {0xAA, 0x05, 0x12, 0x10, 0x00, 0xAD};
uint8_t Temperature_Down[] = {0xAA, 0x05, 0x12, 0x20, 0x00, 0x9D};

static void play_mp3(const uint8_t *data, const char *file_name)
{
    com_set_awake(false);
    MP3_after_awake = true;
    usart_send_data(data);
    audio_mp3_play_file_async(file_name);
}

void sr_handler_task(void *pvParam)
{
    QueueHandle_t xQueue = (QueueHandle_t)pvParam;

    while (true)
    {
        sr_result_t result;
        xQueueReceive(xQueue, &result, portMAX_DELAY);

        if (ESP_MN_STATE_TIMEOUT == result.state)
        { 
            MY_LOGI("timeout");
            continue;
        }

        if (ESP_MN_STATE_DETECTED == result.state)
        {
            MY_LOGI("mn detected");
            
            if (is_awake == false)
            {
                
                switch (result.command_id)
                {
                case 0:
                    stop_play_mp3();
                    com_status_change(SPEAKING);
                    com_set_awake(true);
                    audio_mp3_play_file_async("105.mp3");
                    break;
                case 1:
                    stop_play_mp3();
                    com_status_change(SPEAKING);
                    com_set_awake(true);
                    audio_mp3_play_file_async("105.mp3");
                    break;
                case 2:
                    stop_play_mp3();
                    com_status_change(SPEAKING);
                    com_set_awake(true);
                    audio_mp3_play_file_async("105.mp3");
                    break;
                default:
                    break;
                }
            }
            else
            {
                com_set_awake(true);
                com_status_change(SPEAKING);
                switch (result.command_id)
                {
                case 0:
                    stop_play_mp3();
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 1:
                    stop_play_mp3();
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 2:
                    stop_play_mp3();
                    com_set_awake(true);
                    audio_mp3_play_file_async("103.mp3");
                    break;
                case 3:
                    com_set_awake(false);
                    audio_mp3_play_file_async("106.mp3");
                    com_status_change(IDLE);
                    break;
                case 4:
                    play_mp3(Head_Zero, "109.mp3");
                    break;
                case 5:
                    play_mp3(Head_One, "110.mp3");
                    break;
                case 6:
                    play_mp3(Head_Two, "111.mp3");
                    break;
                case 7:
                    play_mp3(Head_Three, "112.mp3");
                    break;
                case 8:
                    play_mp3(Head_Four, "113.mp3");
                    break;
                case 9:
                    play_mp3(Head_Five, "114.mp3");
                    break;
                case 10:
                    play_mp3(Head_Six, "115.mp3");
                    break;
                case 11:
                    play_mp3(Head_Seven, "116.mp3");
                    break;
                case 12:
                    play_mp3(Head_Eight, "117.mp3");
                    break;
                case 13:
                    play_mp3(Head_Nine, "118.mp3");
                    break;
                case 14:
                    play_mp3(Head_Ten, "119.mp3");
                    break;
                case 15:
                    play_mp3(Foot_Zero, "163.mp3");
                    break;
                case 16:
                    play_mp3(Foot_One, "164.mp3");
                    break;
                case 17:
                    play_mp3(Foot_Two, "165.mp3");
                    break;
                case 18:
                    play_mp3(Foot_Three, "166.mp3");
                    break;
                case 19:
                    play_mp3(Foot_Four, "167.mp3");
                    break;
                case 20:
                    play_mp3(Foot_Five, "168.mp3");
                    break;
                case 21:
                    play_mp3(Foot_Six, "169.mp3");
                    break;
                case 22:
                    play_mp3(Foot_Seven, "170.mp3");
                    break;
                case 23:
                    play_mp3(Foot_Eight, "171.mp3");
                    break;
                case 24:
                    play_mp3(Foot_Nine, "172.mp3");
                    break;
                case 25:
                    play_mp3(Foot_Ten, "173.mp3");
                    break;
                case 26:
                    play_mp3(Head_Rise, "108.mp3");
                    break;
                case 27:
                    play_mp3(Head_Down, "120.mp3");
                    break;
                case 28:
                    play_mp3(Foot_Rise, "121.mp3");
                    break;
                case 29:
                    play_mp3(Foot_Down, "122.mp3");
                    break;
                case 30:
                    play_mp3(Both_Rise, "123.mp3");
                    break;
                case 31:
                    play_mp3(Both_Down, "124.mp3");
                    break;
                case 32:
                    usart_send_data(Volume_up);
                    if(volume_num<100)
                    {
                        volume_num+=10;
                    }
                    else
                    {
                        volume_num=100;
                    }
                    inf_es8311_set_volume(volume_num);
                    audio_mp3_play_file_async("125.mp3");
                    break;
                case 33:
                    usart_send_data(Volume_down);
                    if(volume_num>0)
                    {
                        volume_num-=10;
                    }
                    else
                    {
                        volume_num=0;
                    }
                    inf_es8311_set_volume(volume_num); 
                    audio_mp3_play_file_async("126.mp3");
                    break;
                case 34:
                    play_mp3(Light_On, "127.mp3");
                    break;
                case 35:
                    play_mp3(Light_Off, "128.mp3");
                    break;
                case 36:
                    play_mp3(Light_On, "127.mp3");
                    break;
                case 37:
                    play_mp3(Light_Off, "128.mp3");
                    break;
                case 38:
                    play_mp3(Sleep_Mode, "129.mp3");
                    break;
                case 39:
                    play_mp3(Sleep_Mode, "129.mp3");
                    break;
                case 40:
                    play_mp3(Relax_Mode, "130.mp3");
                    break;
                case 41:
                    play_mp3(Memory_Leisure_Mode, "131.mp3");
                    break;
                case 42:
                    play_mp3(Deep_Sleep_Mode, "132.mp3");
                    break;
                case 43:
                    play_mp3(Memory_DeepSleep_Mode, "133.mp3");
                    break;
                case 44:
                    play_mp3(Sleep_Aid_Mode, "134.mp3");
                    break;
                case 45:
                    play_mp3(Memory_SleepAid_Mode, "135.mp3");
                    break;
                case 46:
                    play_mp3(Relaxation_Mode, "136.mp3");
                    break;
                case 47:
                    play_mp3(Memory_Relax_Mode, "137.mp3");
                    break;
                case 48:
                    play_mp3(Wakeup_Mode, "138.mp3");
                    break;
                case 49:
                    play_mp3(Waist_Support_Mode, "139.mp3");
                    break;
                case 50:
                    play_mp3(Softer_Lumbar_Support, "140.mp3");
                    break;
                case 51:
                    play_mp3(Firmer_Lumbar_Support, "140.mp3");
                    break;
                case 52:
                    play_mp3(Left_Lumbar_Support_Mode, "141.mp3");
                    break;
                case 53:
                    play_mp3(Right_Lumbar_Support_Mode, "142.mp3");
                    break;
                case 54:
                    play_mp3(Softer_Left_Lumbar_Support, "141.mp3");
                    break;
                case 55:
                    play_mp3(Softer_Right_Lumbar_Support, "142.mp3");
                    break;
                case 56:
                    play_mp3(Firmer_Left_Lumbar_Support, "141.mp3");
                    break;
                case 57:
                    play_mp3(Firmer_Right_Lumbar_Support, "142.mp3");
                    break;
                case 58:
                    play_mp3(Turn_Off_Waist_Support, "143.mp3");
                    break;
                case 59:
                    play_mp3(Left_Lumbar_Support_Off, "144.mp3");
                    break;
                case 60:
                    play_mp3(Right_Lumbar_Support_Off, "145.mp3");
                    break;
                case 61:
                    play_mp3(Heat_On, "146.mp3");
                    break;
                case 62:
                    play_mp3(Heat_Off, "147.mp3");
                    break;
                case 63:
                    play_mp3(Temperature_Up, "148.mp3");
                    break;
                case 64:
                    play_mp3(Temperature_Down, "149.mp3");
                    break;
                case 65:
                    play_mp3(Stop, "150.mp3");
                    break;
                case 66:
                    play_mp3(Head_Massage_On, "151.mp3");
                    break;
                case 67:
                    play_mp3(Head_Massage_On, "151.mp3");
                    break;
                case 68:
                    play_mp3(Head_Massage_Off, "152.mp3");
                    break;
                case 69:
                    play_mp3(Foot_Massage_On, "153.mp3");
                    break;
                case 70:
                    play_mp3(Foot_Massage_On, "153.mp3");
                    break;
                case 71:
                    play_mp3(Foot_Massage_Off, "154.mp3");
                    break;
                case 72:
                    play_mp3(Turn_On_Relaxation, "155.mp3");
                    break;
                case 73:
                    play_mp3(Turn_Off_Relaxation, "156.mp3");
                    break;
                case 74:
                    play_mp3(Massage_Mode_One, "157.mp3");
                    break;
                case 75:
                    play_mp3(Massage_Mode_Two, "158.mp3");
                    break;
                case 76:
                    play_mp3(Massage_Mode_Three, "159.mp3");
                    break;
                case 77:
                    play_mp3(Massage_10_Minutes, "160.mp3");
                    break;
                case 78:
                    play_mp3(Massage_Mode_One, "161.mp3");
                    break;
                case 79:
                    play_mp3(Massage_Mode_One, "162.mp3");
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
