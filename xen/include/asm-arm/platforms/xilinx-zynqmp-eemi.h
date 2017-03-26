/* Xen specifics:  */
/* Main PM Mediator entry.  */
bool zynqmp_eemi_mediate(register_t fid,
                         register_t a0,
                         register_t a1,
                         register_t a2,
                         register_t a3,
                         register_t a4,
                         register_t a5,
                         register_t *ret);

/* Service calls.  */
#define SVC_MASK             0x3F000000
#define SVC_SIP              0x02000000    /* SoC Implementation Specific.  */
#define PM_SIP_SVC           0xC2000000

/* SMC function IDs for SiP Service queries */
#define ZYNQMP_SIP_SVC_CALL_COUNT       0xff00
#define ZYNQMP_SIP_SVC_UID              0xff01
#define ZYNQMP_SIP_SVC_VERSION          0xff03

/* Generic PM defs:  */
/*
 * Version number is a 32bit value, like:
 * (PM_VERSION_MAJOR << 16) | PM_VERSION_MINOR
 */
#define PM_VERSION_MAJOR	0
#define PM_VERSION_MINOR	2

#define PM_VERSION	((PM_VERSION_MAJOR << 16) | PM_VERSION_MINOR)

/* Capabilities for RAM */
#define PM_CAP_ACCESS	0x1U
#define PM_CAP_CONTEXT	0x2U

#define MAX_LATENCY	(~0U)
#define MAX_QOS		100U

/* State arguments of the self suspend */
#define PM_STATE_CPU_IDLE		0x0U
#define PM_STATE_SUSPEND_TO_RAM		0xFU

enum pm_api_id {
	/* Miscellaneous API functions: */
	PM_GET_API_VERSION = 1, /* Do not change or move */
	PM_SET_CONFIGURATION,
	PM_GET_NODE_STATUS,
	PM_GET_OP_CHARACTERISTIC,
	PM_REGISTER_NOTIFIER,
	/* API for suspending of PUs: */
	PM_REQ_SUSPEND,
	PM_SELF_SUSPEND,
	PM_FORCE_POWERDOWN,
	PM_ABORT_SUSPEND,
	PM_REQ_WAKEUP,
	PM_SET_WAKEUP_SOURCE,
	PM_SYSTEM_SHUTDOWN,
	/* API for managing PM slaves: */
	PM_REQ_NODE,
	PM_RELEASE_NODE,
	PM_SET_REQUIREMENT,
	PM_SET_MAX_LATENCY,
	/* Direct control API functions: */
	PM_RESET_ASSERT,
	PM_RESET_GET_STATUS,
	PM_MMIO_WRITE,
	PM_MMIO_READ,
	PM_INIT,
	PM_FPGA_LOAD,
	PM_FPGA_GET_STATUS,
	PM_GET_CHIPID,
	PM_API_MAX
};

enum pm_node_id {
	NODE_UNKNOWN = 0,
	NODE_APU,
	NODE_APU_0,
	NODE_APU_1,
	NODE_APU_2,
	NODE_APU_3,
	NODE_RPU,
	NODE_RPU_0,
	NODE_RPU_1,
	NODE_PL,
	NODE_FPD,
	NODE_OCM_BANK_0,
	NODE_OCM_BANK_1,
	NODE_OCM_BANK_2,
	NODE_OCM_BANK_3,
	NODE_TCM_0_A,
	NODE_TCM_0_B,
	NODE_TCM_1_A,
	NODE_TCM_1_B,
	NODE_L2,
	NODE_GPU_PP_0,
	NODE_GPU_PP_1,
	NODE_USB_0,
	NODE_USB_1,
	NODE_TTC_0,
	NODE_TTC_1,
	NODE_TTC_2,
	NODE_TTC_3,
	NODE_SATA,
	NODE_ETH_0,
	NODE_ETH_1,
	NODE_ETH_2,
	NODE_ETH_3,
	NODE_UART_0,
	NODE_UART_1,
	NODE_SPI_0,
	NODE_SPI_1,
	NODE_I2C_0,
	NODE_I2C_1,
	NODE_SD_0,
	NODE_SD_1,
	NODE_DP,
	NODE_GDMA,
	NODE_ADMA,
	NODE_NAND,
	NODE_QSPI,
	NODE_GPIO,
	NODE_CAN_0,
	NODE_CAN_1,
	NODE_AFI,
	NODE_APLL,
	NODE_VPLL,
	NODE_DPLL,
	NODE_RPLL,
	NODE_IOPLL,
	NODE_DDR,
	NODE_IPI_APU,
	NODE_IPI_RPU_0,
	NODE_GPU,
	NODE_PCIE,
	NODE_PCAP,
	NODE_RTC,
	NODE_MAX
};

