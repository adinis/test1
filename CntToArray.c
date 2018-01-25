/*
 ============================================================================
 Name        : CntToArray.c
 Author      : jimmy
 Version     :
 Copyright   : NXPSW
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfa_dsp_fw.h"

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;

#define MEMTRACK_MAX_WORDS           150
#define LSMODEL_MAX_WORDS            150
#define TFA98XX_MAXTAG              (138)
#define FW_VAR_API_VERSION          (521)
#define TFA_MAX_PROFILES                (64)
#define TFA_MAX_VSTEPS                  (64)
#define TFA_MAX_VSTEP_MSG_MARKER        (100) /* This marker  is used to indicate if all msgs need to be written to the device */
#define TFA_MAX_MSGS                    (10)
/* static limits */
#define TFACONT_MAXDEVS  (4)   /* maximum nr of devices */
#define TFACONT_MAXPROFS (16) /* maximum nr of profiles */
#define TFA_MAX_CNT_LENGTH (256*1024)

enum tfa98xx_error {
	TFA_ERROR = -1,
	TFA98XX_ERROR_OK = 0,
	TFA98XX_ERROR_DEVICE,		/* 1. Currently only used to keep in sync with tfa_error */
	TFA98XX_ERROR_BAD_PARAMETER,	/* 2. */
	TFA98XX_ERROR_FAIL,             /* 3. generic failure, avoid mislead message */
	TFA98XX_ERROR_NO_CLOCK,          /* 4. no clock detected */
	TFA98XX_ERROR_STATE_TIMED_OUT,	/* 5. */
	TFA98XX_ERROR_DSP_NOT_RUNNING,	/* 6. communication with the DSP failed */
	TFA98XX_ERROR_AMPON,            /* 7. amp is still running */
	TFA98XX_ERROR_NOT_OPEN,	        /* 8. the given handle is not open */
	TFA98XX_ERROR_IN_USE,	        /* 9. too many handles */
	TFA98XX_ERROR_BUFFER_TOO_SMALL, /* 10. if a buffer is too small */
	/* the expected response did not occur within the expected time */
	TFA98XX_ERROR_BUFFER_RPC_BASE = 100,
	TFA98XX_ERROR_RPC_BUSY = 101,
	TFA98XX_ERROR_RPC_MOD_ID = 102,
	TFA98XX_ERROR_RPC_PARAM_ID = 103,
	TFA98XX_ERROR_RPC_INVALID_CC = 104,
	TFA98XX_ERROR_RPC_INVALID_SEQ = 105,
	TFA98XX_ERROR_RPC_INVALID_PARAM = 106,
	TFA98XX_ERROR_RPC_BUFFER_OVERFLOW = 107,
	TFA98XX_ERROR_RPC_CALIB_BUSY = 108,
	TFA98XX_ERROR_RPC_CALIB_FAILED = 109,
	TFA98XX_ERROR_NOT_IMPLEMENTED,
	TFA98XX_ERROR_NOT_SUPPORTED,
	TFA98XX_ERROR_I2C_FATAL,	/* Fatal I2C error occurred */
	/* Nonfatal I2C error, and retry count reached */
	TFA98XX_ERROR_I2C_NON_FATAL,
	TFA98XX_ERROR_OTHER = 1000
};

/*
 * parameter container file
 */
/*
 * descriptors
 */
enum tfa_descriptor_type {
	dsc_device,		// device list
	dsc_profile,		// profile list
	dsc_register,	        // register patch
	dsc_string,		// ascii, zero terminated string
	dsc_file,		// filename + file contents
	dsc_patch,               // patch file
	dsc_marker,		// marker to indicate end of a list
	dsc_mode,
	dsc_set_input_select,
	dsc_set_output_select,
	dsc_set_program_config,
	dsc_set_lag_w,
	dsc_set_gains,
	dsc_set_vbat_factors,
	dsc_set_senses_cal,
	dsc_set_senses_delay,
	dsc_bit_field,
	dsc_default,             // used to reset bitfields to there default values
	dsc_livedata,
	dsc_livedata_string,
	dsc_group,
	dsc_cmd,
	dsc_set_mb_drc,
	dsc_filter,
	dsc_no_init,
	dsc_features,
	dsc_cf_mem, // coolflux memory x,y,io
	dsc_last	// trailer
};

enum tfa_error {
	tfa_error_ok, /**< no error */
	tfa_error_device, /**< no response from device */
	tfa_error_bad_param, /**< parameter no accepted */
	tfa_error_noclock, /**< required clock not present */
	tfa_error_timeout, /**< a timeout occurred */
	tfa_error_dsp, /**< a DSP error was returned */
	tfa_error_container, /**< no or wrong container file */
	tfa_error_max /**< impossible value, max enum */
};

#pragma pack(1)

struct tfa_desc_ptr {
	uint32_t offset:24;
	uint32_t  type:8; // (== enum tfa_descriptor_type, assure 8bits length)
};

/*
 * generic file descriptor
 */
struct tfa_file_dsc {
	struct tfa_desc_ptr name;
	uint32_t size;	// file data length in bytes
	uint8_t data[]; //payload
};


/*
 * device descriptor list
 */
struct tfa_device_list {
	uint8_t length;			// nr of items in the list
	uint8_t bus;			// bus
	uint8_t dev;			// device
	uint8_t func;			// subfunction or subdevice
	uint32_t devid;		        // device  hw fw id
	struct tfa_desc_ptr name;	        // device name
	struct tfa_desc_ptr list[];	        // items list
};

/*
 * profile descriptor list
 */
struct tfa_profile_list {
	uint32_t length:8;		// nr of items in the list + name
	uint32_t group:8;		// profile group number
	uint32_t id:16;			// profile ID
	struct tfa_desc_ptr name;	        // profile name
	struct tfa_desc_ptr list[];	        // items list (lenght-1 items)
};
#define TFA_PROFID 0x1234

/*
 * livedata descriptor list
 */
struct tfa_livedata_list {
	uint32_t length:8;		// nr of items in the list
	uint32_t id:24;			// profile ID
	struct tfa_desc_ptr name;	        // livedata name
	struct tfa_desc_ptr list[];	        // items list
};
#define TFA_LIVEDATAID 0x5678

/*
 * Bitfield descriptor
 */
struct tfa_bitfield {
	uint16_t  value;
	uint16_t  field; // ==datasheet defined, 16 bits
};

/*
 * Bitfield enumuration bits descriptor
 */
struct tfa_bf_enum {
	unsigned int  len:4;		// this is the actual length-1
	unsigned int  pos:4;
	unsigned int  address:8;
};

/*
 * Register patch descriptor
 */
struct tfa_reg_patch {
	uint8_t   address;	// register address
	uint16_t  value;	// value to write
	uint16_t  mask;		// mask of bits to write
};

/*
 * Mode descriptor
 */
struct tfa_mode {
	int value;	// mode value, maps to enum tfa98xx_mode
};

/*
 * NoInit descriptor
 */
struct tfa_no_init {
	uint8_t value;	// noInit value
};

/*
 * Features descriptor
 */
struct tfa_features {
	uint16_t value[3];	// features value
};

struct tfa_cmd {
	uint16_t id;
	unsigned char value[3];
};

/*
 * the container file
 *   - the size field is 32bits long (generic=16)
 *   - all char types are in ASCII
 */
#define NXPTFA_PM_VERSION  '1'
#define NXPTFA_PM3_VERSION '3'
#define NXPTFA_PM_SUBVERSION '1'
struct tfa_container {
    char id[2];          // "XX" : XX=type
    char version[2];     // "V_" : V=version, vv=subversion
    char subversion[2];  // "vv" : vv=subversion
    uint32_t size;       // data size in bytes following CRC
    uint32_t crc;        // 32-bits CRC for following data
    uint16_t rev;		 // "extra chars for rev nr"
    char customer[8];    // name of customer??
    char application[8]; // application name??
    char type[8];		 // application type name??
    uint16_t ndev;	 	 // "nr of device lists"
    uint16_t nprof;	 	 // "nr of profile lists"
    uint16_t nlivedata;          // "nr of livedata lists"
    struct tfa_desc_ptr index[]; // start of item index table
};

