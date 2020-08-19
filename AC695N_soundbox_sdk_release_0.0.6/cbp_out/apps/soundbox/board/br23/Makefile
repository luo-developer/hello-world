LIBS = \
--start-group \
   $(AR_DIR)/cpu.a \
   $(AR_DIR)/system.a \
   $(AR_DIR)/btstack.a \
   $(AR_DIR)/ai_stack.a \
   $(AR_DIR)/btctrler.a \
   $(AR_DIR)/media.a \
   $(AR_DIR)/libAptFilt_pi32v2_OnChip.a \
   $(AR_DIR)/libEchoSuppress_pi32v2_OnChip.a \
   $(AR_DIR)/libNoiseSuppress_pi32v2_OnChip.a \
   $(AR_DIR)/libSplittingFilter_pi32v2_OnChip.a \
   $(AR_DIR)/libDelayEstimate_pi32v2_OnChip.a \
   $(AR_DIR)/libSingleMicSystem_pi32v2_OnChip.a \
   $(AR_DIR)/lib_esco_repair.a \
   $(AR_DIR)/mp3_dec_lib.a \
   $(AR_DIR)/mp3tsy_dec_lib.a \
   $(AR_DIR)/wma_dec_lib.a \
   $(AR_DIR)/wtg_dec_lib.a \
   $(AR_DIR)/wav_dec_lib.a \
   $(AR_DIR)/flac_dec_lib.a \
   $(AR_DIR)/ape_dec_lib.a \
   $(AR_DIR)/m4a_dec_lib.a \
   $(AR_DIR)/amr_dec_lib.a \
   $(AR_DIR)/dts_dec_lib.a \
   $(AR_DIR)/mp3_decstream_lib.a \
   $(AR_DIR)/wma_decstream_lib.a \
   $(AR_DIR)/sbc_eng_lib.a \
   $(AR_DIR)/compressor.a \
   $(AR_DIR)/crossover_coff.a \
   $(AR_DIR)/limiter.a \
   $(AR_DIR)/lib_mp2_code.a \
   $(AR_DIR)/lib_adpcm_code.a \
   $(AR_DIR)/mp3_enc_lib.a \
   $(AR_DIR)/opus_enc_lib.a \
   $(AR_DIR)/speex_enc_lib.a \
   $(AR_DIR)/lib_pitchshifter.a \
   $(AR_DIR)/lib_reverb_cal.a \
   $(AR_DIR)/lib_rmono2stereo.a \
   $(AR_DIR)/bfilterfun_lib.a \
   $(AR_DIR)/libFFT_pi32v2_OnChip.a \
   $(AR_DIR)/libOpcore_maskrom_pi32v2_OnChip.a \
   $(AR_DIR)/lib_resample_cal.a \
   $(AR_DIR)/crypto_toolbox_Ospeed.a \
   $(AR_DIR)/lib_g726_decode.a \
   $(AR_DIR)/lib_g726_code.a \
   $(AR_DIR)/limiter_noiseGate.a \
   $(AR_DIR)/howling.a \
   $(AR_DIR)/math.a \
   $(AR_DIR)/lib_midi_dec.a \
   $(AR_DIR)/JL_rcsp.a \
   $(AR_DIR)/bt_hash_enc.a \
   $(AR_DIR)/filtparam_cal_lib.a \
   $(AR_DIR)/agreement.a \
--end-group

#$(AR_DIR)/crypto_toolbox_Osize.a \
   $(AR_DIR)/libepmotion.a \

ifeq ($(OTA_UPDATE),y)
LIBS += \
   $(AR_DIR)/update.a \

endif

ifeq ($(CONFIG_UI_ENABLE),y)
LIBS += \
   $(AR_DIR)/res.a \
   $(AR_DIR)/ui.a \
   $(AR_DIR)/font.a \

endif



objs += \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/irq_config.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_demo.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac6954a_demo.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac6952e_lighter.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac6955f_headset_mono.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_charging_bin.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_multimedia_charging_bin.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_btemitter.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_tws_box.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_tws.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac695x_soundcard.o \