enum pm_request_ack {
	REQ_ACK_NO = 1,
	REQ_ACK_BLOCKING,
	REQ_ACK_NON_BLOCKING,
};

enum pm_abort_reason {
	ABORT_REASON_WKUP_EVENT = 100,
	ABORT_REASON_PU_BUSY,
	ABORT_REASON_NO_PWRDN,
	ABORT_REASON_UNKNOWN,
};

enum pm_suspend_reason {
	SUSPEND_REASON_PU_REQ = 201,
	SUSPEND_REASON_ALERT,
	SUSPEND_REASON_SYS_SHUTDOWN,
};

enum pm_ram_state {
	PM_RAM_STATE_OFF = 1,
	PM_RAM_STATE_RETENTION,
	PM_RAM_STATE_ON,
};

enum pm_opchar_type {
	PM_OPCHAR_TYPE_POWER = 1,
	PM_OPCHAR_TYPE_TEMP,
	PM_OPCHAR_TYPE_LATENCY,
};

/**
 * @PM_RET_SUCCESS:		success
 * @PM_RET_ERROR_ARGS:		illegal arguments provided
 * @PM_RET_ERROR_ACCESS:	access rights violation
 * @PM_RET_ERROR_TIMEOUT:	timeout in communication with PMU
 * @PM_RET_ERROR_NOTSUPPORTED:	feature not supported
 * @PM_RET_ERROR_PROC:		node is not a processor node
 * @PM_RET_ERROR_API_ID:	illegal API ID
 * @PM_RET_ERROR_OTHER:		other error
 */
enum pm_ret_status {
	PM_RET_SUCCESS,
	PM_RET_ERROR_ARGS,
	PM_RET_ERROR_ACCESS,
	PM_RET_ERROR_TIMEOUT,
	PM_RET_ERROR_NOTSUPPORTED,
	PM_RET_ERROR_PROC,
	PM_RET_ERROR_API_ID,
	PM_RET_ERROR_FAILURE,
	PM_RET_ERROR_COMMUNIC,
	PM_RET_ERROR_DOUBLEREQ,
	PM_RET_ERROR_OTHER,
};

/**
 * @PM_INITIAL_BOOT:	boot is a fresh system startup
 * @PM_RESUME:		boot is a resume
 * @PM_BOOT_ERROR:	error, boot cause cannot be identified
 */
enum pm_boot_status {
	PM_INITIAL_BOOT,
	PM_RESUME,
	PM_BOOT_ERROR,
};

enum pm_shutdown_type {
	PMF_SHUTDOWN_TYPE_SHUTDOWN,
	PMF_SHUTDOWN_TYPE_RESET,
};

enum pm_shutdown_subtype {
	PMF_SHUTDOWN_SUBTYPE_SUBSYSTEM,
	PMF_SHUTDOWN_SUBTYPE_PS_ONLY,
	PMF_SHUTDOWN_SUBTYPE_SYSTEM,
};