struct tfa_header {
	uint16_t id;
    char version[2];     // "V_" : V=version, vv=subversion
    char subversion[2];  // "vv" : vv=subversion
    uint16_t size;       // data size in bytes following CRC
    uint32_t crc;        // 32-bits CRC for following data
    char customer[8];    // ?œname of customer??
    char application[8]; // ?œapplication name??
    char type[8];		 // ?œapplication type name??
};


/*
 * volumestepMax2 file
 */
struct tfa_volume_step_max2_file {
	struct tfa_header hdr;
	uint8_t version[3];
	uint8_t nr_of_vsteps;
	uint8_t vsteps_bin[];
};

typedef struct uint24 {
  uint8_t b[3];
} uint24_t;

struct tfa_volume_step_message_info {
	uint8_t nr_of_messages;
	uint8_t message_type;
	uint24_t message_length;
	uint8_t cmd_id[3];
	uint8_t parameter_data[];
};

struct tfa_volume_step_register_info {
	uint8_t nr_of_registers;
	uint16_t register_info[];
};

struct tfa_partial_msg_block {
	uint8_t offset;
	uint16_t change;
	uint8_t update[16][3];
} __packed;

enum pool_control {
	POOL_NOT_SUPPORT,
	POOL_ALLOC,
	POOL_FREE,
	POOL_GET,
	POOL_RETURN,
	POOL_MAX_CONTROL
};

#define POOL_MAX_INDEX 6

struct tfa98xx_buffer_pool {
	int size;
	unsigned char in_use;
	void* pool;
};

#define HDR(c1,c2) (c2<<8|c1) // little endian
enum tfa_header_type {
    params_hdr		= HDR('P','M'), /* containter file */
    volstep_hdr	 	= HDR('V','P'),
    patch_hdr	 	= HDR('P','A'),
    speaker_hdr	 	= HDR('S','P'),
    preset_hdr	 	= HDR('P','R'),
    config_hdr	 	= HDR('C','O'),
    equalizer_hdr	= HDR('E','Q'),
    drc_hdr		= HDR('D','R'),
    msg_hdr		= HDR('M','G'),	/* generic message */
    info_hdr		= HDR('I','N')
};

struct tfa_msg_file {
	struct tfa_header hdr;
	uint8_t data[];
};

struct tfa_msg {
	uint8_t msg_size;
    unsigned char cmd_id[3];
	int data[9];
};

#define LVM_MAXENUM (0xffff)
enum tfadsp_event_en
{
 TFADSP_CMD_ACK              =  1,   /**< Command handling is completed */
 TFADSP_SOFT_MUTE_READY      =  8,   /**< Muting completed */
 TFADSP_VOLUME_READY         = 16,   /**< Volume change completed */
 TFADSP_DAMAGED_SPEAKER      = 32,   /**< Damaged speaker was detected */
 TFADSP_CALIBRATE_DONE       = 64,   /**< Calibration is completed */
 TFADSP_SPARSESIG_DETECTED   = 128,  /**< Sparse signal detected */
 TFADSP_CMD_READY            = 256,  /**< Ready to receive commands */
 TFADSP_EXT_PWRUP 			= 0x8000,/**< DSP API has started, powered up */
 TFADSP_EXT_PWRDOWN 		= 0x8001,/**< DSP API stopped, power down */
 TFADSP_EVENT_DUMMY = LVM_MAXENUM
};

#define TFADSP_DSP_BUFFER_POOL

enum feature_support {
	SUPPORT_NOT_SET, /* the default is not set yet, so = 0 */
	SUPPORT_NO,
	SUPPORT_YES
};

enum instream_state {
	BIT_PSTREAM = 1, /* b0 */
	BIT_CSTREAM = 2, /* b1 */
	BIT_SAMSTREAM = 4 /* b2 */
};

enum tfa98xx_dai_bitmap {
	TFA98XX_DAI_I2S  =  0x01,
	TFA98XX_DAI_TDM  =  0x02,
	TFA98XX_DAI_PDM  =  0x04,
};

struct tfa98xx_handle_private {
	int in_use;
	int buffer_size;
	unsigned char slave_address;
	unsigned short rev;
	unsigned char tfa_family; /* tfa1/tfa2 */
	enum feature_support support_drc;
	enum feature_support support_framework;
	enum feature_support support_saam;
	int sw_feature_bits[2]; /* cached feature bits data */
	int hw_feature_bits; /* cached feature bits data */
	int profile;	/* cached active profile */
	int vstep[2]; /* cached active vsteps */
	unsigned char spkr_count;
	unsigned char spkr_select;
	unsigned char support_tcoef;
	enum tfa98xx_dai_bitmap daimap;
	int mohm[3]; /* > speaker calibration values in milli ohms -1 is error */
	//struct tfa_device_ops dev_ops; // remark by jimmy
	uint16_t interrupt_enable[3];
	uint16_t interrupt_status[3];
	int ext_dsp; /* respond to external DSP: 0:none, 1:cold, 2:warm  */
	int is_cold; /* respond to MANSTATE, before tfa_run_speaker_boost */
	enum tfadsp_event_en tfadsp_event;
	int default_boost_trip_level;
	int saam_use_case; /* 0: not in use, 1: RaM / SaM only, 2: bidirectional */
	int stream_state; /* b0: pstream (Rx), b1: cstream (Tx), b2: samstream (SaaM) */
#if defined(TFADSP_DSP_BUFFER_POOL)
	struct tfa98xx_buffer_pool buf_pool[POOL_MAX_INDEX];
#endif
};

struct tfa_spk_header {
	struct tfa_header hdr;
	char name[8];				// speaker nick name (e.g. ?œdumbo??
	char vendor[16];
	char type[8];
	//	dimensions (mm)
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint16_t ohm;
};

struct tfa_fw_ver {
	uint8_t Major;
	uint8_t minor;
	uint8_t minor_update:6;
	uint8_t Update:2;
};

struct tfa_speaker_file {
	struct tfa_header hdr;
	char name[8];				// speaker nick name (e.g. ?œdumbo??
	char vendor[16];
	char type[8];
	//	dimensions (mm)
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	uint8_t data[]; //payload TFA98XX_SPEAKERPARAMETER_LENGTH
};

static struct tfa_container* g_cont = NULL; /* container file */
static int g_devs=-1; // nr of devices TODO use direct access to cont?
static struct tfa_device_list *g_dev[TFACONT_MAXDEVS];
static int g_profs[TFACONT_MAXDEVS];
static struct tfa_profile_list  *g_prof[TFACONT_MAXDEVS][TFACONT_MAXPROFS];
static int is_cold = 1;
static int buf_pool_size[POOL_MAX_INDEX] = {64*1024, 64*1024, 64*1024, 64*1024, 64*1024, 8*1024};

#define MAX_HANDLES 4
struct tfa98xx_handle_private handles_local[MAX_HANDLES];

uint32_t swap_uint32(uint32_t val)
{
	uint32_t b0,b1,b2,b3;
	b0 = (val & 0xff) << 24;
	b1 = (val & 0xff00) << 8;
	b2 = (val & 0xff0000) >> 8;
	b3 = (val & 0xff000000) >> 24;

	return (b0|b1|b2|b3);
}

uint16_t cpu_to_be16(uint16_t val)
{
	uint16_t b0,b1;
	b0 = (val & 0xff) << 8;
	b1 = (val & 0xff00) >> 8;

	return (b0|b1);
}

#define BIT(x) (1 << (x))

