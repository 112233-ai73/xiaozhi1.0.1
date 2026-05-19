idf_component_register(SRCS "main.c"
                    "com/com_status.c"
                    "audio/audio_init.c"
                    "audio/audio_sr.c"
                    "audio/audio_sr_handler.c"
                    "audio/audio_mp3_decode.c"
                    "com/command_word.c"
                    "usart/usart_init.c"
                    "bsp/bsp_wifi.c"
                    INCLUDE_DIRS "."
                                "com"
                                "audio"
                                "bsp"
                    EMBED_TXTFILES "../managed_components/espressif__esp-sr/model/multinet_model/fst/commands_cn.txt"
                    )
spiffs_create_partition_image(mp3 ./asset FLASH_IN_PROJECT)

