/*
 * tfa_dsp_fw.h
 *
 *  Created on: 2017. 12. 5.
 *      Author: nxp12753
 */

#ifndef TFA_DSP_FW_H_
#define TFA_DSP_FW_H_

/* module Ids */
#define MODULE_FRAMEWORK        0x80
#define MODULE_SPEAKERBOOST     0x81
#define MODULE_BIQUADFILTERBANK 0x82

/* RPC commands */
/* SET */
#define FW_PAR_ID_SET_BAT_FACTORS       0x02
#define FW_PAR_ID_SET_MEMORY            0x03
#define FW_PAR_ID_SET_SENSES_DELAY      0x04
#define FW_PAR_ID_SETSENSESCAL          0x05
#define FW_PAR_ID_SET_INPUT_SELECTOR    0x06
#define FW_PAR_ID_SET_OUTPUT_SELECTOR   0x08
#define FW_PAR_ID_SET_PROGRAM_CONFIG    0x09
#define FW_PAR_ID_SET_GAINS             0x0A
#define FW_PAR_ID_SET_MEMTRACK          0x0B
#define FW_PAR_ID_SET_FWKUSECASE		0x11
#define FW_PAR_ID_SET_HW_CONFIG			0x13
#define FW_PAR_ID_SET_CHIP_TEMPSELECTOR	0x14
#define TFA1_FW_PAR_ID_SET_CURRENT_DELAY 0x03
#define TFA1_FW_PAR_ID_SET_CURFRAC_DELAY 0x06

/* GET */
#define FW_PAR_ID_GET_MEMORY            0x83
#define FW_PAR_ID_GLOBAL_GET_INFO       0x84
#define FW_PAR_ID_GET_FEATURE_INFO      0x85
#define FW_PAR_ID_GET_MEMTRACK          0x8B
#define FW_PAR_ID_GET_TAG               0xFF
#define FW_PAR_ID_GET_API_VERSION 		0xFE
#define FW_PAR_ID_GET_STATUS_CHANGE		0x8D

/* Load a full model into SpeakerBoost. */
/* SET */
#define SB_PARAM_SET_ALGO_PARAMS        0x00
#define SB_PARAM_SET_LAGW               0x01
#define SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET	0x02
#define SB_PARAM_SET_MUTE				0x03
#define SB_PARAM_SET_VOLUME				0x04
#define SB_PARAM_SET_RE25C				0x05
#define SB_PARAM_SET_LSMODEL            0x06
#define SB_PARAM_SET_MBDRC              0x07
#define SB_PARAM_SET_MBDRC_WITHOUT_RESET	0x08
#define SB_PARAM_SET_DRC                0x0F

/* GET */
#define SB_PARAM_GET_ALGO_PARAMS        0x80
#define SB_PARAM_GET_LAGW               0x81
#define SB_PARAM_GET_RE25C              0x85
#define SB_PARAM_GET_LSMODEL            0x86
#define SB_PARAM_GET_MBDRC	        	0x87
#define SB_PARAM_GET_MBDRC_DYNAMICS		0x89
#define SB_PARAM_GET_TAG                0xFF
#define SB_PARAM_SET_EXCURSION_FILTERS 	0x0A	/* SetExcursionFilters */
#define SB_PARAM_SET_DATA_LOGGER       	0x0D
#define SB_PARAM_SET_CONFIG	        	0x0E	/* Load a config */
#define SB_PARAM_SET_AGCINS             0x10
#define SB_PARAM_SET_CURRENT_DELAY      0x03
#define SB_PARAM_GET_STATE              0xC0
#define SB_PARAM_GET_XMODEL             0xC1	/* Gets current Excursion Model. */

/* sets the speaker calibration impedance (@25 degrees celsius) */
#define SB_PARAM_SET_RE0                0x89

/*	SET: TAPTRIGGER */
#define TAP_PARAM_SET_ALGO_PARAMS		0x01
#define TAP_PARAM_SET_DECIMATION_PARAMS 0x02

/* GET: TAPTRIGGER*/
#define TAP_PARAM_GET_ALGO_PARAMS		0x81
#define TAP_PARAM_GET_TAP_RESULTS		0x84

#define BFB_PAR_ID_SET_COEFS            0x00
#define BFB_PAR_ID_GET_COEFS            0x80
#define BFB_PAR_ID_GET_CONFIG           0x81

#endif /* TFA_DSP_FW_H_ */