char *get_command_string(uint8_t module_id, uint8_t param_id)
{
	if(module_id == MODULE_FRAMEWORK) { // MODULE_FRAMEWORK
		if(param_id == FW_PAR_ID_SET_BAT_FACTORS)
			return "FW_PAR_ID_SET_BAT_FACTORS";
		else if(param_id == FW_PAR_ID_SET_MEMORY)
			return "FW_PAR_ID_SET_MEMORY";
		else if(param_id == FW_PAR_ID_SET_SENSES_DELAY)
			return "FW_PAR_ID_SET_SENSES_DELAY";
		else if(param_id == FW_PAR_ID_SETSENSESCAL)
			return "FW_PAR_ID_SETSENSESCAL";
		else if(param_id == FW_PAR_ID_SET_INPUT_SELECTOR)
			return "FW_PAR_ID_SET_INPUT_SELECTOR";
		else if(param_id == FW_PAR_ID_SET_OUTPUT_SELECTOR)
			return "FW_PAR_ID_SET_OUTPUT_SELECTOR";
		else if(param_id == FW_PAR_ID_SET_PROGRAM_CONFIG)
			return "FW_PAR_ID_SET_PROGRAM_CONFIG";
		else if(param_id == FW_PAR_ID_SET_GAINS)
			return "FW_PAR_ID_SET_GAINS";
		else if(param_id == FW_PAR_ID_SET_MEMTRACK)
			return "FW_PAR_ID_SET_MEMTRACK";
		else if(param_id == FW_PAR_ID_SET_FWKUSECASE)
			return "FW_PAR_ID_SET_FWKUSECASE";
		else if(param_id == FW_PAR_ID_SET_HW_CONFIG)
			return "FW_PAR_ID_SET_HW_CONFIG";
		else if(param_id == FW_PAR_ID_SET_CHIP_TEMPSELECTOR)
			return "FW_PAR_ID_SET_CHIP_TEMPSELECTOR";
		else if(param_id == TFA1_FW_PAR_ID_SET_CURRENT_DELAY)
			return "TFA1_FW_PAR_ID_SET_CURRENT_DELAY";
		else if(param_id == TFA1_FW_PAR_ID_SET_CURFRAC_DELAY)
			return "TFA1_FW_PAR_ID_SET_CURFRAC_DELAY";
		else if(param_id == FW_PAR_ID_GET_MEMORY)
			return "FW_PAR_ID_GET_MEMORY";
		else if(param_id == FW_PAR_ID_GLOBAL_GET_INFO)
			return "FW_PAR_ID_GLOBAL_GET_INFO";
		else if(param_id == FW_PAR_ID_GET_FEATURE_INFO)
			return "FW_PAR_ID_GET_FEATURE_INFO";
		else if(param_id == FW_PAR_ID_GET_MEMTRACK)
			return "FW_PAR_ID_GET_MEMTRACK";
		else if(param_id == FW_PAR_ID_GET_TAG)
			return "FW_PAR_ID_GET_TAG";
		else if(param_id == FW_PAR_ID_GET_API_VERSION)
			return "FW_PAR_ID_GET_API_VERSION";
		else if(param_id == FW_PAR_ID_GET_STATUS_CHANGE)
			return "FW_PAR_ID_GET_STATUS_CHANGE";
	} else if(module_id == MODULE_SPEAKERBOOST) { // MODULE_SPEAKERBOOST
		if(param_id == SB_PARAM_SET_ALGO_PARAMS)
			return "SB_PARAM_SET_ALGO_PARAMS";
		else if(param_id == SB_PARAM_SET_LAGW)
			return "SB_PARAM_SET_LAGW";
		else if(param_id == SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET)
			return "SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET";
		else if(param_id == SB_PARAM_SET_MUTE)
			return "SB_PARAM_SET_MUTE";
		else if(param_id == SB_PARAM_SET_VOLUME)
			return "SB_PARAM_SET_VOLUME";
		else if(param_id == SB_PARAM_SET_RE25C)
			return "SB_PARAM_SET_RE25C";
		else if(param_id == SB_PARAM_SET_LSMODEL)
			return "SB_PARAM_SET_LSMODEL";
		else if(param_id == SB_PARAM_SET_MBDRC)
			return "SB_PARAM_SET_MBDRC";
		else if(param_id == SB_PARAM_SET_MBDRC_WITHOUT_RESET)
			return "SB_PARAM_SET_MBDRC_WITHOUT_RESET";
		else if(param_id == SB_PARAM_SET_DRC)
			return "SB_PARAM_SET_DRC";
		else if(param_id == SB_PARAM_GET_ALGO_PARAMS)
			return "SB_PARAM_GET_ALGO_PARAMS";
		else if(param_id == SB_PARAM_GET_LAGW)
			return "SB_PARAM_GET_LAGW";
		else if(param_id == SB_PARAM_GET_RE25C)
			return "SB_PARAM_GET_RE25C";
		else if(param_id == SB_PARAM_GET_LSMODEL)
			return "SB_PARAM_GET_LSMODEL";
		else if(param_id == SB_PARAM_GET_MBDRC)
			return "SB_PARAM_GET_MBDRC";
		else if(param_id == SB_PARAM_GET_MBDRC_DYNAMICS)
			return "SB_PARAM_GET_MBDRC_DYNAMICS";
		else if(param_id == SB_PARAM_GET_TAG)
			return "SB_PARAM_GET_TAG";
		else if(param_id == SB_PARAM_SET_EXCURSION_FILTERS)
			return "SB_PARAM_SET_EXCURSION_FILTERS";
		else if(param_id == SB_PARAM_SET_DATA_LOGGER)
			return "SB_PARAM_SET_DATA_LOGGER";
		else if(param_id == SB_PARAM_SET_CONFIG)
			return "SB_PARAM_SET_CONFIG";
		else if(param_id == SB_PARAM_SET_AGCINS)
			return "SB_PARAM_SET_AGCINS";
		else if(param_id == SB_PARAM_SET_CURRENT_DELAY)
			return "SB_PARAM_SET_CURRENT_DELAY";
		else if(param_id == SB_PARAM_GET_STATE)
			return "SB_PARAM_GET_STATE";
		else if(param_id == SB_PARAM_GET_XMODEL)
			return "SB_PARAM_GET_XMODEL";
		else if(param_id == SB_PARAM_SET_RE0)
			return "SB_PARAM_SET_RE0";
	} else if(module_id == MODULE_BIQUADFILTERBANK) { // MODULE_BIQUADFILTERBANK
		if(param_id == BFB_PAR_ID_SET_COEFS)
			return "BFB_PAR_ID_SET_COEFS";
		else if(param_id == BFB_PAR_ID_GET_COEFS)
			return "BFB_PAR_ID_GET_COEFS";
		else if(param_id == BFB_PAR_ID_GET_CONFIG)
			return "BFB_PAR_ID_GET_CONFIG";
	}
	return "unknown_command";
}

/*
 * return device list dsc from index
 */
struct tfa_device_list *tfa_cont_get_dev_list(struct tfa_container * cont, int dev_idx)
{
	uint8_t *base = (uint8_t *) cont;

	if ( (dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dsc_device)
		return NULL;

	base += cont->index[dev_idx].offset;
	return (struct tfa_device_list *) base;
}

struct tfa_profile_list *tfa_cont_get_dev_prof_list(struct tfa_container * cont, int dev_idx,
					   int prof_idx)
{
	struct tfa_device_list *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;

	dev = tfa_cont_get_dev_list(cont, dev_idx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dsc_profile) {
				if (prof_idx == hit++)
					return (struct tfa_profile_list *) (dev->list[idx].offset + base);
			}
		}
	}

	return NULL;
}

struct tfa_device_list *tfa_cont_device(int dev_idx) {
	if(dev_idx < g_devs) {
		if (g_dev[dev_idx] == NULL)
			return NULL;
		return g_dev[dev_idx];
	}
	//pr_err("Devlist index too high:%d!", idx);
	return NULL;
}