enum pm_reset {
	PM_RESET_START = 999,
	PM_RESET_PCIE_CFG,
	PM_RESET_PCIE_BRIDGE,
	PM_RESET_PCIE_CTRL,
	PM_RESET_DP,
	PM_RESET_SWDT_CRF,
	PM_RESET_AFI_FM5,
	PM_RESET_AFI_FM4,
	PM_RESET_AFI_FM3,
	PM_RESET_AFI_FM2,
	PM_RESET_AFI_FM1,
	PM_RESET_AFI_FM0,
	PM_RESET_GDMA,
	PM_RESET_GPU_PP1,
	PM_RESET_GPU_PP0,
	PM_RESET_GPU,
	PM_RESET_GT,
	PM_RESET_SATA,
	PM_RESET_ACPU3_PWRON,
	PM_RESET_ACPU2_PWRON,
	PM_RESET_ACPU1_PWRON,
	PM_RESET_ACPU0_PWRON,
	PM_RESET_APU_L2,
	PM_RESET_ACPU3,
	PM_RESET_ACPU2,
	PM_RESET_ACPU1,
	PM_RESET_ACPU0,
	PM_RESET_DDR,
	PM_RESET_APM_FPD,
	PM_RESET_SOFT,
	PM_RESET_GEM0,
	PM_RESET_GEM1,
	PM_RESET_GEM2,
	PM_RESET_GEM3,
	PM_RESET_QSPI,
	PM_RESET_UART0,
	PM_RESET_UART1,
	PM_RESET_SPI0,
	PM_RESET_SPI1,
	PM_RESET_SDIO0,
	PM_RESET_SDIO1,
	PM_RESET_CAN0,
	PM_RESET_CAN1,
	PM_RESET_I2C0,
	PM_RESET_I2C1,
	PM_RESET_TTC0,
	PM_RESET_TTC1,
	PM_RESET_TTC2,
	PM_RESET_TTC3,
	PM_RESET_SWDT_CRL,
	PM_RESET_NAND,
	PM_RESET_ADMA,
	PM_RESET_GPIO,
	PM_RESET_IOU_CC,
	PM_RESET_TIMESTAMP,
	PM_RESET_RPU_R50,
	PM_RESET_RPU_R51,
	PM_RESET_RPU_AMBA,
	PM_RESET_OCM,
	PM_RESET_RPU_PGE,
	PM_RESET_USB0_CORERESET,
	PM_RESET_USB1_CORERESET,
	PM_RESET_USB0_HIBERRESET,
	PM_RESET_USB1_HIBERRESET,
	PM_RESET_USB0_APB,
	PM_RESET_USB1_APB,
	PM_RESET_IPI,
	PM_RESET_APM_LPD,
	PM_RESET_RTC,
	PM_RESET_SYSMON,
	PM_RESET_AFI_FM6,
	PM_RESET_LPD_SWDT,
	PM_RESET_FPD,
	PM_RESET_RPU_DBG1,
	PM_RESET_RPU_DBG0,
	PM_RESET_DBG_LPD,
	PM_RESET_DBG_FPD,
	PM_RESET_APLL,
	PM_RESET_DPLL,
	PM_RESET_VPLL,
	PM_RESET_IOPLL,
	PM_RESET_RPLL,
	PM_RESET_GPO3_PL_0,
	PM_RESET_GPO3_PL_1,
	PM_RESET_GPO3_PL_2,
	PM_RESET_GPO3_PL_3,
	PM_RESET_GPO3_PL_4,
	PM_RESET_GPO3_PL_5,
	PM_RESET_GPO3_PL_6,
	PM_RESET_GPO3_PL_7,
	PM_RESET_GPO3_PL_8,
	PM_RESET_GPO3_PL_9,
	PM_RESET_GPO3_PL_10,
	PM_RESET_GPO3_PL_11,
	PM_RESET_GPO3_PL_12,
	PM_RESET_GPO3_PL_13,
	PM_RESET_GPO3_PL_14,
	PM_RESET_GPO3_PL_15,
	PM_RESET_GPO3_PL_16,
	PM_RESET_GPO3_PL_17,
	PM_RESET_GPO3_PL_18,
	PM_RESET_GPO3_PL_19,
	PM_RESET_GPO3_PL_20,
	PM_RESET_GPO3_PL_21,
	PM_RESET_GPO3_PL_22,
	PM_RESET_GPO3_PL_23,
	PM_RESET_GPO3_PL_24,
	PM_RESET_GPO3_PL_25,
	PM_RESET_GPO3_PL_26,
	PM_RESET_GPO3_PL_27,
	PM_RESET_GPO3_PL_28,
	PM_RESET_GPO3_PL_29,
	PM_RESET_GPO3_PL_30,
	PM_RESET_GPO3_PL_31,
	PM_RESET_RPU_LS,
	PM_RESET_PS_ONLY,
	PM_RESET_PL,
	PM_RESET_END
};