static struct tfa_volume_step_message_info *
tfa_cont_get_msg_info_from_reg(struct tfa_volume_step_register_info *reg_info)
{
	char *p = (char*) reg_info;
	p += sizeof(reg_info->nr_of_registers) + (reg_info->nr_of_registers * sizeof(uint32_t));
	return (struct tfa_volume_step_message_info*) p;
}

static int
tfa_cont_get_msg_len(struct  tfa_volume_step_message_info *msg_info)
{
	return (msg_info->message_length.b[0] << 16) + (msg_info->message_length.b[1] << 8) + msg_info->message_length.b[2];
}

static struct tfa_volume_step_message_info *
tfa_cont_get_next_msg_info(struct  tfa_volume_step_message_info *msg_info)
{
	char *p = (char*) msg_info;
	int msgLen = tfa_cont_get_msg_len(msg_info);
	int type = msg_info->message_type;

	p += sizeof(msg_info->message_type) + sizeof(msg_info->message_length);
	if (type == 3)
		p += msgLen;
	else
		p += msgLen * 3;

	return (struct tfa_volume_step_message_info*) p;
}

static struct  tfa_volume_step_register_info*
tfa_cont_get_next_reg_from_end_info(struct  tfa_volume_step_message_info *msg_info)
{
	char *p = (char*) msg_info;
	p += sizeof(msg_info->nr_of_messages);
	return (struct tfa_volume_step_register_info*) p;

}

static struct tfa_volume_step_register_info*
tfa_cont_get_reg_for_vstep(struct tfa_volume_step_max2_file *vp, int idx)
{
	int i, j, nrMessage;

	struct tfa_volume_step_register_info *reg_info
		= (struct tfa_volume_step_register_info*) vp->vsteps_bin;
	struct tfa_volume_step_message_info *msg_info = NULL;

	for (i = 0; i < idx; i++) {
		msg_info = tfa_cont_get_msg_info_from_reg(reg_info);
		nrMessage = msg_info->nr_of_messages;

		for (j = 0; j < nrMessage; j++) {
			msg_info = tfa_cont_get_next_msg_info(msg_info);
		}
		reg_info = tfa_cont_get_next_reg_from_end_info(msg_info);
	}

	return reg_info;
}

#define tfa98xx_handle_t int
int tfa98xx_buffer_pool_access(tfa98xx_handle_t handle, int r_index, size_t g_size, int control)
{
	int index;

	switch (control) {
		case POOL_GET: // get
			for (index = 0; index < POOL_MAX_INDEX; index++)
			{
				if (handles_local[handle].buf_pool[index].in_use)
					continue;
				if (handles_local[handle].buf_pool[index].size < g_size)
					continue;
				handles_local[handle].buf_pool[index].in_use = 1;
				//printf("dev %d - get buffer_pool[%d]\n", handle, index);
				return index;
			}

			printf("dev %d - failed to get buffer_pool\n", handle);
			break;

		case POOL_RETURN: // return
			if (handles_local[handle].buf_pool[r_index].in_use == 0) {
				printf("dev %d - buffer_pool[%d] is not in use\n", handle, r_index);
				break;
			}

			//printf("dev %d - return buffer_pool[%d]\n", handle, r_index);
			memset(handles_local[handle].buf_pool[r_index].pool, 0, handles_local[handle].buf_pool[r_index].size);
			handles_local[handle].buf_pool[r_index].in_use = 0;

			return 0;
			break;

		default:
			printf("wrong control\n");
			break;
	}

	return -1;
}

int tfa98xx_cnt_max_device(void) {
	return (g_cont != NULL) ? ((g_cont->ndev < TFACONT_MAXDEVS) ? g_cont->ndev : TFACONT_MAXDEVS) : 0;
}

enum tfa98xx_error tfa_buffer_pool(int index, int size, int control)
{
	int dev, devcount = tfa98xx_cnt_max_device();

	switch (control) {
		case POOL_ALLOC: // allocate
			for (dev = 0; dev < devcount; dev++) {
				handles_local[dev].buf_pool[index].pool = malloc(size);
				if (handles_local[dev].buf_pool[index].pool == NULL)
				{
					//printf("tfa_buffer_pool: dev %d - buffer_pool[%d] - kmalloc error %d bytes\n", dev, index, size);
					handles_local[dev].buf_pool[index].size = 0;
					handles_local[dev].buf_pool[index].in_use = 1;
					return TFA98XX_ERROR_FAIL;
				}
				//printf("tfa_buffer_pool: dev %d - buffer_pool[%d] - malloc allocated %d bytes\n", dev, index, size);
				handles_local[dev].buf_pool[index].size = size;
				handles_local[dev].buf_pool[index].in_use = 0;
			}
			break;

		case POOL_FREE: // deallocate
			for (dev = 0; dev < devcount; dev++) {
				if (handles_local[dev].buf_pool[index].pool != NULL)
					free(handles_local[dev].buf_pool[index].pool);
				//printf("tfa_buffer_pool: dev %d - buffer_pool[%d] - free\n", dev, index);
				handles_local[dev].buf_pool[index].pool = NULL;
				handles_local[dev].buf_pool[index].size = 0;
				handles_local[dev].buf_pool[index].in_use = 0;
			}
			break;

		default:
			printf("tfa_buffer_pool: wrong control\n");
			break;
	}

	return TFA98XX_ERROR_OK;
}

int32_t g_out32buf[512] = {0,};
static uint32_t tfa_msg24to32(int32_t *out32buf, const uint8_t *in24buf, int length)
{
	int i = 0;
	int cmd_index = 0;
	int32_t tmp;
	uint32_t buf32_len = (length/3)*4;

	while (length > 0) {
		tmp = ((uint8_t)in24buf[i] << 16)
			+ ((uint8_t)in24buf[i+1] << 8)
			+ (uint8_t)in24buf[i+2];

		/* Sign extend to 32-bit from 24-bit */
		out32buf[cmd_index] = ((int32_t)tmp << 8) >> 8;

		cmd_index++;
		i = i+3;
		length = length-3;
	}

	return buf32_len;
}

FILE * pFileHeader = NULL;
uint8_t cmd_count = 1;
void fwrite_message(uint32_t* command, uint32_t length, char *str_cmd)
{
  char buffer[256];
  uint8_t pos = 0;
  uint32_t size = length / 4;

  if(pFileHeader == NULL)
	  return;

  fprintf(pFileHeader, "\n// %s\n", str_cmd);

  memset((int8_t*)buffer, 0x0, 256);
  sprintf(buffer, "%s%d%s", "const int CMD", cmd_count, "[]={");
  pos = strlen(buffer);

  for(int i = 0; i < size ; i++)
  {
    char one_32bit[11];
    sprintf(one_32bit, "0x%08x", command[i]);
    if((i % 20) == 0 &&  i != 0 && size != (i -1)) //every 20th, put new line
    {
      fprintf(pFileHeader, "%s\n", buffer);
      memset((int8_t*)buffer, 0x0, 256);

      sprintf(buffer, "%s", "                 ");
      pos = strlen(buffer);
    }
    sprintf(buffer + pos, "%s,", one_32bit);
    pos = strlen(buffer);
  }

  if(pos > 1)
  {
    buffer[pos - 1] = '}';
    buffer[pos] = ';';
  }
  fprintf(pFileHeader, "%s\n", buffer);
}

void print_message(uint32_t* command, uint32_t length)
{
  char buffer[256];
  uint8_t pos = 0;
  uint32_t size = length / 4;

  memset((int8_t*)buffer, 0x0, 256);
  sprintf(buffer, "%s", "[CMD] const int MSG[]={");
  pos = strlen(buffer);

  for(int i = 0; i < size ; i++)
  {
    char one_32bit[11];
    sprintf(one_32bit, "0x%08x", command[i]);
    if((i % 20) == 0 &&  i != 0 && size != (i -1)) //every 20th, put new line
    {
      printf("%s\n", buffer);
      memset((int8_t*)buffer, 0x0, 256);

      sprintf(buffer, "%s", "[CMD]                  ");
      pos = strlen(buffer);
    }
    sprintf(buffer + pos, "%s,", one_32bit);
    pos = strlen(buffer);
  }

  if(pos > 1)
  {
    buffer[pos - 1] = '}';
    buffer[pos] = ';';
  }
  printf("%s\n", buffer);
}

enum tfa98xx_error dsp_msg(tfa98xx_handle_t device_index, int buffer_size, uint8_t *buffer)
{
	//printf("dsp_msg : idx=%d, size=%d, cmd=0x%02x%02x%02x\n", device_index, buffer_size, buffer[0], buffer[1], buffer[2]);

	uint32_t length_32bit = tfa_msg24to32(g_out32buf, buffer, buffer_size);
	printf("Set Command --> [%s], size=%d\n", get_command_string(buffer[1], buffer[2]), length_32bit);
	//printf("dsp_msg : 32bit_length = %d\n", length_32bit);
	//print_message((uint32_t *)g_out32buf, length_32bit);
	fwrite_message((uint32_t *)g_out32buf, length_32bit, get_command_string(buffer[1], buffer[2]));
	cmd_count++;

	return TFA98XX_ERROR_OK;
}

#define NR_COEFFS 6
#define NR_BIQUADS 28
#define BQ_SIZE (3 * NR_COEFFS)
#define DSP_MSG_OVERHEAD 27

#pragma pack (push, 1)
struct dsp_msg_all_coeff {
	uint8_t select_eq[3];
	uint8_t biquad[NR_BIQUADS][NR_COEFFS][3];
};
#pragma pack (pop)

static const int eq_biquads[] = {
	10, 10, 2, 2, 2, 2
};
#define NR_EQ (int)(sizeof(eq_biquads) / sizeof(int))
enum tfa98xx_error dsp_partial_coefficients(tfa98xx_handle_t dev_idx, uint8_t *prev, uint8_t *next)
{
	uint8_t bq, eq;
	int eq_offset;
	int new_cost, old_cost;
	uint32_t eq_biquad_mask[NR_EQ];
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct dsp_msg_all_coeff *data1 = (struct dsp_msg_all_coeff *)prev;
	struct dsp_msg_all_coeff *data2 = (struct dsp_msg_all_coeff *)next;

	old_cost = DSP_MSG_OVERHEAD + 3 + sizeof(struct dsp_msg_all_coeff);
	new_cost = 0;

	eq_offset = 0;
	for (eq=0; eq<NR_EQ; eq++) {
		uint8_t *eq1 = &data1->biquad[eq_offset][0][0];
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		eq_biquad_mask[eq] = 0;

		if (memcmp(eq1, eq2, BQ_SIZE*eq_biquads[eq]) != 0) {
			int nr_bq = 0;
			int bq_sz, eq_sz;

			for (bq=0; bq < eq_biquads[eq]; bq++) {
				uint8_t *bq1 = &eq1[bq*BQ_SIZE];
				uint8_t *bq2 = &eq2[bq*BQ_SIZE];

				if (memcmp(bq1, bq2, BQ_SIZE) != 0) {
					eq_biquad_mask[eq] |= (1<<bq);
					nr_bq++;
				}
			}

			bq_sz = (2 * 3 + BQ_SIZE) * nr_bq;
			eq_sz = 2 * 3 + BQ_SIZE * eq_biquads[eq];

			/* dsp message i2c transaction overhead */
			bq_sz += DSP_MSG_OVERHEAD * nr_bq;
			eq_sz += DSP_MSG_OVERHEAD;

			if (bq_sz >= eq_sz) {
				eq_biquad_mask[eq] = 0xffffffff;

				new_cost += eq_sz;

			} else {
				new_cost += bq_sz;
			}
		}
		printf("eq_biquad_mask[%d] = 0x%.8x\n", eq, eq_biquad_mask[eq]);

		eq_offset += eq_biquads[eq];
	}

	printf("cost for writing all coefficients     = %d\n", old_cost);
	printf("cost for writing changed coefficients = %d\n", new_cost);

	if (new_cost >= old_cost) {
		const int buffer_sz = 3 + sizeof(struct dsp_msg_all_coeff);
		uint8_t *buffer;

		buffer = malloc(buffer_sz);
		if (buffer == NULL)
			return TFA98XX_ERROR_FAIL;

		/* cmd id */
		buffer[0] = 0x00;
		buffer[1] = 0x82;
		buffer[2] = 0x00;

		/* parameters */
		memcpy(&buffer[3], data2, sizeof(struct dsp_msg_all_coeff));
		err = dsp_msg(dev_idx, buffer_sz, (uint8_t *)buffer);

		free(buffer);
		if (err)
			return err;

	} else {
		eq_offset = 0;
		for (eq=0; eq<NR_EQ; eq++) {
			uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

			if (eq_biquad_mask[eq] == 0xffffffff) {
				const int msg_sz = 6 + BQ_SIZE * eq_biquads[eq];
				uint8_t *msg;

				msg = malloc(msg_sz);
				if (msg == NULL)
					return TFA98XX_ERROR_FAIL;

				/* cmd id */
				msg[0] = 0x00;
				msg[1] = 0x82;
				msg[2] = 0x00;

				/* select eq and bq */
				msg[3] = 0x00;
				msg[4] = eq+1;
				msg[5] = 0x00; /* all biquads */

				/* biquad parameters */
				memcpy(&msg[6], eq2, BQ_SIZE * eq_biquads[eq]);
				err = dsp_msg(dev_idx, msg_sz, (uint8_t *)msg);

				free(msg);
				if (err)
					return err;

			} else if (eq_biquad_mask[eq] != 0) {
				for(bq=0; bq < eq_biquads[eq]; bq++) {

					if (eq_biquad_mask[eq] & (1<<bq)) {
						uint8_t *bq2 = &eq2[bq*BQ_SIZE];
						const int msg_sz = 6 + BQ_SIZE;
						uint8_t *msg;

						msg = malloc(msg_sz);
						if (msg == NULL)
							return TFA98XX_ERROR_FAIL;

						/* cmd id */
						msg[0] = 0x00;
						msg[1] = 0x82;
						msg[2] = 0x00;

						/* select eq and bq*/
						msg[3] = 0x00;
						msg[4] = eq+1;
						msg[5] = bq+1;

						/* biquad parameters */
						memcpy(&msg[6], bq2, BQ_SIZE);
						err = dsp_msg(dev_idx, msg_sz, (uint8_t *)msg);

						free(msg);
						if (err)
							return err;
					}
				}
			}
			eq_offset += eq_biquads[eq];
		}
	}

	return err;
}

static enum tfa98xx_error tfa_cont_write_vstepMax2_One(int dev_idx, struct tfa_volume_step_message_info *new_msg,
						    struct tfa_volume_step_message_info *old_msg, int enable_partial_update)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int len = (tfa_cont_get_msg_len(new_msg) - 1) * 3;
	char *buf = (char*)new_msg->parameter_data;
	uint8_t *partial = NULL;
	int partial_size = 0;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buffer_p_index = -1, partial_p_index = -1;
#endif
	uint8_t cmdid[3];
	int use_partial_coeff = 0;

	if (enable_partial_update) {
		if (new_msg->message_type != old_msg->message_type) {
			printf("Message type differ - Disable Partial Update\n");
			enable_partial_update = 0;
		} else if (tfa_cont_get_msg_len(new_msg) != tfa_cont_get_msg_len(old_msg)) {
			printf("Message Length differ - Disable Partial Update\n");
			enable_partial_update = 0;
		}
	}

	if ((enable_partial_update) && (new_msg->message_type == 1)) {
		/* No patial updates for message type 1 (Coefficients) */
		enable_partial_update = 0;
	#if 0 // TODO rev=0x3b72 in the log
		if ((tfa98xx_dev_revision(dev_idx) & 0xff ) == 0x88)
			use_partial_coeff = 1;
	#endif
	}

	/* Change Message Len to the actual buffer len */
	memcpy(cmdid, new_msg->cmd_id, sizeof(cmdid));

	/* The algoparams and mbdrc msg id will be changed to the reset type when SBSL=0
	 * if SBSL=1 the msg will remain unchanged. It's up to the tuning engineer to choose the 'without_reset'
	 * types inside the vstep. In other words: the reset msg is applied during SBSL==0 else it remains unchanged.
	 */
	//printf("is_cold = %d\n", is_cold);
	// if (TFA_GET_BF(dev_idx, SBSL) == 0) {
	if (is_cold == 1) {
		//uint8_t org_cmd = cmdid[2];

		if ((new_msg->message_type == 0) &&
			(cmdid[2] != SB_PARAM_SET_ALGO_PARAMS)) // SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET
		{
			//printf("P-ID for SetAlgoParams modified! cmdid[2]=0x%2x (to 0x00)\n", cmdid[2]);
			cmdid[2] = SB_PARAM_SET_ALGO_PARAMS;
		} else if ((new_msg->message_type == 2) &&
			(cmdid[2] != SB_PARAM_SET_MBDRC)) // SB_PARAM_SET_MBDRC_WITHOUT_RESET
		{
			//printf("P-ID for SetMBDrc modified! cmdid[2]=0x%2x (to 0x07)\n", cmdid[2]);
			cmdid[2] = SB_PARAM_SET_MBDRC;
		}

		//if (org_cmd != cmdid[2])
		//	printf("P-ID: cmdid[2]=0x%02x to 0x%02x\n", org_cmd, cmdid[2]);
	}

	/*
	 * +sizeof(struct tfa_partial_msg_block) will allow to fit one
	 * additonnal partial block If the partial update goes over the len of
	 * a regular message ,we can safely write our block and check afterward
	 * that we are over the size of a usual update
	 */
	if (enable_partial_update) {
		partial_size = (sizeof(uint8_t) * len) + sizeof(struct tfa_partial_msg_block);
#if defined(TFADSP_DSP_BUFFER_POOL)
		partial_p_index = tfa98xx_buffer_pool_access(dev_idx, -1, partial_size, POOL_GET);
		if (partial_p_index != -1) {
			//printf("allocated from buffer_pool[%d] for %d bytes\n", partial_p_index, partial_size);
			partial = (uint8_t *)(handles_local[dev_idx].buf_pool[partial_p_index].pool);
		} else {
			partial = malloc(partial_size);
		}
#else
		partial = kmalloc(partial_size, GFP_KERNEL);
#endif // TFADSP_DSP_BUFFER_POOL
		if (!partial)
			printf("Partial update memory error - Disabling\n");
	}

	if (partial) {
		uint8_t offset = 0, i = 0;
		uint16_t *change;
		uint8_t *n = new_msg->parameter_data;
		uint8_t *o = old_msg->parameter_data;
		uint8_t *p = partial;
		uint8_t* trim = partial;

		/* set dspFiltersReset */
		*p++ = 0x02;
		*p++ = 0x00;
		*p++ = 0x00;

		while ((o < (old_msg->parameter_data + len)) &&
		      (p < (partial + len - 3))) {
			if ((offset == 0xff) ||
			    (memcmp(n, o, 3 * sizeof(uint8_t)))) {
				*p++ = offset;
				change = (uint16_t*) p;
				*change = 0;
				p += 2;

				for (i = 0;
				     (i < 16) && (o < (old_msg->parameter_data + len));
				    i++, n += 3, o += 3) {
					if (memcmp(n, o, 3 * sizeof(uint8_t))) {
						*change |= BIT(i);
						memcpy(p, n, 3);
						p += 3;
						trim = p;
					}
				}

				offset = 0;
				*change = cpu_to_be16(*change);
			} else {
				n += 3;
				o += 3;
				offset++;
			}
		}

		if (trim == partial) {
			printf("No Change in message - discarding %d bytes\n", len);
			len = 0;

		} else if (trim < (partial + len - 3)) {
			printf("Using partial update: %d -> %d bytes\n", len , (int)(trim-partial+3));

			/* Add the termination marker */
			memset(trim, 0x00, 3);
			trim += 3;

			/* Signal This will be a partial update */
			cmdid[2] |= BIT(6);
			buf = (char*) partial;
			len = (int)(trim - partial);
		} else {
			printf("Partial too big - use regular update\n");
		}
	}

	if (use_partial_coeff) {
		err = dsp_partial_coefficients(dev_idx, old_msg->parameter_data, new_msg->parameter_data);
	} else if (len) {
		uint8_t *buffer;
		//printf("Command-ID used: 0x%02x%02x%02x \n", cmdid[0], cmdid[1], cmdid[2]);

#if defined(TFADSP_DSP_BUFFER_POOL)
		buffer_p_index = tfa98xx_buffer_pool_access(dev_idx, -1, 3 + len, POOL_GET);
		if (buffer_p_index != -1) {
			//printf("allocated from buffer_pool[%d] for %d bytes\n", buffer_p_index, 3 + len);
			buffer = (uint8_t *)(handles_local[dev_idx].buf_pool[buffer_p_index].pool);
		} else {
			buffer = malloc(3 + len);
			if (buffer == NULL) {
				printf("kmalloc error %d bytes\n", 3 + len);
				goto tfa_cont_write_vstepMax2_One_error_exit;
			}
		}
#else
		buffer = kmalloc(3 + len, GFP_KERNEL);
		if (buffer == NULL)
			goto tfa_cont_write_vstepMax2_One_error_exit;
#endif // TFADSP_DSP_BUFFER_POOL

		memcpy(&buffer[0], cmdid, 3);
		memcpy(&buffer[3], buf, len);
		err = dsp_msg(dev_idx, 3 + len, (uint8_t *)buffer);

#if defined(TFADSP_DSP_BUFFER_POOL)
		if (buffer_p_index != -1) {
			tfa98xx_buffer_pool_access(dev_idx, buffer_p_index, 0, POOL_RETURN);
		} else {
			free(buffer);
		}
#else
		kfree(buffer);
#endif // TFADSP_DSP_BUFFER_POOL
	}

tfa_cont_write_vstepMax2_One_error_exit:
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (partial_p_index != -1) {
		tfa98xx_buffer_pool_access(dev_idx, partial_p_index, 0, POOL_RETURN);
	} else {
		if (partial)
			free(partial);
	}
#else
	if (partial)
		kfree(partial);
#endif // TFADSP_DSP_BUFFER_POOL

	return err;
}

void create_dsp_buffer_msg(struct tfa_msg *msg, char *buffer, int *size)
{
        int i, j = 0;

        /* Copy cmd_id. Remember that the cmd_id is reversed */
        buffer[0] = msg->cmd_id[2];
        buffer[1] = msg->cmd_id[1];
        buffer[2] = msg->cmd_id[0];

        /* Copy the data to the buffer */
        for(i=3; i<3+(msg->msg_size*3); i++) {
                buffer[i] = (uint8_t) ((msg->data[j] >> 16) & 0xffff);
                i++;
                buffer[i] = (uint8_t) ((msg->data[j] >> 8) & 0xff);
                i++;
                buffer[i] = (uint8_t) (msg->data[j] & 0xff);
                j++;
        }

        *size = (3+(msg->msg_size*3)) * sizeof(char);
}

int tfa_set_swvstep(tfa98xx_handle_t handle, unsigned short new_value)
{
	///int mtpk, active_value = tfa_get_swvstep(handle);
	//unsigned short rev = handles_local[handle].rev & 0xff;
	handles_local[handle].vstep[0]=new_value;
	handles_local[handle].vstep[1]=new_value;

	//return active_value;
	return 0;
}

static enum tfa98xx_error tfa_cont_write_vstepMax2(int dev_idx, struct tfa_volume_step_max2_file *vp, int vstep_idx, int vstep_msg_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	static struct tfa_volume_step_register_info *p_reg_info = NULL;
	struct tfa_volume_step_register_info *reg_info = NULL;
	struct tfa_volume_step_message_info *msg_info = NULL, *p_msg_info = NULL;
	//struct tfa_bitfield bit_f;
	int i, nr_messages, enp = 0;

	if(vstep_idx >= vp->nr_of_vsteps) {
		printf("Volumestep %d is not available \n", vstep_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if ( p_reg_info == NULL) {
		//printf("Inital vstep write\n");
		enp = 0;
 	}

	reg_info = tfa_cont_get_reg_for_vstep(vp, vstep_idx);

	msg_info = tfa_cont_get_msg_info_from_reg(reg_info);
	nr_messages = msg_info->nr_of_messages;

	if (enp) {
		p_msg_info = tfa_cont_get_msg_info_from_reg(p_reg_info);
		if (nr_messages != p_msg_info->nr_of_messages) {
			printf("Message different - Disable partial update\n");
			enp = 0;
		}
	}

	for (i = 0; i < nr_messages; i++) {
		/* Messagetype(3) is Smartstudio Info! Dont send this! */
		if(msg_info->message_type == 3) {
			//printf("Skipping Message Type 3\n");
			/* message_length is in bytes */
			msg_info = tfa_cont_get_next_msg_info(msg_info);
			if(enp)
				p_msg_info = tfa_cont_get_next_msg_info(p_msg_info);
			continue;
		}

		/* If no vstepMsgIndex is passed on, all message needs to be send */
		if ((vstep_msg_idx >= TFA_MAX_VSTEP_MSG_MARKER) || (vstep_msg_idx == i)) {
			err = tfa_cont_write_vstepMax2_One(dev_idx, msg_info, p_msg_info, enp);
			if (err != TFA98XX_ERROR_OK) {
				/*
				 * Force a full update for the next write
				 * As the current status of the DSP is unknown
				 */
				p_reg_info = NULL;
				return err;
			}
		}

		msg_info = tfa_cont_get_next_msg_info(msg_info);
		if(enp)
			p_msg_info = tfa_cont_get_next_msg_info(p_msg_info);
	}

	p_reg_info = reg_info;

#if 0 // remark by jimmy
	for(i=0; i<reg_info->nr_of_registers*2; i++) {
		/* Byte swap the datasheetname */
		bit_f.field = (uint16_t)(reg_info->register_info[i]>>8) | (reg_info->register_info[i]<<8);
		i++;
		bit_f.value = (uint16_t)reg_info->register_info[i]>>8;
		err = tfa_run_write_bitfield(dev_idx , bit_f);
		if (err != TFA98XX_ERROR_OK)
			return err;
	}
#endif

	/* Save the current vstep */
	tfa_set_swvstep(dev_idx, (unsigned short)vstep_idx);

	return err;
}

enum tfa98xx_error tfa_cont_write_file(int dev_idx,  struct tfa_file_dsc *file, int vstep_idx, int vstep_msg_idx)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_header *hdr = (struct tfa_header *)file->data;
	enum tfa_header_type type;
	int size;

	type = (enum tfa_header_type) hdr->id;

	switch (type) {
	case msg_hdr: /* generic DSP message */
		//printf("tfa_cont_write_file : type=msg_hdr\n");
		size = hdr->size - sizeof(struct tfa_msg_file);
		err = dsp_msg(dev_idx, size, (uint8_t *)((struct tfa_msg_file *)hdr)->data);
		break;
	case volstep_hdr:
		// vstep_idx=0, vstep_msg_idx=100
		tfa_cont_write_vstepMax2(dev_idx, (struct tfa_volume_step_max2_file *)hdr, vstep_idx, vstep_msg_idx);
		//printf("tfa_cont_write_file : type=volstep_hdr\n");
		break;
	case speaker_hdr:
		//printf("tfa_cont_write_file : type=speaker_hdr\n");
		size = hdr->size - sizeof(struct tfa_spk_header) - sizeof(struct tfa_fw_ver);
		err = dsp_msg(dev_idx, size, (uint8_t *)(((struct tfa_speaker_file *)hdr)->data + (sizeof(struct tfa_fw_ver))));
		break;
	case preset_hdr:
		//printf("tfa_cont_write_file : type=preset_hdr\n");
		break;
	case equalizer_hdr:
		//printf("tfa_cont_write_file : type=equalizer_hdr\n");
		break;
	case patch_hdr:
		//printf("tfa_cont_write_file : type=patch_hdr\n");
		break;
	case config_hdr:
		//printf("tfa_cont_write_file : type=config_hdr\n");
		break;
	case drc_hdr:
		//printf("tfa_cont_write_file : type=drc_hdr\n");
		break;
	case info_hdr:
		//printf("tfa_cont_write_file : type=info_hdr\n");
		/* Ignore */
		break;
	default:
		printf("Header is of unknown type: 0x%x\n", type);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return err;
}

enum tfa98xx_error tfa_cont_write_files(int dev_idx) {
	struct tfa_device_list *dev = tfa_cont_device(dev_idx);
	struct tfa_file_dsc *file;
	//struct tfa_cmd *cmd;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0}; //every word requires 3 bytes, and 3 is the msg
	int i, size = 0;

	if ( !dev ) {
		return TFA98XX_ERROR_BAD_PARAMETER;
	}
	/* process the list and write all files  */
	for(i=0;i<dev->length;i++) {
		if ( dev->list[i].type == dsc_file ) {
			file = (struct tfa_file_dsc *)(dev->list[i].offset+(uint8_t *)g_cont);
			if ( tfa_cont_write_file(dev_idx,  file, 0 , TFA_MAX_VSTEP_MSG_MARKER) ){ // 0, 100
				return TFA98XX_ERROR_BAD_PARAMETER;
			}
		}

		if  ( dev->list[i].type == dsc_set_input_select ||
		      dev->list[i].type == dsc_set_output_select ||
		      dev->list[i].type == dsc_set_program_config ||
		      dev->list[i].type == dsc_set_lag_w ||
		      dev->list[i].type == dsc_set_gains ||
		      dev->list[i].type == dsc_set_vbat_factors ||
		      dev->list[i].type == dsc_set_senses_cal ||
		      dev->list[i].type == dsc_set_senses_delay ||
		      dev->list[i].type == dsc_set_mb_drc ) {
			//create_dsp_buffer_msg((struct tfa_msg *) ( dev->list[i].offset+(char*)g_cont), buffer, &size);

			err = dsp_msg(dev_idx, size, (uint8_t *)buffer);
		}

		if  ( dev->list[i].type == dsc_cmd ) {
			size = *(uint16_t *)(dev->list[i].offset+(char*)g_cont);
			err = dsp_msg(dev_idx, size,  (uint8_t *)(dev->list[i].offset+2+(char*)g_cont));
		#if 0
			if ( tfa98xx_cnt_verbose ) {
				cmd = (struct tfa_cmd *)(dev->list[i].offset+(uint8_t *)g_cont);
				printf("Writing cmd=0x%02x%02x%02x \n", cmd->value[0], cmd->value[1], cmd->value[2]);
			}
		#endif
		}
		if (err != TFA98XX_ERROR_OK)
			break;

		if  ( dev->list[i].type == dsc_cf_mem ) {
			//err = tfa_run_write_dsp_mem(dev_idx, (struct tfa_dsp_mem *)(dev->list[i].offset+(uint8_t *)g_cont));
		}

		if (err != TFA98XX_ERROR_OK)
			break;
	}

	return err;
}

struct tfa_profile_list* tfa_cont_profile(int dev_idx, int prof_ipx) {
	if ( dev_idx >= g_devs) {
		printf("Devlist index too high");
		return NULL;
	}
	if ( prof_ipx >= g_profs[dev_idx]) {
		printf("Proflist index too high");
		return NULL;
	}

	return g_prof[dev_idx][prof_ipx];
}

enum tfa98xx_error tfa_cont_write_files_prof(int dev_idx, int prof_idx, int vstep_idx) {
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_profile_list *prof = tfa_cont_profile(dev_idx, prof_idx);
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0}; //every word requires 3 bytes, and 3 is the msg
	unsigned int i;
	struct tfa_file_dsc *file;
	//struct tfa_patch_file *patchfile;
	int size;

	if ( !prof ) {
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	//printf("tfa_cont_write_files_prof : length=%d, name=%s\n", prof->length, prof->name.offset + (uint8_t*)g_cont);

	/* process the list and write all files  */
	for(i=0;i<prof->length;i++) {
		switch (prof->list[i].type) {
			case dsc_file:
				//printf("tfa_cont_write_files_prof : type=dsc_file\n");
				file = (struct tfa_file_dsc *)(prof->list[i].offset+(uint8_t *)g_cont);
				err = tfa_cont_write_file(dev_idx,  file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
				break;
			case dsc_patch:
				//printf("tfa_cont_write_files_prof : type=dsc_patch\n");
				break;
			case dsc_cf_mem:
				//printf("tfa_cont_write_files_prof : type=dsc_cf_mem\n");
				break;
			case dsc_set_input_select:
			case dsc_set_output_select:
			case dsc_set_program_config:
			case dsc_set_lag_w:
			case dsc_set_gains:
			case dsc_set_vbat_factors:
			case dsc_set_senses_cal:
			case dsc_set_senses_delay:
			case dsc_set_mb_drc:
				//printf("tfa_cont_write_files_prof : type=%d\n", prof->list[i].type);
				create_dsp_buffer_msg((struct tfa_msg *)(prof->list[i].offset+(uint8_t *)g_cont), buffer, &size);
				err = dsp_msg(dev_idx, size, (uint8_t *)buffer);
				break;
			default:
				/* ignore any other type */
				break;
		}
	}

	return err;
}

static void cont_get_devs(struct tfa_container *cont) {
	struct tfa_profile_list *prof;
    //struct tfa_livedata_list *lived;
	int i,j;
	int count;

	// get nr of devlists+1
	for(i=0 ; i < cont->ndev ; i++) {
		g_dev[i] = tfa_cont_get_dev_list(cont, i); // cache it
	}

	g_devs=cont->ndev;
	// walk through devices and get the profile lists
	for (i = 0; i < g_devs; i++) {
		j=0;
		count=0;
		while ((prof = tfa_cont_get_dev_prof_list(cont, i, j)) != NULL) {
			count++;
			g_prof[i][j++] = prof;
		}
		g_profs[i] = count;    // count the nr of profiles per device
	}

        g_devs=cont->ndev;
}

char *get_profile_name(uint8_t device_idx, uint8_t profile_idx)
{
	struct tfa_profile_list *prof = tfa_cont_profile(device_idx, profile_idx);
	return (char *)(prof->name.offset + (uint8_t*)g_cont);
}

uint8_t nr_device = 0;
uint8_t nr_profile = 0;

enum tfa_error tfa_load_cnt(void *cnt, int length) {
	struct tfa_container  *cntbuf = (struct tfa_container  *)cnt;

	g_cont = NULL;

	if (length > TFA_MAX_CNT_LENGTH) {
		printf("incorrect length\n");
		return tfa_error_container;
	}

	nr_device = cntbuf->ndev;
	nr_profile = cntbuf->nprof;

	printf("id=%.2s, version=%.2s%.2s, ", cntbuf->id, cntbuf->version, cntbuf->subversion);
	printf("customer=%.8s, application=%.8s, type=%.8s\n", cntbuf->customer, cntbuf->application, cntbuf->type);
	printf("device# : %d, profile# : %d\n", nr_device, nr_profile);


	if (HDR(cntbuf->id[0],cntbuf->id[1]) == 0) {
		printf("header is 0\n");
		return tfa_error_container;
	}

	if ( (HDR(cntbuf->id[0],cntbuf->id[1])) != params_hdr ) {
		printf("wrong header type: 0x%02x 0x%02x\n", cntbuf->id[0],cntbuf->id[1]);
		return tfa_error_container;
	}

	if (cntbuf->size == 0) {
		printf("data size is 0\n");
		return tfa_error_container;
	}

	/* check sub version level */
	if ( (cntbuf->subversion[1] == NXPTFA_PM_SUBVERSION) &&
		 (cntbuf->subversion[0] == '0') ) {
		g_cont = cntbuf;
		cont_get_devs(g_cont);
	} else {
		printf("container sub-version not supported: %c%c\n",
				cntbuf->subversion[0], cntbuf->subversion[1]);
		return tfa_error_container;
	}

	return tfa_error_ok;
}

int main(int argc, char* argv[]) {
	FILE * pFileCnt = NULL;
	int file_size;

	if (argc == 1) { // no argument
		pFileCnt = fopen("Tfa9872.cnt", "rb"); // default
	} else {
		pFileCnt = fopen(argv[1], "rb"); // input .cnt file
	}

	if (pFileCnt == NULL)
	{
		printf("File open fail\n");
		exit(-1);
	}
	fseek(pFileCnt, 0, SEEK_END);
	file_size = ftell(pFileCnt);
	fseek(pFileCnt, 0, SEEK_SET); // roll-back

	uint8_t* cnt_buffer = malloc(TFA_MAX_CNT_LENGTH);
	if ((int)fread(cnt_buffer, sizeof(uint8_t), file_size, pFileCnt) != file_size)
	{
		printf("File read fail\n");
		fclose(pFileCnt);
		free(cnt_buffer);
		return -1;
	}
	fclose(pFileCnt);

/********************************************************************************/
	int index = 0;
	int dev_idx = 0;
	int profile_idx = 0;

	tfa_load_cnt((void *)cnt_buffer, file_size);
	for(index = 0; index < POOL_MAX_INDEX; index++)
		tfa_buffer_pool(index, buf_pool_size[index], POOL_ALLOC);

	printf("############### Container File is Loaded ###############\n");

#if 0
	printf("Select a profile index in the below items (0~%d)\n", nr_profile-1);
	for(int i=0; i< nr_profile; i++)
		printf("%d.%s\n", i, get_profile_name(0, i));
	scanf("%d", &profile_idx);

	if(profile_idx < 0 || profile_idx >= nr_profile) {
		printf("Selected profile index is wrong!!!\n");
		return EXIT_SUCCESS;
	}
	printf("Selected profile : %d.%s\n", profile_idx, get_profile_name(dev_idx, profile_idx));
#endif

	pFileHeader = fopen("tfadsp_commands.h", "wt");
	cmd_count = 1;
	fprintf(pFileHeader, "/* %s%d, %s%s */\n", "device index : ", dev_idx, "profile name : ", get_profile_name(dev_idx, profile_idx));

	tfa_cont_write_files(dev_idx);
	tfa_cont_write_files_prof(dev_idx, profile_idx, 0); // device_index, profile_index, vstep_index

	if(pFileHeader) {
		fclose(pFileHeader);
		pFileHeader = NULL;
	}

	for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(index, 0, POOL_FREE);
/********************************************************************************/

	free(cnt_buffer);

	printf("\ntfadsp_commands.h is generated successfully~\n");
	return EXIT_SUCCESS;
}
